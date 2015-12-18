/* A simple http server for performance test.
   Copyright (C) 2013 Sun, Junyi <ccnusjy@gmail.com> */

/* https://github.com/fxsjy/httpstub */

#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <errno.h>

#define MAX_EPOLL_FD 4096
#define MAX_BUF_SIZE (1<<20)
#define WORKER_COUNT 2

int ep_fd[WORKER_COUNT],listen_fd;
int g_delay;
int g_shutdown_flag;
int g_quiet;
FILE *g_logger;
int g_pipe[WORKER_COUNT][2];

enum version_t {
        HTTP_1_0 = 10,
        HTTP_1_1 = 11
};
struct io_data_t {
        int fd;
        struct sockaddr_in addr;
        char *in_buf;
        char *out_buf;
        int in_buf_cur;
        int out_buf_cur;
        int out_buf_total;
        int keep_alive;
        enum version_t version;
};

struct slice_t {
        char *begin;
        size_t size;
};

struct thread_data_t{
        struct slice_t data_from_file;
        int myep_fd;
        int mypipe_fd;
};

static void *handle_io_loop(void *param);

static void httpstub_log(const char *fmt, ...);

static void setnonblocking(int fd)
{
        int opts;
        opts = fcntl(fd, F_GETFL);
        if (opts < 0) {
                fprintf(stderr, "fcntl failed\n");
                return;
        }
        opts = opts | O_NONBLOCK;
        if (fcntl(fd, F_SETFL, opts) < 0) {
                fprintf(stderr, "fcntl failed\n");
                return;
        }
        return;
}

static void usage()
{
        printf("usage:  httpstub -p <port> -f <data file> -d <delay (ms)> [-q quiet] \n");
}

static struct slice_t load_data(char *fname)
{
        struct stat buf;
        char *bin = NULL;
        FILE *fptr;
        int ret;
        struct slice_t result;
        ret = stat(fname, &buf);
        if (ret < 0) {
                printf("open %s failed\n", fname);
                perror("");
                exit(1);
        }
        printf(">> size of %s is %d\n", fname, (int)buf.st_size);
        if (buf.st_size <= 0) {
                printf("the file is empty or broken\n");
                exit(1);
        }
        if (buf.st_size <= 0 || buf.st_size > MAX_BUF_SIZE) {
                printf("file is too large\n");
                exit(1);
        }
        bin = (char *)malloc(sizeof(char) * buf.st_size + 1);
        bin[buf.st_size] = '\0';
        result.size = buf.st_size;
        result.begin = bin;
        fptr = fopen(fname, "rb");
        if(fread(bin, buf.st_size, 1, fptr)<=0){
                perror("failed to read file");
                exit(1);
        };
        fclose(fptr);
        return result;
}

static struct io_data_t * alloc_io_data(int client_fd, struct sockaddr_in *client_addr)
{
        struct io_data_t *io_data_ptr = (struct io_data_t *)malloc(sizeof(struct io_data_t));
        io_data_ptr->fd = client_fd;
        io_data_ptr->in_buf = (char *)malloc(4096);
        io_data_ptr->out_buf = (char *)malloc(MAX_BUF_SIZE);
        io_data_ptr->in_buf_cur = 0;
        io_data_ptr->out_buf_cur = 0;
        io_data_ptr->keep_alive = 1;
        if (client_addr)
                io_data_ptr->addr = *client_addr;
        return io_data_ptr;
}

static void destroy_io_data(struct io_data_t *io_data_ptr)
{
        if(NULL == io_data_ptr)return;
        if(io_data_ptr->in_buf)free(io_data_ptr->in_buf);
        if(io_data_ptr->out_buf)free(io_data_ptr->out_buf);
        io_data_ptr->in_buf = NULL;
        io_data_ptr->out_buf = NULL;
        free(io_data_ptr);
}

void exit_hook(int number)
{
        close(listen_fd);
        g_shutdown_flag=1;
        printf(">> [%d]will shutdown...[%d]\n", getpid(),number);
}

