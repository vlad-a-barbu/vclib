#ifndef DBG_MALLOC_H
#define DBG_MALLOC_H

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

typedef struct FreeAllocation {
	size_t idx;
	struct FreeAllocation *next;
} FreeAllocation;

static FreeAllocation *allocation_freelist_head;
static FreeAllocation *allocation_freelist_tail;

#define INITIAL_ALLOCATIONS_CAP 100

static unsigned char DEADBEEF[4] = {0xDE, 0xAD, 0xBE, 0xEF};

void *malloc_deadbeef(size_t size) {
	void *ptr;
	size_t deadbeef_size = size + 4;
	assert(deadbeef_size > size);

	ptr = malloc(deadbeef_size);
	if (ptr == NULL) {
		return NULL;
	}

	memset(ptr, 0xCC, size);
	memcpy((char *)ptr + size, DEADBEEF, 4);
	return ptr;
}

int deadbeef(void *ptr, size_t size) {
	return memcmp(DEADBEEF, (char *)ptr + size, 4) == 0;
}

void check_overflows(void) {
	Allocation *alloc;
	size_t i;
	int ok = 1;

	for (i = 0; i < allocations.size; ++i) {
		alloc = &allocations.ptr[i];
		if (alloc->size == 0) continue;
		if (!deadbeef(alloc->ptr, alloc->size)) {
			printf("buffer overflow: addr %p | size %zu | location %s:%d\n",
					alloc->ptr, alloc->size, alloc->file, alloc->line);
			ok = 0;
		}
	}

	assert("buffer overflow allocation sites listed above" && ok);
}

void *dbg_malloc_internal(size_t size, const char *file, int line) {
	Allocation alloc;
	size_t alloc_slot;
	FreeAllocation *free_allocation;

	if (allocations.cap == 0) {
		assert(INITIAL_ALLOCATIONS_CAP);
		allocations.cap = INITIAL_ALLOCATIONS_CAP;
		allocations.ptr = malloc(allocations.cap * sizeof(Allocation));
		assert("OOM" && allocations.ptr);
	}

	check_overflows();

	if (allocation_freelist_head) {
		alloc_slot = allocation_freelist_head->idx;
		free_allocation = allocation_freelist_head;
		allocation_freelist_head = allocation_freelist_head->next;
		if (allocation_freelist_head == NULL) {
			allocation_freelist_tail = NULL;
		}
		free(free_allocation);
	} else {
		alloc_slot = allocations.size;
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

void dbg_free_internal(void *ptr, const char *file, int line) {
	Allocation *alloc;
	size_t alloc_slot;
	FreeAllocation *free_allocation;

	for (alloc_slot = 0; alloc_slot < allocations.size; ++alloc_slot) {
		alloc = &allocations.ptr[alloc_slot];
		if (alloc->ptr == ptr) break;
	}

	if (alloc_slot == allocations.size) {
		printf("allocation not found | addr %p | location %s:%d\n",
				ptr, file, line);
		assert(0);
	}
	if (alloc->size == 0) {
		printf("double free | addr %p | location %s:%d\n",
				ptr, file, line);
		assert(0);
	}
	if (!deadbeef(ptr, alloc->size)) {
		printf("buffer overflow | addr %p | location %s:%d\n",
				ptr, file, line);
		assert(0);
	}

	free(alloc->ptr);
	memset(alloc, 0, sizeof(Allocation));

	free_allocation = malloc(sizeof(FreeAllocation));
	assert("OOM" && free_allocation);
	free_allocation->idx = alloc_slot;
	free_allocation->next = NULL;

	if (allocation_freelist_tail == NULL) {
		assert(allocation_freelist_head == NULL);
		allocation_freelist_head = allocation_freelist_tail = free_allocation;
	} else {
		allocation_freelist_tail->next = free_allocation;
		allocation_freelist_tail = free_allocation;
	}
}

void dbg_malloc_report(void) {
	size_t i, leak_count = 0;

	for (i = 0; i < allocations.size; ++i) {
		if (allocations.ptr[i].size > 0) {
			fprintf(stderr, "memory leak: %zu bytes at %s:%d\n",
					allocations.ptr[i].size,
					allocations.ptr[i].file,
					allocations.ptr[i].line);
			leak_count += 1;
		}
	}

	if (leak_count) {
		fprintf(stderr, "total leaks: %zu leaks\n", leak_count);
	}

	check_overflows();
}

#ifdef DBG_MALLOC_USE_PREFIX

#define dbg_malloc(size) dbg_malloc_internal(size, __FILE__, __LINE__)
#define dbg_free(ptr) dbg_free_internal(ptr, __FILE__, __LINE__)

#else

#define malloc(size) dbg_malloc_internal(size, __FILE__, __LINE__)
#define free(ptr) dbg_free_internal(ptr, __FILE__, __LINE__)

#endif /* OVERRIDE_MALLOC_API */

#endif /* DBG_MALLOC_H */
