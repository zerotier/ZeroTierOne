#include <ctype.h>
#include <errno.h>

#include "command.h"
#include "hiutil.h"
#include "hiarray.h"


static uint64_t cmd_id = 0;          /* command id counter */


/*
 * Return true, if the redis command take no key, otherwise
 * return false
 */
static int
redis_argz(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_PING:
    case CMD_REQ_REDIS_QUIT:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts no arguments, otherwise
 * return false
 */
static int
redis_arg0(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_EXISTS:
    case CMD_REQ_REDIS_PERSIST:
    case CMD_REQ_REDIS_PTTL:
    case CMD_REQ_REDIS_SORT:
    case CMD_REQ_REDIS_TTL:
    case CMD_REQ_REDIS_TYPE:
    case CMD_REQ_REDIS_DUMP:

    case CMD_REQ_REDIS_DECR:
    case CMD_REQ_REDIS_GET:
    case CMD_REQ_REDIS_INCR:
    case CMD_REQ_REDIS_STRLEN:

    case CMD_REQ_REDIS_HGETALL:
    case CMD_REQ_REDIS_HKEYS:
    case CMD_REQ_REDIS_HLEN:
    case CMD_REQ_REDIS_HVALS:

    case CMD_REQ_REDIS_LLEN:
    case CMD_REQ_REDIS_LPOP:
    case CMD_REQ_REDIS_RPOP:

    case CMD_REQ_REDIS_SCARD:
    case CMD_REQ_REDIS_SMEMBERS:
    case CMD_REQ_REDIS_SPOP:

    case CMD_REQ_REDIS_ZCARD:
    case CMD_REQ_REDIS_PFCOUNT:
    case CMD_REQ_REDIS_AUTH:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 1 argument, otherwise
 * return false
 */
static int
redis_arg1(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_EXPIRE:
    case CMD_REQ_REDIS_EXPIREAT:
    case CMD_REQ_REDIS_PEXPIRE:
    case CMD_REQ_REDIS_PEXPIREAT:

    case CMD_REQ_REDIS_APPEND:
    case CMD_REQ_REDIS_DECRBY:
    case CMD_REQ_REDIS_GETBIT:
    case CMD_REQ_REDIS_GETSET:
    case CMD_REQ_REDIS_INCRBY:
    case CMD_REQ_REDIS_INCRBYFLOAT:
    case CMD_REQ_REDIS_SETNX:

    case CMD_REQ_REDIS_HEXISTS:
    case CMD_REQ_REDIS_HGET:

    case CMD_REQ_REDIS_LINDEX:
    case CMD_REQ_REDIS_LPUSHX:
    case CMD_REQ_REDIS_RPOPLPUSH:
    case CMD_REQ_REDIS_RPUSHX:

    case CMD_REQ_REDIS_SISMEMBER:

    case CMD_REQ_REDIS_ZRANK:
    case CMD_REQ_REDIS_ZREVRANK:
    case CMD_REQ_REDIS_ZSCORE:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 2 arguments, otherwise
 * return false
 */
static int
redis_arg2(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_GETRANGE:
    case CMD_REQ_REDIS_PSETEX:
    case CMD_REQ_REDIS_SETBIT:
    case CMD_REQ_REDIS_SETEX:
    case CMD_REQ_REDIS_SETRANGE:

    case CMD_REQ_REDIS_HINCRBY:
    case CMD_REQ_REDIS_HINCRBYFLOAT:
    case CMD_REQ_REDIS_HSET:
    case CMD_REQ_REDIS_HSETNX:

    case CMD_REQ_REDIS_LRANGE:
    case CMD_REQ_REDIS_LREM:
    case CMD_REQ_REDIS_LSET:
    case CMD_REQ_REDIS_LTRIM:

    case CMD_REQ_REDIS_SMOVE:

    case CMD_REQ_REDIS_ZCOUNT:
    case CMD_REQ_REDIS_ZLEXCOUNT:
    case CMD_REQ_REDIS_ZINCRBY:
    case CMD_REQ_REDIS_ZREMRANGEBYLEX:
    case CMD_REQ_REDIS_ZREMRANGEBYRANK:
    case CMD_REQ_REDIS_ZREMRANGEBYSCORE:

    case CMD_REQ_REDIS_RESTORE:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 3 arguments, otherwise
 * return false
 */
static int
redis_arg3(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_LINSERT:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts 0 or more arguments, otherwise
 * return false
 */
static int
redis_argn(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_BITCOUNT:

    case CMD_REQ_REDIS_SET:
    case CMD_REQ_REDIS_HDEL:
    case CMD_REQ_REDIS_HMGET:
    case CMD_REQ_REDIS_HMSET:
    case CMD_REQ_REDIS_HSCAN:

    case CMD_REQ_REDIS_LPUSH:
    case CMD_REQ_REDIS_RPUSH:

    case CMD_REQ_REDIS_SADD:
    case CMD_REQ_REDIS_SDIFF:
    case CMD_REQ_REDIS_SDIFFSTORE:
    case CMD_REQ_REDIS_SINTER:
    case CMD_REQ_REDIS_SINTERSTORE:
    case CMD_REQ_REDIS_SREM:
    case CMD_REQ_REDIS_SUNION:
    case CMD_REQ_REDIS_SUNIONSTORE:
    case CMD_REQ_REDIS_SRANDMEMBER:
    case CMD_REQ_REDIS_SSCAN:

    case CMD_REQ_REDIS_PFADD:
    case CMD_REQ_REDIS_PFMERGE:

    case CMD_REQ_REDIS_ZADD:
    case CMD_REQ_REDIS_ZINTERSTORE:
    case CMD_REQ_REDIS_ZRANGE:
    case CMD_REQ_REDIS_ZRANGEBYSCORE:
    case CMD_REQ_REDIS_ZREM:
    case CMD_REQ_REDIS_ZREVRANGE:
    case CMD_REQ_REDIS_ZRANGEBYLEX:
    case CMD_REQ_REDIS_ZREVRANGEBYSCORE:
    case CMD_REQ_REDIS_ZUNIONSTORE:
    case CMD_REQ_REDIS_ZSCAN:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is a vector command accepting one or
 * more keys, otherwise return false
 */
static int
redis_argx(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_MGET:
    case CMD_REQ_REDIS_DEL:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is a vector command accepting one or
 * more key-value pairs, otherwise return false
 */
static int
redis_argkvx(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_MSET:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is either EVAL or EVALSHA. These commands
 * have a special format with exactly 2 arguments, followed by one or more keys,
 * followed by zero or more arguments (the documentation online seems to suggest
 * that at least one argument is required, but that shouldn't be the case).
 */
static int
redis_argeval(struct cmd *r)
{
    switch (r->type) {
    case CMD_REQ_REDIS_EVAL:
    case CMD_REQ_REDIS_EVALSHA:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Reference: http://redis.io/topics/protocol
 *
 * Redis >= 1.2 uses the unified protocol to send requests to the Redis
 * server. In the unified protocol all the arguments sent to the server
 * are binary safe and every request has the following general form:
 *
 *   *<number of arguments> CR LF
 *   $<number of bytes of argument 1> CR LF
 *   <argument data> CR LF
 *   ...
 *   $<number of bytes of argument N> CR LF
 *   <argument data> CR LF
 *
 * Before the unified request protocol, redis protocol for requests supported
 * the following commands
 * 1). Inline commands: simple commands where arguments are just space
 *     separated strings. No binary safeness is possible.
 * 2). Bulk commands: bulk commands are exactly like inline commands, but
 *     the last argument is handled in a special way in order to allow for
 *     a binary-safe last argument.
 *
 * only supports the Redis unified protocol for requests.
 */
void
redis_parse_cmd(struct cmd *r)
{
    int len;
    char *p, *m, *token = NULL;
    char *cmd_end;
    char ch;
    uint32_t rlen = 0;  /* running length in parsing fsa */
    uint32_t rnarg = 0; /* running # arg used by parsing fsa */
    enum {
        SW_START,
        SW_NARG,
        SW_NARG_LF,
        SW_REQ_TYPE_LEN,
        SW_REQ_TYPE_LEN_LF,
        SW_REQ_TYPE,
        SW_REQ_TYPE_LF,
        SW_KEY_LEN,
        SW_KEY_LEN_LF,
        SW_KEY,
        SW_KEY_LF,
        SW_ARG1_LEN,
        SW_ARG1_LEN_LF,
        SW_ARG1,
        SW_ARG1_LF,
        SW_ARG2_LEN,
        SW_ARG2_LEN_LF,
        SW_ARG2,
        SW_ARG2_LF,
        SW_ARG3_LEN,
        SW_ARG3_LEN_LF,
        SW_ARG3,
        SW_ARG3_LF,
        SW_ARGN_LEN,
        SW_ARGN_LEN_LF,
        SW_ARGN,
        SW_ARGN_LF,
        SW_SENTINEL
    } state;

    state = SW_START;
    cmd_end = r->cmd + r->clen;

    ASSERT(state >= SW_START && state < SW_SENTINEL);
    ASSERT(r->cmd != NULL && r->clen > 0);

    for (p = r->cmd; p < cmd_end; p++) {
        ch = *p;

        switch (state) {

        case SW_START:
        case SW_NARG:
            if (token == NULL) {
                if (ch != '*') {
                    goto error;
                }
                token = p;
                /* req_start <- p */
                r->narg_start = p;
                rnarg = 0;
                state = SW_NARG;
            } else if (isdigit(ch)) {
                rnarg = rnarg * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (rnarg == 0) {
                    goto error;
                }
                r->narg = rnarg;
                r->narg_end = p;
                token = NULL;
                state = SW_NARG_LF;
            } else {
                goto error;
            }

            break;

        case SW_NARG_LF:
            switch (ch) {
            case LF:
                state = SW_REQ_TYPE_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_REQ_TYPE_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                token = p;
                rlen = 0;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (rlen == 0 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_REQ_TYPE_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_REQ_TYPE_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_REQ_TYPE;
                break;

            default:
                goto error;
            }

            break;

        case SW_REQ_TYPE:
            if (token == NULL) {
                token = p;
            }

            m = token + rlen;
            if (m >= cmd_end) {
                //m = cmd_end - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            m = token;
            token = NULL;
            r->type = CMD_UNKNOWN;

            switch (p - m) {

            case 3:
                if (str3icmp(m, 'g', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_GET;
                    break;
                }

                if (str3icmp(m, 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_SET;
                    break;
                }

                if (str3icmp(m, 't', 't', 'l')) {
                    r->type = CMD_REQ_REDIS_TTL;
                    break;
                }

                if (str3icmp(m, 'd', 'e', 'l')) {
                    r->type = CMD_REQ_REDIS_DEL;
                    break;
                }

                break;

            case 4:
                if (str4icmp(m, 'p', 't', 't', 'l')) {
                    r->type = CMD_REQ_REDIS_PTTL;
                    break;
                }

                if (str4icmp(m, 'd', 'e', 'c', 'r')) {
                    r->type = CMD_REQ_REDIS_DECR;
                    break;
                }

                if (str4icmp(m, 'd', 'u', 'm', 'p')) {
                    r->type = CMD_REQ_REDIS_DUMP;
                    break;
                }

                if (str4icmp(m, 'h', 'd', 'e', 'l')) {
                    r->type = CMD_REQ_REDIS_HDEL;
                    break;
                }

                if (str4icmp(m, 'h', 'g', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_HGET;
                    break;
                }

                if (str4icmp(m, 'h', 'l', 'e', 'n')) {
                    r->type = CMD_REQ_REDIS_HLEN;
                    break;
                }

                if (str4icmp(m, 'h', 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_HSET;
                    break;
                }

                if (str4icmp(m, 'i', 'n', 'c', 'r')) {
                    r->type = CMD_REQ_REDIS_INCR;
                    break;
                }

                if (str4icmp(m, 'l', 'l', 'e', 'n')) {
                    r->type = CMD_REQ_REDIS_LLEN;
                    break;
                }

                if (str4icmp(m, 'l', 'p', 'o', 'p')) {
                    r->type = CMD_REQ_REDIS_LPOP;
                    break;
                }

                if (str4icmp(m, 'l', 'r', 'e', 'm')) {
                    r->type = CMD_REQ_REDIS_LREM;
                    break;
                }

                if (str4icmp(m, 'l', 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_LSET;
                    break;
                }

                if (str4icmp(m, 'r', 'p', 'o', 'p')) {
                    r->type = CMD_REQ_REDIS_RPOP;
                    break;
                }

                if (str4icmp(m, 's', 'a', 'd', 'd')) {
                    r->type = CMD_REQ_REDIS_SADD;
                    break;
                }

                if (str4icmp(m, 's', 'p', 'o', 'p')) {
                    r->type = CMD_REQ_REDIS_SPOP;
                    break;
                }

                if (str4icmp(m, 's', 'r', 'e', 'm')) {
                    r->type = CMD_REQ_REDIS_SREM;
                    break;
                }

                if (str4icmp(m, 't', 'y', 'p', 'e')) {
                    r->type = CMD_REQ_REDIS_TYPE;
                    break;
                }

                if (str4icmp(m, 'm', 'g', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_MGET;
                    break;
                }
                if (str4icmp(m, 'm', 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_MSET;
                    break;
                }

                if (str4icmp(m, 'z', 'a', 'd', 'd')) {
                    r->type = CMD_REQ_REDIS_ZADD;
                    break;
                }

                if (str4icmp(m, 'z', 'r', 'e', 'm')) {
                    r->type = CMD_REQ_REDIS_ZREM;
                    break;
                }

                if (str4icmp(m, 'e', 'v', 'a', 'l')) {
                    r->type = CMD_REQ_REDIS_EVAL;
                    break;
                }

                if (str4icmp(m, 's', 'o', 'r', 't')) {
                    r->type = CMD_REQ_REDIS_SORT;
                    break;
                }

                if (str4icmp(m, 'p', 'i', 'n', 'g')) {
                    r->type = CMD_REQ_REDIS_PING;
                    r->noforward = 1;
                    break;
                }

                if (str4icmp(m, 'q', 'u', 'i', 't')) {
                    r->type = CMD_REQ_REDIS_QUIT;
                    r->quit = 1;
                    break;
                }

                if (str4icmp(m, 'a', 'u', 't', 'h')) {
                    r->type = CMD_REQ_REDIS_AUTH;
                    r->noforward = 1;
                    break;
                }

                break;

            case 5:
                if (str5icmp(m, 'h', 'k', 'e', 'y', 's')) {
                    r->type = CMD_REQ_REDIS_HKEYS;
                    break;
                }

                if (str5icmp(m, 'h', 'm', 'g', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_HMGET;
                    break;
                }

                if (str5icmp(m, 'h', 'm', 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_HMSET;
                    break;
                }

                if (str5icmp(m, 'h', 'v', 'a', 'l', 's')) {
                    r->type = CMD_REQ_REDIS_HVALS;
                    break;
                }

                if (str5icmp(m, 'h', 's', 'c', 'a', 'n')) {
                    r->type = CMD_REQ_REDIS_HSCAN;
                    break;
                }

                if (str5icmp(m, 'l', 'p', 'u', 's', 'h')) {
                    r->type = CMD_REQ_REDIS_LPUSH;
                    break;
                }

                if (str5icmp(m, 'l', 't', 'r', 'i', 'm')) {
                    r->type = CMD_REQ_REDIS_LTRIM;
                    break;
                }

                if (str5icmp(m, 'r', 'p', 'u', 's', 'h')) {
                    r->type = CMD_REQ_REDIS_RPUSH;
                    break;
                }

                if (str5icmp(m, 's', 'c', 'a', 'r', 'd')) {
                    r->type = CMD_REQ_REDIS_SCARD;
                    break;
                }

                if (str5icmp(m, 's', 'd', 'i', 'f', 'f')) {
                    r->type = CMD_REQ_REDIS_SDIFF;
                    break;
                }

                if (str5icmp(m, 's', 'e', 't', 'e', 'x')) {
                    r->type = CMD_REQ_REDIS_SETEX;
                    break;
                }

                if (str5icmp(m, 's', 'e', 't', 'n', 'x')) {
                    r->type = CMD_REQ_REDIS_SETNX;
                    break;
                }

                if (str5icmp(m, 's', 'm', 'o', 'v', 'e')) {
                    r->type = CMD_REQ_REDIS_SMOVE;
                    break;
                }

                if (str5icmp(m, 's', 's', 'c', 'a', 'n')) {
                    r->type = CMD_REQ_REDIS_SSCAN;
                    break;
                }

                if (str5icmp(m, 'z', 'c', 'a', 'r', 'd')) {
                    r->type = CMD_REQ_REDIS_ZCARD;
                    break;
                }

                if (str5icmp(m, 'z', 'r', 'a', 'n', 'k')) {
                    r->type = CMD_REQ_REDIS_ZRANK;
                    break;
                }

                if (str5icmp(m, 'z', 's', 'c', 'a', 'n')) {
                    r->type = CMD_REQ_REDIS_ZSCAN;
                    break;
                }

                if (str5icmp(m, 'p', 'f', 'a', 'd', 'd')) {
                    r->type = CMD_REQ_REDIS_PFADD;
                    break;
                }

                break;

            case 6:
                if (str6icmp(m, 'a', 'p', 'p', 'e', 'n', 'd')) {
                    r->type = CMD_REQ_REDIS_APPEND;
                    break;
                }

                if (str6icmp(m, 'd', 'e', 'c', 'r', 'b', 'y')) {
                    r->type = CMD_REQ_REDIS_DECRBY;
                    break;
                }

                if (str6icmp(m, 'e', 'x', 'i', 's', 't', 's')) {
                    r->type = CMD_REQ_REDIS_EXISTS;
                    break;
                }

                if (str6icmp(m, 'e', 'x', 'p', 'i', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_EXPIRE;
                    break;
                }

                if (str6icmp(m, 'g', 'e', 't', 'b', 'i', 't')) {
                    r->type = CMD_REQ_REDIS_GETBIT;
                    break;
                }

                if (str6icmp(m, 'g', 'e', 't', 's', 'e', 't')) {
                    r->type = CMD_REQ_REDIS_GETSET;
                    break;
                }

                if (str6icmp(m, 'p', 's', 'e', 't', 'e', 'x')) {
                    r->type = CMD_REQ_REDIS_PSETEX;
                    break;
                }

                if (str6icmp(m, 'h', 's', 'e', 't', 'n', 'x')) {
                    r->type = CMD_REQ_REDIS_HSETNX;
                    break;
                }

                if (str6icmp(m, 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = CMD_REQ_REDIS_INCRBY;
                    break;
                }

                if (str6icmp(m, 'l', 'i', 'n', 'd', 'e', 'x')) {
                    r->type = CMD_REQ_REDIS_LINDEX;
                    break;
                }

                if (str6icmp(m, 'l', 'p', 'u', 's', 'h', 'x')) {
                    r->type = CMD_REQ_REDIS_LPUSHX;
                    break;
                }

                if (str6icmp(m, 'l', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_LRANGE;
                    break;
                }

                if (str6icmp(m, 'r', 'p', 'u', 's', 'h', 'x')) {
                    r->type = CMD_REQ_REDIS_RPUSHX;
                    break;
                }

                if (str6icmp(m, 's', 'e', 't', 'b', 'i', 't')) {
                    r->type = CMD_REQ_REDIS_SETBIT;
                    break;
                }

                if (str6icmp(m, 's', 'i', 'n', 't', 'e', 'r')) {
                    r->type = CMD_REQ_REDIS_SINTER;
                    break;
                }

                if (str6icmp(m, 's', 't', 'r', 'l', 'e', 'n')) {
                    r->type = CMD_REQ_REDIS_STRLEN;
                    break;
                }

                if (str6icmp(m, 's', 'u', 'n', 'i', 'o', 'n')) {
                    r->type = CMD_REQ_REDIS_SUNION;
                    break;
                }

                if (str6icmp(m, 'z', 'c', 'o', 'u', 'n', 't')) {
                    r->type = CMD_REQ_REDIS_ZCOUNT;
                    break;
                }

                if (str6icmp(m, 'z', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_ZRANGE;
                    break;
                }

                if (str6icmp(m, 'z', 's', 'c', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZSCORE;
                    break;
                }

                break;

            case 7:
                if (str7icmp(m, 'p', 'e', 'r', 's', 'i', 's', 't')) {
                    r->type = CMD_REQ_REDIS_PERSIST;
                    break;
                }

                if (str7icmp(m, 'p', 'e', 'x', 'p', 'i', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_PEXPIRE;
                    break;
                }

                if (str7icmp(m, 'h', 'e', 'x', 'i', 's', 't', 's')) {
                    r->type = CMD_REQ_REDIS_HEXISTS;
                    break;
                }

                if (str7icmp(m, 'h', 'g', 'e', 't', 'a', 'l', 'l')) {
                    r->type = CMD_REQ_REDIS_HGETALL;
                    break;
                }

                if (str7icmp(m, 'h', 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = CMD_REQ_REDIS_HINCRBY;
                    break;
                }

                if (str7icmp(m, 'l', 'i', 'n', 's', 'e', 'r', 't')) {
                    r->type = CMD_REQ_REDIS_LINSERT;
                    break;
                }

                if (str7icmp(m, 'z', 'i', 'n', 'c', 'r', 'b', 'y')) {
                    r->type = CMD_REQ_REDIS_ZINCRBY;
                    break;
                }

                if (str7icmp(m, 'e', 'v', 'a', 'l', 's', 'h', 'a')) {
                    r->type = CMD_REQ_REDIS_EVALSHA;
                    break;
                }

                if (str7icmp(m, 'r', 'e', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_RESTORE;
                    break;
                }

                if (str7icmp(m, 'p', 'f', 'c', 'o', 'u', 'n', 't')) {
                    r->type = CMD_REQ_REDIS_PFCOUNT;
                    break;
                }

                if (str7icmp(m, 'p', 'f', 'm', 'e', 'r', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_PFMERGE;
                    break;
                }

                break;

            case 8:
                if (str8icmp(m, 'e', 'x', 'p', 'i', 'r', 'e', 'a', 't')) {
                    r->type = CMD_REQ_REDIS_EXPIREAT;
                    break;
                }

                if (str8icmp(m, 'b', 'i', 't', 'c', 'o', 'u', 'n', 't')) {
                    r->type = CMD_REQ_REDIS_BITCOUNT;
                    break;
                }

                if (str8icmp(m, 'g', 'e', 't', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_GETRANGE;
                    break;
                }

                if (str8icmp(m, 's', 'e', 't', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_SETRANGE;
                    break;
                }

                if (str8icmp(m, 's', 'm', 'e', 'm', 'b', 'e', 'r', 's')) {
                    r->type = CMD_REQ_REDIS_SMEMBERS;
                    break;
                }

                if (str8icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'k')) {
                    r->type = CMD_REQ_REDIS_ZREVRANK;
                    break;
                }

                break;

            case 9:
                if (str9icmp(m, 'p', 'e', 'x', 'p', 'i', 'r', 'e', 'a', 't')) {
                    r->type = CMD_REQ_REDIS_PEXPIREAT;
                    break;
                }

                if (str9icmp(m, 'r', 'p', 'o', 'p', 'l', 'p', 'u', 's', 'h')) {
                    r->type = CMD_REQ_REDIS_RPOPLPUSH;
                    break;
                }

                if (str9icmp(m, 's', 'i', 's', 'm', 'e', 'm', 'b', 'e', 'r')) {
                    r->type = CMD_REQ_REDIS_SISMEMBER;
                    break;
                }

                if (str9icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'g', 'e')) {
                    r->type = CMD_REQ_REDIS_ZREVRANGE;
                    break;
                }

                if (str9icmp(m, 'z', 'l', 'e', 'x', 'c', 'o', 'u', 'n', 't')) {
                    r->type = CMD_REQ_REDIS_ZLEXCOUNT;
                    break;
                }

                break;

            case 10:
                if (str10icmp(m, 's', 'd', 'i', 'f', 'f', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_SDIFFSTORE;
                    break;
                }

            case 11:
                if (str11icmp(m, 'i', 'n', 'c', 'r', 'b', 'y', 'f', 'l', 'o', 'a', 't')) {
                    r->type = CMD_REQ_REDIS_INCRBYFLOAT;
                    break;
                }

                if (str11icmp(m, 's', 'i', 'n', 't', 'e', 'r', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_SINTERSTORE;
                    break;
                }

                if (str11icmp(m, 's', 'r', 'a', 'n', 'd', 'm', 'e', 'm', 'b', 'e', 'r')) {
                    r->type = CMD_REQ_REDIS_SRANDMEMBER;
                    break;
                }

                if (str11icmp(m, 's', 'u', 'n', 'i', 'o', 'n', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_SUNIONSTORE;
                    break;
                }

                if (str11icmp(m, 'z', 'i', 'n', 't', 'e', 'r', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZINTERSTORE;
                    break;
                }

                if (str11icmp(m, 'z', 'u', 'n', 'i', 'o', 'n', 's', 't', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZUNIONSTORE;
                    break;
                }

                if (str11icmp(m, 'z', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 'l', 'e', 'x')) {
                    r->type = CMD_REQ_REDIS_ZRANGEBYLEX;
                    break;
                }

                break;

            case 12:
                if (str12icmp(m, 'h', 'i', 'n', 'c', 'r', 'b', 'y', 'f', 'l', 'o', 'a', 't')) {
                    r->type = CMD_REQ_REDIS_HINCRBYFLOAT;
                    break;
                }


                break;

            case 13:
                if (str13icmp(m, 'z', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZRANGEBYSCORE;
                    break;
                }

                break;

            case 14:
                if (str14icmp(m, 'z', 'r', 'e', 'm', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 'l', 'e', 'x')) {
                    r->type = CMD_REQ_REDIS_ZREMRANGEBYLEX;
                    break;
                }

                break;

            case 15:
                if (str15icmp(m, 'z', 'r', 'e', 'm', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 'r', 'a', 'n', 'k')) {
                    r->type = CMD_REQ_REDIS_ZREMRANGEBYRANK;
                    break;
                }

                break;

            case 16:
                if (str16icmp(m, 'z', 'r', 'e', 'm', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZREMRANGEBYSCORE;
                    break;
                }

                if (str16icmp(m, 'z', 'r', 'e', 'v', 'r', 'a', 'n', 'g', 'e', 'b', 'y', 's', 'c', 'o', 'r', 'e')) {
                    r->type = CMD_REQ_REDIS_ZREVRANGEBYSCORE;
                    break;
                }

                break;

            default:
                break;
            }

            if (r->type == CMD_UNKNOWN) {
                goto error;
            }

            state = SW_REQ_TYPE_LF;
            break;

        case SW_REQ_TYPE_LF:
            switch (ch) {
            case LF:
                if (redis_argz(r)) {
                    goto done;
                } else if (redis_argeval(r)) {
                    state = SW_ARG1_LEN;
                } else {
                    state = SW_KEY_LEN;
                }
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                token = p;
                rlen = 0;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                
                if (rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_KEY_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_KEY_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_KEY;
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY:
            if (token == NULL) {
                token = p;
            }

            m = token + rlen;
            if (m >= cmd_end) {
                //m = b->last - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            } else {        /* got a key */
                struct keypos *kpos;

                p = m;      /* move forward by rlen bytes */
                rlen = 0;
                m = token;
                token = NULL;

                kpos = hiarray_push(r->keys);
                if (kpos == NULL) {
                    goto enomem;
                }
                kpos->start = m;
                kpos->end = p;
                //kpos->v_len = 0;

                state = SW_KEY_LF;
            }

            break;

        case SW_KEY_LF:
            switch (ch) {
            case LF:
                if (redis_arg0(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg1(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_arg2(r)) {
                    if (rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_arg3(r)) {
                    if (rnarg != 3) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_KEY_LEN;
                } else if (redis_argkvx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    if (r->narg % 2 == 0) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argeval(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG1_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;

                /*
                //for mset value length
                if(redis_argkvx(r))
                {
                    struct keypos *kpos;
                    uint32_t array_len = array_n(r->keys);
                    if(array_len == 0)
                    {
                        goto error;
                    }
                    
                    kpos = array_n(r->keys, array_len-1);
                    if (kpos == NULL || kpos->v_len != 0) {
                        goto error;
                    }

                    kpos->v_len = rlen;
                }
                */
                state = SW_ARG1_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG1_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG1;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG1:
            m = p + rlen;
            if (m >= cmd_end) {
                //rlen -= (uint32_t)(b->last - p);
                //m = b->last - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;

            state = SW_ARG1_LF;

            break;

        case SW_ARG1_LF:
            switch (ch) {
            case LF:
                if (redis_arg1(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg2(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_arg3(r)) {
                    if (rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else if (redis_argeval(r)) {
                    if (rnarg < 2) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_argkvx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_KEY_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARG2_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG2_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG2;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2:
            if (token == NULL && redis_argeval(r)) {
                /*
                 * For EVAL/EVALSHA, ARG2 represents the # key/arg pairs which must
                 * be tokenized and stored in contiguous memory.
                 */
                token = p;
            }

            m = p + rlen;
            if (m >= cmd_end) {
                //rlen -= (uint32_t)(b->last - p);
                //m = b->last - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;

            if (redis_argeval(r)) {
                uint32_t nkey;
                char *chp;

                /*
                 * For EVAL/EVALSHA, we need to find the integer value of this
                 * argument. It tells us the number of keys in the script, and
                 * we need to error out if number of keys is 0. At this point,
                 * both p and m point to the end of the argument and r->token
                 * points to the start.
                 */
                if (p - token < 1) {
                    goto error;
                }

                for (nkey = 0, chp = token; chp < p; chp++) {
                    if (isdigit(*chp)) {
                        nkey = nkey * 10 + (uint32_t)(*chp - '0');
                    } else {
                        goto error;
                    }
                }
                if (nkey == 0) {
                    goto error;
                }

                token = NULL;
            }

            state = SW_ARG2_LF;

            break;

        case SW_ARG2_LF:
            switch (ch) {
            case LF:
                if (redis_arg2(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg3(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG3_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else if (redis_argeval(r)) {
                    if (rnarg < 1) {
                        goto error;
                    }
                    state = SW_KEY_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARG3_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG3_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG3;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3:
            m = p + rlen;
            if (m >= cmd_end) {
                //rlen -= (uint32_t)(b->last - p);
                //m = b->last - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            state = SW_ARG3_LF;

            break;

        case SW_ARG3_LF:
            switch (ch) {
            case LF:
                if (redis_arg3(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARGN_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARGN_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARGN;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN:
            m = p + rlen;
            if (m >= cmd_end) {
                //rlen -= (uint32_t)(b->last - p);
                //m = b->last - 1;
                //p = m;
                //break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            state = SW_ARGN_LF;

            break;

        case SW_ARGN_LF:
            switch (ch) {
            case LF:
                if (redis_argn(r) || redis_argeval(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_SENTINEL:
        default:
            NOT_REACHED();
            break;
        }
    }

    ASSERT(p == cmd_end);

    return;

done:

    ASSERT(r->type > CMD_UNKNOWN && r->type < CMD_SENTINEL);
    
    r->result = CMD_PARSE_OK;

    return;

enomem:
    
    r->result = CMD_PARSE_ENOMEM;

    return;

error:
    
    r->result = CMD_PARSE_ERROR;
    errno = EINVAL;
    if(r->errstr == NULL){
        r->errstr = hi_alloc(100*sizeof(*r->errstr));
    }

    len = _scnprintf(r->errstr, 100, "Parse command error. Cmd type: %d, state: %d, break position: %d.", 
        r->type, state, (int)(p - r->cmd));
    r->errstr[len] = '\0';
}

struct cmd *command_get()
{
    struct cmd *command;
    command = hi_alloc(sizeof(struct cmd));
    if(command == NULL)
    {
        return NULL;
    }
        
    command->id = ++cmd_id;
    command->result = CMD_PARSE_OK;
    command->errstr = NULL;
    command->type = CMD_UNKNOWN;
    command->cmd = NULL;
    command->clen = 0;
    command->keys = NULL;
    command->narg_start = NULL;
    command->narg_end = NULL;
    command->narg = 0;
    command->quit = 0;
    command->noforward = 0;
    command->slot_num = -1;
    command->frag_seq = NULL;
    command->reply = NULL;
    command->sub_commands = NULL;

    command->keys = hiarray_create(1, sizeof(struct keypos));
    if (command->keys == NULL) 
    {
        hi_free(command);
        return NULL;
    }

    return command;
}

void command_destroy(struct cmd *command)
{
    if(command == NULL)
    {
        return;
    }

    if(command->cmd != NULL)
    {
        free(command->cmd);
    }

    if(command->errstr != NULL){
        hi_free(command->errstr);
    }

    if(command->keys != NULL)
    {
        command->keys->nelem = 0;
        hiarray_destroy(command->keys);
    }

    if(command->frag_seq != NULL)
    {
        hi_free(command->frag_seq);
        command->frag_seq = NULL;
    }

    if(command->reply != NULL)
    {
        freeReplyObject(command->reply);
    }

    if(command->sub_commands != NULL)
    {
        listRelease(command->sub_commands);
    }
    
    hi_free(command);
}


