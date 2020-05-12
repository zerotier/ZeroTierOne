#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/tcp.h>


#include "hiutil.h"

#ifdef HI_HAVE_BACKTRACE
# include <execinfo.h>
#endif

int
hi_set_blocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
}

int
hi_set_nonblocking(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }

    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

int
hi_set_reuseaddr(int sd)
{
    int reuse;
    socklen_t len;

    reuse = 1;
    len = sizeof(reuse);

    return setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
}

/*
 * Disable Nagle algorithm on TCP socket.
 *
 * This option helps to minimize transmit latency by disabling coalescing
 * of data to fill up a TCP segment inside the kernel. Sockets with this
 * option must use readv() or writev() to do data transfer in bulk and
 * hence avoid the overhead of small packets.
 */
int
hi_set_tcpnodelay(int sd)
{
    int nodelay;
    socklen_t len;

    nodelay = 1;
    len = sizeof(nodelay);

    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

int
hi_set_linger(int sd, int timeout)
{
    struct linger linger;
    socklen_t len;

    linger.l_onoff = 1;
    linger.l_linger = timeout;

    len = sizeof(linger);

    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

int
hi_set_sndbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
}

int
hi_set_rcvbuf(int sd, int size)
{
    socklen_t len;

    len = sizeof(size);

    return setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
}

int
hi_get_soerror(int sd)
{
    int status, err;
    socklen_t len;

    err = 0;
    len = sizeof(err);

    status = getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }

    return status;
}

int
hi_get_sndbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int
hi_get_rcvbuf(int sd)
{
    int status, size;
    socklen_t len;

    size = 0;
    len = sizeof(size);

    status = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }

    return size;
}

int
_hi_atoi(uint8_t *line, size_t n)
{
    int value;

    if (n == 0) {
        return -1;
    }

    for (value = 0; n--; line++) {
        if (*line < '0' || *line > '9') {
            return -1;
        }

        value = value * 10 + (*line - '0');
    }

    if (value < 0) {
        return -1;
    }

    return value;
}

void 
_hi_itoa(uint8_t *s, int num)
{
    uint8_t c;
    uint8_t sign = 0;

    if(s == NULL)
    {
        return;
    }

    uint32_t len, i;
    len = 0;

    if(num < 0)
    {
        sign = 1;
        num = abs(num);
    }
    else if(num == 0)
    {
        s[len++] = '0';
        return;
    }

    while(num % 10 || num /10)
    {
        c = num %10 + '0';
        num = num /10;
        s[len+1] = s[len];
        s[len] = c;
        len ++;
    }

    if(sign == 1)
    {
        s[len++] = '-';
    }

    s[len] = '\0';
    
    for(i = 0; i < len/2; i ++)
    {
        c = s[i];
        s[i] = s[len - i -1];
        s[len - i -1] = c;
    }

}


int
hi_valid_port(int n)
{
    if (n < 1 || n > UINT16_MAX) {
        return 0;
    }

    return 1;
}

int _uint_len(uint32_t num)
{
    int n = 0;

    if(num == 0)
    {
        return 1;
    }

    while(num != 0)
    {
        n ++;
        num /= 10;
    }

    return n;
}

void *
_hi_alloc(size_t size, const char *name, int line)
{
    void *p;

    ASSERT(size != 0);

    p = malloc(size);

    if(name == NULL && line == 1)
    {

    }

    return p;
}

void *
_hi_zalloc(size_t size, const char *name, int line)
{
    void *p;

    p = _hi_alloc(size, name, line);
    if (p != NULL) {
        memset(p, 0, size);
    }

    return p;
}

void *
_hi_calloc(size_t nmemb, size_t size, const char *name, int line)
{
    return _hi_zalloc(nmemb * size, name, line);
}

void *
_hi_realloc(void *ptr, size_t size, const char *name, int line)
{
    void *p;

    ASSERT(size != 0);

    p = realloc(ptr, size);

    if(name == NULL && line == 1)
    {

    }
    
    return p;
}