int main(int argc, char **argv)
{
        const char *ip_binding = "0.0.0.0";
        int port_listening = 8402;
        char *data_file=NULL;
        int opt;
        int on = 1;

        int client_fd=0;
        int worker_count=WORKER_COUNT,i;
        register int worker_pointer = 0;

        struct sockaddr_in server_addr;
        struct slice_t data_from_file;

        pthread_t tid[WORKER_COUNT];
        pthread_attr_t tattr[WORKER_COUNT];
        struct thread_data_t tdata[WORKER_COUNT];

        char ip_buf[256] = { 0 };
        struct sockaddr_in client_addr;
        socklen_t client_n;


        g_delay = 0;
        g_shutdown_flag = 0;
        if (argc == 1) {
                usage();
                return 1;
        }
        g_quiet = 0;
        while ((opt = getopt(argc, argv, "l:p:f:d:hq")) != -1) {
                switch (opt) {
                        case 'l':
                                ip_binding = strdup(optarg);
                                break;
                        case 'p':
                                port_listening = atoi(optarg);
                                if (port_listening == 0) {
                                        printf(">> invalid port : %s\n", optarg);
                                        exit(1);
                                }
                                break;
                        case 'f':
                                data_file = strdup(optarg);
                                break;
                        case 'd':
                                g_delay = atoi(optarg);
                                break;
                        case 'q':
                                g_quiet = 1;
                                break;
                        case 'h':
                                usage();
                                return 1;
                }

        }
        printf(">> IP listening:%s\n", ip_binding);
        printf(">> port: %d\n", port_listening);
        printf(">> data_file: %s\n", data_file);
        printf(">> reponse delay(MS): %d\n", g_delay);
        printf(">> quite:%d\n",g_quiet);

        if (NULL == data_file || strlen(data_file) == 0) {
                printf("\033[31m-data file is needed!~ \033[0m\n");
                usage();
                return 1;
        }

        g_logger = fopen("stub.log", "a");
        if (g_logger ==NULL) {
                perror("create log file stub.log failed.");
                exit(1);
        }

        data_from_file = load_data(data_file);

        signal(SIGPIPE, SIG_IGN);
        signal(SIGINT, exit_hook);
        signal(SIGKILL, exit_hook);
        signal(SIGQUIT, exit_hook);
        signal(SIGTERM, exit_hook);
        signal(SIGHUP, exit_hook);

        for(i=0;i<WORKER_COUNT;i++){
                if(pipe(g_pipe[i])<0){
                        perror("failed to create pipe");
                        exit(1);
                }
        }

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (-1 == listen_fd) {
                perror("listen faild!");
                exit(-1);
        }

        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, (int[]) {1}, sizeof(int));
        setsockopt(listen_fd, IPPROTO_TCP, TCP_QUICKACK, (int[]) {1}, sizeof(int));

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons((short)port_listening);
        server_addr.sin_addr.s_addr = inet_addr(ip_binding);

        if (-1 == bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
                perror("bind error");
                exit(-1);
        }

        if (-1 == listen(listen_fd, 32)) {
                perror("listen error");
                exit(-1);
        }

        for(i=0;i<worker_count;i++){
                ep_fd[i] = epoll_create(MAX_EPOLL_FD);
                if(ep_fd[i]<0){
                        perror("epoll_create failed.");
                        exit(-1);
                }
        }

        for(i=0;i<worker_count;i++){
                pthread_attr_init(tattr+i);
                pthread_attr_setdetachstate(tattr+i, PTHREAD_CREATE_JOINABLE);
                tdata[i].data_from_file = data_from_file;
                tdata[i].myep_fd = ep_fd[i];
                tdata[i].mypipe_fd = g_pipe[i][0];
                if (pthread_create(tid+i, tattr+i, handle_io_loop, tdata+i ) != 0) {
                        fprintf(stderr, "pthread_create failed\n");
                        return -1;
                }

        }


        while(1){
                if ((client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_n)) > 0) {
                        if(write(g_pipe[worker_pointer][1],(char*)&client_fd,4)<0){
                                perror("failed to write pipe");
                                exit(1);
                        }
                        inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
                        httpstub_log("[CONN]Connection from %s", ip_buf);
                        worker_pointer++;
                        if(worker_pointer == worker_count) worker_pointer=0;
                }
                else if(errno == EBADF && g_shutdown_flag){
                        break;
                }
                else{
                        if(0 == g_shutdown_flag){
                                perror("please check ulimit -n");
                                sleep(1);
                        }
                }
        }

        free(data_from_file.begin);

        for(i=0; i< worker_count; i++){
                close(ep_fd[i]);
        }

        if(client_fd<0 && 0==g_shutdown_flag){
                perror("Accep failed, try ulimit -n");
                httpstub_log("[ERROR]too many fds open, try ulimit -n");
                g_shutdown_flag = 1;
        }
        fclose(g_logger);
        printf(">> [%d]waiting worker thread....\n",getpid());

        for(i=0; i< worker_count; i++)
                pthread_join(tid[i], NULL);

        printf(">> [%d]Bye~\n",getpid());
        return 0;
}

