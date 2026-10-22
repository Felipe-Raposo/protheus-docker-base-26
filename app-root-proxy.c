/* Encaminha a porta pública.
 *
 * O navegador reaproveita a conexão em que baixou /webapp/ para pedir
 * /app-root depois do login. O MPP não conhece esse caminho. Cada pedido
 * HTTP com tamanho conhecido é encaminhado sozinho; /app-root vai ao
 * HTTPSERVER e o resto ao AppServer. Resposta sem tamanho (canal longo)
 * e protocolo binário seguem inteiros para o AppServer. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define CAP 65536

typedef struct {
    int fd;
    char buf[CAP];
    int n;
} Side;

static int dial(int port) {
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    for (int attempt = 0; attempt < 100; attempt++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return -1;
        if (connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0)
            return fd;
        close(fd);
        usleep(50000);
    }
    return -1;
}

static int write_all(int fd, const char *buf, int n) {
    int off = 0;
    while (off < n) {
        int w = (int)write(fd, buf + off, (size_t)(n - off));
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += w;
    }
    return 0;
}

static int read_some(int fd, char *buf, int cap, int timeout_ms) {
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;
    for (;;) {
        int rc = poll(&pfd, 1, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (rc == 0)
            return 0;
        rc = (int)read(fd, buf, (size_t)cap);
        if (rc < 0 && errno == EINTR)
            continue;
        return rc;
    }
}

static void drop_front(Side *s, int n) {
    if (n >= s->n) {
        s->n = 0;
        return;
    }
    memmove(s->buf, s->buf + n, (size_t)(s->n - n));
    s->n -= n;
}

static int pull(Side *s, int timeout_ms) {
    int r;
    if (s->n >= CAP)
        return -1;
    r = read_some(s->fd, s->buf + s->n, CAP - s->n, timeout_ms);
    if (r > 0)
        s->n += r;
    return r;
}

static void pump(int left, int right) {
    struct pollfd fds[2];
    char buf[16384];

    fds[0].fd = left;
    fds[0].events = POLLIN;
    fds[1].fd = right;
    fds[1].events = POLLIN;

    for (;;) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        for (int i = 0; i < 2; i++) {
            int n;
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;
            n = (int)read(fds[i].fd, buf, sizeof buf);
            if (n <= 0)
                return;
            if (write_all(fds[1 - i].fd, buf, n) < 0)
                return;
        }
    }
}

static int starts_with(const char *buf, int n, const char *lit) {
    size_t len = strlen(lit);
    return n >= (int)len && memcmp(buf, lit, len) == 0;
}

static int is_http(const char *buf, int n) {
    return starts_with(buf, n, "GET ") || starts_with(buf, n, "HEAD ") ||
           starts_with(buf, n, "POST ") || starts_with(buf, n, "PUT ") ||
           starts_with(buf, n, "DELETE ") || starts_with(buf, n, "OPTIONS ") ||
           starts_with(buf, n, "PATCH ");
}

static int path_is_app_root(const char *buf, int n) {
    const char *path;
    const char *end;
    const char *prefix = "/app-root";
    int plen = 9;
    char next;

    path = memchr(buf, ' ', (size_t)n);
    if (path == NULL)
        return 0;
    path++;
    end = memchr(buf, '\n', (size_t)n);
    if (end == NULL || path >= end)
        return 0;
    if (end - path < plen || memcmp(path, prefix, (size_t)plen) != 0)
        return 0;
    next = (path + plen < end) ? path[plen] : ' ';
    return next == ' ' || next == '/' || next == '?' || next == '\r';
}

static int find_header_end(const char *buf, int n) {
    for (int i = 0; i + 3 < n; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return i + 4;
    }
    return -1;
}

static int header_value(const char *headers, int n, const char *name, int *value_len) {
    size_t namelen = strlen(name);
    int i = 0;

    while (i + 1 < n) {
        int line = i;
        int colon;
        int value;
        int end;

        while (i + 1 < n && !(headers[i] == '\r' && headers[i + 1] == '\n'))
            i++;
        end = i;
        i += 2;
        if (end == line)
            break;
        colon = line;
        while (colon < end && headers[colon] != ':')
            colon++;
        if (colon >= end || (size_t)(colon - line) != namelen)
            continue;
        if (strncasecmp(headers + line, name, namelen) != 0)
            continue;
        value = colon + 1;
        while (value < end && (headers[value] == ' ' || headers[value] == '\t'))
            value++;
        *value_len = end - value;
        return value;
    }
    return -1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/* Repassa um corpo chunked e devolve quantos bytes o compõem.
 * -1 se ainda faltam dados no buffer. */
