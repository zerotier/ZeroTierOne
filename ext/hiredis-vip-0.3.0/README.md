
# HIREDIS-VIP

Hiredis-vip is a C client library for the [Redis](http://redis.io/) database.

Hiredis-vip supported redis cluster.

Hiredis-vip fully contained and based on [Hiredis](https://github.com/redis/hiredis) .

## CLUSTER SUPPORT

### FEATURES:

* **`SUPPORT REDIS CLUSTER`**:
    * Connect to redis cluster and run commands.

* **`SUPPORT MULTI-KEY COMMAND`**:
    * Support `MSET`, `MGET` and `DEL`.
	
* **`SUPPORT PIPELING`**:
    * Support redis pipeline and can contain multi-key command like above.
	
* **`SUPPORT Asynchronous API`**:
    * User can run commands with asynchronous mode.

### CLUSTER API:

```c
redisClusterContext *redisClusterConnect(const char *addrs, int flags);
redisClusterContext *redisClusterConnectWithTimeout(const char *addrs, const struct timeval tv, int flags);
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

redisClusterAsyncContext *redisClusterAsyncConnect(const char *addrs, int flags);
int redisClusterAsyncSetConnectCallback(redisClusterAsyncContext *acc, redisConnectCallback *fn);
int redisClusterAsyncSetDisconnectCallback(redisClusterAsyncContext *acc, redisDisconnectCallback *fn);
int redisClusterAsyncFormattedCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, char *cmd, int len);
int redisClustervAsyncCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, const char *format, va_list ap);
int redisClusterAsyncCommand(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, const char *format, ...);
int redisClusterAsyncCommandArgv(redisClusterAsyncContext *acc, redisClusterCallbackFn *fn, void *privdata, int argc, const char **argv, const size_t *argvlen);

void redisClusterAsyncDisconnect(redisClusterAsyncContext *acc);
void redisClusterAsyncFree(redisClusterAsyncContext *acc);
```

## Quick usage

If you want used but not read the follow, please reference the examples:
https://github.com/vipshop/hiredis-vip/wiki

## Cluster synchronous API

To consume the synchronous API, there are only a few function calls that need to be introduced:

```c
redisClusterContext *redisClusterConnect(const char *addrs, int flags);
void redisClusterSetMaxRedirect(redisClusterContext *cc, int max_redirect_count);
void *redisClusterCommand(redisClusterContext *cc, const char *format, ...);
void redisClusterFree(redisClusterContext *cc);
```

### Cluster connecting

The function `redisClusterConnect` is used to create a so-called `redisClusterContext`. The
context is where Hiredis-vip Cluster holds state for connections. The `redisClusterContext`
struct has an integer `err` field that is non-zero when the connection is in
an error state. The field `errstr` will contain a string with a description of
the error.
After trying to connect to Redis using `redisClusterContext` you should
check the `err` field to see if establishing the connection was successful:
```c
redisClusterContext *cc = redisClusterConnect("127.0.0.1:6379", HIRCLUSTER_FLAG_NULL);
if (cc != NULL && cc->err) {
    printf("Error: %s\n", cc->errstr);
    // handle error
}
```

### Cluster sending commands

The next that will be introduced is `redisClusterCommand`. 
This function takes a format similar to printf. In the simplest form,
it is used like this:
```c
reply = redisClusterCommand(clustercontext, "SET foo bar");
```

The specifier `%s` interpolates a string in the command, and uses `strlen` to
determine the length of the string:
```c
reply = redisClusterCommand(clustercontext, "SET foo %s", value);
```
Internally, Hiredis-vip splits the command in different arguments and will
convert it to the protocol used to communicate with Redis.
One or more spaces separates arguments, so you can use the specifiers
anywhere in an argument:
```c
reply = redisClusterCommand(clustercontext, "SET key:%s %s", myid, value);
```

### Cluster multi-key commands

Hiredis-vip supports mget/mset/del multi-key commands.
Those multi-key commands is highly effective.
Millions of keys in one mget command just used several seconds.

Example:
```c
reply = redisClusterCommand(clustercontext, "mget %s %s %s %s", key1, key2, key3, key4);
```

### Cluster cleaning up

To disconnect and free the context the following function can be used:
```c
void redisClusterFree(redisClusterContext *cc);
```
This function immediately closes the socket and then frees the allocations done in
creating the context.

### Cluster pipelining

The function `redisClusterGetReply` is exported as part of the Hiredis API and can be used 
when a reply is expected on the socket. To pipeline commands, the only things that needs 
to be done is filling up the output buffer. For this cause, two commands can be used that 
are identical to the `redisClusterCommand` family, apart from not returning a reply:
```c
int redisClusterAppendCommand(redisClusterContext *cc, const char *format, ...);
int redisClusterAppendCommandArgv(redisClusterContext *cc, int argc, const char **argv);
```
After calling either function one or more times, `redisClusterGetReply` can be used to receive the
subsequent replies. The return value for this function is either `REDIS_OK` or `REDIS_ERR`, where
the latter means an error occurred while reading a reply. Just as with the other commands,
the `err` field in the context can be used to find out what the cause of this error is.
```c
void redisClusterReset(redisClusterContext *cc);
```
Warning: You must call `redisClusterReset` function after one pipelining anyway.

The following examples shows a simple cluster pipeline:
```c
redisReply *reply;
redisClusterAppendCommand(clusterContext,"SET foo bar");
redisClusterAppendCommand(clusterContext,"GET foo");
redisClusterGetReply(clusterContext,&reply); // reply for SET
freeReplyObject(reply);
redisClusterGetReply(clusterContext,&reply); // reply for GET
freeReplyObject(reply);
redisClusterReset(clusterContext);
```

## Cluster asynchronous API

Hiredis-vip comes with an cluster asynchronous API that works easily with any event library.
Now we just support and test for libevent and redis ae, if you need for other event libraries,
please contact with us, and we will support it quickly.

### Connecting

The function `redisAsyncConnect` can be used to establish a non-blocking connection to
Redis. It returns a pointer to the newly created `redisAsyncContext` struct. The `err` field
should be checked after creation to see if there were errors creating the connection.
Because the connection that will be created is non-blocking, the kernel is not able to
instantly return if the specified host and port is able to accept a connection.
```c
redisClusterAsyncContext *acc = redisClusterAsyncConnect("127.0.0.1:6379", HIRCLUSTER_FLAG_NULL);
if (acc->err) {
    printf("Error: %s\n", acc->errstr);
    // handle error
}
```

The cluster asynchronous context can hold a disconnect callback function that is called when the
connection is disconnected (either because of an error or per user request). This function should
have the following prototype:
```c
void(const redisAsyncContext *c, int status);
```
On a disconnect, the `status` argument is set to `REDIS_OK` when disconnection was initiated by the
user, or `REDIS_ERR` when the disconnection was caused by an error. When it is `REDIS_ERR`, the `err`
field in the context can be accessed to find out the cause of the error.

You not need to reconnect in the disconnect callback, hiredis-vip will reconnect this connection itself
when commands come to this redis node.

Setting the disconnect callback can only be done once per context. For subsequent calls it will
return `REDIS_ERR`. The function to set the disconnect callback has the following prototype:
```c
int redisClusterAsyncSetDisconnectCallback(redisClusterAsyncContext *acc, redisDisconnectCallback *fn);
```
### Sending commands and their callbacks

In an cluster asynchronous context, commands are automatically pipelined due to the nature of an event loop.
Therefore, unlike the cluster synchronous API, there is only a single way to send commands.
Because commands are sent to Redis cluster asynchronously, issuing a command requires a callback function
that is called when the reply is received. Reply callbacks should have the following prototype:
```c
void(redisClusterAsyncContext *acc, void *reply, void *privdata);
```
The `privdata` argument can be used to curry arbitrary data to the callback from the point where
the command is initially queued for execution.

The functions that can be used to issue commands in an asynchronous context are:
```c
int redisClusterAsyncCommand(
  redisClusterAsyncContext *acc, 
  redisClusterCallbackFn *fn, 
  void *privdata, const char *format, ...);
```
This function work like their blocking counterparts. The return value is `REDIS_OK` when the command
was successfully added to the output buffer and `REDIS_ERR` otherwise. Example: when the connection
is being disconnected per user-request, no new commands may be added to the output buffer and `REDIS_ERR` is
returned on calls to the `redisClusterAsyncCommand` family.

If the reply for a command with a `NULL` callback is read, it is immediately freed. When the callback
for a command is non-`NULL`, the memory is freed immediately following the callback: the reply is only
valid for the duration of the callback.

All pending callbacks are called with a `NULL` reply when the context encountered an error.

### Disconnecting

An cluster asynchronous connection can be terminated using:
```c
void redisClusterAsyncDisconnect(redisClusterAsyncContext *acc);
```
When this function is called, the connection is **not** immediately terminated. Instead, new
commands are no longer accepted and the connection is only terminated when all pending commands
have been written to the socket, their respective replies have been read and their respective
callbacks have been executed. After this, the disconnection callback is executed with the
`REDIS_OK` status and the context object is freed.

### Hooking it up to event library *X*

There are a few hooks that need to be set on the cluster context object after it is created.
See the `adapters/` directory for bindings to *ae* and *libevent*.

## AUTHORS

Hiredis-vip was maintained and used at vipshop(https://github.com/vipshop).
The redis client library part in hiredis-vip is same as hiredis(https://github.com/redis/hiredis).
The redis cluster client library part in hiredis-vip is written by deep(https://github.com/deep011).
Hiredis-vip is released under the BSD license.
