
#include "fmacros.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "hircluster.h"
#include "hiutil.h"
#include "adlist.h"
#include "hiarray.h"
#include "command.h"
#include "dict.c"

#define REDIS_COMMAND_CLUSTER_NODES "CLUSTER NODES"
#define REDIS_COMMAND_CLUSTER_SLOTS "CLUSTER SLOTS"

#define REDIS_COMMAND_ASKING "ASKING"
#define REDIS_COMMAND_PING "PING"

#define REDIS_PROTOCOL_ASKING "*1\r\n$6\r\nASKING\r\n"

#define IP_PORT_SEPARATOR ":"

#define CLUSTER_ADDRESS_SEPARATOR ","

#define CLUSTER_DEFAULT_MAX_REDIRECT_COUNT 5

typedef struct cluster_async_data
{
    redisClusterAsyncContext *acc;
    struct cmd *command;
    redisClusterCallbackFn *callback;
    int retry_count;
    void *privdata;
}cluster_async_data;

typedef enum CLUSTER_ERR_TYPE{
    CLUSTER_NOT_ERR = 0,
    CLUSTER_ERR_MOVED,
    CLUSTER_ERR_ASK,
    CLUSTER_ERR_TRYAGAIN,
    CLUSTER_ERR_CROSSSLOT,
    CLUSTER_ERR_CLUSTERDOWN,
    CLUSTER_ERR_SENTINEL
}CLUSTER_ERR_TYPE;

static void cluster_node_deinit(cluster_node *node);
static void cluster_slot_destroy(cluster_slot *slot);
static void cluster_open_slot_destroy(copen_slot *oslot);

void listClusterNodeDestructor(void *val)
{
    cluster_node_deinit(val);

    hi_free(val);
}

void listClusterSlotDestructor(void *val)
{
    cluster_slot_destroy(val);
}

unsigned int dictSdsHash(const void *key) {
    return dictGenHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    int l1,l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

void dictSdsDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

void dictClusterNodeDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    cluster_node_deinit(val);

    hi_free(val);
}

/* Cluster nodes hash table, mapping nodes 
 * name(437c719f50dc9d0745032f3b280ce7ecc40792ac)  
 * or addresses(1.2.3.4:6379) to clusterNode structures.
 * Those nodes need destroy.
 */
dictType clusterNodesDictType = {
    dictSdsHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCompare,          /* key compare */
    dictSdsDestructor,          /* key destructor */
    dictClusterNodeDestructor   /* val destructor */
};

/* Cluster nodes hash table, mapping nodes 
 * name(437c719f50dc9d0745032f3b280ce7ecc40792ac)  
 * or addresses(1.2.3.4:6379) to clusterNode structures.
 * Those nodes do not need destroy.
 */
dictType clusterNodesRefDictType = {
    dictSdsHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCompare,          /* key compare */
    dictSdsDestructor,          /* key destructor */
    NULL                        /* val destructor */
};


void listCommandFree(void *command)
{
    struct cmd *cmd = command;
    command_destroy(cmd);
}

/* Defined in hiredis.c */
void __redisSetError(redisContext *c, int type, const char *str);

/* Forward declaration of function in hiredis.c */
int __redisAppendCommand(redisContext *c, const char *cmd, size_t len);

/* Helper function for the redisClusterCommand* family of functions.
 *
 * Write a formatted command to the output buffer. If the given context is
 * blocking, immediately read the reply into the "reply" pointer. When the
 * context is non-blocking, the "reply" pointer will not be used and the
 * command is simply appended to the write buffer.
 *
 * Returns the reply when a reply was succesfully retrieved. Returns NULL
 * otherwise. When NULL is returned in a blocking context, the error field
 * in the context will be set.
 */
static void *__redisBlockForReply(redisContext *c) {
    void *reply;

    if (c->flags & REDIS_BLOCK) {
        if (redisGetReply(c,&reply) != REDIS_OK)
            return NULL;
        return reply;
    }
    return NULL;
}


/* -----------------------------------------------------------------------------
 * Key space handling
 * -------------------------------------------------------------------------- */

/* We have 16384 hash slots. The hash slot of a given key is obtained
 * as the least significant 14 bits of the crc16 of the key.
 *
 * However if the key contains the {...} pattern, only the part between
 * { and } is hashed. This may be useful in the future to force certain
 * keys to be in the same node (assuming no resharding is in progress). */
static unsigned int keyHashSlot(char *key, int keylen) {
    int s, e; /* start-end indexes of { and } */

    for (s = 0; s < keylen; s++)
        if (key[s] == '{') break;

    /* No '{' ? Hash the whole key. This is the base case. */
    if (s == keylen) return crc16(key,keylen) & 0x3FFF;

    /* '{' found? Check if we have the corresponding '}'. */
    for (e = s+1; e < keylen; e++)
        if (key[e] == '}') break;

    /* No '}' or nothing betweeen {} ? Hash the whole key. */
    if (e == keylen || e == s+1) return crc16(key,keylen) & 0x3FFF;

    /* If we are here there is both a { and a } on its right. Hash
     * what is in the middle between { and }. */
    return crc16(key+s+1,e-s-1) & 0x3FFF;
}

static void __redisClusterSetError(redisClusterContext *cc, int type, const char *str) {
    size_t len;

    if(cc == NULL){
        return;
    }

    cc->err = type;
    if (str != NULL) {
        len = strlen(str);
        len = len < (sizeof(cc->errstr)-1) ? len : (sizeof(cc->errstr)-1);
        memcpy(cc->errstr,str,len);
        cc->errstr[len] = '\0';
    } else {
        /* Only REDIS_ERR_IO may lack a description! */
        assert(type == REDIS_ERR_IO);
        __redis_strerror_r(errno, cc->errstr, sizeof(cc->errstr));
    }
}

static int cluster_reply_error_type(redisReply *reply)
{

    if(reply == NULL)
    {
        return REDIS_ERR;
    }

    if(reply->type == REDIS_REPLY_ERROR)
    {
        if((int)strlen(REDIS_ERROR_MOVED) < reply->len && 
            strncmp(reply->str, REDIS_ERROR_MOVED, strlen(REDIS_ERROR_MOVED)) == 0)
        {
            return CLUSTER_ERR_MOVED;
        }
        else if((int)strlen(REDIS_ERROR_ASK) < reply->len && 
            strncmp(reply->str, REDIS_ERROR_ASK, strlen(REDIS_ERROR_ASK)) == 0)
        {
            return CLUSTER_ERR_ASK;
        }
        else if((int)strlen(REDIS_ERROR_TRYAGAIN) < reply->len && 
            strncmp(reply->str, REDIS_ERROR_TRYAGAIN, strlen(REDIS_ERROR_TRYAGAIN)) == 0)
        {
            return CLUSTER_ERR_TRYAGAIN;
        }
        else if((int)strlen(REDIS_ERROR_CROSSSLOT) < reply->len && 
            strncmp(reply->str, REDIS_ERROR_CROSSSLOT, strlen(REDIS_ERROR_CROSSSLOT)) == 0)
        {
            return CLUSTER_ERR_CROSSSLOT;
        }
        else if((int)strlen(REDIS_ERROR_CLUSTERDOWN) < reply->len && 
            strncmp(reply->str, REDIS_ERROR_CLUSTERDOWN, strlen(REDIS_ERROR_CLUSTERDOWN)) == 0)
        {
            return CLUSTER_ERR_CLUSTERDOWN;
        }
        else
        {
            return CLUSTER_ERR_SENTINEL;
        }
    }

    return CLUSTER_NOT_ERR;
}

static int cluster_node_init(cluster_node *node)
{
    if(node == NULL){
        return REDIS_ERR;
    }
    
    node->name = NULL;
    node->addr = NULL;
    node->host = NULL;
    node->port = 0;
    node->role = REDIS_ROLE_NULL;
    node->myself = 0;
    node->slaves = NULL;
    node->con = NULL;
    node->acon = NULL;
    node->slots = NULL;
    node->failure_count = 0;
    node->data = NULL;
    node->migrating = NULL;
    node->importing = NULL;
    
    return REDIS_OK;
}

static void cluster_node_deinit(cluster_node *node)
{   
    copen_slot **oslot;
    
    if(node == NULL)
    {
        return;
    }

    sdsfree(node->name);
    sdsfree(node->addr);
    sdsfree(node->host);
    node->port = 0;
    node->role = REDIS_ROLE_NULL;
    node->myself = 0;

    if(node->con != NULL)
    {
        redisFree(node->con);
    }

    if(node->acon != NULL)
    {
        redisAsyncFree(node->acon);
    }

    if(node->slots != NULL)
    {
        listRelease(node->slots);
    }

    if(node->slaves != NULL)
    {
        listRelease(node->slaves);
    }

    if(node->migrating)
    {
        while(hiarray_n(node->migrating))
        {
            oslot = hiarray_pop(node->migrating);
            cluster_open_slot_destroy(*oslot);
        }
        
        hiarray_destroy(node->migrating);
        node->migrating = NULL;
    }

    if(node->importing)
    {
        while(hiarray_n(node->importing))
        {
            oslot = hiarray_pop(node->importing);
            cluster_open_slot_destroy(*oslot);
        }
        
        hiarray_destroy(node->importing);
        node->importing = NULL;
    }
}

static int cluster_slot_init(cluster_slot *slot, cluster_node *node)
{
    slot->start = 0;
    slot->end = 0;
    slot->node = node;
    
    return REDIS_OK;
}

static cluster_slot *cluster_slot_create(cluster_node *node)
{
    cluster_slot *slot;

    slot = hi_alloc(sizeof(*slot));
    if(slot == NULL){
        return NULL;
    }

    cluster_slot_init(slot, node);

    if(node != NULL){
        ASSERT(node->role == REDIS_ROLE_MASTER);
        if(node->slots == NULL){
            node->slots = listCreate();
            if(node->slots == NULL)
            {
                cluster_slot_destroy(slot);
                return NULL;
            }

            node->slots->free = listClusterSlotDestructor;
        }
        
        listAddNodeTail(node->slots, slot);
    }
    
    return slot;
}

static int cluster_slot_ref_node(cluster_slot * slot, cluster_node *node)
{
    if(slot == NULL || node == NULL){
        return REDIS_ERR;
    }

    
    if(node->role != REDIS_ROLE_MASTER){
        return REDIS_ERR;
    }
    
    if(node->slots == NULL){
        node->slots = listCreate();
        if(node->slots == NULL)
        {
            return REDIS_ERR;
        }

        node->slots->free = listClusterSlotDestructor;
    }
    
    listAddNodeTail(node->slots, slot);
    slot->node = node;
    
    return REDIS_OK;
}

static void cluster_slot_destroy(cluster_slot *slot)
{
    slot->start = 0;
    slot->end = 0;
    slot->node = NULL;
    
    hi_free(slot);
}

static copen_slot *cluster_open_slot_create(uint32_t slot_num, int migrate, 
    sds remote_name, cluster_node *node)
{
    copen_slot *oslot;

    oslot = hi_alloc(sizeof(*oslot));
    if(oslot == NULL){
        return NULL;
    }

    oslot->slot_num = 0;
    oslot->migrate = 0;
    oslot->node = NULL;
    oslot->remote_name = NULL;

    oslot->slot_num = slot_num;
    oslot->migrate = migrate;
    oslot->node = node;
    oslot->remote_name = sdsdup(remote_name);

    return oslot;
}

static void cluster_open_slot_destroy(copen_slot *oslot)
{
    oslot->slot_num = 0;
    oslot->migrate = 0;
    oslot->node = NULL;

    if(oslot->remote_name != NULL){
        sdsfree(oslot->remote_name);
        oslot->remote_name = NULL;
    }
    
    hi_free(oslot);
}

/**
  * Return a new node with the "cluster slots" command reply.
  */
static cluster_node *node_get_with_slots(
    redisClusterContext *cc, redisReply *host_elem, 
    redisReply *port_elem, uint8_t role)
{
    cluster_node *node = NULL;

    if(host_elem == NULL || port_elem == NULL){
        return NULL;
    }

    if(host_elem->type != REDIS_REPLY_STRING ||
        host_elem->len <= 0){
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "Command(cluster slots) reply error: "
            "node ip is not string.");
        goto error;
    }

    if(port_elem->type != REDIS_REPLY_INTEGER ||
        port_elem->integer <= 0){
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "Command(cluster slots) reply error: "
            "node port is not integer.");
        goto error;
    }

    if(!hi_valid_port((int)port_elem->integer)){
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "Command(cluster slots) reply error: "
            "node port is not valid.");
        goto error;
    }

    node = hi_alloc(sizeof(cluster_node));
    if(node == NULL){
        __redisClusterSetError(cc,
            REDIS_ERR_OOM,"Out of memory");
        goto error;
    }
    
    cluster_node_init(node);

    if(role == REDIS_ROLE_MASTER){
        node->slots = listCreate();
        if(node->slots == NULL){
            hi_free(node);
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "slots for node listCreate error");
            goto error;
        }

        node->slots->free = listClusterSlotDestructor;
    }
    
    node->name = NULL; 
    node->addr = sdsnewlen(host_elem->str, host_elem->len);
    node->addr = sdscatfmt(node->addr, ":%i", port_elem->integer);
    
    node->host = sdsnewlen(host_elem->str, host_elem->len);
    node->port = (int)port_elem->integer;
    node->role = role;
    
    return node;

error:
    
    if(node != NULL){
        hi_free(node);
    }

    return NULL;
}

/**
  * Return a new node with the "cluster nodes" command reply.
  */