static int chunked_span(const char *buf, int n, int hend) {
    int i = hend;

    for (;;) {
        int line_end = -1;
        int size = 0;
        int j;
        int saw = 0;

        for (j = i; j + 1 < n; j++) {
            if (buf[j] == '\r' && buf[j + 1] == '\n') {
                line_end = j;
                break;
            }
        }
        if (line_end < 0)
            return -1;
        for (j = i; j < line_end; j++) {
            int d;
            if (buf[j] == ';')
                break;
            d = hex_value(buf[j]);
            if (d < 0)
                return -2;
            saw = 1;
            size = (size << 4) + d;
        }
        if (!saw)
            return -2;
        j = line_end + 2;
        if (size == 0) {
            int end = find_header_end(buf + j - 2, n - (j - 2));
            if (end < 0)
                return -1;
            return (j - 2) + end;
        }
        if (n < j + size + 2)
            return -1;
        if (buf[j + size] != '\r' || buf[j + size + 1] != '\n')
            return -2;
        i = j + size + 2;
    }
}

/* 0 = mensagem completa consumida e escrita.
 * 1 = sem tamanho; o que já estava no buffer foi escrito e o resto deve ser spliced.
 * -1 = erro.
 * no_body: HEAD, não há corpo.
 * missing_is_empty: pedido sem Content-Length tem corpo vazio. */
static int forward_message(Side *src, int dst, int no_body, int missing_is_empty) {
    int hend = find_header_end(src->buf, src->n);
    int vlen = 0;
    int vpos;
    int content_length = -1;
    int chunked = 0;
    int total;

    if (hend < 0)
        return -1;
    vpos = header_value(src->buf, hend, "Content-Length", &vlen);
    if (vpos >= 0)
        content_length = atoi(src->buf + vpos);
    vpos = header_value(src->buf, hend, "Transfer-Encoding", &vlen);
    if (vpos >= 0 && vlen >= 7 && strncasecmp(src->buf + vpos, "chunked", 7) == 0)
        chunked = 1;
    if (no_body)
        content_length = 0;
    else if (content_length < 0 && !chunked && missing_is_empty)
        content_length = 0;

    if (chunked && content_length < 0) {
        for (;;) {
            int span = chunked_span(src->buf, src->n, hend);
            if (span == -2)
                return -1;
            if (span >= 0) {
                if (write_all(dst, src->buf, span) < 0)
                    return -1;
                drop_front(src, span);
                return 0;
            }
            if (src->n >= CAP)
                break;
            if (pull(src, 30000) <= 0)
                return -1;
        }
        if (write_all(dst, src->buf, src->n) < 0)
            return -1;
        src->n = 0;
        return 1;
    }

    if (content_length < 0) {
        if (write_all(dst, src->buf, src->n) < 0)
            return -1;
        src->n = 0;
        return 1;
    }

    total = hend + content_length;
    while (src->n < total) {
        int got = src->n;
        int r;
        if (write_all(dst, src->buf, got) < 0)
            return -1;
        src->n = 0;
        total -= got;
        r = read_some(src->fd, src->buf, total > CAP ? CAP : total, 60000);
        if (r <= 0)
            return -1;
        src->n = r;
    }
    if (write_all(dst, src->buf, total) < 0)
        return -1;
    drop_front(src, total);
    return 0;
}

