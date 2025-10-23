#ifndef V_MALLOC_H
#define V_MALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct {
	void *ptr;
	size_t size;
	const char *file;
	int line;
} Allocation;

static struct {
	Allocation *ptr;
	size_t size;
	size_t cap;
} allocations;

#define INITIAL_ALLOCATIONS_CAP 100

static char DEAD_BEEF[4] = { 0xDE, 0xAD, 0xBE, 0xEF };

void *malloc_deadbeef(size_t size) {
	void *ptr;
	size_t deadbeef_size = size + 4;
	assert(deadbeef_size > size);

	ptr = malloc(deadbeef_size);
	if (ptr == NULL) {
		return NULL;
	}

	memcpy((char *)ptr + size, DEAD_BEEF, 4);
	return ptr;
}

int has_deadbeef(void *ptr, size_t size) {
	return memcmp(DEAD_BEEF, (char *)ptr + size, 4) == 0;
}

void *v_malloc_internal(size_t size, const char *file, int line) {
	Allocation alloc;
	size_t alloc_slot;

	if (allocations.cap == 0) {
		assert(INITIAL_ALLOCATIONS_CAP);
		allocations.cap = INITIAL_ALLOCATIONS_CAP;
		allocations.ptr = malloc(allocations.cap * sizeof(Allocation));
		assert("OOM" && allocations.ptr);
	}

	for (alloc_slot = 0; alloc_slot < allocations.size; ++alloc_slot) {
		if (allocations.ptr[alloc_slot].size == 0) {
			break;
		}
	}

	if (alloc_slot == allocations.size && allocations.size == allocations.cap) {
		allocations.cap *= 2;
		allocations.ptr = realloc(allocations.ptr, allocations.cap * sizeof(Allocation));
		assert("OOM" && allocations.ptr);
	}

	alloc.ptr = malloc_deadbeef(size);
	assert("OOM" && alloc.ptr);
	alloc.size = size;
	alloc.file = file;
	alloc.line = line;

	allocations.ptr[alloc_slot] = alloc;
	if (alloc_slot == allocations.size) {
		allocations.size += 1;
	}

	return alloc.ptr;
}

void v_free_internal(void *ptr, const char *file, int line) {
	Allocation *alloc;
	size_t alloc_slot;

	for (alloc_slot = 0; alloc_slot < allocations.size; ++alloc_slot) {
		alloc = &allocations.ptr[alloc_slot];
		if (alloc->ptr == ptr) {
			break;
		}
	}

	assert("allocation not found" && alloc_slot < allocations.size);
	assert("double free" && alloc->size > 0);
	assert("buffer overflow" && has_deadbeef(ptr, alloc->size));

	free(alloc->ptr);
	memset(alloc, 0, sizeof(Allocation));
}

#ifdef OVERRIDE_MALLOC_API

#define malloc(size) v_malloc_internal(size, __FILE__, __LINE__)
#define free(ptr) v_free_internal(ptr, __FILE__, __LINE__)

#else

#define v_malloc(size) v_malloc_internal(size, __FILE__, __LINE__)
#define v_free(ptr) v_free_internal(ptr, __FILE__, __LINE__)

#endif /* OVERRIDE_MALLOC_API */

void v_malloc_report(void) {
	size_t i, leak_count = 0;
	for (i = 0; i < allocations.size; ++i) {
		if (allocations.ptr[i].size > 0) {
			fprintf(stderr, "LEAK: %zu bytes at %s:%d\n",
					allocations.ptr[i].size,
					allocations.ptr[i].file,
					allocations.ptr[i].line);
			leak_count++;
		}
	}
	if (leak_count > 0) {
		fprintf(stderr, "TOTAL: %zu leaks\n", leak_count);
	}
}

#endif /* V_MALLOC_H */