static cluster_node *node_get_with_nodes(
    redisClusterContext *cc,
    sds *node_infos, int info_count, uint8_t role)
{
    sds *ip_port = NULL;
    int count_ip_port = 0;
    cluster_node *node;

    if(info_count < 8)
    {
        return NULL;
    }

    node = hi_alloc(sizeof(cluster_node));
    if(node == NULL)
    {
        __redisClusterSetError(cc,
            REDIS_ERR_OOM,"Out of memory");
        goto error;
    }
    
    cluster_node_init(node);

    if(role == REDIS_ROLE_MASTER)
    {
        node->slots = listCreate();
        if(node->slots == NULL)
        {
            hi_free(node);
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "slots for node listCreate error");
            goto error;
        }

        node->slots->free = listClusterSlotDestructor;
    }
    
    node->name = node_infos[0]; 
    node->addr = node_infos[1];
    
    ip_port = sdssplitlen(node_infos[1], sdslen(node_infos[1]), 
        IP_PORT_SEPARATOR, strlen(IP_PORT_SEPARATOR), &count_ip_port);
    if(ip_port == NULL || count_ip_port != 2)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "split ip port error");
        goto error;
    }
    node->host = ip_port[0];
    node->port = hi_atoi(ip_port[1], sdslen(ip_port[1]));
    node->role = role;

    sdsfree(ip_port[1]);
    free(ip_port);

    node_infos[0] = NULL;
    node_infos[1] = NULL;
    
    return node;

error:
    if(ip_port != NULL)
    {
        sdsfreesplitres(ip_port, count_ip_port);
    }

    if(node != NULL)
    {
        hi_free(node);
    }

    return NULL;
}

static void cluster_nodes_swap_ctx(dict *nodes_f, dict *nodes_t)
{
    dictIterator *di;
    dictEntry *de_f, *de_t;
    cluster_node *node_f, *node_t;
    redisContext *c;
    redisAsyncContext *ac;

    if(nodes_f == NULL || nodes_t == NULL){
        return;
    }

    di = dictGetIterator(nodes_t);
    while((de_t = dictNext(di)) != NULL){
        node_t = dictGetEntryVal(de_t);
        if(node_t == NULL){
            continue;
        }
        
        de_f = dictFind(nodes_f, node_t->addr);
        if(de_f == NULL){
            continue;
        }

        node_f = dictGetEntryVal(de_f);
        if(node_f->con != NULL){
            c = node_f->con;
            node_f->con = node_t->con;
            node_t->con = c;
        }

        if(node_f->acon != NULL){
            ac = node_f->acon;
            node_f->acon = node_t->acon;
            node_t->acon = ac;

            node_t->acon->data = node_t;
            if (node_f->acon)
                node_f->acon->data = node_f;
        }
    }

    dictReleaseIterator(di);
    
}

static int
cluster_slot_start_cmp(const void *t1, const void *t2)
{
    const cluster_slot **s1 = t1, **s2 = t2;

    return (*s1)->start > (*s2)->start?1:-1;
}

static int
cluster_master_slave_mapping_with_name(redisClusterContext *cc,
    dict **nodes, cluster_node *node, sds master_name)
{
    int ret;
    dictEntry *di;
    cluster_node *node_old;
    listNode *lnode;

    if(node == NULL || master_name == NULL)
    {
        return REDIS_ERR;
    }

    if(*nodes == NULL)
    {
        *nodes = dictCreate(
            &clusterNodesRefDictType, NULL);
    }

    di = dictFind(*nodes, master_name);
    if(di == NULL)
    {
        ret = dictAdd(*nodes, 
            sdsnewlen(master_name, sdslen(master_name)), node);
        if(ret != DICT_OK)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "the address already exists in the nodes");
            return REDIS_ERR;
        }

    }
    else
    {
        node_old = dictGetEntryVal(di);
        if(node_old == NULL)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "dict get value null");
            return REDIS_ERR;
        }

        if(node->role == REDIS_ROLE_MASTER &&
            node_old->role == REDIS_ROLE_MASTER)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "two masters have the same name");
            return REDIS_ERR;
        }
        else if(node->role == REDIS_ROLE_MASTER
            && node_old->role == REDIS_ROLE_SLAVE)
        {
            if(node->slaves == NULL)
            {
                node->slaves = listCreate();
                if(node->slaves == NULL)
                {
                    __redisClusterSetError(cc,REDIS_ERR_OOM,
                        "Out of memory");
                    return REDIS_ERR;
                }

                node->slaves->free = 
                    listClusterNodeDestructor;
            }
        
            if(node_old->slaves != NULL)
            {
                node_old->slaves->free = NULL;
                while(listLength(node_old->slaves) > 0)
                {
                    lnode = listFirst(node_old->slaves);
                    listAddNodeHead(node->slaves, lnode->value);
                    listDelNode(node_old->slaves, lnode);
                }
                listRelease(node_old->slaves);
                node_old->slaves = NULL;
            }

            listAddNodeHead(node->slaves, node_old);

            dictSetHashVal(*nodes, di, node);
        }
        else if(node->role == REDIS_ROLE_SLAVE)
        {
            if(node_old->slaves == NULL)
            {
                node_old->slaves = listCreate();
                if(node_old->slaves == NULL)
                {
                    __redisClusterSetError(cc,REDIS_ERR_OOM,
                        "Out of memory");
                    return REDIS_ERR;
                }

                node_old->slaves->free = 
                    listClusterNodeDestructor;
            }

            listAddNodeTail(node_old->slaves, node);
        }
        else
        {
            NOT_REACHED();
        }
    }
                
    return REDIS_OK;
}

/**
  * Parse the "cluster slots" command reply to nodes dict.
  */
dict * 
parse_cluster_slots(redisClusterContext *cc,
    redisReply *reply, int flags)
{
    int ret;
    cluster_slot *slot = NULL;
    dict *nodes = NULL;
    dictEntry *den;
    redisReply *elem_slots;
    redisReply *elem_slots_begin, *elem_slots_end;
    redisReply *elem_nodes;
    redisReply *elem_ip, *elem_port;
    cluster_node *master = NULL, *slave;
    sds address;
    uint32_t i, idx;

    if(reply == NULL){
        return NULL;
    }

    nodes = dictCreate(&clusterNodesDictType, NULL);
    if(nodes == NULL){
        __redisClusterSetError(cc,REDIS_ERR_OOM,
            "out of memory");
        goto error;
    }
    
    if(reply->type != REDIS_REPLY_ARRAY || reply->elements <= 0){
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "Command(cluster slots) reply error: "
            "reply is not an array.");
        goto error;
    }

    for(i = 0; i < reply->elements; i ++){
        elem_slots = reply->element[i];
        if(elem_slots->type != REDIS_REPLY_ARRAY || 
            elem_slots->elements < 3){
            __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                "Command(cluster slots) reply error: "
                "first sub_reply is not an array.");
            goto error;
        }
        
        slot = cluster_slot_create(NULL);
        if(slot == NULL){
            __redisClusterSetError(cc, REDIS_ERR_OOM, 
                "Slot create failed: out of memory.");
            goto error;
        }

        //one slots region
        for(idx = 0; idx < elem_slots->elements; idx ++){
            if(idx == 0){
                elem_slots_begin = elem_slots->element[idx];
                if(elem_slots_begin->type != REDIS_REPLY_INTEGER){
                    __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                        "Command(cluster slots) reply error: "
                        "slot begin is not an integer.");
                    goto error;
                }
                slot->start = (int)(elem_slots_begin->integer);
            }else if(idx == 1){
                elem_slots_end = elem_slots->element[idx];
                if(elem_slots_end->type != REDIS_REPLY_INTEGER){
                    __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                        "Command(cluster slots) reply error: "
                        "slot end is not an integer.");
                    goto error;
                }
                
                slot->end = (int)(elem_slots_end->integer);

                if(slot->start > slot->end){
                    __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                        "Command(cluster slots) reply error: "
                        "slot begin is bigger than slot end.");
                    goto error;
                }
            }else{
                elem_nodes = elem_slots->element[idx];
                if(elem_nodes->type != REDIS_REPLY_ARRAY || 
                    elem_nodes->elements != 3){
                    __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                        "Command(cluster slots) reply error: "
                        "nodes sub_reply is not an correct array.");
                    goto error;
                }

                elem_ip = elem_nodes->element[0];
                elem_port = elem_nodes->element[1];

                if(elem_ip == NULL || elem_port == NULL ||
                    elem_ip->type != REDIS_REPLY_STRING || 
                    elem_port->type != REDIS_REPLY_INTEGER){
                    __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                        "Command(cluster slots) reply error: "
                        "master ip or port is not correct.");
                    goto error;
                }

                //this is master.
                if(idx == 2){
                    address = sdsnewlen(elem_ip->str, elem_ip->len);
                    address = sdscatfmt(address, ":%i", elem_port->integer);

                    den = dictFind(nodes, address);
                    //master already exits, break to the next slots region.
                    if(den != NULL){
                        sdsfree(address);

                        master = dictGetEntryVal(den);
                        ret = cluster_slot_ref_node(slot, master);
                        if(ret != REDIS_OK){
                            __redisClusterSetError(cc, REDIS_ERR_OOM, 
                                "Slot ref node failed: out of memory.");
                            goto error;
                        }

                        slot = NULL;
                        break;
                    }

                    sdsfree(address);
                    master = node_get_with_slots(cc, elem_ip, 
                        elem_port, REDIS_ROLE_MASTER);
                    if(master == NULL){
                        goto error;
                    }

                    ret = dictAdd(nodes, 
                        sdsnewlen(master->addr, sdslen(master->addr)), master);
                    if(ret != DICT_OK){
                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                            "The address already exists in the nodes");
                        cluster_node_deinit(master);
                        hi_free(master);
                        goto error;
                    }
                    
                    ret = cluster_slot_ref_node(slot, master);
                    if(ret != REDIS_OK){
                        __redisClusterSetError(cc, REDIS_ERR_OOM, 
                            "Slot ref node failed: out of memory.");
                        goto error;
                    }

                    slot = NULL;
                }else if(flags & HIRCLUSTER_FLAG_ADD_SLAVE){
                    slave = node_get_with_slots(cc, elem_ip, 
                            elem_port, REDIS_ROLE_SLAVE);
                    if(slave == NULL){
                        goto error;
                    }

                    if(master->slaves == NULL){
                        master->slaves = listCreate();
                        if(master->slaves == NULL){
                            __redisClusterSetError(cc,REDIS_ERR_OOM,
                                "Out of memory");
                            cluster_node_deinit(slave);
                            goto error;
                        }

                        master->slaves->free = 
                            listClusterNodeDestructor;
                    }

                    listAddNodeTail(master->slaves, slave);
                }
            }
        }
    }

    return nodes;

error:

    if(nodes != NULL){
        dictRelease(nodes);
    }

    if(slot != NULL){
        cluster_slot_destroy(slot);
    }
    
    return NULL;
}

/**
  * Parse the "cluster nodes" command reply to nodes dict.
  */
dict *
parse_cluster_nodes(redisClusterContext *cc, 
    char *str, int str_len, int flags)
{
    int ret;
    dict *nodes = NULL;
    dict *nodes_name = NULL;
    cluster_node *master, *slave;
    cluster_slot *slot;
    char *pos, *start, *end, *line_start, *line_end;
    char *role;
    int role_len;
    uint8_t myself = 0;
    int slot_start, slot_end;
    sds *part = NULL, *slot_start_end = NULL;
    int count_part = 0, count_slot_start_end = 0;
    int k;
    int len;

    nodes = dictCreate(&clusterNodesDictType, NULL);
    if(nodes == NULL){
        __redisClusterSetError(cc,REDIS_ERR_OOM,
            "out of memory");
        goto error;
    }

    start = str;
    end = start + str_len;
    
    line_start = start;

    for(pos = start; pos < end; pos ++){
        if(*pos == '\n'){
            line_end = pos - 1;
            len = line_end - line_start;
            
            part = sdssplitlen(line_start, len + 1, " ", 1, &count_part);

            if(part == NULL || count_part < 8){
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    "split cluster nodes error");
                goto error;
            }

            //the address string is ":0", skip this node.
            if(sdslen(part[1]) == 2 && strcmp(part[1], ":0") == 0){
                sdsfreesplitres(part, count_part);
                count_part = 0;
                part = NULL;
                
                start = pos + 1;
                line_start = start;
                pos = start;
                
                continue;
            }

            if(sdslen(part[2]) >= 7 && memcmp(part[2], "myself,", 7) == 0){
                role_len = sdslen(part[2]) - 7;
                role = part[2] + 7;
                myself = 1;
            }else{
                role_len = sdslen(part[2]);
                role = part[2];
            }

            //add master node
            if(role_len >= 6 && memcmp(role, "master", 6) == 0){
                if(count_part < 8){
                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                        "Master node parts number error: less than 8.");
                    goto error;
                }
                
                master = node_get_with_nodes(cc, 
                    part, count_part, REDIS_ROLE_MASTER);
                if(master == NULL){
                    goto error;
                }

                ret = dictAdd(nodes, 
                    sdsnewlen(master->addr, sdslen(master->addr)), master);
                if(ret != DICT_OK){
                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                        "The address already exists in the nodes");
                    cluster_node_deinit(master);
                    hi_free(master);
                    goto error;
                }

                if(flags & HIRCLUSTER_FLAG_ADD_SLAVE){
                    ret = cluster_master_slave_mapping_with_name(cc, 
                        &nodes_name, master, master->name);
                    if(ret != REDIS_OK){
                        cluster_node_deinit(master);
                        hi_free(master);
                        goto error;
                    }
                }

                if(myself) master->myself = 1;
                
                for(k = 8; k < count_part; k ++){
                    slot_start_end = sdssplitlen(part[k], 
                        sdslen(part[k]), "-", 1, &count_slot_start_end);
                    
                    if(slot_start_end == NULL){
                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                            "split slot start end error(NULL)");
                        goto error;
                    }else if(count_slot_start_end == 1){
                        slot_start = 
                            hi_atoi(slot_start_end[0], sdslen(slot_start_end[0]));
                        slot_end = slot_start;
                    }else if(count_slot_start_end == 2){
                        slot_start = 
                            hi_atoi(slot_start_end[0], sdslen(slot_start_end[0]));;
                        slot_end = 
                            hi_atoi(slot_start_end[1], sdslen(slot_start_end[1]));;
                    }else{
                        //add open slot for master
                        if(flags & HIRCLUSTER_FLAG_ADD_OPENSLOT && 
                            count_slot_start_end == 3 && 
                            sdslen(slot_start_end[0]) > 1 &&
                            sdslen(slot_start_end[1]) == 1 && 
                            sdslen(slot_start_end[2]) > 1 && 
                            slot_start_end[0][0] == '[' && 
                            slot_start_end[2][sdslen(slot_start_end[2])-1] == ']'){
                            
                            copen_slot *oslot, **oslot_elem;
                            
                            sdsrange(slot_start_end[0], 1, -1);
                            sdsrange(slot_start_end[2], 0, -2);
                            
                            if(slot_start_end[1][0] == '>'){
                                oslot = cluster_open_slot_create(
                                    hi_atoi(slot_start_end[0],
                                    sdslen(slot_start_end[0])), 
                                    1, slot_start_end[2], master);
                                if(oslot == NULL){
                                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                        "create open slot error");
                                    goto error;
                                }
 
                                if(master->migrating == NULL){
                                    master->migrating = hiarray_create(1, sizeof(oslot));
                                    if(master->migrating == NULL){
                                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                            "create migrating array error");
                                        cluster_open_slot_destroy(oslot);
                                        goto error;
                                    }
                                }

                                oslot_elem = hiarray_push(master->migrating);
                                if(oslot_elem == NULL){
                                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                        "Push migrating array error: out of memory");
                                    cluster_open_slot_destroy(oslot);
                                    goto error;
                                }

                                *oslot_elem = oslot;
                            }else if(slot_start_end[1][0] == '<'){
                                oslot = cluster_open_slot_create(hi_atoi(slot_start_end[0],
                                    sdslen(slot_start_end[0])), 0, slot_start_end[2],
                                    master);
                                if(oslot == NULL){
                                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                        "create open slot error");
                                    goto error;
                                }

                                if(master->importing == NULL){
                                    master->importing = hiarray_create(1, sizeof(oslot));
                                    if(master->importing == NULL){
                                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                            "create migrating array error");
                                        cluster_open_slot_destroy(oslot);
                                        goto error;
                                    }
                                }

                                oslot_elem = hiarray_push(master->importing);
                                if(oslot_elem == NULL){
                                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                        "push migrating array error: out of memory");
                                    cluster_open_slot_destroy(oslot);
                                    goto error;
                                }

                                *oslot_elem = oslot;
                            }
                        }
                        
                        slot_start = -1;
                        slot_end = -1;
                    }
                    
                    sdsfreesplitres(slot_start_end, count_slot_start_end);
                    count_slot_start_end = 0;
                    slot_start_end = NULL;

                    if(slot_start < 0 || slot_end < 0 || 
                        slot_start > slot_end || slot_end >= REDIS_CLUSTER_SLOTS){
                        continue;
                    }

                    slot = cluster_slot_create(master);
                    if(slot == NULL){
                        __redisClusterSetError(cc,REDIS_ERR_OOM,
                            "Out of memory");
                        goto error;
                    }
                    
                    slot->start = (uint32_t)slot_start;
                    slot->end = (uint32_t)slot_end;                    
                }

            }
            //add slave node
            else if((flags & HIRCLUSTER_FLAG_ADD_SLAVE) && 
                (role_len >= 5 && memcmp(role, "slave", 5) == 0)){
                slave = node_get_with_nodes(cc, part, 
                    count_part, REDIS_ROLE_SLAVE);
                if(slave == NULL){
                    goto error;
                }

                ret = cluster_master_slave_mapping_with_name(cc, 
                    &nodes_name, slave, part[3]);
                if(ret != REDIS_OK){
                    cluster_node_deinit(slave);
                    hi_free(slave);
                    goto error;
                }

                if(myself) slave->myself = 1;
            }

            if(myself == 1){
                myself = 0;
            }

            sdsfreesplitres(part, count_part);
            count_part = 0;
            part = NULL;
            
            start = pos + 1;
            line_start = start;
            pos = start;
        }
    }

    if(nodes_name != NULL){
        dictRelease(nodes_name);
    }
    
    return nodes;