void
_hi_free(void *ptr, const char *name, int line)
{
    ASSERT(ptr != NULL);

    if(name == NULL && line == 1)
    {

    }

    free(ptr);
}

void
hi_stacktrace(int skip_count)
{
    if(skip_count > 0)
    {

    }

#ifdef HI_HAVE_BACKTRACE
    void *stack[64];
    char **symbols;
    int size, i, j;

    size = backtrace(stack, 64);
    symbols = backtrace_symbols(stack, size);
    if (symbols == NULL) {
        return;
    }

    skip_count++; /* skip the current frame also */

    for (i = skip_count, j = 0; i < size; i++, j++) {
        printf("[%d] %s\n", j, symbols[i]);
    }

    free(symbols);
#endif
}

void
hi_stacktrace_fd(int fd)
{
    if(fd > 0)
    {
        
    }
#ifdef HI_HAVE_BACKTRACE
    void *stack[64];
    int size;

    size = backtrace(stack, 64);
    backtrace_symbols_fd(stack, size, fd);
#endif
}

void
hi_assert(const char *cond, const char *file, int line, int panic)
{
    
    printf("File: %s Line: %d: %s\n", file, line, cond);
    
    if (panic) {
        hi_stacktrace(1);
        abort();
    }
    abort();
}

int
_vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    int n;

    n = vsnprintf(buf, size, fmt, args);

    /*
     * The return value is the number of characters which would be written
     * into buf not including the trailing '\0'. If size is == 0 the
     * function returns 0.
     *
     * On error, the function also returns 0. This is to allow idiom such
     * as len += _vscnprintf(...)
     *
     * See: http://lwn.net/Articles/69419/
     */
    if (n <= 0) {
        return 0;
    }

    if (n < (int) size) {
        return n;
    }

    return (int)(size - 1);
}

int
_scnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    int n;

    va_start(args, fmt);
    n = _vscnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

/*
 * Send n bytes on a blocking descriptor
 */
ssize_t
_hi_sendn(int sd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nsend;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        nsend = send(sd, ptr, nleft, 0);
        if (nsend < 0) {
            if (errno == EINTR) {
                continue;
            }
            return nsend;
        }
        if (nsend == 0) {
            return -1;
        }

        nleft -= (size_t)nsend;
        ptr += nsend;
    }

    return (ssize_t)n;
}

/*
 * Recv n bytes from a blocking descriptor
 */
ssize_t
_hi_recvn(int sd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nrecv;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        nrecv = recv(sd, ptr, nleft, 0);
        if (nrecv < 0) {
            if (errno == EINTR) {
                continue;
            }
            return nrecv;
        }
        if (nrecv == 0) {
            break;
        }

        nleft -= (size_t)nrecv;
        ptr += nrecv;
    }

    return (ssize_t)(n - nleft);
}

/*
 * Return the current time in microseconds since Epoch
 */
int64_t
hi_usec_now(void)
{
    struct timeval now;
    int64_t usec;
    int status;

    status = gettimeofday(&now, NULL);
    if (status < 0) {
        return -1;
    }

    usec = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;

    return usec;
}

/*
 * Return the current time in milliseconds since Epoch
 */
int64_t
hi_msec_now(void)
{
    return hi_usec_now() / 1000LL;
}

void print_string_with_length(char *s, size_t len)
{
    char *token;
    for(token = s; token <= s + len; token ++)
    {
        printf("%c", *token);
    }
    printf("\n");
}

void print_string_with_length_fix_CRLF(char *s, size_t len)
{
    char *token;
    for(token = s; token < s + len; token ++)
    {
        if(*token == CR)
        {
            printf("\\r");
        }
        else if(*token == LF)
        {
            printf("\\n");
        }
        else
        {
            printf("%c", *token);
        }
    }
    printf("\n");
}

