#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define BUFF_LEN 500000

int
listen_tcp(int port, int backlog, int *sock);

void *
handler(void *arg);

int
main()
{
	int sock;
	if (listen_tcp(3000, 128, &sock) != 0)
		return 1;

	while (1)
	{
		struct sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);

		int client_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);
		printf("client connected at %d\n", client_sock);

		pthread_t thread_id;
		pthread_create(&thread_id, NULL, handler, (void *)&client_sock);
	}

	return 0;
}

void *
handler(void *arg)
{
	int client_sock = *(int *)arg;
	char buff[BUFF_LEN];

	ssize_t n = recv(client_sock, (void *)buff, BUFF_LEN, 0);
	printf("[%d] %s\n", client_sock, buff);

	send(client_sock, (void *)buff, n, 0);

	close(client_sock);

	return NULL;
}

int
listen_tcp(int port, int backlog, int *sock_out)
{
	int sock = socket(PF_INET, SOCK_STREAM, 0);

	int reuse_addr = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse_addr, sizeof(reuse_addr));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
	{
		perror("bind");
		return 1;
	}

	if (listen(sock, backlog) != 0)
	{
		perror("listen");
		return 1;
	}

	*sock_out = sock;
	printf("listening on port %d - backlog %d\n", port, backlog);

	return 0;
}
