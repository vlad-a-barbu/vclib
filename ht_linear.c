/* DO NOT USE STUFF FROM THIS FILE, IT IS WIP & FULL OF BUGS */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef RELEASE
#include "dbg_malloc.h"
#endif

/* 
	memory layout is a sequence of [SSSS|[VVV...VVV]
								   [-^^-][---^^^---]
								    size    value
*/
typedef struct {
	char *ptr;
	size_t reserved_size_bytes; /* num bytes reserved to represent the size of the following value */
	size_t size;
	size_t capacity;
} VSV_Buffer; /* variable sized values buffer */

VSV_Buffer vsv_create(size_t initial_capacity, size_t reserved_size_bytes) {
	VSV_Buffer vsv;
	assert(initial_capacity && reserved_size_bytes);
	vsv.ptr = malloc(initial_capacity);
	assert("OOM" && vsv.ptr);
	vsv.capacity = initial_capacity;
	vsv.size = 0;
	vsv.reserved_size_bytes = reserved_size_bytes;
	return vsv;
}

void vsv_destroy(VSV_Buffer *vsv) {
	assert(vsv && vsv->capacity && vsv->ptr);
	free(vsv->ptr);
	memset(vsv, 0, sizeof(*vsv));
}

void vsv_ensure_value_size_within_reserved_size_bytes(VSV_Buffer *vsv, size_t value_size) {
	assert( value_size <= (1ULL << (8 * vsv->reserved_size_bytes)) );
}

void vsv_push(VSV_Buffer *vsv, void *value, size_t value_size) {
	size_t allocation_size;
	vsv_ensure_value_size_within_reserved_size_bytes(vsv, value_size);

	allocation_size = vsv->reserved_size_bytes + value_size;
	while (vsv->size + allocation_size > vsv->capacity) {
		vsv->capacity *= 2;
		vsv->ptr = realloc(vsv->ptr, vsv->capacity);
		assert("OOM" && vsv->ptr);
	}

	memcpy((char *)vsv->ptr + vsv->size, 
			&value_size,
			vsv->reserved_size_bytes);

	memcpy((char *)vsv->ptr + vsv->size + vsv->reserved_size_bytes,
			value,
			value_size);

	vsv->size += allocation_size;
}

size_t vsv_get_nth(VSV_Buffer *vsv, size_t n, void **out_value) {
	size_t current_size, i, offset = 0;

	for (i = 0; i < n; ++i) {
		if (offset + vsv->reserved_size_bytes > vsv->size) {
			if (out_value) {
				*out_value = NULL;
			}
			return 0;
		}
		memcpy(&current_size, (char *)vsv->ptr + offset, vsv->reserved_size_bytes);
		offset += vsv->reserved_size_bytes + current_size;
	}

	memcpy(&current_size, (char *)vsv->ptr + offset, vsv->reserved_size_bytes);
	if (out_value) {		
		*out_value = (char *)vsv->ptr + offset + vsv->reserved_size_bytes;
	}
    
	return current_size;
}

typedef size_t (*HashFn)(char *key, size_t key_size);

size_t fnv1a(char *key, size_t key_size) {
	size_t hash = 0xcbf29ce484222325ULL;
	for (size_t i = 0; i < key_size; ++i) {
		hash ^= key[i];
		hash *= 0x100000001b3ULL;
	}
	return hash;
}

typedef struct {
	void *keys;
	void *values;
	size_t size;
	HashFn hash;
} HashTable;

HashTable ht_create(size_t key_size, size_t value_size, size_t max_size_estimate, HashFn hash) {
	HashTable ht;
	size_t initial_size = max_size_estimate * 4;

	ht.keys = malloc(key_size * initial_size);
	assert("OOM" && ht.keys);
	memset(ht.keys, 0, key_size * initial_size);
	ht.key_size = key_size;

	ht.values = malloc(value_size * initial_size);
	assert("OOM" && ht.values);
	memset(ht.values, 0, value_size * initial_size);
	ht.value_size = value_size;

	ht.size = initial_size;
	ht.hash = hash;

	return ht;
}

void ht_destroy(HashTable *ht) {
	assert(ht && ht->cap && ht->keys && ht->values);
	free(ht->keys);
	free(ht->values);
	memset(ht, 0, sizeof(*ht));
}

void *ht_nth_key(HashTable *ht, size_t n) {
	return (char *)ht->keys + n * ht->key_size;
}

void *ht_nth_value(HashTable *ht, size_t n) {
	return (char *)ht->values + n * ht->value_size;
}

void ht_set(HashTable *ht, void *key, void *value) {
	void *k, *v;
	int key_exists = 0;
	size_t i = ht->hash(key, ht->key_size);

	k = ht_nth_key(ht, i);
	while (k != NULL) {
		if (memcmp(k, key, ht->key_size) == 0) {
			key_exists = 1;
			break;
		}
		k = (char *)k + key_size;
		i += 1;
		/* TODO: resize */
		assert(i < ht->size);
	}
	v = ht_nth_value(ht, i);

	if (key_exists) {
		memset(v, value, ht->value_size);
	} else {
		// copy the mem for key & value so that the ht owns it
	}
}

void *ht_get(HashTable *ht, void *key) {
	void *k;
	size_t i = ht->hash(key, ht->key_size);

	k = ht_nth_key(ht, i);
	while (memcmp(k, key, ht->key_size) != 0) {
		k = (char *)k + key_size;
		i += 1;
		if (i == ht->size) {
			return NULL;
		}
	}

	return ht_nth_value(ht, i);
}

int main(void) {
	HashTable ht = ht_create(sizeof())
	return 0;
}
