
#ifndef __HIRCLUSTER_H
#define __HIRCLUSTER_H

#include "hiredis.h"
#include "async.h"

#define HIREDIS_VIP_MAJOR 0
#define HIREDIS_VIP_MINOR 3
#define HIREDIS_VIP_PATCH 0

#define REDIS_CLUSTER_SLOTS 16384

#define REDIS_ROLE_NULL     0
#define REDIS_ROLE_MASTER   1
#define REDIS_ROLE_SLAVE    2


#define HIRCLUSTER_FLAG_NULL                0x0
/* The flag to decide whether add slave node in 
  * redisClusterContext->nodes. This is set in the
  * least significant bit of the flags field in 
  * redisClusterContext. (1000000000000) */
#define HIRCLUSTER_FLAG_ADD_SLAVE           0x1000
/* The flag to decide whether add open slot  
  * for master node. (10000000000000) */
#define HIRCLUSTER_FLAG_ADD_OPENSLOT        0x2000
/* The flag to decide whether get the route 
  * table by 'cluster slots' command. Default   
  * is 'cluster nodes' command.*/
#define HIRCLUSTER_FLAG_ROUTE_USE_SLOTS     0x4000

struct dict;
struct hilist;

typedef struct cluster_node
{
    sds name;
    sds addr;
    sds host;
    int port;
    uint8_t role;
    uint8_t myself;   /* myself ? */
    redisContext *con;
    redisAsyncContext *acon;
    struct hilist *slots;
    struct hilist *slaves;
    int failure_count;
    void *data;     /* Not used by hiredis */
    struct hiarray *migrating;  /* copen_slot[] */
    struct hiarray *importing;  /* copen_slot[] */
}cluster_node;

typedef struct cluster_slot
{
    uint32_t start;
    uint32_t end;
    cluster_node *node; /* master that this slot region belong to */
}cluster_slot;

typedef struct copen_slot
{
    uint32_t slot_num;  /* slot number */
    int migrate;        /* migrating or importing? */
    sds remote_name;    /* name for the node that this slot migrating to/importing from */
    cluster_node *node; /* master that this slot belong to */
}copen_slot;

#ifdef __cplusplus
extern "C" {
#endif

/* Context for a connection to Redis cluster */
typedef struct redisClusterContext {
    int err; /* Error flags, 0 when there is no error */
    char errstr[128]; /* String representation of error when applicable */
    sds ip;
    int port;

    int flags;

    enum redisConnectionType connection_type;
    struct timeval *timeout;
    
    struct hiarray *slots;

    struct dict *nodes;
    cluster_node *table[REDIS_CLUSTER_SLOTS];

    uint64_t route_version;

    int max_redirect_count;
    int retry_count;

    struct hilist *requests;

    int need_update_route;
    int64_t update_route_time;
} redisClusterContext;

redisClusterContext *redisClusterConnect(const char *addrs, int flags);
redisClusterContext *redisClusterConnectWithTimeout(const char *addrs, 
    const struct timeval tv, int flags);
redisClusterContext *redisClusterConnectNonBlock(const char *addrs, int flags);

void redisClusterFree(redisClusterContext *cc);

void redisClusterSetMaxRedirect(redisClusterContext *cc, int max_redirect_count);

void *redisClusterFormattedCommand(redisClusterContext *cc, char *cmd, int len);
void *redisClustervCommand(redisClusterContext *cc, const char *format, va_list ap);
void *redisClusterCommand(redisClusterContext *cc, const char *format, ...);
void *redisClusterCommandArgv(redisClusterContext *cc, int argc, const char **argv, const size_t *argvlen);

redisContext *ctx_get_by_node(struct cluster_node *node, const struct timeval *timeout, int flags);

int redisClusterAppendFormattedCommand(redisClusterContext *cc, char *cmd, int len);
int redisClustervAppendCommand(redisClusterContext *cc, const char *format, va_list ap);
int redisClusterAppendCommand(redisClusterContext *cc, const char *format, ...);
int redisClusterAppendCommandArgv(redisClusterContext *cc, int argc, const char **argv, const size_t *argvlen);
int redisClusterGetReply(redisClusterContext *cc, void **reply);
void redisClusterReset(redisClusterContext *cc);

int cluster_update_route(redisClusterContext *cc);
int test_cluster_update_route(redisClusterContext *cc);
struct dict *parse_cluster_nodes(redisClusterContext *cc, char *str, int str_len, int flags);
struct dict *parse_cluster_slots(redisClusterContext *cc, redisReply *reply, int flags);


/*############redis cluster async############*/

struct redisClusterAsyncContext;

typedef int (adapterAttachFn)(redisAsyncContext*, void*);

typedef void (redisClusterCallbackFn)(struct redisClusterAsyncContext*, void*, void*);

/* Context for an async connection to Redis */
typedef struct redisClusterAsyncContext {
    
    redisClusterContext *cc;

    /* Setup error flags so they can be used directly. */
    int err;
    char errstr[128]; /* String representation of error when applicable */

    /* Not used by hiredis */
    void *data;

    void *adapter;
    adapterAttachFn *attach_fn;

    /* Called when either the connection is terminated due to an error or per
     * user request. The status is set accordingly (REDIS_OK, REDIS_ERR). */
    redisDisconnectCallback *onDisconnect;

    /* Called when the first write event was received. */
    redisConnectCallback *onConnect;

} redisClusterAsyncContext;

redisClusterAsyncContext *redisClusterAsyncConnect(const char *addrs, int flags);
int redisClusterAsyncSetConnectCallback(redisClusterAsyncContext *acc, redisConnectCallback *fn);
int redisClusterAsyncSetDisconnectCallback(redisClusterAsyncContext *acc, redisDisconnectCallback *fn);
int redisClusterAsyncFormattedCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, char *cmd, int len);
int redisClustervAsyncCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, const char *format, va_list ap);
int redisClusterAsyncCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, const char *format, ...);
int redisClusterAsyncCommandArgv(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, int argc, const char **argv, const size_t *argvlen);
void redisClusterAsyncDisconnect(redisClusterAsyncContext *acc);
void redisClusterAsyncFree(redisClusterAsyncContext *acc);

redisAsyncContext *actx_get_by_node(redisClusterAsyncContext *acc, cluster_node *node);

#ifdef __cplusplus
}
#endif

#endif