static void destroy_fd(int myep_fd, int client_fd, struct io_data_t *data_ptr, int case_no)
{
        struct epoll_event ev;
        ev.data.ptr = data_ptr;
        epoll_ctl(myep_fd, EPOLL_CTL_DEL, client_fd, &ev);
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
        destroy_io_data(data_ptr);
        httpstub_log("[DEBUG] close case %d",case_no);
}

static void httpstub_log(const char *fmt, ...)
{
        if(0 == g_quiet){
                char msg[4096];
                char buf[64];
                time_t now = time(NULL);
                va_list ap;
                va_start(ap, fmt);
                vsnprintf(msg, sizeof(msg), fmt, ap);
                va_end(ap);
                strftime(buf, sizeof(buf), "%d %b %H:%M:%S", localtime(&now));
                fprintf(g_logger, "[%d] %s %s\n", (int)getpid(), buf, msg);
                fflush(g_logger);
        }
}

static void handle_output(int myep_fd, struct io_data_t *client_io_ptr)
{
        int cfd, ret, case_no;
        struct epoll_event ev;

        cfd = client_io_ptr->fd;
        ret = send(cfd, client_io_ptr->out_buf + client_io_ptr->out_buf_cur, client_io_ptr->out_buf_total - client_io_ptr->out_buf_cur, MSG_NOSIGNAL);
        if (ret >= 0)
                client_io_ptr->out_buf_cur += ret;

        httpstub_log("[DEBUG]out_buf_cur %d", client_io_ptr->out_buf_cur);
        httpstub_log("[DEBUG]out_buf_total %d", client_io_ptr->out_buf_total);

        //printf("ret:%d\n",ret);
        //printf("errno:%d\n", errno);
        if (0 == ret || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                //printf("loose 2\n");
                case_no = 2;
                //perror("send");
                //printf("cfd: %d\n", cfd);
                destroy_fd(myep_fd, cfd, client_io_ptr, case_no);
                return;
        }
        if (client_io_ptr->out_buf_cur == client_io_ptr->out_buf_total) {     //have sent all
                httpstub_log("[NOTICE] all messages have been sent.(%d bytes)", client_io_ptr->out_buf_total);
                //printf("alive: %d\n", client_io_ptr->keep_alive);
                if (client_io_ptr->version == HTTP_1_0 && 0 == client_io_ptr->keep_alive) {
                        case_no = 4;
                        destroy_fd(myep_fd, cfd, client_io_ptr, case_no);
                        return;
                }
                ev.data.ptr = client_io_ptr;
                ev.events = EPOLLIN;
                epoll_ctl(myep_fd, EPOLL_CTL_MOD, cfd, &ev);
        }

}


