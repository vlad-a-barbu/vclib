#include "v.h"

static int
String8Eq(String8 key1, String8 key2) {
    return (key1.len == key2.len) && 
           (memcmp(key1.ptr, key2.ptr, key1.len) == 0);
}

static void *
nth_no_bounds_checking(void *values, size_t n, size_t value_size)
{
	return (char *)values + value_size * n;
}

typedef struct {
	Arena arena_keys;
	Arena arena_keys_ptrs;
	Arena arena_values;
	String8 *keys;
	void *values;
	size_t size;
	size_t capacity;
} HashTable_Bucket;

HashTable_Bucket
HashTable_Bucket_Create(size_t max_capacity, size_t initial_capacity, size_t max_key_length, size_t value_size, Arena *arena)
{
	HashTable_Bucket bucket;
	char *arena_keys_buff, *arena_keys_ptrs_buff, *arena_values_buff;
	Arena arena_keys, arena_keys_ptrs, arena_values;
	String8 *keys;
	void *values;

	arena_keys_buff = arena_alloc(arena, max_capacity * sizeof(String8));
	assert(arena_keys_buff);
	arena_keys = arena_init(arena_keys_buff, max_capacity * sizeof(String8));

	arena_keys_ptrs_buff = arena_alloc(arena, max_capacity * max_key_length);
	assert(arena_keys_ptrs_buff);
	arena_keys_ptrs = arena_init(arena_keys_ptrs_buff, max_capacity * max_key_length);

	arena_values_buff = arena_alloc(arena, max_capacity * value_size);
	assert(arena_values_buff);
	arena_values = arena_init(arena_values_buff, max_capacity * value_size);

	keys = arena_alloc(&arena_keys, initial_capacity * sizeof(String8));
	assert(keys);

	values = arena_alloc(&arena_values, initial_capacity * value_size);
	assert(values);

	memset(&bucket, 0, sizeof(bucket));
	bucket.arena_keys = arena_keys;
	bucket.arena_keys_ptrs = arena_keys_ptrs;
	bucket.arena_values = arena_values;
	bucket.keys = keys;
	bucket.values = values;
	bucket.capacity = initial_capacity;

	return bucket;
}

static size_t
HashTable_Bucket_Add(HashTable_Bucket *bucket, String8 key, void *value, size_t value_size)
{
	size_t i;
	String8 *keys;
	void *values;
	char *key_ptr;

	for (i = 0; i < bucket->size; ++i)
	{
		if (String8Eq(bucket->keys[i], key))
		{
			memcpy(nth_no_bounds_checking(bucket->values, i, value_size),
				value,
				value_size);
			return i;
		}
	}

	if (bucket->size == bucket->capacity)
	{
		bucket->capacity *= 2;

		keys = arena_resize_last(&bucket->arena_keys, bucket->capacity * sizeof(String8));
		assert(keys);

		values = arena_resize_last(&bucket->arena_values, bucket->capacity * value_size);
		assert(values);

		bucket->keys = keys;
		bucket->values = values;
	}

	key_ptr = arena_alloc(&bucket->arena_keys_ptrs, key.len);
	assert(key_ptr);
	memcpy(key_ptr, key.ptr, key.len);

	key.ptr = key_ptr;
	memcpy(&bucket->keys[bucket->size], &key, sizeof(key));

	memcpy(nth_no_bounds_checking(bucket->values, bucket->size, value_size),
		value,
		value_size);

	return bucket->size++;
}

static size_t
HashTable_Bucket_Search(HashTable_Bucket *bucket, String8 search_key)
{
	size_t i;
	String8 current_key;

	for (i = 0; i < bucket->size; ++i)
	{
		current_key = bucket->keys[i];
		if (String8Eq(current_key, search_key))
		{
			return i;
		}
	}

	return bucket->size;
}

typedef size_t (*HashFn)(String8 key);

typedef struct {
	Arena arena;
	size_t bucket_initial_capacity;
	HashFn hash_fn;
	size_t max_key_length;
	size_t value_size;

	struct {
		HashTable_Bucket *ptr;
		size_t size;
		size_t capacity;
	} buckets;
} HashTable;