error:
        
    if(part != NULL){
        sdsfreesplitres(part, count_part);
        count_part = 0;
        part = NULL;
    }

    if(slot_start_end != NULL){
        sdsfreesplitres(slot_start_end, count_slot_start_end);
        count_slot_start_end = 0;
        slot_start_end = NULL;
    }

    if(nodes != NULL){
        dictRelease(nodes);
    }

    if(nodes_name != NULL){
        dictRelease(nodes_name);
    }
    
    return NULL;
}

/**
  * Update route with the "cluster nodes" or "cluster slots" command reply.
  */
static int 
cluster_update_route_by_addr(redisClusterContext *cc, 
    const char *ip, int port)
{
    redisContext *c = NULL;
    redisReply *reply = NULL;
    dict *nodes = NULL;
    struct hiarray *slots = NULL;
    cluster_node *master;
    cluster_slot *slot, **slot_elem;
    dictIterator *dit = NULL;
    dictEntry *den;
    listIter *lit = NULL;
    listNode *lnode;
    cluster_node *table[REDIS_CLUSTER_SLOTS];
    uint32_t j, k;

    if(cc == NULL){
        return REDIS_ERR;
    }

    if(ip == NULL || port <= 0){
        __redisClusterSetError(cc,
            REDIS_ERR_OTHER,"Ip or port error!");
        goto error;
    }

    if(cc->timeout){
        c = redisConnectWithTimeout(ip, port, *cc->timeout);
    }else{
        c = redisConnect(ip, port);
    }
        
    if (c == NULL){
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "Init redis context error(return NULL)");
        goto error;
    }else if(c->err){
        __redisClusterSetError(cc,c->err,c->errstr);
        goto error;
    }

    if(cc->flags & HIRCLUSTER_FLAG_ROUTE_USE_SLOTS){
        reply = redisCommand(c, REDIS_COMMAND_CLUSTER_SLOTS);
        if(reply == NULL){
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "Command(cluster slots) reply error(NULL).");
            goto error;
        }else if(reply->type != REDIS_REPLY_ARRAY){
            if(reply->type == REDIS_REPLY_ERROR){
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    reply->str);
            }else{
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    "Command(cluster slots) reply error: type is not array.");
            }
            
            goto error;
        }

        nodes = parse_cluster_slots(cc, reply, cc->flags);
    }else{
        reply = redisCommand(c, REDIS_COMMAND_CLUSTER_NODES);
        if(reply == NULL){
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "Command(cluster nodes) reply error(NULL).");
            goto error;
        }else if(reply->type != REDIS_REPLY_STRING){
            if(reply->type == REDIS_REPLY_ERROR){
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    reply->str);
            }else{
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    "Command(cluster nodes) reply error: type is not string.");
            }
            
            goto error;
        }

        nodes = parse_cluster_nodes(cc, reply->str, reply->len, cc->flags);
    }

    if(nodes == NULL){
        goto error;
    }
    
    memset(table, 0, REDIS_CLUSTER_SLOTS*sizeof(cluster_node *));
    
    slots = hiarray_create(dictSize(nodes), sizeof(cluster_slot*));
    if(slots == NULL){
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "Slots array create failed: out of memory");
        goto error;
    }
    
    dit = dictGetIterator(nodes);
    if(dit == NULL){
        __redisClusterSetError(cc,REDIS_ERR_OOM,
            "Dict get iterator failed: out of memory");
        goto error;
    }
    
    while((den = dictNext(dit))){
        master = dictGetEntryVal(den);
        if(master->role != REDIS_ROLE_MASTER){
            __redisClusterSetError(cc,REDIS_ERR_OOM,
                "Node role must be master");
            goto error;
        }

        if(master->slots == NULL){
            continue;
        }
        
        lit = listGetIterator(master->slots, AL_START_HEAD);
        if(lit == NULL){
            __redisClusterSetError(cc, REDIS_ERR_OOM,
                "List get iterator failed: out of memory");
            goto error;
        }
        
        while((lnode = listNext(lit))){
            slot = listNodeValue(lnode);
            if(slot->start > slot->end || 
                slot->end >= REDIS_CLUSTER_SLOTS){
                __redisClusterSetError(cc, REDIS_ERR_OTHER,
                    "Slot region for node is error");
                goto error;
            }
            
            slot_elem = hiarray_push(slots);
            *slot_elem = slot;
        }

        listReleaseIterator(lit);
    }

    dictReleaseIterator(dit);

    hiarray_sort(slots, cluster_slot_start_cmp);
    for(j = 0; j < hiarray_n(slots); j ++){
        slot_elem = hiarray_get(slots, j);
        
        for(k = (*slot_elem)->start; k <= (*slot_elem)->end; k ++){
            if(table[k] != NULL){
                __redisClusterSetError(cc, REDIS_ERR_OTHER,
                    "Diffent node hold a same slot");
                goto error;
            }
            
            table[k] = (*slot_elem)->node;
        }
    }
    
    cluster_nodes_swap_ctx(cc->nodes, nodes);
    if(cc->nodes != NULL){
        dictRelease(cc->nodes);
        cc->nodes = NULL;
    }
    cc->nodes = nodes;

    if(cc->slots != NULL)
    {
        cc->slots->nelem = 0;
        hiarray_destroy(cc->slots);
        cc->slots = NULL;
    }
    cc->slots = slots;

    memcpy(cc->table, table, REDIS_CLUSTER_SLOTS*sizeof(cluster_node *));
    cc->route_version ++;
    
    freeReplyObject(reply);

    if(c != NULL){
        redisFree(c);
    }
    
    return REDIS_OK;

error:

    if(dit != NULL){
        dictReleaseIterator(dit);
    }

    if(lit != NULL){
        listReleaseIterator(lit);    
    }

    if(slots != NULL)
    {
        if(slots == cc->slots)
        {
            cc->slots = NULL;
        }
        
        slots->nelem = 0;
        hiarray_destroy(slots);
    }

    if(nodes != NULL){
        if(nodes == cc->nodes){
            cc->nodes = NULL;
        }

        dictRelease(nodes);
    }

    if(reply != NULL){
        freeReplyObject(reply);
        reply = NULL;
    }

    if(c != NULL){
        redisFree(c);
    }
    
    return REDIS_ERR;
}


/**
  * Update route with the "cluster nodes" command reply.
  */
static int 
cluster_update_route_with_nodes_old(redisClusterContext *cc, 
    const char *ip, int port)
{
    int ret;
    redisContext *c = NULL;
    redisReply *reply = NULL;
    struct hiarray *slots = NULL;
    dict *nodes = NULL;
    dict *nodes_name = NULL;
    cluster_node *master, *slave;
    cluster_slot **slot;
    char *pos, *start, *end, *line_start, *line_end;
    char *role;
    int role_len;
    uint8_t myself = 0;
    int slot_start, slot_end;
    sds *part = NULL, *slot_start_end = NULL;
    int count_part = 0, count_slot_start_end = 0;
    int j, k;
    int len;
    cluster_node *table[REDIS_CLUSTER_SLOTS] = {NULL};

    if(cc == NULL)
    {
        return REDIS_ERR;
    }

    if(ip == NULL || port <= 0)
    {
        __redisClusterSetError(cc,
            REDIS_ERR_OTHER,"ip or port error!");
        goto error;
    }

    if(cc->timeout)
    {
        c = redisConnectWithTimeout(ip, port, *cc->timeout);
    }
    else
    {
        c = redisConnect(ip, port);
    }
        
    if (c == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "init redis context error(return NULL)");
        goto error;
    }
    else if(c->err)
    {
        __redisClusterSetError(cc,c->err,c->errstr);
        goto error;
    }

    reply = redisCommand(c, REDIS_COMMAND_CLUSTER_NODES);

    if(reply == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "command(cluster nodes) reply error(NULL)");
        goto error;
    }
    else if(reply->type != REDIS_REPLY_STRING)
    {
        if(reply->type == REDIS_REPLY_ERROR)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                reply->str);
        }
        else
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "command(cluster nodes) reply error(type is not string)");
        }
        
        goto error;
    }

    nodes = dictCreate(&clusterNodesDictType, NULL);
    
    slots = hiarray_create(10, sizeof(cluster_slot*));
    if(slots == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "array create error");
        goto error;
    }

    start = reply->str;
    end = start + reply->len;
    
    line_start = start;

    for(pos = start; pos < end; pos ++)
    {
        if(*pos == '\n')
        {
            line_end = pos - 1;
            len = line_end - line_start;
            
            part = sdssplitlen(line_start, len + 1, " ", 1, &count_part);

            if(part == NULL || count_part < 8)
            {
                __redisClusterSetError(cc,REDIS_ERR_OTHER,
                    "split cluster nodes error");
                goto error;
            }

            //the address string is ":0", skip this node.
            if(sdslen(part[1]) == 2 && strcmp(part[1], ":0") == 0)
            {
                sdsfreesplitres(part, count_part);
                count_part = 0;
                part = NULL;
                
                start = pos + 1;
                line_start = start;
                pos = start;
                
                continue;
            }

            if(sdslen(part[2]) >= 7 && memcmp(part[2], "myself,", 7) == 0)
            {
                role_len = sdslen(part[2]) - 7;
                role = part[2] + 7;
                myself = 1;
            }
            else
            {
                role_len = sdslen(part[2]);
                role = part[2];
            }

            //add master node
            if(role_len >= 6 && memcmp(role, "master", 6) == 0)
            {
                if(count_part < 8)
                {
                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                        "master node part number error");
                    goto error;
                }
                
                master = node_get_with_nodes(cc, 
                    part, count_part, REDIS_ROLE_MASTER);
                if(master == NULL)
                {
                    goto error;
                }

                ret = dictAdd(nodes, 
                    sdsnewlen(master->addr, sdslen(master->addr)), master);
                if(ret != DICT_OK)
                {
                    __redisClusterSetError(cc,REDIS_ERR_OTHER,
                        "the address already exists in the nodes");
                    cluster_node_deinit(master);
                    hi_free(master);
                    goto error;
                }

                if(cc->flags & HIRCLUSTER_FLAG_ADD_SLAVE)
                {
                    ret = cluster_master_slave_mapping_with_name(cc, 
                        &nodes_name, master, master->name);
                    if(ret != REDIS_OK)
                    {
                        cluster_node_deinit(master);
                        hi_free(master);
                        goto error;
                    }
                }
                
                if(myself == 1)
                {
                    master->con = c;
                    c = NULL;
                }
                
                for(k = 8; k < count_part; k ++)
                {
                    slot_start_end = sdssplitlen(part[k], 
                        sdslen(part[k]), "-", 1, &count_slot_start_end);
                    
                    if(slot_start_end == NULL)
                    {
                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                            "split slot start end error(NULL)");
                        goto error;
                    }
                    else if(count_slot_start_end == 1)
                    {
                        slot_start = 
                            hi_atoi(slot_start_end[0], sdslen(slot_start_end[0]));
                        slot_end = slot_start;
                    }
                    else if(count_slot_start_end == 2)
                    {
                        slot_start = 
                            hi_atoi(slot_start_end[0], sdslen(slot_start_end[0]));;
                        slot_end = 
                            hi_atoi(slot_start_end[1], sdslen(slot_start_end[1]));;
                    }
                    else
                    {
                        slot_start = -1;
                        slot_end = -1;
                    }
                    
                    sdsfreesplitres(slot_start_end, count_slot_start_end);
                    count_slot_start_end = 0;
                    slot_start_end = NULL;

                    if(slot_start < 0 || slot_end < 0 || 
                        slot_start > slot_end || slot_end >= REDIS_CLUSTER_SLOTS)
                    {
                        continue;
                    }

                    for(j = slot_start; j <= slot_end; j ++)
                    {
                        if(table[j] != NULL)
                        {
                            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                                "diffent node hold a same slot");
                            goto error;
                        }
                        table[j] = master;
                    }
                    
                    slot = hiarray_push(slots);
                    if(slot == NULL)
                    {
                        __redisClusterSetError(cc,REDIS_ERR_OTHER,
                            "slot push in array error");
                        goto error;
                    }

                    *slot = cluster_slot_create(master);
                    if(*slot == NULL)
                    {
                        __redisClusterSetError(cc,REDIS_ERR_OOM,
                            "Out of memory");
                        goto error;
                    }

                    (*slot)->start = (uint32_t)slot_start;
                    (*slot)->end = (uint32_t)slot_end;                    
                }

            }
            //add slave node
            else if((cc->flags & HIRCLUSTER_FLAG_ADD_SLAVE) && 
                (role_len >= 5 && memcmp(role, "slave", 5) == 0))
            {
                slave = node_get_with_nodes(cc, part, 
                    count_part, REDIS_ROLE_SLAVE);
                if(slave == NULL)
                {
                    goto error;
                }

                ret = cluster_master_slave_mapping_with_name(cc, 
                    &nodes_name, slave, part[3]);
                if(ret != REDIS_OK)
                {
                    cluster_node_deinit(slave);
                    hi_free(slave);
                    goto error;
                }
                
                if(myself == 1)
                {
                    slave->con = c;
                    c = NULL;
                }
            }

            if(myself == 1)
            {
                myself = 0;
            }

            sdsfreesplitres(part, count_part);
            count_part = 0;
            part = NULL;
            
            start = pos + 1;
            line_start = start;
            pos = start;
        }
    }

    if(cc->slots != NULL)
    {
        cc->slots->nelem = 0;
        hiarray_destroy(cc->slots);
        cc->slots = NULL;
    }
    cc->slots = slots;

    cluster_nodes_swap_ctx(cc->nodes, nodes);

    if(cc->nodes != NULL)
    {
        dictRelease(cc->nodes);
        cc->nodes = NULL;
    }
    cc->nodes = nodes;

    hiarray_sort(cc->slots, cluster_slot_start_cmp);

    memcpy(cc->table, table, REDIS_CLUSTER_SLOTS*sizeof(cluster_node *));
    cc->route_version ++;
    
    freeReplyObject(reply);

    if(c != NULL)
    {
        redisFree(c);
    }

    if(nodes_name != NULL)
    {
        dictRelease(nodes_name);
    }
    
    return REDIS_OK;