static void handle_input(int myep_fd, struct io_data_t *client_io_ptr, struct slice_t data_from_file, const char *rsps_msg_fmt, int delay)
{
        int npos = 0;
        int total = 0;
        int ret = 0;
        int case_no = 0;
        char headmsg[256];
        char *sep = NULL;
        const char *CRLF = "\r\n\r\n";
        const char *LF = "\n\n";
        const char *sep_flag=NULL;

        struct epoll_event ev;
        int cfd = client_io_ptr->fd;
        int pkg_len = 0;

        assert(client_io_ptr->in_buf_cur >= 0);
        ret = recv(cfd, client_io_ptr->in_buf + client_io_ptr->in_buf_cur, 512, MSG_DONTWAIT);
        //printf("%u\n",(unsigned int)pthread_self());
        if (0 == ret || (ret < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                case_no = 1;
                //perror("++++++++");
                destroy_fd(myep_fd, cfd, client_io_ptr, case_no);
                return;
        }

        client_io_ptr->in_buf_cur += ret;
        client_io_ptr->in_buf[client_io_ptr->in_buf_cur] = '\0';

        sep = strstr(client_io_ptr->in_buf, CRLF);
        if (NULL == sep) {
                sep = strstr(client_io_ptr->in_buf, LF);
                if (NULL == sep)
                        return;
                else
                        sep_flag = LF;
        } else {
                sep_flag = CRLF;
        }

        if (strstr(client_io_ptr->in_buf, "GET ") == client_io_ptr->in_buf) {
                if (strstr(client_io_ptr->in_buf, "HTTP/1.0") != NULL) {
                        client_io_ptr->version = HTTP_1_0;
                        if (NULL == strstr(client_io_ptr->in_buf, "Connection: Keep-Alive")) {
                                client_io_ptr->keep_alive = 0;
                        }
                } else {
                        client_io_ptr->version = HTTP_1_1;
                }
        }
        npos = strcspn(client_io_ptr->in_buf, "\r\n");
        if (npos > 250)
                npos = 250;
        memcpy(headmsg, client_io_ptr->in_buf, npos);
        headmsg[npos] = '\0';
        httpstub_log("[RECV] %s ", headmsg);

        pkg_len = sep - client_io_ptr->in_buf + strlen(sep_flag);

        assert(pkg_len >= 0);
        assert(client_io_ptr->in_buf_cur - pkg_len >= 0);
        memmove(client_io_ptr->in_buf, sep + strlen(sep_flag), client_io_ptr->in_buf_cur - pkg_len);
        client_io_ptr->in_buf_cur -= pkg_len;

        client_io_ptr->out_buf_cur = 0;
        total = snprintf(client_io_ptr->out_buf, MAX_BUF_SIZE, rsps_msg_fmt, data_from_file.size);
        memcpy(client_io_ptr->out_buf + total, data_from_file.begin, data_from_file.size);
        total += data_from_file.size;
        httpstub_log("[DEBUG]total:%d", total);
        client_io_ptr->out_buf_total = total;

        ev.data.ptr = client_io_ptr;
        ev.events = EPOLLOUT;
        epoll_ctl(myep_fd, EPOLL_CTL_MOD, cfd, &ev);
        if (delay > 0) {
                //printf("usleep: %d\n",(int)(g_delay*2000/nfds) );
                usleep(delay);
        }
}

static void * handle_io_loop(void *param)
{
        register int i;
        int cfd, nfds, case_no, new_sock_fd;
        struct epoll_event events[MAX_EPOLL_FD],ev;

        const char *rsps_msg_fmt = "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nConnection: Keep-Alive\r\nContent-Type: text/plain\r\n\r\n";

        struct io_data_t *client_io_ptr;

        struct thread_data_t my_tdata  = *(struct thread_data_t*)param;

        ev.data.fd = my_tdata.mypipe_fd;
        ev.events = EPOLLIN;
        epoll_ctl(my_tdata.myep_fd,EPOLL_CTL_ADD,my_tdata.mypipe_fd,&ev);

        while (1) {
                nfds = epoll_wait(my_tdata.myep_fd, events, MAX_EPOLL_FD, 1000);
                //printf("nfds:%d, epoll fd:%d\n",nfds,my_tdata.myep_fd);
                if(nfds<=0 && 0!=g_shutdown_flag){
                        break;
                }
                for (i = 0; i < nfds && nfds>0; i++) {
                        if( (events[i].data.fd == my_tdata.mypipe_fd) && (events[i].events & EPOLLIN)){
                                if(read(my_tdata.mypipe_fd,&new_sock_fd,4)==-1){
                                        perror("faild to read pipe");
                                        exit(1);
                                }
				setnonblocking(new_sock_fd);
                                ev.data.ptr = alloc_io_data(new_sock_fd, (struct sockaddr_in *)NULL);
                                ev.events = EPOLLIN;
                                epoll_ctl(my_tdata.myep_fd, EPOLL_CTL_ADD, new_sock_fd, &ev);
                                continue;
                        }
                        client_io_ptr = (struct io_data_t *)events[i].data.ptr;
                        if(client_io_ptr->fd<=0) continue;

                        if (events[i].events & EPOLLIN) {
                                handle_input(my_tdata.myep_fd, client_io_ptr, my_tdata.data_from_file, rsps_msg_fmt, (int)(g_delay * 1000 / nfds));

                        } else if (events[i].events & EPOLLOUT) {
                                handle_output(my_tdata.myep_fd, client_io_ptr);

                        } else if (events[i].events & EPOLLERR) {
                                cfd = client_io_ptr->fd;
                                case_no = 3;
                                destroy_fd(my_tdata.myep_fd, cfd, client_io_ptr, case_no);
                        }
                }
        }
        return NULL;
}