HashTable
HashTable_Create(size_t initial_bucket_count,
		HashFn hash_fn,
		size_t max_key_length, 
		size_t value_size,
		char *buffer, size_t buffer_size,
		size_t bucket_max_capacity,
		size_t bucket_initial_capacity)
{
	HashTable hash_table;
	Arena arena;

	memset(&hash_table, 0, sizeof(hash_table));
	arena = arena_init(buffer, buffer_size);

	hash_table.arena = arena;
	hash_table.bucket_initial_capacity = bucket_initial_capacity;
	hash_table.hash_fn = hash_fn;
	hash_table.max_key_length = max_key_length;
	hash_table.value_size = value_size;
	
	HashTable_Bucket *buckets = arena_alloc(&arena, sizeof(HashTable_Bucket) * initial_bucket_count);
	assert(buckets);
	hash_table.buckets.ptr = buckets;
	hash_table.buckets.capacity = initial_bucket_count;
	while (hash_table.buckets.size < initial_bucket_count)
	{
		hash_table.buckets.ptr[hash_table.buckets.size++] = HashTable_Bucket_Create(bucket_max_capacity,
												bucket_initial_capacity,
												max_key_length,
												value_size,
												&arena);
	}

	return hash_table;
}

int
HashTable_Set(HashTable *hash_table, char *key, void *value, Arena *arena)
{
	String8 key_str;
	size_t bucket_id;
	HashTable_Bucket *bucket;

	key_str = string8_create(key, arena);
	if (key_str.len > hash_table->max_key_length)
	{
		return 0;
	}

	bucket_id = hash_table->hash_fn(key_str) % hash_table->buckets.size;
	bucket = &hash_table->buckets.ptr[bucket_id];

	HashTable_Bucket_Add(bucket, key_str, value, hash_table->value_size);

	return 1;
}

int
HashTable_Get(HashTable *hash_table, char *key, void *out_value, Arena *arena)
{
	String8 key_str;
	size_t bucket_id;
	HashTable_Bucket bucket;
	size_t elem_id;

	key_str = string8_create(key, arena);
	if (key_str.len > hash_table->max_key_length)
	{
		return 0;
	}

	bucket_id = hash_table->hash_fn(key_str) % hash_table->buckets.size;
	bucket = hash_table->buckets.ptr[bucket_id];

	elem_id = HashTable_Bucket_Search(&bucket, key_str);
	if (elem_id == bucket.size)
	{
		return 0;
	}
	
	if (out_value)
	{
		memcpy(out_value,
			nth_no_bounds_checking(bucket.values, elem_id, hash_table->value_size),
			hash_table->value_size);
	}

	return 1;
}

size_t FNV1a_Hash(String8 key) {
	size_t hash = 0xcbf29ce484222325ULL;  // FNV offset basis
	for (size_t i = 0; i < key.len; i++) {
		hash ^= (unsigned char)key.ptr[i];
		hash *= 0x100000001b3ULL;  // FNV prime
	}
	return hash;
}

#define TMP_BUFFER_SIZE 0x1000
static char tmp_buffer[TMP_BUFFER_SIZE];

#define BUFFER_SIZE (100 * 1024 * 1024)
static char buffer[BUFFER_SIZE];

#define INITIAL_BUCKET_COUNT 10000
#define MAX_KEY_LENGTH 20
#define BUCKET_MAX_CAPACITY 160
#define BUCKET_INITIAL_CAPACITY 5

typedef struct {
	float x;
	float y;
} Point;

int
main(void)
{
	Arena tmp_arena;
	HashTable hash_table;
	Point p;
	size_t i;
	char ps[10];

	tmp_arena = arena_init(tmp_buffer, TMP_BUFFER_SIZE);

	hash_table = HashTable_Create(INITIAL_BUCKET_COUNT,
					FNV1a_Hash,
					MAX_KEY_LENGTH,
					sizeof(Point),
					buffer, BUFFER_SIZE,
					BUCKET_MAX_CAPACITY,
					BUCKET_INITIAL_CAPACITY);

	for (i = 0; i < 1000000; ++i)
	{
		p.x = p.y = i % 10;
		sprintf(ps, "p%zu", i);
		if (tmp_arena.size + MAX_KEY_LENGTH > tmp_arena.capacity) {
			arena_reset(&tmp_arena);
		}
		HashTable_Set(&hash_table, ps, &p, &tmp_arena);
	}
	
	for (i = 0; i < 1000000; ++i)
	{
		p.x = p.y = i % 10;
		sprintf(ps, "p%zu", i);
		if (tmp_arena.size + MAX_KEY_LENGTH > tmp_arena.capacity) {
			arena_reset(&tmp_arena);
		}
		assert(HashTable_Get(&hash_table, ps, NULL, &tmp_arena));
	}

	return 0;
}
