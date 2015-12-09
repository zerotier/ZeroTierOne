/* httpd.c - multi-client httpd, with cgi and dirindex support, in <500 LOC.
 * Run as: httpd [-p port] <root>
 * u+x or g+x files are considered cgi programs.
 */

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define LINEBUFMAX 4096
#define REQBUFMAX 4096
#define FILEBUFMAX 4096

static const char *docroot;
static int printreqs = 0;

struct reactor {
	int epfd;
};

struct socket {
	int fd;
	struct sockaddr_in sa;
	struct reactor *r;
	void (*read)(struct socket *);
	void (*write)(struct socket *);
	void (*close)(struct socket *);
	void *priv;
};

struct client {
	struct socket *s;
	void (*line)(struct client *, char *);
	void (*writedone)(struct client *);

	char *rbuf;
	size_t rbufsize;
	size_t rbuffill;
	char *wbuf;
	size_t wbufsize;
	size_t wbuffill;

	char *reqmethod;
	char *requrl;

	int fillfd;
};

static void udie(const char *prefix) {
	perror(prefix);
	abort();
}

static void *xmalloc(size_t sz) {
	void *p = malloc(sz);
	if (!p)
		abort();
	memset(p, 0, sz);
	return p;
}

static char *xstrdup(const char *s) {
	char *n = strdup(s);
	if (!n)
		abort();
	return n;
}

static void strlcpy(char *dest, const char *src, size_t n) {
	strncpy(dest, src, n - 1);
	dest[n - 1] = '\0';
}

static void strlcat(char *dest, const char *src, size_t n) {
	strncat(dest, src, n - 1);
	dest[n - 1] = '\0';
}

static struct reactor *reactor_new(void) {
	struct reactor *r = xmalloc(sizeof *r);
	r->epfd = epoll_create1(0);
	if (r->epfd < 0)
		udie("epoll_create1()");
	return r;
}

static struct socket *reactor_add(struct reactor *r, int fd) {
	struct socket *s = xmalloc(sizeof *s);
	struct epoll_event evt;

	s->fd = fd;
	s->r = r;
	evt.events = 0;
	evt.data.ptr = s;
	if (epoll_ctl(r->epfd, EPOLL_CTL_ADD, fd, &evt) < 0)
		udie("epoll_ctl()");
	return s;
};

static void reactor_refresh(struct reactor *r, struct socket *s) {
	struct epoll_event evt;
	evt.events = 0;
	evt.data.ptr = s;
	if (s->read)
		evt.events |= EPOLLIN;
	if (s->write)
		evt.events |= EPOLLOUT;
	if (s->close)
		evt.events |= EPOLLRDHUP;
	if (epoll_ctl(r->epfd, EPOLL_CTL_MOD, s->fd, &evt) < 0)
		udie("epoll_ctl()");
}

static void reactor_del(struct reactor *r, struct socket *s) {
	if (epoll_ctl(r->epfd, EPOLL_CTL_DEL, s->fd, NULL) < 0)
		udie("epoll_ctl()");
	free(s);
}

