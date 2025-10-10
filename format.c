#include "vb.h"

#define BUFF_SZ 10 * 1024 * 1024
static char buff[BUFF_SZ];

int main(int argc, char *argv[]) {
	if (argc < 3) {
		printf("usage: %s [file] [tabwidth]\n", argv[0]);
		return 1;
	}
	Arena arena = arena_init(buff, BUFF_SZ);
	int tabwidth = atoi(argv[2]);
	format_tabs_over_spaces(argv[1], argv[1], 0x100, tabwidth, &arena);
	return 0;
}