error:
        
    if(part != NULL)
    {
        sdsfreesplitres(part, count_part);
        count_part = 0;
        part = NULL;
    }

    if(slot_start_end != NULL)
    {
        sdsfreesplitres(slot_start_end, count_slot_start_end);
        count_slot_start_end = 0;
        slot_start_end = NULL;
    }

    if(slots != NULL)
    {
        if(slots == cc->slots)
        {
            cc->slots = NULL;
        }

        slots->nelem = 0;
        hiarray_destroy(slots);
    }

    if(nodes != NULL)
    {
        if(nodes == cc->nodes)
        {
            cc->nodes = NULL;
        }

        dictRelease(nodes);
    }

    if(nodes_name != NULL)
    {
        dictRelease(nodes_name);
    }

    if(reply != NULL)
    {
        freeReplyObject(reply);
        reply = NULL;
    }

    if(c != NULL)
    {
        redisFree(c);
    }
    
    return REDIS_ERR;
}

int
cluster_update_route(redisClusterContext *cc)
{
    int ret;
    int flag_err_not_set = 1;
    cluster_node *node;
    dictIterator *it;
    dictEntry *de;
    
    if(cc == NULL)
    {
        return REDIS_ERR;
    }

    if(cc->ip != NULL && cc->port > 0)
    {
        ret = cluster_update_route_by_addr(cc, cc->ip, cc->port);
        if(ret == REDIS_OK)
        {
            return REDIS_OK;
        }

        flag_err_not_set = 0;
    }

    if(cc->nodes == NULL)
    {
        if(flag_err_not_set)
        {
            __redisClusterSetError(cc, REDIS_ERR_OTHER, "no server address");
        }
        
        return REDIS_ERR;
    }

    it = dictGetIterator(cc->nodes);
    while ((de = dictNext(it)) != NULL)
    {
        node = dictGetEntryVal(de);
        if(node == NULL || node->host == NULL || node->port < 0)
        {
            continue;
        }

        ret = cluster_update_route_by_addr(cc, node->host, node->port);
        if(ret == REDIS_OK)
        {
            if(cc->err)
            {
                cc->err = 0;
                memset(cc->errstr, '\0', strlen(cc->errstr));
            }
            
            dictReleaseIterator(it);
            return REDIS_OK;
        }

        flag_err_not_set = 0;
    }
    
    dictReleaseIterator(it);

    if(flag_err_not_set)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "no valid server address");
    }

    return REDIS_ERR;
}

static void print_cluster_node_list(redisClusterContext *cc)
{
    dictIterator *di = NULL;
    dictEntry *de;
    listIter *it;
    listNode *ln;
    cluster_node *master, *slave;
    hilist *slaves;

    if(cc == NULL)
    {
        return;
    }

    di = dictGetIterator(cc->nodes);

    printf("name\taddress\trole\tslaves\n");
    
    while((de = dictNext(di)) != NULL) {
        master = dictGetEntryVal(de);

        printf("%s\t%s\t%d\t%s\n",master->name, master->addr, 
            master->role, master->slaves?"hava":"null");

        slaves = master->slaves;
        if(slaves == NULL)
        {
            continue;
        }
        
        it = listGetIterator(slaves, AL_START_HEAD);
        while((ln = listNext(it)) != NULL)
        {
            slave = listNodeValue(ln);
            printf("%s\t%s\t%d\t%s\n",slave->name, slave->addr, 
                slave->role, slave->slaves?"hava":"null");
        }

        listReleaseIterator(it);

        printf("\n");
    }
}


int test_cluster_update_route(redisClusterContext *cc)
{
    int ret;
    
    ret = cluster_update_route(cc);

    //print_cluster_node_list(cc);
    
    return ret;
}

static redisClusterContext *redisClusterContextInit(void) {
    redisClusterContext *cc;

    cc = calloc(1,sizeof(redisClusterContext));
    if (cc == NULL)
        return NULL;

    cc->err = 0;
    cc->errstr[0] = '\0';
    cc->ip = NULL;
    cc->port = 0;
    cc->flags = 0;
    cc->timeout = NULL;
    cc->nodes = NULL;
    cc->slots = NULL;
    cc->max_redirect_count = CLUSTER_DEFAULT_MAX_REDIRECT_COUNT;
    cc->retry_count = 0;
    cc->requests = NULL;
    cc->need_update_route = 0;
    cc->update_route_time = 0LL;

    cc->route_version = 0LL;

    memset(cc->table, 0, REDIS_CLUSTER_SLOTS*sizeof(cluster_node *));
    
    return cc;
}

void redisClusterFree(redisClusterContext *cc) {
    
    if (cc == NULL)
        return;

    if(cc->ip)
    {
        sdsfree(cc->ip);
        cc->ip = NULL;
    }

    if (cc->timeout)
    {
        free(cc->timeout);
    }

    memset(cc->table, 0, REDIS_CLUSTER_SLOTS*sizeof(cluster_node *));

    if(cc->slots != NULL)
    {
        cc->slots->nelem = 0;
        hiarray_destroy(cc->slots);
        cc->slots = NULL;
    }

    if(cc->nodes != NULL)
    {
        dictRelease(cc->nodes);
    }

    if(cc->requests != NULL)
    {
        listRelease(cc->requests);
    }
    
    free(cc);
}

static int redisClusterAddNode(redisClusterContext *cc, const char *addr)
{
    dictEntry *node_entry;
    cluster_node *node;
    sds *ip_port = NULL;
    int ip_port_count = 0;
    sds ip;
    int port;
    
    if(cc == NULL)
    {
        return REDIS_ERR;
    }

    if(cc->nodes == NULL)
    {
        cc->nodes = dictCreate(&clusterNodesDictType, NULL);
        if(cc->nodes == NULL)
        {
            return REDIS_ERR;
        }
    }

    node_entry = dictFind(cc->nodes, addr);
    if(node_entry == NULL)
    {
        ip_port = sdssplitlen(addr, strlen(addr), 
            IP_PORT_SEPARATOR, strlen(IP_PORT_SEPARATOR), &ip_port_count);
        if(ip_port == NULL || ip_port_count != 2 || 
            sdslen(ip_port[0]) <= 0 || sdslen(ip_port[1]) <= 0)
        {
            if(ip_port != NULL)
            {
                sdsfreesplitres(ip_port, ip_port_count);
            }
            __redisClusterSetError(cc,REDIS_ERR_OTHER,"server address is error(correct is like: 127.0.0.1:1234)");
            return REDIS_ERR;
        }

        ip = ip_port[0];
        port = hi_atoi(ip_port[1], sdslen(ip_port[1]));

        if(port <= 0)
        {
            sdsfreesplitres(ip_port, ip_port_count);
            __redisClusterSetError(cc,REDIS_ERR_OTHER,"server port is error");
            return REDIS_ERR;
        }

        sdsfree(ip_port[1]);
        free(ip_port);
        ip_port = NULL;
    
        node = hi_alloc(sizeof(cluster_node));
        if(node == NULL)
        {
            sdsfree(ip);
            __redisClusterSetError(cc,REDIS_ERR_OTHER,"alloc cluster node error");
            return REDIS_ERR;
        }

        cluster_node_init(node);

        node->addr = sdsnew(addr);
        if(node->addr == NULL)
        {
            sdsfree(ip);
            hi_free(node);
            __redisClusterSetError(cc,REDIS_ERR_OTHER,"new node address error");
            return REDIS_ERR;
        }

        node->host = ip;
        node->port = port;

        dictAdd(cc->nodes, sdsnewlen(node->addr, sdslen(node->addr)), node);
    }
    
    return REDIS_OK;
}


/* Connect to a Redis cluster. On error the field error in the returned
 * context will be set to the return value of the error function.
 * When no set of reply functions is given, the default set will be used. */
static redisClusterContext *_redisClusterConnect(redisClusterContext *cc, const char *addrs) {

    int ret;
    sds *address = NULL;
    int address_count = 0;
    int i;

    if(cc == NULL)
    {
        return NULL;
    }
    

    address = sdssplitlen(addrs, strlen(addrs), CLUSTER_ADDRESS_SEPARATOR, 
        strlen(CLUSTER_ADDRESS_SEPARATOR), &address_count);
    if(address == NULL || address_count <= 0)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,"servers address is error(correct is like: 127.0.0.1:1234,127.0.0.2:5678)");
        return cc;
    }

    for(i = 0; i < address_count; i ++)
    {
        ret = redisClusterAddNode(cc, address[i]);
        if(ret != REDIS_OK)
        {
            sdsfreesplitres(address, address_count);
            return cc;
        }
    }

    sdsfreesplitres(address, address_count);
    
    cluster_update_route(cc);

    return cc;
}

redisClusterContext *redisClusterConnect(const char *addrs, int flags)
{
    redisClusterContext *cc;

    cc = redisClusterContextInit();

    if(cc == NULL)
    {
        return NULL;
    }

    cc->flags |= REDIS_BLOCK;
    if(flags)
    {
        cc->flags |= flags;
    }
    
    return _redisClusterConnect(cc, addrs);
}

redisClusterContext *redisClusterConnectWithTimeout(
    const char *addrs, const struct timeval tv, int flags)
{
    redisClusterContext *cc;

    cc = redisClusterContextInit();

    if(cc == NULL)
    {
        return NULL;
    }

    cc->flags |= REDIS_BLOCK;
    if(flags)
    {
        cc->flags |= flags;
    }
    
    if (cc->timeout == NULL)
    {
        cc->timeout = malloc(sizeof(struct timeval));
    }
    
    memcpy(cc->timeout, &tv, sizeof(struct timeval));
    
    return _redisClusterConnect(cc, addrs);
}

redisClusterContext *redisClusterConnectNonBlock(const char *addrs, int flags) {

    redisClusterContext *cc;

    cc = redisClusterContextInit();

    if(cc == NULL)
    {
        return NULL;
    }

    cc->flags &= ~REDIS_BLOCK;
    if(flags)
    {
        cc->flags |= flags;
    }
    
    return _redisClusterConnect(cc, addrs);
}

redisContext *ctx_get_by_node(cluster_node *node, 
    const struct timeval *timeout, int flags)
{
    redisContext *c = NULL;
    if(node == NULL)
    {
        return NULL;
    }

    c = node->con;
    if(c != NULL)
    {
        if(c->err)
        {
            redisReconnect(c);
        }

        return c;
    }

    if(node->host == NULL || node->port <= 0)
    {
        return NULL;
    }

    if(flags & REDIS_BLOCK)
    {
        if(timeout)
        {
            c = redisConnectWithTimeout(node->host, node->port, *timeout);
        }
        else
        {
            c = redisConnect(node->host, node->port);
        }
    }
    else
    {
        c = redisConnectNonBlock(node->host, node->port);
    }

    node->con = c;

    return c;
}