static int fill_headers(Side *s, int detect_binary) {
    for (;;) {
        int hend = find_header_end(s->buf, s->n);
        if (hend >= 0)
            return hend;
        if (detect_binary && s->n > 0 && !is_http(s->buf, s->n) &&
            (memchr(s->buf, '\n', (size_t)s->n) != NULL || s->n >= 8))
            return -2;
        if (pull(s, s->n == 0 ? 600000 : 20000) <= 0)
            return -1;
    }
}

static void splice_rest(Side *client, Side *upstream) {
    if (client->n > 0)
        write_all(upstream->fd, client->buf, client->n);
    if (upstream->n > 0)
        write_all(client->fd, upstream->buf, upstream->n);
    pump(client->fd, upstream->fd);
}

static void handle(int client_fd, int app_port, int static_port) {
    Side client;
    Side app;
    int app_fd = -1;

    memset(&client, 0, sizeof client);
    memset(&app, 0, sizeof app);
    client.fd = client_fd;

    for (;;) {
        int hend = fill_headers(&client, 1);
        int to_static;
        int dst;
        int is_head;
        int rc;
        Side *upstream;
        Side oneshot;

        if (hend == -1)
            break;
        if (hend == -2) {
            if (app_fd < 0)
                app_fd = dial(app_port);
            if (app_fd >= 0) {
                app.fd = app_fd;
                splice_rest(&client, &app);
            }
            break;
        }

        to_static = path_is_app_root(client.buf, client.n);
        is_head = starts_with(client.buf, client.n, "HEAD ");
        if (to_static) {
            memset(&oneshot, 0, sizeof oneshot);
            oneshot.fd = dial(static_port);
            if (oneshot.fd < 0)
                break;
            upstream = &oneshot;
            dst = oneshot.fd;
        } else {
            if (app_fd < 0) {
                app_fd = dial(app_port);
                app.fd = app_fd;
            }
            if (app_fd < 0)
                break;
            upstream = &app;
            dst = app_fd;
        }

        rc = forward_message(&client, dst, is_head, 1);
        if (rc < 0) {
            if (to_static)
                close(oneshot.fd);
            break;
        }
        if (rc == 1) {
            splice_rest(&client, upstream);
            if (to_static)
                close(oneshot.fd);
            break;
        }

        rc = fill_headers(upstream, 0);
        if (rc < 0) {
            if (to_static)
                close(oneshot.fd);
            break;
        }
        rc = forward_message(upstream, client.fd, is_head, 0);
        if (to_static)
            close(oneshot.fd);
        if (rc < 0)
            break;
        if (rc == 1) {
            splice_rest(&client, upstream);
            break;
        }
    }

    if (app_fd >= 0)
        close(app_fd);
    close(client_fd);
}

int main(int argc, char **argv) {
    int listen_port = argc > 1 ? atoi(argv[1]) : 1234;
    int app_port = argc > 2 ? atoi(argv[2]) : 1235;
    int static_port = argc > 3 ? atoi(argv[3]) : 9995;
    int server;
    int one = 1;
    struct sockaddr_in addr;

    signal(SIGCHLD, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
        return 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)listen_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("app-root-proxy");
        return 1;
    }
    if (listen(server, 128) < 0)
        return 1;

    fprintf(stderr, "app-root-proxy escutando %d -> app %d, /app-root %d\n",
            listen_port, app_port, static_port);
    fflush(stderr);

    for (;;) {
        int client = accept(server, NULL, NULL);
        pid_t pid;

        if (client < 0) {
            if (errno == EINTR)
                continue;
            return 1;
        }
        pid = fork();
        if (pid == 0) {
            close(server);
            handle(client, app_port, static_port);
            _exit(0);
        }
        close(client);
    }
}
