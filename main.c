#define LOG_MSG_BUFFER_SIZE 0x1000
#define LOG_BUFFER_SIZE 0x1000
#include "vb.h"

#define BUFF_SZ 3 * 1024 * 1024
static char buff[BUFF_SZ];

int main(int argc, char *argv[]) {
	Arena arena;
	const char *content;

	logger_init(stdout);
	arena = arena_init(buff, BUFF_SZ);

	if (argc < 2) {
		printf("usage: %s [file]\n", argv[0]);
		return 1;
	}

	content = read_entire_file_cstring(argv[1], &arena, NULL);

	if (!content) {
		log_error("read_entire_file");
		return 1;
	} else {
		log_infof("file: %s", content);
	}

	return 0;
}

