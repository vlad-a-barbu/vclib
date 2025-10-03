#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char *buffer;
	size_t buffer_size;
	size_t current_offset;
	size_t previous_offset;
} Arena;

Arena arena_init(char *buffer, size_t buffer_size) {
	Arena arena;

	memset(&arena, 0, sizeof(Arena));
	memset(buffer, 0, buffer_size);

	arena.buffer = buffer;
	arena.buffer_size = buffer_size;

	return arena;
}

static uintptr_t align(uintptr_t addr, size_t alignment) {
	uintptr_t aligned_addr;

	aligned_addr = (addr + (uintptr_t)alignment - 1) & ~((uintptr_t)alignment - 1);

	return aligned_addr;
}

static int is_power_of_two(size_t x) {
	return (x & (x-1)) == 0;
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment) {
	uintptr_t aligned_addr;
	size_t aligned_offset;
	void *ptr;

	assert(is_power_of_two(alignment));

	aligned_addr = (uintptr_t)arena->buffer + (uintptr_t)arena->current_offset;
	aligned_addr = align(aligned_addr, alignment);

	aligned_offset = aligned_addr - (uintptr_t)arena->buffer;
	
	if (aligned_offset + size > arena->buffer_size) {
		return NULL;
	}

	ptr = &arena->buffer[aligned_offset];
	memset(ptr, 0, size);

	arena->previous_offset = aligned_offset;
	arena->current_offset = aligned_offset + size;

	return ptr;
}

#define ARENA_DEFAULT_ALIGNMENT sizeof(void *)

#define arena_alloc(arena, size) arena_alloc_aligned(arena, size, ARENA_DEFAULT_ALIGNMENT)

void *arena_resize_last(Arena *arena, size_t size) {
	size_t last_allocation_size;
	void *ptr;

	assert(arena->current_offset > arena->previous_offset);
	last_allocation_size = arena->current_offset - arena->previous_offset;

	ptr = &arena->buffer[arena->previous_offset];

	if (last_allocation_size == size) {
		return ptr;
	}

	if (arena->previous_offset + size > arena->buffer_size) {
		return NULL;
	}

	if (size > last_allocation_size) {
		memset((void *)((uintptr_t)ptr + last_allocation_size), 0, size - last_allocation_size);
	} else {
		memset((void *)((uintptr_t)ptr + size), 0, last_allocation_size - size);
	}

	arena->current_offset = arena->previous_offset + size;

	return ptr;
}

typedef struct {
	Arena *arena;
	size_t current_offset;
	size_t previous_offset;
} ArenaSave;

ArenaSave arena_save(Arena *arena) {
	ArenaSave save;

	save.arena = arena;
	save.current_offset = arena->current_offset;
	save.previous_offset = arena->previous_offset;

	return save;
}

void arena_restore(Arena *arena, ArenaSave save) {
	arena->current_offset = save.current_offset;
	arena->previous_offset = save.previous_offset;
}

typedef struct {
	char *ptr;
	size_t len;
} String8;
 
String8 read_entire_file(const char *path, Arena *arena) {
	String8 content;
	FILE *file;
	size_t file_size, read_size;
	char *read_buffer;
	ArenaSave save;

	memset(&content, 0, sizeof(String8));

	if ((file = fopen(path, "rb")) == NULL) {
		return content;
	}

	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	save = arena_save(arena);
	if ((read_buffer = arena_alloc(arena, file_size)) == NULL) {
		fclose(file);
		return content;
	}

	read_size = fread(read_buffer, sizeof(char), file_size, file);
	fclose(file);

	if (read_size < file_size) {
		arena_restore(arena, save);
		return content;
	}

	content.ptr = read_buffer;
	content.len = file_size;

	return content;
}

#define BUFF_SZ (1 * 1024 * 1024)
static char BUFF[BUFF_SZ];

#define PROG_NAME "dq"

int main(int argc, char **argv) {
	Arena arena;
	String8 file_content;

	arena = arena_init(BUFF, BUFF_SZ);

	if (argc < 2) {
		printf("usage: %s [file]\n", PROG_NAME);
		return 1;
	}

	file_content = read_entire_file(argv[1], &arena);
	if (!file_content.ptr) {
		printf("error: read_entire_file | %s\n", argv[1]);
	}

	fwrite(file_content.ptr, sizeof(char), file_content.len, stdout);
	
	return 0;
}