static void reactor_run(struct reactor *r) {
	struct epoll_event evts[16];
	int n;
	int i;
	struct socket *s;

	n = epoll_wait(r->epfd, evts, sizeof(evts) / sizeof(evts[0]), -1);
	if (n < 0)
		udie("epoll_wait()");
	for (i = 0; i < n; i++) {
		s = evts[i].data.ptr;
		if (evts[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
			if (s->close)
				s->close(s);
			reactor_del(r, s);
		} else if ((evts[i].events & EPOLLIN) && s->read) {
			s->read(s);
		} else if ((evts[i].events & EPOLLOUT) && s->write) {
			s->write(s);
		}
	}
}

static void reqline(struct client *, char *);

static struct client *client_new(struct socket *s) {
	struct client *c = xmalloc(sizeof *c);
	c->s = s;
	c->rbuf = xmalloc(REQBUFMAX);
	c->rbufsize = REQBUFMAX;
	c->rbuffill = 0;
	c->line = reqline;
	c->wbufsize = 0;
	c->wbuffill = 0;
	c->writedone = NULL;
	return c;
}

static void client_read(struct socket *s) {
	struct client *c = s->priv;
	char *p;
	ssize_t len;

	len = read(s->fd, c->rbuf + c->rbuffill, c->rbufsize - c->rbuffill);
	if (len < 0)
		udie("read()");
	c->rbuffill += len;
	while ((p = strstr(c->rbuf, "\n"))) {
		*p = '\0';
		if (p > c->rbuf && p[-1] == '\r')
			p[-1] = '\0';
		p++;
		c->line(c, c->rbuf);
		memmove(c->rbuf, p, c->rbufsize - (p - c->rbuf));
		c->rbuffill -= (p - c->rbuf);
		memset(c->rbuf + c->rbuffill, 0, c->rbufsize - c->rbuffill);
	}
}

static void client_write(struct socket *s) {
	struct client *c = s->priv;
	ssize_t len;

	len = write(s->fd, c->wbuf, c->wbuffill);
	if (len < 0)
		udie("write()");
	if ((size_t)len < c->wbuffill)
		memmove(c->wbuf, c->wbuf + len, c->wbuffill - len);
	c->wbuffill -= len;
	if (c->wbuffill)
		return;
	free(c->wbuf);
	c->wbuf = NULL;
	c->wbufsize = 0;
	s->write = NULL;
	c->writedone(c);
}

static void client_writeb(struct client *c, const char *buf, size_t len) {
	if (!c->wbufsize || c->wbufsize - c->wbuffill < len) {
		size_t growby = len - (c->wbufsize - c->wbuffill);
		c->wbuf = realloc(c->wbuf, c->wbufsize + growby);
		c->wbufsize += growby;
	}
	memcpy(c->wbuf + c->wbuffill, buf, len);
	c->wbuffill += len;
	if (!c->s->write) {
		c->s->write = client_write;
		reactor_refresh(c->s->r, c->s);
	}
}

static void client_writeln(struct client *c, const char *fmt, ...) {
	char buf[LINEBUFMAX];
	va_list ap;
	char *p;

	va_start(ap, fmt);
	vsnprintf(buf, LINEBUFMAX, fmt, ap);
	va_end(ap);

	p = buf + strlen(buf);
	if (p > buf + LINEBUFMAX - 2)
		p = buf + LINEBUFMAX - 2;
	*p++ = '\r';
	*p++ = '\n';
	client_writeb(c, buf, p - buf);
}

static void client_writedone(struct client *c) {
	close(c->s->fd);
}

static void client_refillbuf(struct client *c) {
	char buf[FILEBUFMAX];
	ssize_t len;

	len = read(c->fillfd, buf, sizeof(buf));
	if (len < 0)
		udie("read()");
	if (len == 0) {
		c->writedone = client_writedone;
		close(c->fillfd);
	} else {
		c->writedone = client_refillbuf;
	}
	client_writeb(c, buf, len);
}

static void client_close(struct socket *s) {
	struct client *c = s->priv;
	free(c->reqmethod);
	free(c->requrl);
	free(c->rbuf);
	free(c->wbuf);
	free(c);
	/* ... */
}

static void listener_read(struct socket *s) {
	struct sockaddr_in sa;
	socklen_t salen = sizeof(sa);
	int nfd = accept(s->fd, (struct sockaddr *)&sa, &salen);
	struct socket *n;
	if (nfd == -1)
		udie("accept()");
	if (fcntl(nfd, F_SETFD, FD_CLOEXEC) < 0)
		udie("fcntl()");
	n = reactor_add(s->r, nfd);
	memcpy(&n->sa, &sa, sizeof(n->sa));
	n->read = client_read;
	n->close = client_close;
	reactor_refresh(s->r, n);
	n->priv = client_new(n);
}

static void error(struct client *c, int code) {
	client_writeln(c, "HTTP/1.1 %u Error", code);
	client_writeln(c, "");
	c->writedone = client_writedone;
}

static void iptobuf(struct client *c, char *buf) {
	unsigned int ip = ntohl(c->s->sa.sin_addr.s_addr);
	sprintf(buf, "%u.%u.%u.%u", (ip >> 24) & 0xFF,
	        (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}

static void runcgi(struct client *c, const char *prog, const char *args) {
	char buf[] = "REMOTE_ADDR=255.255.255.255";
	iptobuf(c, buf + strlen("REMOTE_ADDR="));
	putenv(buf);
	dup2(c->s->fd, 0);
	dup2(c->s->fd, 1);
	dup2(c->s->fd, 2);
	execl(prog, prog, args, NULL);
}

static void cgi(struct client *c, const char *prog, const char *args) {
	int p;
	p = fork();
	if (!p)
		runcgi(c, prog, args);
	else if (p < 0)
		error(c, 500);
	else
		c->writedone = client_writedone;
}

static void genindex(struct client *c, const char *url) {
	DIR *d = fdopendir(c->fillfd);
	struct dirent *e;

	client_writeln(c, "Content-Type: text/html");
	client_writeln(c, "");

	client_writeln(c, "<html>");
	client_writeln(c, "  <head>");
	client_writeln(c, "    <title>Index of %s</title>", url);
	client_writeln(c, "  </head>");
	client_writeln(c, "  <body>");
	client_writeln(c, "    <h1>Index of %s</h1>", url);
	client_writeln(c, "    <ul>");
	while ((e = readdir(d))) {
		client_writeln(c, "      <li>");
		client_writeln(c, "        <a href=\"%s%s%s\">%s</a>", url,
		               url[strlen(url) - 1] == '/' ? "" : "/", e->d_name,
		               e->d_name);
		client_writeln(c, "      </li>");
	}
	client_writeln(c, "    </ul>");
	client_writeln(c, "  </body>");
	client_writeln(c, "</html>");
	closedir(d);
	c->writedone = client_writedone;
}

static void get(struct client *c, char *url) {
	char rp[PATH_MAX];
	char *rpcanon;
	char *rest;
	struct stat st;

	strlcpy(rp, docroot, sizeof(rp));
	if ((rest = strchr(url, '?')))
		*rest++ = '\0';
	strlcat(rp, url, sizeof(rp));
	rpcanon = realpath(rp, NULL);
	if (!rpcanon) {
		error(c, 404);
		return;
	}

	if (strstr(rpcanon, docroot) != rpcanon) {
		error(c, 403);
		free(rpcanon);
		return;
	}

	c->fillfd = open(rpcanon, O_RDONLY);
	if (c->fillfd == -1) {
		free(rpcanon);
		error(c, 403);	/* XXX: not all open() failures are 403s */
		return;
	}

	if (fstat(c->fillfd, &st) == -1)
		udie("fstat()");

	client_writeln(c, "HTTP/1.1 200 OK");

	if (S_ISDIR(st.st_mode)) {
		genindex(c, url);
	} else if (st.st_mode & (S_IXUSR | S_IXGRP)) {
		cgi(c, rpcanon, rest);
	} else {
		client_writeln(c, "");
		client_refillbuf(c);
	}
	free(rpcanon);
}

static void reqdone(struct client *c) {
	if (printreqs) {
		char buf[32];
		iptobuf(c, buf);
		printf("%s %s %s\n", buf, c->reqmethod, c->requrl);
	}
	if (!strcasecmp(c->reqmethod, "GET"))
		get(c, c->requrl);
	else
		error(c, 405);
}

static void reqhdr(struct client *c, char *line) {

	if (!strlen(line)) {
		reqdone(c);
		return;
	}

	/* XXX */
}

static void reqline(struct client *c, char *line) {
	char *method, *url, *version;

	method = strtok(line, " ");
	url = strtok(NULL, " ");
	version = strtok(NULL, " ");

	if (!method || !url) {
		error(c, 400);
		return;
	}

	c->reqmethod = xstrdup(method);
	c->requrl = xstrdup(url);
	c->line = reqhdr;
}

static int serve(int port) {
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in sa;
	if (sfd == -1)
		udie("socket()");
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if (bind(sfd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
		udie("bind()");
	if (listen(sfd, 20) < 0)
		udie("listen()");
	if (fcntl(sfd, F_SETFD, FD_CLOEXEC) < 0)
		udie("fcntl()");
	return sfd;
}

static void usage(const char *progn) {
	printf("Usage: %s [-p port] [-v] <root>\n", progn);
}

int main(int argc, char *argv[]) {
	struct reactor *r = reactor_new();
	struct socket *listener;
	int opt;
	int port = 80;
	
	while ((opt = getopt(argc, argv, "p:v")) != -1) {
		switch (opt) {
			case 'p':
				port = atoi(optarg);
				break;
			case 'v':
				printreqs = 1;
				break;
			default:
				usage(argv[0]);
				exit(1);
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		exit(1);
	}

	docroot = argv[optind];

	listener = reactor_add(r, serve(port));
	listener->read = listener_read;
	reactor_refresh(r, listener);

	signal(SIGCHLD, SIG_IGN);

	while (1) {
		reactor_run(r);
	}
}
