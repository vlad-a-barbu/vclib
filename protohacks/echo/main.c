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
    assert(darray && darray->ptr && "null ptrs");
    FREE(darray->ptr);
    darray->len = 0;
    darray->cap = 0;
}

void vb_darray_push(struct vb_darray *darray, const char *ptr, size_t len)
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

// todo: err handling
int listen_tcp()
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    char reuse_addr = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addr;
    getaddrinfo("127.0.0.1", PORT, &hints, &addr);
    if (bind(sfd, addr->ai_addr, addr->ai_addrlen) != 0)
        perror("bind");
    freeaddrinfo(addr);
    listen(sfd, 10);
    printf("listening on port %s\n", PORT);
}

int main()
{
    int sfd = listen_tcp();

    do
    {
        struct sockaddr caddr;
        socklen_t caddr_len;
        int cfd = accept(sfd, &caddr, &caddr_len);

        char buff[0x100];
        struct vb_darray darray = vb_create_darray(0x100);
        char *send_ptr;
        size_t send_len;
        char err = 0;

        // recv
        while (1)
        {
            ssize_t n = recv(cfd, buff, sizeof(buff), 0);
            if (n <= 0)
            {
                if (n == -1)
                {
                    err = 1;
                }
                break;
            }
            vb_darray_push(&darray, buff, n);
        }
        if (err)
        {
            perror("recv");
            goto end;
        }

        // send
        send_ptr = darray.ptr;
        send_len = darray.len;
        while (1)
        {
            ssize_t n = send(cfd, send_ptr, send_len, 0);
            if (n <= 0)
            {
                if (n == -1)
                {
                    err = 1;
                }
                break;
            }
            send_ptr += n;
            send_len -= n;
        }
        if (err == 1)
        {
            perror("send");
            goto end;
        }
        assert(send_len == 0 && "sent != recvd");

    end:
        vb_destroy_darray(&darray);
        close(cfd);

    } while (NDBG);

    return 0;
}
