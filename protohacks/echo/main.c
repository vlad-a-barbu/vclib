#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

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
create_darray(size_t cap)
{
    char *ptr = ALLOC(cap);
    assert(ptr && "allocation failed");
    struct vb_darray res = {ptr, 0, cap};
    return res;
}

void destroy_darray(struct vb_darray *darray)
{
    assert(darray && darray->ptr && "null ptrs");
    FREE(darray->ptr);
    darray->len = 0;
    darray->cap = 0;
}

void darray_push(struct vb_darray *darray, const char *ptr, size_t len)
{
    assert(darray && darray->ptr && "nulls ptrs");

    if (darray->len + len > darray->cap)
    {
        size_t new_cap;
        if (len > darray->cap)
            new_cap = len * 2;
        else
            new_cap = darray->cap * 2;

        darray->ptr = REALLOC(darray->ptr, new_cap);
        darray->cap = new_cap;
    }

    char *dst = darray->ptr + darray->len;
    for (size_t i = 0; i < len; ++i)
        *(dst + i) = *(ptr + i);

    darray->len += len;
}

int main()
{
    int sfd = socket(PF_INET, SOCK_STREAM, 0);
    char reuse_addr = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *addr;
    getaddrinfo("127.0.0.1", "4200", &hints, &addr);
    if (bind(sfd, addr->ai_addr, addr->ai_addrlen) != 0)
        perror("bind");
    listen(sfd, 10);
    printf("listening on port 4200\n");

    while (1)
    {
        struct sockaddr caddr;
        socklen_t caddr_len;
        int cfd = accept(sfd, &caddr, &caddr_len);

        // recv
        char buff[0x100];
        struct vb_darray darray = create_darray(0x100);
        char err = 0;
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
            darray_push(&darray, buff, n);
        }
        if (err)
        {
            perror("recv");
            goto end;
        }

        // send
        char *send_ptr = darray.ptr;
        size_t send_len = darray.len;
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
            send_len -= n;
        }
        if (err == 1)
        {
            perror("send");
            goto end;
        }
        assert(send_len == 0 && "sent != recvd");

    end:
        destroy_darray(&darray);
        close(cfd);
    }

    return 0;
}