static cluster_node *node_get_by_slot(redisClusterContext *cc, uint32_t slot_num)
{
    struct hiarray *slots;
    uint32_t slot_count;
    cluster_slot **slot;
    uint32_t middle, start, end;
    uint8_t stop = 0;
    
    if(cc == NULL)
    {
        return NULL;
    }

    if(slot_num >= REDIS_CLUSTER_SLOTS)
    {
        return NULL;
    }

    slots = cc->slots;
    if(slots == NULL)
    {
        return NULL;
    }
    slot_count = hiarray_n(slots);

    start = 0;
    end = slot_count - 1;
    middle = 0;

    do{
        if(start >= end)
        {
            stop = 1;
            middle = end;
        }
        else
        {
            middle = start + (end - start)/2;
        }

        ASSERT(middle < slot_count);

        slot = hiarray_get(slots, middle);
        if((*slot)->start > slot_num)
        {
            end = middle - 1;
        }
        else if((*slot)->end < slot_num)
        {
            start = middle + 1;
        }
        else
        {
            return (*slot)->node;
        }
            
        
    }while(!stop);

    printf("slot_num : %d\n", slot_num);
    printf("slot_count : %d\n", slot_count);
    printf("start : %d\n", start);
    printf("end : %d\n", end);
    printf("middle : %d\n", middle);

    return NULL;
}


static cluster_node *node_get_by_table(redisClusterContext *cc, uint32_t slot_num)
{   
    if(cc == NULL)
    {
        return NULL;
    }

    if(slot_num >= REDIS_CLUSTER_SLOTS)
    {
        return NULL;
    }

    return cc->table[slot_num];
    
}

static cluster_node *node_get_witch_connected(redisClusterContext *cc)
{
    dictIterator *di;
    dictEntry *de;
    struct cluster_node *node;
    redisContext *c = NULL;
    redisReply *reply = NULL;

    if(cc == NULL || cc->nodes == NULL)
    {
        return NULL;
    }

    di = dictGetIterator(cc->nodes);
    while((de = dictNext(di)) != NULL)
    {
        node = dictGetEntryVal(de);
        if(node == NULL)
        {
            continue;
        }
        
        c = ctx_get_by_node(node, cc->timeout, REDIS_BLOCK);
        if(c == NULL || c->err)
        {
            continue;
        }

        reply = redisCommand(c, REDIS_COMMAND_PING);
        if(reply != NULL && reply->type == REDIS_REPLY_STATUS &&
            reply->str != NULL && strcmp(reply->str, "PONG") == 0)
        {
            freeReplyObject(reply);
            reply = NULL;
            
            dictReleaseIterator(di);            
        
            return node;
        }
        else if(reply != NULL)
        {
            freeReplyObject(reply);
            reply = NULL;
        }
    }

    dictReleaseIterator(di);

    return NULL;
}

static int slot_get_by_command(redisClusterContext *cc, char *cmd, int len)
{
    struct cmd *command = NULL;
    struct keypos *kp;
    int key_count;
    uint32_t i;
    int slot_num = -1;

    if(cc == NULL || cmd == NULL || len <= 0)
    {
        goto done;
    }

    command = command_get();
    if(command == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto done;
    }
    
    command->cmd = cmd;
    command->clen = len;
    redis_parse_cmd(command);
    if(command->result != CMD_PARSE_OK)
    {
        __redisClusterSetError(cc, REDIS_ERR_PROTOCOL, "parse command error");
        goto done;
    }

    key_count = hiarray_n(command->keys);

    if(key_count <= 0)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "no keys in command(must have keys for redis cluster mode)");
        goto done;
    }
    else if(key_count == 1)
    {
        kp = hiarray_get(command->keys, 0);
        slot_num = keyHashSlot(kp->start, kp->end - kp->start);

        goto done;
    }
    
    for(i = 0; i < hiarray_n(command->keys); i ++)
    {
        kp = hiarray_get(command->keys, i);

        slot_num = keyHashSlot(kp->start, kp->end - kp->start);
    }

done:
    
    if(command != NULL)
    {
        command->cmd = NULL;
        command_destroy(command);
    }
    
    return slot_num;
}

/* Get the cluster config from one node.
  * Return value: config_value string must free by usr.
  */
static char * cluster_config_get(redisClusterContext *cc, 
    const char *config_name, int *config_value_len)
{
    redisContext *c;
    cluster_node *node;
    redisReply *reply = NULL, *sub_reply;
    char *config_value = NULL;

    if(cc == NULL || config_name == NULL
        || config_value_len == NULL)
    {
        return NULL;
    }
    
    node = node_get_witch_connected(cc);
    if(node == NULL)
    {
        __redisClusterSetError(cc, 
            REDIS_ERR_OTHER, "no reachable node in cluster");
        goto error;
    }

    c = ctx_get_by_node(node, cc->timeout, cc->flags);
    
    reply = redisCommand(c, "config get %s", config_name);
    if(reply == NULL)
    {
        __redisClusterSetError(cc, 
            REDIS_ERR_OTHER, "reply for config get is null");
        goto error;
    }

    if(reply->type != REDIS_REPLY_ARRAY)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply for config get type is not array");
        goto error;
    }

    if(reply->elements != 2)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply for config get elements number is not 2");
        goto error;
    }

    sub_reply = reply->element[0];
    if(sub_reply == NULL || sub_reply->type != REDIS_REPLY_STRING)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply for config get config name is not string");
        goto error;
    }

    if(strcmp(sub_reply->str, config_name))
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply for config get config name is not we want");
        goto error;
    }

    sub_reply = reply->element[1];
    if(sub_reply == NULL || sub_reply->type != REDIS_REPLY_STRING)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply for config get config value type is not string");
        goto error;
    }

    config_value = sub_reply->str;
    *config_value_len = sub_reply->len;
    sub_reply->str= NULL;

    if(reply != NULL)
    {
        freeReplyObject(reply);    
    }

    return config_value;

error:

    if(reply != NULL)
    {
        freeReplyObject(reply);    
    }

    return NULL;
}

/* Helper function for the redisClusterAppendCommand* family of functions.
 *
 * Write a formatted command to the output buffer. When this family
 * is used, you need to call redisGetReply yourself to retrieve
 * the reply (or replies in pub/sub).
 */
static int __redisClusterAppendCommand(redisClusterContext *cc, 
    struct cmd *command) {

    cluster_node *node;
    redisContext *c = NULL;

    if(cc == NULL || command == NULL)
    {
        return REDIS_ERR;
    }
    
    node = node_get_by_table(cc, (uint32_t)command->slot_num);
    if(node == NULL)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "node get by slot error");
        return REDIS_ERR;
    }

    c = ctx_get_by_node(node, cc->timeout, cc->flags);
    if(c == NULL)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "ctx get by node is null");
        return REDIS_ERR;
    }
    else if(c->err)
    {
        __redisClusterSetError(cc, c->err, c->errstr);
        return REDIS_ERR;
    }

    if (__redisAppendCommand(c, command->cmd, command->clen) != REDIS_OK) 
    {
        __redisClusterSetError(cc, c->err, c->errstr);
        return REDIS_ERR;
    }
    
    return REDIS_OK;
}

/* Helper function for the redisClusterGetReply* family of functions.
 */
static int __redisClusterGetReply(redisClusterContext *cc, int slot_num, void **reply)
{
    cluster_node *node;
    redisContext *c;

    if(cc == NULL || slot_num < 0 || reply == NULL)
    {
        return REDIS_ERR;
    }

    node = node_get_by_table(cc, (uint32_t)slot_num);
    if(node == NULL)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "node get by table is null");
        return REDIS_ERR;
    }

    c = ctx_get_by_node(node, cc->timeout, cc->flags);
    if(c == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    }
    else if(c->err)
    {
        if(cc->need_update_route == 0)
        {
            cc->retry_count ++;
            if(cc->retry_count > cc->max_redirect_count)
            {
                cc->need_update_route = 1;
                cc->retry_count = 0;
            }
        }
        __redisClusterSetError(cc, c->err, c->errstr);
        return REDIS_ERR;
    }

    if(redisGetReply(c, reply) != REDIS_OK)
    {
        __redisClusterSetError(cc, c->err, c->errstr);
        return REDIS_ERR;
    }
    
    if(cluster_reply_error_type(*reply) == CLUSTER_ERR_MOVED)
    {
        cc->need_update_route = 1;
    }

    return REDIS_OK;
}

static cluster_node *node_get_by_ask_error_reply(
    redisClusterContext *cc, redisReply *reply)
{
    sds *part = NULL, *ip_port = NULL;
    int part_len = 0, ip_port_len;
    dictEntry *de;
    cluster_node *node = NULL;

    if(cc == NULL || reply == NULL)
    {
        return NULL;
    }

    if(cluster_reply_error_type(reply) != CLUSTER_ERR_ASK)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "reply is not ask error!");
        return NULL;
    }
    
    part = sdssplitlen(reply->str, reply->len, " ", 1, &part_len);

    if(part != NULL && part_len == 3)
    {
        ip_port = sdssplitlen(part[2], sdslen(part[2]), 
            ":", 1, &ip_port_len);

        if(ip_port != NULL && ip_port_len == 2)
        {
            de = dictFind(cc->nodes, part[2]);
            if(de == NULL)
            {
                node = hi_alloc(sizeof(cluster_node));
                if(node == NULL)
                {
                    __redisClusterSetError(cc, 
                        REDIS_ERR_OOM, "Out of memory");

                    goto done;
                }

                cluster_node_init(node);
                node->addr = part[1];
                node->host = ip_port[0];
                node->port = hi_atoi(ip_port[1], sdslen(ip_port[1]));
                node->role = REDIS_ROLE_MASTER;

                dictAdd(cc->nodes, sdsnewlen(node->addr, sdslen(node->addr)), node);
                
                part = NULL;
                ip_port = NULL;
            }
            else
            {
                node = de->val;

                goto done;
            }
        }
        else
        {
            __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                "ask error reply address part parse error!");

            goto done;
        }

    }
    else
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, 
            "ask error reply parse error!");

        goto done;
    }

done:

    if(part != NULL)
    {
        sdsfreesplitres(part, part_len);
        part = NULL;
    }

    if(ip_port != NULL)
    {
        sdsfreesplitres(ip_port, ip_port_len);
        ip_port = NULL;
    }
    
    return node;
}

static void *redis_cluster_command_execute(redisClusterContext *cc, 
    struct cmd *command)
{
    int ret;
    void *reply = NULL;
    cluster_node *node;
    redisContext *c = NULL;
    int error_type;

retry:
    
    node = node_get_by_table(cc, (uint32_t)command->slot_num);
    if(node == NULL)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "node get by table error");
        return NULL;
    }

    c = ctx_get_by_node(node, cc->timeout, cc->flags);
    if(c == NULL)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "ctx get by node is null");
        return NULL;
    }
    else if(c->err)
    {
        node = node_get_witch_connected(cc);
        if(node == NULL)
        {
            __redisClusterSetError(cc, REDIS_ERR_OTHER, "no reachable node in cluster");
            return NULL;
        }

        cc->retry_count ++;
        if(cc->retry_count > cc->max_redirect_count)
        {
            __redisClusterSetError(cc, REDIS_ERR_CLUSTER_TOO_MANY_REDIRECT, 
                "too many cluster redirect");
            return NULL;
        }

        c = ctx_get_by_node(node, cc->timeout, cc->flags);
        if(c == NULL)
        {
            __redisClusterSetError(cc, REDIS_ERR_OTHER, "ctx get by node error");
            return NULL;
        }
        else if(c->err)
        {
            __redisClusterSetError(cc, c->err, c->errstr);
            return NULL;
        }
    }

ask_retry:

    if (__redisAppendCommand(c,command->cmd, command->clen) != REDIS_OK) 
    {
        __redisClusterSetError(cc, c->err, c->errstr);
        return NULL;
    }
    
    reply = __redisBlockForReply(c);
    if(reply == NULL)
    {
        __redisClusterSetError(cc, c->err, c->errstr);
        return NULL;
    }

    error_type = cluster_reply_error_type(reply);
    if(error_type > CLUSTER_NOT_ERR && error_type < CLUSTER_ERR_SENTINEL)
    {
        cc->retry_count ++;
        if(cc->retry_count > cc->max_redirect_count)
        {
            __redisClusterSetError(cc, REDIS_ERR_CLUSTER_TOO_MANY_REDIRECT, 
                "too many cluster redirect");
            freeReplyObject(reply);
            return NULL;
        }
        
        switch(error_type)
        {
        case CLUSTER_ERR_MOVED:
            freeReplyObject(reply);
            reply = NULL;
            ret = cluster_update_route(cc);
            if(ret != REDIS_OK)
            {
                __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                    "route update error, please recreate redisClusterContext!");
                return NULL;
            }
            
            goto retry;
            
            break;
        case CLUSTER_ERR_ASK:
            node = node_get_by_ask_error_reply(cc, reply);
            if(node == NULL)
            {
                freeReplyObject(reply);
                return NULL;
            }

            freeReplyObject(reply);
            reply = NULL;

            c = ctx_get_by_node(node, cc->timeout, cc->flags);
            if(c == NULL)
            {
                __redisClusterSetError(cc, REDIS_ERR_OTHER, "ctx get by node error");
                return NULL;
            }
            else if(c->err)
            {
                __redisClusterSetError(cc, c->err, c->errstr);
                return NULL;
            }

            reply = redisCommand(c, REDIS_COMMAND_ASKING);
            if(reply == NULL)
            {
                __redisClusterSetError(cc, c->err, c->errstr);
                return NULL;
            }

            freeReplyObject(reply);
            reply = NULL;
            
            goto ask_retry;

            break;
        case CLUSTER_ERR_TRYAGAIN:
        case CLUSTER_ERR_CROSSSLOT:
        case CLUSTER_ERR_CLUSTERDOWN:
            freeReplyObject(reply);
            reply = NULL;
            goto retry;
            
            break;
        default:

            break;
        }
    }
    
    return reply;
}

