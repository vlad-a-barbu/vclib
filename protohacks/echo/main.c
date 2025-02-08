#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define PORT "4200"

#ifndef DEBUG
#define DBG 0
#define NDBG 1
#else
#define DBG 1
#define NDBG 0
#endif

#define SCOPE(body, end) \
    do                   \
    {                    \
        body;            \
    _end:                \
        end;             \
    } while (0)

#define RET goto _end

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
    assert(darray && darray->ptr && "nulls ptrs");

    if (darray->len + len > darray->cap)
    {
        while (darray->len + len > darray->cap)
            darray->cap *= 2;
        darray->ptr = (char *)REALLOC(darray->ptr, darray->cap);
    }

    char *dst = darray->ptr + darray->len;
    for (size_t i = 0; i < len; ++i)
        *dst++ = *ptr++;

    darray->len += len;
}

int 
vb_listen_tcp(int *result)
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
    if (getaddrinfo("127.0.0.1", PORT, &hints, &addr) != 0)
        return -1;

    if (bind(sfd, addr->ai_addr, addr->ai_addrlen) != 0)
        return -1;
    freeaddrinfo(addr);

    if (listen(sfd, 10) != 0)
        return -1;
    printf("listening on port %s\n", PORT);

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
main()
{
    int sfd;
    if (vb_listen_tcp(&sfd) != 0)
    {
        perror("vb_listen_tcp");
        return 1;
    }

    do
    {
        struct sockaddr caddr;
        socklen_t caddr_len;
        int cfd = accept(sfd, &caddr, &caddr_len);
        struct vb_darray darray = vb_create_darray(0x100);

        SCOPE(
        {
            if (vb_recv_tcp(cfd, &darray) != 0)
            {
                perror("recv");
                RET;
            }
            if (vb_send_tcp(cfd, &darray) != 0)
            {
                perror("send");
            }
        },
        {
            vb_destroy_darray(&darray);
            close(cfd);
        });

    } while (NDBG);

    return 0;
}
