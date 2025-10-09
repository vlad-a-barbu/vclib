#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#ifdef _WIN32
    #if defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    #elif defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable: 4996)
    #endif
#endif

/* BEGIN ARENA */

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

/* END ARENA */

/* BEGIN IO */

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

const char *read_entire_file_cstring(const char *path, Arena *arena, size_t *out_size_no_nullterm) {
	FILE *file;
	size_t file_size, read_size;
	char *read_buffer;
	ArenaSave save;

	if ((file = fopen(path, "rb")) == NULL) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	save = arena_save(arena);
	if ((read_buffer = arena_alloc(arena, file_size + 1)) == NULL) {
		fclose(file);
		return NULL;
	}

	read_size = fread(read_buffer, sizeof(char), file_size, file);
	fclose(file);

	if (read_size < file_size) {
		arena_restore(arena, save);
		return NULL;
	}
	
	read_buffer[read_size] = '\0';
	if (out_size_no_nullterm) {
		*out_size_no_nullterm = read_size;
	}
	return read_buffer;
}

/* END IO */

/* BEGIN LOG */

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION mutex_t;
    #define mutex_init(m) InitializeCriticalSection(m)
    #define mutex_destroy(m) DeleteCriticalSection(m)
    #define mutex_lock(m) EnterCriticalSection(m)
    #define mutex_unlock(m) LeaveCriticalSection(m)
#else
    #include <pthread.h>
    typedef pthread_mutex_t mutex_t;
    #define mutex_init(m) pthread_mutex_init(m, NULL)
    #define mutex_destroy(m) pthread_mutex_destroy(m)
    #define mutex_lock(m) pthread_mutex_lock(m)
    #define mutex_unlock(m) pthread_mutex_unlock(m)
#endif

#ifndef LOG_BUFFER_SIZE
#define LOG_BUFFER_SIZE 0x200
#endif
static char log_buffer[LOG_BUFFER_SIZE];

#ifndef LOG_MSG_BUFFER_SIZE
#define LOG_MSG_BUFFER_SIZE 0x100
#endif
static char log_msg_buffer[LOG_MSG_BUFFER_SIZE];

static struct {
    FILE *file;
    char *buffer;
    size_t buffer_size;
    char *msg_buffer;
    size_t msg_buffer_size;
    mutex_t lock;
    int initialized;
} logger;

void
logger_init(FILE *file) {
    assert(file);
    logger.file = file;
    logger.buffer = log_buffer;
    logger.buffer_size = LOG_BUFFER_SIZE;
    logger.msg_buffer = log_msg_buffer;
    logger.msg_buffer_size = LOG_MSG_BUFFER_SIZE;
    mutex_init(&logger.lock);
    logger.initialized = 1;
}

static void
logger_write(const char *tag, const char *file, const char *function, int line_number) {
    size_t write_len;

    assert(logger.initialized);
    
    mutex_lock(&logger.lock);

    write_len = snprintf(logger.buffer, logger.buffer_size,
            "[%s] %s | %s L%d | %s\n",
            tag, file, function, line_number, logger.msg_buffer);
    
    assert(write_len >= 0 && write_len < logger.buffer_size);
    assert(write_len == fwrite(logger.buffer, sizeof(char), write_len, logger.file));

    fflush(logger.file);
    
    mutex_unlock(&logger.lock);
}

#define log(tag, message) 											\
	do {													\
		size_t write_len = snprintf(logger.msg_buffer, logger.msg_buffer_size, message); 		\
		assert(write_len >= 0 && write_len < logger.msg_buffer_size);					\
		logger_write(tag, __FILE__, __func__, __LINE__);						\
	} while(0);												\

#define log_debug(message) log("DEBUG", message)
#define log_info(message) log("INFO", message)
#define log_warning(message) log("WARNING", message)
#define log_error(message) log("ERROR", message)

#define logf(tag, template, ...) 										\
	do {													\
		size_t write_len = snprintf(logger.msg_buffer, logger.msg_buffer_size, template, __VA_ARGS__); 	\
		assert(write_len >= 0 && write_len < logger.msg_buffer_size);					\
		logger_write(tag, __FILE__, __func__, __LINE__);						\
	} while(0);												\

#define log_debugf(template, ...) logf("DEBUG", template, __VA_ARGS__)
#define log_infof(template, ...) logf("INFO", template, __VA_ARGS__)
#define log_warningf(template, ...) logf("WARNING", template, __VA_ARGS__)
#define log_errorf(template, ...) logf("ERROR", template, __VA_ARGS__)

/* END LOG */

/* BEGIN SOCKET */

int listen_tcp(const char *port, int backlog) {
	int s, on;
	struct addrinfo hints, *res;

	if ((s = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		return -1;
	}

	on = 1;	
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(int)) == -1) {
		close(s);
		return -1;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo("0.0.0.0", port, &hints, &res) != 0) {
		close(s);
		return -1;
	}

	if (bind(s, res->ai_addr, res->ai_addrlen) == -1) {
		freeaddrinfo(res);
		close(s);
		return -1;
	}

	freeaddrinfo(res);

	if (listen(s, backlog) == -1) {
		close(s);
		return -1;
	}
	
	return s;
}


/* END SOCKET */