static int command_pre_fragment(redisClusterContext *cc, 
    struct cmd *command, hilist *commands)
{
    
    struct keypos *kp, *sub_kp;
    uint32_t key_count;
    uint32_t i, j;
    uint32_t idx;
    uint32_t key_len;
    int slot_num = -1;
    struct cmd *sub_command;
    struct cmd **sub_commands = NULL;
    char num_str[12];
    uint8_t num_str_len;
    

    if(command == NULL || commands == NULL)
    {
        goto done;
    }

    key_count = hiarray_n(command->keys);

    sub_commands = hi_zalloc(REDIS_CLUSTER_SLOTS * sizeof(*sub_commands));
    if (sub_commands == NULL) 
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto done;
    }

    command->frag_seq = hi_alloc(key_count * sizeof(*command->frag_seq));
    if(command->frag_seq == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto done;
    }
    
    
    for(i = 0; i < key_count; i ++)
    {
        kp = hiarray_get(command->keys, i);

        slot_num = keyHashSlot(kp->start, kp->end - kp->start);

        if(slot_num < 0 || slot_num >= REDIS_CLUSTER_SLOTS)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,"keyHashSlot return error");
            goto done;
        }

        if (sub_commands[slot_num] == NULL) {
            sub_commands[slot_num] = command_get();
            if (sub_commands[slot_num] == NULL) {
                __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
                slot_num = -1;
                goto done;
            }
        }

        command->frag_seq[i] = sub_command = sub_commands[slot_num];

        sub_command->narg++;

        sub_kp = hiarray_push(sub_command->keys);
        if (sub_kp == NULL) {
            __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
            slot_num = -1;
            goto done;
        }
        
        sub_kp->start = kp->start;
        sub_kp->end = kp->end;

        key_len = (uint32_t)(kp->end - kp->start);

        sub_command->clen += key_len + uint_len(key_len);

        sub_command->slot_num = slot_num;

        if (command->type == CMD_REQ_REDIS_MSET) {
            uint32_t len = 0;
            char *p;

            for (p = sub_kp->end + 1; !isdigit(*p); p++){}
            
            p = sub_kp->end + 1;
            while(!isdigit(*p))
            {
                p ++;
            }

            for (; isdigit(*p); p++) {              
                len = len * 10 + (uint32_t)(*p - '0');
            }
            
            len += CRLF_LEN * 2;
            len += (p - sub_kp->end);
            sub_kp->remain_len = len;
            sub_command->clen += len;
        }
    }

    for (i = 0; i < REDIS_CLUSTER_SLOTS; i++) {     /* prepend command header */
        sub_command = sub_commands[i];
        if (sub_command == NULL) {
            continue;
        }

        idx = 0;            
        if (command->type == CMD_REQ_REDIS_MGET) {
            //"*%d\r\n$4\r\nmget\r\n"
            
            sub_command->clen += 5*sub_command->narg;

            sub_command->narg ++;

            hi_itoa(num_str, sub_command->narg);
            num_str_len = (uint8_t)(strlen(num_str));

            sub_command->clen += 13 + num_str_len;

            sub_command->cmd = hi_zalloc(sub_command->clen * sizeof(*sub_command->cmd));
            if(sub_command->cmd == NULL)
            {
                __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
                slot_num = -1;
                goto done;
            }

            sub_command->cmd[idx++] = '*';
            memcpy(sub_command->cmd + idx, num_str, num_str_len);
            idx += num_str_len;
            memcpy(sub_command->cmd + idx, "\r\n$4\r\nmget\r\n", 12);
            idx += 12;
            
            for(j = 0; j < hiarray_n(sub_command->keys); j ++)
            {
                kp = hiarray_get(sub_command->keys, j);
                key_len = (uint32_t)(kp->end - kp->start);
                hi_itoa(num_str, key_len);
                num_str_len = strlen(num_str);

                sub_command->cmd[idx++] = '$';
                memcpy(sub_command->cmd + idx, num_str, num_str_len);
                idx += num_str_len;
                memcpy(sub_command->cmd + idx, CRLF, CRLF_LEN);
                idx += CRLF_LEN;
                memcpy(sub_command->cmd + idx, kp->start, key_len);
                idx += key_len;
                memcpy(sub_command->cmd + idx, CRLF, CRLF_LEN);
                idx += CRLF_LEN;
            }
        } else if (command->type == CMD_REQ_REDIS_DEL) {
            //"*%d\r\n$3\r\ndel\r\n"
            
            sub_command->clen += 5*sub_command->narg;

            sub_command->narg ++;

            hi_itoa(num_str, sub_command->narg);
            num_str_len = (uint8_t)strlen(num_str);
            
            sub_command->clen += 12 + num_str_len;

            sub_command->cmd = hi_zalloc(sub_command->clen * sizeof(*sub_command->cmd));
            if(sub_command->cmd == NULL)
            {
                __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
                slot_num = -1;
                goto done;
            }

            sub_command->cmd[idx++] = '*';
            memcpy(sub_command->cmd + idx, num_str, num_str_len);
            idx += num_str_len;
            memcpy(sub_command->cmd + idx, "\r\n$3\r\ndel\r\n", 11);
            idx += 11;

            for(j = 0; j < hiarray_n(sub_command->keys); j ++)
            {
                kp = hiarray_get(sub_command->keys, j);
                key_len = (uint32_t)(kp->end - kp->start);
                hi_itoa(num_str, key_len);
                num_str_len = strlen(num_str);

                sub_command->cmd[idx++] = '$';
                memcpy(sub_command->cmd + idx, num_str, num_str_len);
                idx += num_str_len;
                memcpy(sub_command->cmd + idx, CRLF, CRLF_LEN);
                idx += CRLF_LEN;
                memcpy(sub_command->cmd + idx, kp->start, key_len);
                idx += key_len;
                memcpy(sub_command->cmd + idx, CRLF, CRLF_LEN);
                idx += CRLF_LEN;
            }
        } else if (command->type == CMD_REQ_REDIS_MSET) {
            //"*%d\r\n$4\r\nmset\r\n"
            
            sub_command->clen += 3*sub_command->narg;

            sub_command->narg *= 2;

            sub_command->narg ++;

            hi_itoa(num_str, sub_command->narg);
            num_str_len = (uint8_t)strlen(num_str);
        
            sub_command->clen += 13 + num_str_len;

            sub_command->cmd = hi_zalloc(sub_command->clen * sizeof(*sub_command->cmd));
            if(sub_command->cmd == NULL)
            {
                __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
                slot_num = -1;
                goto done;
            }

            sub_command->cmd[idx++] = '*';
            memcpy(sub_command->cmd + idx, num_str, num_str_len);
            idx += num_str_len;
            memcpy(sub_command->cmd + idx, "\r\n$4\r\nmset\r\n", 12);
            idx += 12;
            
            for(j = 0; j < hiarray_n(sub_command->keys); j ++)
            {
                kp = hiarray_get(sub_command->keys, j);
                key_len = (uint32_t)(kp->end - kp->start);
                hi_itoa(num_str, key_len);
                num_str_len = strlen(num_str);

                sub_command->cmd[idx++] = '$';
                memcpy(sub_command->cmd + idx, num_str, num_str_len);
                idx += num_str_len;
                memcpy(sub_command->cmd + idx, CRLF, CRLF_LEN);
                idx += CRLF_LEN;
                memcpy(sub_command->cmd + idx, kp->start, key_len + kp->remain_len);
                idx += key_len + kp->remain_len;
                
            }
        } else {
            NOT_REACHED();
        }

        //printf("len : %d\n", sub_command->clen);
        //print_string_with_length_fix_CRLF(sub_command->cmd, sub_command->clen);
        
        sub_command->type = command->type;

        listAddNodeTail(commands, sub_command);
    }

done:

    if(sub_commands != NULL)
    {
        hi_free(sub_commands);
    }

    if(slot_num >= 0 && commands != NULL 
        && listLength(commands) == 1)
    {
        listNode *list_node = listFirst(commands);
        listDelNode(commands, list_node);
        if(command->frag_seq)
        {
            hi_free(command->frag_seq);
            command->frag_seq = NULL;
        }

        command->slot_num = slot_num;
    }

    return slot_num;
}

static void *command_post_fragment(redisClusterContext *cc, 
    struct cmd *command, hilist *commands)
{
    struct cmd *sub_command;
    listNode *list_node;
    listIter *list_iter;
    redisReply *reply, *sub_reply;
    long long count = 0;
    
    list_iter = listGetIterator(commands, AL_START_HEAD);
    while((list_node = listNext(list_iter)) != NULL)
    {
        sub_command = list_node->value;
        reply = sub_command->reply;
        if(reply == NULL)
        {
            return NULL;
        }
        else if(reply->type == REDIS_REPLY_ERROR)
        {
            return reply;
        }

        if (command->type == CMD_REQ_REDIS_MGET) {
            if(reply->type != REDIS_REPLY_ARRAY)
            {
                __redisClusterSetError(cc,REDIS_ERR_OTHER,"reply type is error(here only can be array)");
                return NULL;
            }
        }else if(command->type == CMD_REQ_REDIS_DEL){
            if(reply->type != REDIS_REPLY_INTEGER)
            {
                __redisClusterSetError(cc,REDIS_ERR_OTHER,"reply type is error(here only can be integer)");
                return NULL;
            }

            count += reply->integer;
        }else if(command->type == CMD_REQ_REDIS_MSET){
            if(reply->type != REDIS_REPLY_STATUS ||
                reply->len != 2 || strcmp(reply->str, REDIS_STATUS_OK) != 0)
            {
                __redisClusterSetError(cc,REDIS_ERR_OTHER,"reply type is error(here only can be status and ok)");
                return NULL;
            }
        }else {
            NOT_REACHED();
        }
    }

    reply = hi_calloc(1,sizeof(*reply));

    if (reply == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    }

    if (command->type == CMD_REQ_REDIS_MGET) {
        int i;
        uint32_t key_count;

        reply->type = REDIS_REPLY_ARRAY;

        key_count = hiarray_n(command->keys);

        reply->elements = key_count;
        reply->element = hi_calloc(key_count, sizeof(*reply));
        if (reply->element == NULL) {
            freeReplyObject(reply);
            __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
            return NULL;
        }
            
        for (i = key_count - 1; i >= 0; i--) {      /* for each key */
            sub_reply = command->frag_seq[i]->reply;            /* get it's reply */
            if (sub_reply == NULL) {
                freeReplyObject(reply);
                __redisClusterSetError(cc,REDIS_ERR_OTHER,"sub reply is null");
                return NULL;
            }

            if(sub_reply->type == REDIS_REPLY_STRING)
            {
                reply->element[i] = sub_reply;
            }
            else if(sub_reply->type == REDIS_REPLY_ARRAY)
            {
                if(sub_reply->elements == 0)
                {
                    freeReplyObject(reply);
                    __redisClusterSetError(cc,REDIS_ERR_OTHER,"sub reply elements error");
                    return NULL;
                }
                
                reply->element[i] = sub_reply->element[sub_reply->elements - 1];
                sub_reply->elements --;
            }
        }
    }else if(command->type == CMD_REQ_REDIS_DEL){
        reply->type = REDIS_REPLY_INTEGER;
        reply->integer = count;
    }else if(command->type == CMD_REQ_REDIS_MSET){
        reply->type = REDIS_REPLY_STATUS;
        uint32_t str_len = strlen(REDIS_STATUS_OK);
        reply->str = hi_alloc((str_len + 1) * sizeof(char*));
        if(reply->str == NULL)
        {
            freeReplyObject(reply);
            __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
            return NULL;
        }

        reply->len = str_len;
        memcpy(reply->str, REDIS_STATUS_OK, str_len);
        reply->str[str_len] = '\0';
    }else {
        NOT_REACHED();
    }

    return reply;
}

/* 
 * Split the command into subcommands by slot
 * 
 * Returns slot_num
 * If slot_num < 0 or slot_num >=  REDIS_CLUSTER_SLOTS means this function runs error;
 * Otherwise if  the commands > 1 , slot_num is the last subcommand slot number. 
 */
static int command_format_by_slot(redisClusterContext *cc, 
    struct cmd *command, hilist *commands)
{
    struct keypos *kp;
    int key_count;
    int slot_num = -1;

    if(cc == NULL || commands == NULL ||
        command == NULL || 
        command->cmd == NULL || command->clen <= 0)
    {
        goto done;
    }

    
    redis_parse_cmd(command);
    if(command->result == CMD_PARSE_ENOMEM)
    {
        __redisClusterSetError(cc, REDIS_ERR_PROTOCOL, "Parse command error: out of memory");
        goto done;
    }
    else if(command->result != CMD_PARSE_OK)
    {
        __redisClusterSetError(cc, REDIS_ERR_PROTOCOL, command->errstr);
        goto done;
    }

    key_count = hiarray_n(command->keys);

    if(key_count <= 0)
    {
        __redisClusterSetError(cc, REDIS_ERR_OTHER, "No keys in command(must have keys for redis cluster mode)");
        goto done;
    }
    else if(key_count == 1)
    {
        kp = hiarray_get(command->keys, 0);
        slot_num = keyHashSlot(kp->start, kp->end - kp->start);
        command->slot_num = slot_num;

        goto done;
    }

    slot_num = command_pre_fragment(cc, command, commands);

done:
    
    return slot_num;
}


void redisClusterSetMaxRedirect(redisClusterContext *cc, int max_redirect_count)
{
    if(cc == NULL || max_redirect_count <= 0)
    {
        return;
    }

    cc->max_redirect_count = max_redirect_count;
}

