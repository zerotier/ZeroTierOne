/* Functions to help coverity do static analysis on rabbitmq-c */

typedef struct {
} amqp_rpc_reply_t;

/* librabbitmq/amqp_private.h */
void amqp_abort(const char* fmt, ...) { __coverity_panic__(); }

/* tools/common.h */
void die(const char* fmt, ...) { __coverity_panic__(); }
void die_errno(int err, const char* fmt, ...) { __coverity_panic__(); }
void die_amqp_error(int err, const char* fmt, ...) { __coverity_panic__(); }
void die_rpc(amqp_rpc_reply_t r, const char* fmt, ...) { __coverity_panic__(); }

/* examples/utils.h */
void die_on_amqp_error(amqp_rpc_reply_t* r) { __coverity_panic__(); }
void die_on_error(int r) { __coverity_panic__(); }
