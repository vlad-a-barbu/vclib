#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <poll.h>
#include <fcntl.h>

#define PORT "4200"
#define BACKLOG 10

#ifndef DEBUG
#define DBG 0
#define NDBG 1
#else
#define DBG 1
#define NDBG 0
#endif

#ifndef SCOPE
#define SCOPE(body, end) \
    do                   \
    {                    \
        body;            \
    _end:                \
        end;             \
    } while (0)
#define RET goto _end
#endif

#define ALLOC(sz) malloc(sz)
#define REALLOC(ptr, sz) realloc(ptr, sz)
#define FREE(ptr) free(ptr)

struct vb_darray
{
    char   *ptr;
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

void
vb_destroy_darray(struct vb_darray *darray)
{
    assert(darray && darray->ptr && "null ptrs");
    FREE(darray->ptr);
    darray->len = 0;
    darray->cap = 0;
}

void
vb_darray_push(struct vb_darray *darray, const char *ptr, size_t len)
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
    memcpy(dst, ptr, len);
    darray->len += len;
}

int 
vb_listen_tcp(const char *port, int backlog, int *result)
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
        return -1;

    int reuse_addr = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) != 0)
        return -1;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
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

int
vb_recv_tcp(int fd, struct vb_darray *darray)
{
    char buff[0x100];
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

int
vb_socket_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

int
main()
{
    int sfd;
    if (vb_listen_tcp(PORT, BACKLOG, &sfd) != 0)
    {
        perror("vb_listen_tcp");
        return 1;
    }
    printf("listening on port %s\n", PORT);

    int fds[BACKLOG + 1];
    memset(fds, 0, sizeof(fds));
    size_t fds_len = 0;
    fds[fds_len++] = sfd;

    do
    {
        struct sockaddr caddr;
        socklen_t caddr_len;
        int cfd = 0;
        struct vb_darray darray;
        darray.cap = 0;

        SCOPE(
        {
            cfd = accept(sfd, &caddr, &caddr_len);
            if (vb_socket_nonblocking(cfd) != 0)
            {
                perror("vb_socket_nonblocking");
                RET;
            }

            if (vb_recv_tcp(cfd, &darray) != 0)
            {
                switch (errno)
                {
                    #if EAGAIN != EWOULDBLOCK
                    case EAGAIN:
                    case EWOULDBLOCK:
                    #else
                    case EWOULDBLOCK:
                    #endif
                    {
                        perror("todo nbio");
                        RET; 
                    }
                    default:
                    {
                        perror("recv");
                        RET;
                    }
                }
            }
            if (vb_send_tcp(cfd, &darray) != 0)
                perror("send");
        },
        {
            close(cfd);
            if (darray.cap > 0) {}
                vb_destroy_darray(&darray);
        });

    } while (NDBG);

    return 0;
}