void *redisClusterFormattedCommand(redisClusterContext *cc, char *cmd, int len) {
    redisReply *reply = NULL;
    int slot_num;
    struct cmd *command = NULL, *sub_command;
    hilist *commands = NULL;
    listNode *list_node;
    listIter *list_iter = NULL;

    if(cc == NULL)
    {
        return NULL;
    }

    if(cc->err)
    {
        cc->err = 0;
        memset(cc->errstr, '\0', strlen(cc->errstr));
    }  
    
    command = command_get();
    if(command == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    }
    
    command->cmd = cmd;
    command->clen = len;

    commands = listCreate();
    if(commands == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }

    commands->free = listCommandFree;

    slot_num = command_format_by_slot(cc, command, commands);

    if(slot_num < 0)
    {
        goto error;
    }
    else if(slot_num >= REDIS_CLUSTER_SLOTS)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,"slot_num is out of range");
        goto error;
    }

    //all keys belong to one slot
    if(listLength(commands) == 0)
    {
        reply = redis_cluster_command_execute(cc, command);
        goto done;
    }

    ASSERT(listLength(commands) != 1);

    list_iter = listGetIterator(commands, AL_START_HEAD);
    while((list_node = listNext(list_iter)) != NULL)
    {
        sub_command = list_node->value;
        
        reply = redis_cluster_command_execute(cc, sub_command);
        if(reply == NULL)
        {
            goto error;
        }
        else if(reply->type == REDIS_REPLY_ERROR)
        {
            goto done;
        }

        sub_command->reply = reply;
    }

    reply = command_post_fragment(cc, command, commands);
    
done:

    command->cmd = NULL;
    command_destroy(command);

    if(commands != NULL)
    {
        listRelease(commands);
    }

    if(list_iter != NULL)
    {
        listReleaseIterator(list_iter);
    }

    cc->retry_count = 0;
    
    return reply;

error:

    if(command != NULL)
    {
        command->cmd = NULL;
        command_destroy(command);
    }

    if(commands != NULL)
    {
        listRelease(commands);
    }

    if(list_iter != NULL)
    {
        listReleaseIterator(list_iter);
    }

    cc->retry_count = 0;
    
    return NULL;
}

void *redisClustervCommand(redisClusterContext *cc, const char *format, va_list ap) {
    redisReply *reply;
    char *cmd;
    int len;

    if(cc == NULL)
    {
        return NULL;
    }

    len = redisvFormatCommand(&cmd,format,ap);

    if (len == -1) {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    } else if (len == -2) {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,"Invalid format string");
        return NULL;
    }   

    reply = redisClusterFormattedCommand(cc, cmd, len);

    free(cmd);

    return reply;
}

void *redisClusterCommand(redisClusterContext *cc, const char *format, ...) {
    va_list ap;
    redisReply *reply = NULL;
    
    va_start(ap,format);
    reply = redisClustervCommand(cc, format, ap);
    va_end(ap);

    return reply;
}

void *redisClusterCommandArgv(redisClusterContext *cc, int argc, const char **argv, const size_t *argvlen) {
    redisReply *reply = NULL;
    char *cmd;
    int len;

    len = redisFormatCommandArgv(&cmd,argc,argv,argvlen);
    if (len == -1) {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return NULL;
    }
	
    reply = redisClusterFormattedCommand(cc, cmd, len);

    free(cmd);

    return reply;
}

int redisClusterAppendFormattedCommand(redisClusterContext *cc, 
    char *cmd, int len) {
    int slot_num;
    struct cmd *command = NULL, *sub_command;
    hilist *commands = NULL;
    listNode *list_node;
    listIter *list_iter = NULL;

    if(cc->requests == NULL)
    {
        cc->requests = listCreate();
        if(cc->requests == NULL)
        {
            __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
            goto error;
        }

        cc->requests->free = listCommandFree;
    }
    
    command = command_get();
    if(command == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }
    
    command->cmd = cmd;
    command->clen = len;

    commands = listCreate();
    if(commands == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }

    commands->free = listCommandFree;

    slot_num = command_format_by_slot(cc, command, commands);

    if(slot_num < 0)
    {
        goto error;
    }
    else if(slot_num >= REDIS_CLUSTER_SLOTS)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,"slot_num is out of range");
        goto error;
    }

    //all keys belong to one slot
    if(listLength(commands) == 0)
    {
        if(__redisClusterAppendCommand(cc, command) == REDIS_OK)
        {
            goto done;
        }
        else
        {
            goto error;
        }
    }

    ASSERT(listLength(commands) != 1);

    list_iter = listGetIterator(commands, AL_START_HEAD);
    while((list_node = listNext(list_iter)) != NULL)
    {
        sub_command = list_node->value;
        
        if(__redisClusterAppendCommand(cc, sub_command) == REDIS_OK)
        {
            continue;
        }
        else
        {
            goto error;
        }
    }

done:

    if(command->cmd != NULL)
    {
        command->cmd = NULL;
    }
    else
    {
        goto error;
    }

    if(commands != NULL)
    {
        if(listLength(commands) > 0)
        {
            command->sub_commands = commands;
        }
        else
        {
            listRelease(commands);
        }
    }

    if(list_iter != NULL)
    {
        listReleaseIterator(list_iter);
    }

    listAddNodeTail(cc->requests, command);
    
    return REDIS_OK;

error:

    if(command != NULL)
    {
        command->cmd = NULL;
        command_destroy(command);
    }

    if(commands != NULL)
    {
        listRelease(commands);
    }

    if(list_iter != NULL)
    {
        listReleaseIterator(list_iter);
    }

    /* Attention: mybe here we must pop the 
      sub_commands that had append to the nodes.  
      But now we do not handle it. */
    
    return REDIS_ERR;
}


int redisClustervAppendCommand(redisClusterContext *cc, 
    const char *format, va_list ap) {
    int ret;
    char *cmd;
    int len;
    
    len = redisvFormatCommand(&cmd,format,ap);  
    if (len == -1) {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    } else if (len == -2) {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,"Invalid format string");
        return REDIS_ERR;
    }   

    ret = redisClusterAppendFormattedCommand(cc, cmd, len);

    free(cmd);

    return ret;
}

int redisClusterAppendCommand(redisClusterContext *cc, 
    const char *format, ...) {

    int ret;
    va_list ap;

    if(cc == NULL || format == NULL)
    {
        return REDIS_ERR;
    }
    
    va_start(ap,format);
    ret = redisClustervAppendCommand(cc, format, ap);
    va_end(ap);

    return ret;
}

int redisClusterAppendCommandArgv(redisClusterContext *cc, 
    int argc, const char **argv, const size_t *argvlen) {
    int ret;
    char *cmd;
    int len;

    len = redisFormatCommandArgv(&cmd,argc,argv,argvlen);
    if (len == -1) {
        __redisClusterSetError(cc,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    }
    
    ret = redisClusterAppendFormattedCommand(cc, cmd, len);
    
    free(cmd);

    return ret;
}

static int redisCLusterSendAll(redisClusterContext *cc)
{
    dictIterator *di;
    dictEntry *de;
    struct cluster_node *node;
    redisContext *c = NULL;
    int wdone = 0;
    
    if(cc == NULL || cc->nodes == NULL)
    {
        return REDIS_ERR;
    }

    di = dictGetIterator(cc->nodes);
    while((de = dictNext(di)) != NULL)
    {
        node = dictGetEntryVal(de);
        if(node == NULL)
        {
            continue;
        }
        
        c = ctx_get_by_node(node, cc->timeout, cc->flags);
        if(c == NULL)
        {
            continue;
        }

        if (c->flags & REDIS_BLOCK) {
            /* Write until done */
            do {
                if (redisBufferWrite(c,&wdone) == REDIS_ERR)
                {
                    dictReleaseIterator(di);
                    return REDIS_ERR;
                }
            } while (!wdone);
        }
    }
    
    dictReleaseIterator(di);

    return REDIS_OK;
}

static int redisCLusterClearAll(redisClusterContext *cc)
{
    dictIterator *di;
    dictEntry *de;
    struct cluster_node *node;
    redisContext *c = NULL;
    
    if (cc == NULL) {
        return REDIS_ERR;
    }

    if (cc->err) {
        cc->err = 0;
        memset(cc->errstr, '\0', strlen(cc->errstr));
    }

    if (cc->nodes == NULL) {
        return REDIS_ERR;
    }
    di = dictGetIterator(cc->nodes);
    while((de = dictNext(di)) != NULL)
    {
        node = dictGetEntryVal(de);
        if(node == NULL)
        {
            continue;
        }

        c = node->con;
        if(c == NULL)
        {
            continue;
        }

        redisFree(c);
        node->con = NULL;
    }
    
    dictReleaseIterator(di);
    
    return REDIS_OK;
}

int redisClusterGetReply(redisClusterContext *cc, void **reply) {

    struct cmd *command, *sub_command;
    hilist *commands = NULL;
    listNode *list_command, *list_sub_command;
    listIter *list_iter;
    int slot_num;
    void *sub_reply;

    if(cc == NULL || reply == NULL)
        return REDIS_ERR;

    cc->err = 0;
    cc->errstr[0] = '\0';

    *reply = NULL;

    if (cc->requests == NULL)
        return REDIS_ERR;

    list_command = listFirst(cc->requests);

    //no more reply
    if(list_command == NULL)
    {
        *reply = NULL;
        return REDIS_OK;
    }
    
    command = list_command->value;
    if(command == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "command in the requests list is null");
        goto error;
    }
    
    slot_num = command->slot_num;
    if(slot_num >= 0)
    {
        listDelNode(cc->requests, list_command);
        return __redisClusterGetReply(cc, slot_num, reply);
    }

    commands = command->sub_commands;
    if(commands == NULL)
    {
        __redisClusterSetError(cc,REDIS_ERR_OTHER,
            "sub_commands in command is null");
        goto error;
    }

    ASSERT(listLength(commands) != 1);

    list_iter = listGetIterator(commands, AL_START_HEAD);
    while((list_sub_command = listNext(list_iter)) != NULL)
    {
        sub_command = list_sub_command->value;
        if(sub_command == NULL)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "sub_command is null");
            goto error;
        }
        
        slot_num = sub_command->slot_num;
        if(slot_num < 0)
        {
            __redisClusterSetError(cc,REDIS_ERR_OTHER,
                "sub_command slot_num is less then zero");
            goto error;
        }
        
        if(__redisClusterGetReply(cc, slot_num, &sub_reply) != REDIS_OK)
        {
            goto error;
        }

        sub_command->reply = sub_reply;
    }

    *reply = command_post_fragment(cc, command, commands);
    if(*reply == NULL)
    {
        goto error;
    }

    listDelNode(cc->requests, list_command);
    return REDIS_OK;

error:

    listDelNode(cc->requests, list_command);
    return REDIS_ERR;
}

void redisClusterReset(redisClusterContext *cc)
{
    int status;
    void *reply;
    
    if(cc == NULL || cc->nodes == NULL)
    {
        return;
    }

    if (cc->err) {
        redisCLusterClearAll(cc);
    } else {
        redisCLusterSendAll(cc);
        
        do {
            status = redisClusterGetReply(cc, &reply);
            if (status == REDIS_OK) {
                freeReplyObject(reply);
            } else {
                redisCLusterClearAll(cc);
                break;
            }
        } while(reply != NULL);
    }
    
    if(cc->requests)
    {
        listRelease(cc->requests);
        cc->requests = NULL;
    }

    if(cc->need_update_route)
    {
        status = cluster_update_route(cc);
        if(status != REDIS_OK)
        {
            __redisClusterSetError(cc, REDIS_ERR_OTHER, 
                "route update error, please recreate redisClusterContext!");
            return;
        }
        cc->need_update_route = 0;
    }
}

/*############redis cluster async############*/

/* We want the error field to be accessible directly instead of requiring
 * an indirection to the redisContext struct. */
static void __redisClusterAsyncCopyError(redisClusterAsyncContext *acc) {
    if (!acc)
        return;

    redisClusterContext *cc = acc->cc;
    acc->err = cc->err;
    memcpy(acc->errstr, cc->errstr, 128);
}

static void __redisClusterAsyncSetError(redisClusterAsyncContext *acc, 
    int type, const char *str) {
    
    size_t len;

    acc->err = type;
    if (str != NULL) {
        len = strlen(str);
        len = len < (sizeof(acc->errstr)-1) ? len : (sizeof(acc->errstr)-1);
        memcpy(acc->errstr,str,len);
        acc->errstr[len] = '\0';
    } else {
        /* Only REDIS_ERR_IO may lack a description! */
        assert(type == REDIS_ERR_IO);
        __redis_strerror_r(errno, acc->errstr, sizeof(acc->errstr));
    }
}

static redisClusterAsyncContext *redisClusterAsyncInitialize(redisClusterContext *cc) {
    redisClusterAsyncContext *acc;

    if(cc == NULL)
    {
        return NULL;
    }

    acc = hi_alloc(sizeof(redisClusterAsyncContext));
    if (acc == NULL)
        return NULL;

    acc->cc = cc;

    acc->err = 0;
    acc->data = NULL;
    acc->adapter = NULL;
    acc->attach_fn = NULL;

    acc->onConnect = NULL;
    acc->onDisconnect = NULL;

    return acc;
}

static cluster_async_data *cluster_async_data_get(void)
{
    cluster_async_data *cad;

    cad = hi_alloc(sizeof(cluster_async_data));
    if(cad == NULL)
    {
        return NULL;
    }

    cad->acc = NULL;
    cad->command = NULL;
    cad->callback = NULL;
    cad->privdata = NULL;
    cad->retry_count = 0;

    return cad;
}

static void cluster_async_data_free(cluster_async_data *cad)
{
    if(cad == NULL)
    {
        return;
    }

    if(cad->command != NULL)
    {
        command_destroy(cad->command);
    }
    
    hi_free(cad);
    cad = NULL;
}

static void unlinkAsyncContextAndNode(redisAsyncContext* ac)
{
    cluster_node *node;

    if (ac->data) {
        node = (cluster_node *)(ac->data);
        node->acon = NULL;
    }
}

