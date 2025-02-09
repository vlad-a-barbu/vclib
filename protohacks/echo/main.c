#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <fcntl.h>

#define PORT "4200"
#define BACKLOG 128

#ifndef DEBUG
#define DBG 0
#define NDBG 1
#else
#define DBG 1
#define NDBG 0
#endif

#define ALLOC(sz) malloc(sz)
#define REALLOC(ptr, sz) realloc(ptr, sz)
#define FREE(ptr) free(ptr)

struct vb_darray
{
    char *ptr;
    size_t len;
    size_t cap;
};

struct vb_darray
vb_create_darray(size_t cap)
{
    assert(cap > 0 && "invalid initial cap");
    char *ptr = (char *)ALLOC(cap);
    assert(ptr && "allocation failed");
    struct vb_darray res = {ptr, 0, cap};
    return res;
}

void vb_destroy_darray(struct vb_darray *darray)
{
    if (darray == NULL || darray->cap == 0)
        return;
    FREE(darray->ptr);
    darray->len = 0;
    darray->cap = 0;
}

void vb_reset_darray(struct vb_darray *darray)
{
    if (darray == NULL || darray->cap == 0)
        return;
    darray->len = 0;
}

void vb_darray_push(struct vb_darray *darray, const char *ptr, size_t len)
{
    if (darray == NULL || darray->cap == 0)
        *darray = vb_create_darray(len * 2);

    if (darray->len + len > darray->cap)
    {
        while (darray->len + len > darray->cap)
            darray->cap *= 2;
        darray->ptr = (char *)REALLOC(darray->ptr, darray->cap);
    }

    char *dst = darray->ptr + darray->len;
    for (size_t i = 0; i < len; ++i)
        dst[i] = ptr[i];

    darray->len += len;
}

int vb_listen_tcp(const char *port, int backlog, int *result)
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        return -1;

    int reuse_addr = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) != 0)
        return -1;

    struct addrinfo hints = {0};
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addr;
    if (getaddrinfo("127.0.0.1", port, &hints, &addr) != 0)
        return -1;

    if (bind(sfd, addr->ai_addr, addr->ai_addrlen) != 0)
        return -1;
    freeaddrinfo(addr);

    if (listen(sfd, backlog) != 0)
        return -1;

    *result = sfd;
    return 0;
}

int vb_recv_tcp(int fd, struct vb_darray *darray)
{
    char buff[0x100] = {0};
    while (1)
    {
        ssize_t n = recv(fd, buff, sizeof(buff), 0);
        if (n <= 0)
            return n;
        vb_darray_push(darray, buff, n);
    }
}

size_t
vb_send_tcp(int fd, struct vb_darray *darray)
{
    char *send_ptr = darray->ptr;
    size_t send_len = darray->len;
    while (1)
    {
        ssize_t n = send(fd, send_ptr, send_len, 0);
        if (n <= 0)
            return send_len;
        send_ptr += n;
        send_len -= n;
    }
}

int vb_echo_tcp(int fd)
{
    char buff[0x100] = {0};
    ssize_t n = recv(fd, buff, sizeof(buff), 0);
    if (n > 0)
        send(fd, buff, n, 0);
    return n;
}

int vb_socket_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

void vb_remove(int tgt, int *src, size_t *src_len)
{
    size_t len = *src_len;
    for (size_t i = 0; i < len; ++i)
        if (src[i] == tgt)
        {
            for (size_t j = i; j < len - 1; ++j)
                src[j] = src[j + 1];
            *src_len = len - 1;
            break;
        }
}

int main()
{
    int sfd;

    if (vb_listen_tcp(PORT, BACKLOG, &sfd) != 0)
    {
        perror("vb_listen_tcp");
        return 1;
    }

    if (vb_socket_nonblocking(sfd) != 0)
    {
        perror("vb_socket_nonblocking");
        return 1;
    }

    printf("listening on port %s\n", PORT);

    int cfds[BACKLOG] = {0};
    size_t cfds_len = 0;

    do
    {
        struct sockaddr caddr;
        socklen_t caddr_len;
        int cfd = 0;

        cfd = accept(sfd, &caddr, &caddr_len);
        if (cfd == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                for (size_t i = 0; i < cfds_len; ++i)
                {
                    cfd = cfds[i];

                    int n = vb_echo_tcp(cfd);
                    if (n < 0)
                    {
                        if (errno == EWOULDBLOCK)
                            continue;

                        perror("recv");

                        close(cfd);
                        vb_remove(cfd, cfds, &cfds_len);
                        break;
                    }
                    else
                    {
                        if (n == 0)
                        {
                            close(cfd);
                            vb_remove(cfd, cfds, &cfds_len);
                        }
                        break;
                    }
                }
            }
            else
            {
                perror("accept");
                continue;
            }
        }
        else
        {
            if (vb_socket_nonblocking(cfd) != 0)
            {
                perror("vb_socket_nonblocking");
                close(cfd);
            }
            cfds[cfds_len++] = cfd;
        }

    } while (NDBG);

    return 0;
}
