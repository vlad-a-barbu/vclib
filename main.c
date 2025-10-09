#include "vb.h"

#define BUFF_SZ 9 * 1024 * 1024
static char buff[BUFF_SZ];

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("usage: %s [file]\n", argv[0]);
		return 1;
	}
	Arena arena = arena_init(buff, BUFF_SZ);
	format_tabs_over_spaces(argv[1], argv[1], 0x100, 4, &arena);
	return 0;
}