redisAsyncContext * actx_get_by_node(redisClusterAsyncContext *acc, 
    cluster_node *node)
{
    redisAsyncContext *ac;
    
    if(node == NULL)
    {
        return NULL;
    }

    ac = node->acon;
    if(ac != NULL)
    {
        if (ac->c.err == 0) {
            return ac;
        } else {
            NOT_REACHED();
        }
    }

    if(node->host == NULL || node->port <= 0)
    {
        __redisClusterAsyncSetError(acc, REDIS_ERR_OTHER, "node host or port is error");
        return NULL;
    }

    ac = redisAsyncConnect(node->host, node->port);
    if(ac == NULL)
    {
        __redisClusterAsyncSetError(acc, REDIS_ERR_OTHER, "node host or port is error");
        return NULL;
    }

    if(acc->adapter)
    {
        acc->attach_fn(ac, acc->adapter);
    }

    if(acc->onConnect)
    {
        redisAsyncSetConnectCallback(ac, acc->onConnect);
    }

    if(acc->onDisconnect)
    {
        redisAsyncSetDisconnectCallback(ac, acc->onDisconnect);
    }

    ac->data = node;
    ac->dataHandler = unlinkAsyncContextAndNode;
    node->acon = ac;
    
    return ac;
}

static redisAsyncContext *actx_get_after_update_route_by_slot(
    redisClusterAsyncContext *acc, int slot_num)
{
    int ret;
    redisClusterContext *cc;
    redisAsyncContext *ac;
    cluster_node *node;

    if(acc == NULL || slot_num < 0)
    {
        return NULL;
    }

    cc = acc->cc;
    if(cc == NULL)
    {
        return NULL;
    }
    
    ret = cluster_update_route(cc);
    if(ret != REDIS_OK)
    {
        __redisClusterAsyncSetError(acc, REDIS_ERR_OTHER, 
            "route update error, please recreate redisClusterContext!");
        return NULL;
    }

    node = node_get_by_table(cc, (uint32_t)slot_num);
    if(node == NULL)
    {
        __redisClusterAsyncSetError(acc, 
            REDIS_ERR_OTHER, "node get by table error");
        return NULL;
    }

    ac = actx_get_by_node(acc, node);
    if(ac == NULL)
    {
        __redisClusterAsyncSetError(acc, 
            REDIS_ERR_OTHER, "actx get by node error");
        return NULL;
    }
    else if(ac->err)
    {
        __redisClusterAsyncSetError(acc, ac->err, ac->errstr);
        return NULL;
    }

    return ac;
}

redisClusterAsyncContext *redisClusterAsyncConnect(const char *addrs, int flags) {

    redisClusterContext *cc;
    redisClusterAsyncContext *acc;

    cc = redisClusterConnectNonBlock(addrs, flags);
    if(cc == NULL)
    {
        return NULL;
    }

    acc = redisClusterAsyncInitialize(cc);
    if (acc == NULL) {
        redisClusterFree(cc);
        return NULL;
    }
    
    __redisClusterAsyncCopyError(acc);
    
    return acc;
}


int redisClusterAsyncSetConnectCallback(
    redisClusterAsyncContext *acc, redisConnectCallback *fn) 
{    
    if (acc->onConnect == NULL) {
        acc->onConnect = fn;
        return REDIS_OK;
    }
    return REDIS_ERR;
}

int redisClusterAsyncSetDisconnectCallback(
    redisClusterAsyncContext *acc, redisDisconnectCallback *fn)
{
    if (acc->onDisconnect == NULL) {
        acc->onDisconnect = fn;
        return REDIS_OK;
    }
    return REDIS_ERR;
}

static void redisClusterAsyncCallback(redisAsyncContext *ac, void *r, void *privdata) {
    int ret;
    redisReply *reply = r;
    cluster_async_data *cad = privdata;
    redisClusterAsyncContext *acc;
    redisClusterContext *cc;
    redisAsyncContext *ac_retry = NULL;
    int error_type;
    cluster_node *node;
    struct cmd *command;
    int64_t now, next;

    if(cad == NULL)
    {
        goto error;
    }

    acc = cad->acc;
    if(acc == NULL)
    {
        goto error;
    }

    cc = acc->cc;
    if(cc == NULL)
    {
        goto error;
    }

    command = cad->command;
    if(command == NULL)
    {
        goto error;
    }
    
    if(reply == NULL)
    {
        //Note: 
        //I can't decide witch is the best way to deal with connect 
        //problem for hiredis cluster async api.
        //But now the way is : when enough null reply for a node,
        //we will update the route after the cluster node timeout.
        //If you have a better idea, please contact with me. Thank you.
        //My email: diguo58@gmail.com
        
        node = (cluster_node *)(ac->data);
        ASSERT(node != NULL);
        
        __redisClusterAsyncSetError(acc, 
            ac->err, ac->errstr);
        
        if(cc->update_route_time != 0)
        {
            now = hi_usec_now();
            if(now >= cc->update_route_time)
            {
                ret = cluster_update_route(cc);
                if(ret != REDIS_OK)
                {
                    __redisClusterAsyncSetError(acc, REDIS_ERR_OTHER, 
                        "route update error, please recreate redisClusterContext!");
                }
                
                cc->update_route_time = 0LL;
            }
            
            goto done;
        }
        
        node->failure_count ++;
        if(node->failure_count > cc->max_redirect_count)
        {
            char *cluster_timeout_str;
            int cluster_timeout_str_len;
            int cluster_timeout;

            node->failure_count = 0;
            if(cc->update_route_time != 0)
            {
                goto done;
            }
            
            cluster_timeout_str = cluster_config_get(cc, 
                "cluster-node-timeout", &cluster_timeout_str_len);
            if(cluster_timeout_str == NULL)
            {
                __redisClusterAsyncSetError(acc, 
                    cc->err, cc->errstr);
                goto done;
            }

            cluster_timeout = hi_atoi(cluster_timeout_str, 
                cluster_timeout_str_len);
            free(cluster_timeout_str);
            if(cluster_timeout <= 0)
            {
                __redisClusterAsyncSetError(acc, 
                    REDIS_ERR_OTHER, 
                    "cluster_timeout_str convert to integer error");
                goto done;
            }

            now = hi_usec_now();
            if (now < 0) {
                __redisClusterAsyncSetError(acc, 
                    REDIS_ERR_OTHER, 
                    "get now usec time error");
                goto done;
            }

            next = now + (cluster_timeout * 1000LL);

            cc->update_route_time = next;
            
        }

        goto done;
    }

    error_type = cluster_reply_error_type(reply);

    if(error_type > CLUSTER_NOT_ERR && error_type < CLUSTER_ERR_SENTINEL)
    {
        cad->retry_count ++;
        if(cad->retry_count > cc->max_redirect_count)
        {
            cad->retry_count = 0;
            __redisClusterAsyncSetError(acc, 
                REDIS_ERR_CLUSTER_TOO_MANY_REDIRECT, 
                "too many cluster redirect");
            goto done;
        }
        
        switch(error_type)
        {
        case CLUSTER_ERR_MOVED:
            ac_retry = actx_get_after_update_route_by_slot(acc, command->slot_num);
            if(ac_retry == NULL)
            {
                goto done;
            }
            
            break;
        case CLUSTER_ERR_ASK:
            node = node_get_by_ask_error_reply(cc, reply);
            if(node == NULL)
            {
                __redisClusterAsyncSetError(acc, 
                    cc->err, cc->errstr);
                goto done;
            }

            ac_retry = actx_get_by_node(acc, node);
            if(ac_retry == NULL)
            {
                __redisClusterAsyncSetError(acc, 
                    REDIS_ERR_OTHER, "actx get by node error");
                goto done;
            }
            else if(ac_retry->err)
            {
                __redisClusterAsyncSetError(acc, 
                    ac_retry->err, ac_retry->errstr);
                goto done;
            }

            ret = redisAsyncCommand(ac_retry,
                NULL,NULL,REDIS_COMMAND_ASKING);
            if(ret != REDIS_OK)
            {
                goto error;
            }
            
            break;
        case CLUSTER_ERR_TRYAGAIN:
        case CLUSTER_ERR_CROSSSLOT:
        case CLUSTER_ERR_CLUSTERDOWN:
            ac_retry = ac;
            
            break;
        default:

            goto done;
            break;
        }

        goto retry;
    }

done:

    if(acc->err)
    {
        cad->callback(acc, NULL, cad->privdata);
    }
    else
    {
        cad->callback(acc, r, cad->privdata);
    }

    if(cc->err)
    {
        cc->err = 0;
        memset(cc->errstr, '\0', strlen(cc->errstr));
    }

    if(acc->err)
    {
        acc->err = 0;
        memset(acc->errstr, '\0', strlen(acc->errstr));
    }
    
    if(cad != NULL)
    {
        cluster_async_data_free(cad);
    }

    return;

retry:

    ret = redisAsyncFormattedCommand(ac_retry,
        redisClusterAsyncCallback,cad,command->cmd,command->clen);
    if(ret != REDIS_OK)
    {
        goto error;
    }
    
    return;

error:

    if(cad != NULL)
    {
        cluster_async_data_free(cad);
    }
}

int redisClusterAsyncFormattedCommand(redisClusterAsyncContext *acc, 
    redisClusterCallbackFn *fn, void *privdata, char *cmd, int len) {
    
    redisClusterContext *cc;
    int status = REDIS_OK;
    int slot_num;
    cluster_node *node;
    redisAsyncContext *ac;
    struct cmd *command = NULL;
    hilist *commands = NULL;
    cluster_async_data *cad;

    if(acc == NULL)
    {
        return REDIS_ERR;
    }

    cc = acc->cc;

    if(cc->err)
    {
        cc->err = 0;
        memset(cc->errstr, '\0', strlen(cc->errstr));
    }

    if(acc->err)
    {
        acc->err = 0;
        memset(acc->errstr, '\0', strlen(acc->errstr));
    }

    command = command_get();
    if(command == NULL)
    {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }
    
    command->cmd = malloc(len*sizeof(*command->cmd));
    if(command->cmd == NULL)
    {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }
    memcpy(command->cmd, cmd, len);
    command->clen = len;

    commands = listCreate();
    if(commands == NULL)
    {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }

    commands->free = listCommandFree;

    slot_num = command_format_by_slot(cc, command, commands);

    if(slot_num < 0)
    {
        __redisClusterAsyncSetError(acc,
            cc->err, cc->errstr);
        goto error;
    }
    else if(slot_num >= REDIS_CLUSTER_SLOTS)
    {
        __redisClusterAsyncSetError(acc,
            REDIS_ERR_OTHER,"slot_num is out of range");
        goto error;
    }

    //all keys not belong to one slot
    if(listLength(commands) > 0)
    {
        ASSERT(listLength(commands) != 1);
        
        __redisClusterAsyncSetError(acc,REDIS_ERR_OTHER,
            "Asynchronous API now not support multi-key command");
        goto error;
    }

    node = node_get_by_table(cc, (uint32_t) slot_num);
    if(node == NULL)
    {
        __redisClusterAsyncSetError(acc, 
            REDIS_ERR_OTHER, "node get by table error");
        goto error;
    }
    
    ac = actx_get_by_node(acc, node);
    if(ac == NULL)
    {
        __redisClusterAsyncSetError(acc, 
            REDIS_ERR_OTHER, "actx get by node error");
        goto error;
    }
    else if(ac->err)
    {
        __redisClusterAsyncSetError(acc, ac->err, ac->errstr);
        goto error;
    }

    cad = cluster_async_data_get();
    if(cad == NULL)
    {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        goto error;
    }

    cad->acc = acc;
    cad->command = command;
    cad->callback = fn;
    cad->privdata = privdata;
    
    status = redisAsyncFormattedCommand(ac,
        redisClusterAsyncCallback,cad,cmd,len);
    if(status != REDIS_OK)
    {
        goto error;
    }

    if(commands != NULL)
    {
        listRelease(commands);
    }

    return REDIS_OK;

error: 
    
    if(command != NULL)
    {
        command_destroy(command);
    }

    if(commands != NULL)
    {
        listRelease(commands);
    }

    return REDIS_ERR;
}


int redisClustervAsyncCommand(redisClusterAsyncContext *acc, 
    redisClusterCallbackFn *fn, void *privdata, const char *format, va_list ap) {
    int ret;
    char *cmd;
    int len;

    if(acc == NULL)
    {
        return REDIS_ERR;
    }

    len = redisvFormatCommand(&cmd,format,ap);
    if (len == -1) {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    } else if (len == -2) {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OTHER,"Invalid format string");
        return REDIS_ERR;
    }

    ret = redisClusterAsyncFormattedCommand(acc, fn, privdata, cmd, len);

    free(cmd);

    return ret;
}

int redisClusterAsyncCommand(redisClusterAsyncContext *acc, 
    redisClusterCallbackFn *fn, void *privdata, const char *format, ...) {
    int ret;
    va_list ap;

    va_start(ap,format);
    ret = redisClustervAsyncCommand(acc, fn, privdata, format, ap);
    va_end(ap);

    return ret;
}

int redisClusterAsyncCommandArgv(redisClusterAsyncContext *acc, 
    redisClusterCallbackFn *fn, void *privdata, int argc, const char **argv, const size_t *argvlen) {
    int ret;
    char *cmd;
    int len;
    
    len = redisFormatCommandArgv(&cmd,argc,argv,argvlen);
    if (len == -1) {
        __redisClusterAsyncSetError(acc,REDIS_ERR_OOM,"Out of memory");
        return REDIS_ERR;
    }

    ret = redisClusterAsyncFormattedCommand(acc, fn, privdata, cmd, len);

    free(cmd);

    return ret;
}

void redisClusterAsyncDisconnect(redisClusterAsyncContext *acc) {

    redisClusterContext *cc;
    redisAsyncContext *ac;
    dictIterator *di;
    dictEntry *de;
    dict *nodes;
    struct cluster_node *node;

    if(acc == NULL)
    {
        return;
    }

    cc = acc->cc;

    nodes = cc->nodes;

    if(nodes == NULL)
    {
        return;
    }
    
    di = dictGetIterator(nodes);

    while((de = dictNext(di)) != NULL) 
    {
        node = dictGetEntryVal(de);

        ac = node->acon;

        if(ac == NULL || ac->err)
        {
            continue;
        }

        redisAsyncDisconnect(ac);

        node->acon = NULL;
    }
}

void redisClusterAsyncFree(redisClusterAsyncContext *acc)
{
    redisClusterContext *cc;
    
    if(acc == NULL)
    {
        return;
    }

    cc = acc->cc;

    redisClusterFree(cc);

    hi_free(acc);
}

