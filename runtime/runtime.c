// Aria Runtime — C implementation
// Cross-platform: macOS, Linux, Windows
// Linked with LLVM IR output by clang.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// On Windows, 'long' is 32-bit even on 64-bit systems.
// Aria uses i64 (mapped to LLVM i64) for all values including pointers.
// Use a platform-appropriate 64-bit integer type.
#ifdef _WIN32
typedef long long aria_int;
#else
typedef long aria_int;
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <direct.h>
#define read _read
#define write _write
#define open _open
#define close _close
#define stat _stat
#define O_RDONLY _O_RDONLY
typedef int ssize_t;
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

// --- String struct (used by many functions) ---
struct _aria_str {
    char *ptr;
    aria_int len;
};

// --- Forward declarations ---
aria_int _aria_array_new(aria_int capacity);
aria_int _aria_array_append(aria_int arr_ptr, aria_int value);
aria_int _aria_array_slice(aria_int arr_ptr, aria_int start);
void _aria_map_set(aria_int map_ptr, aria_int key_ptr, aria_int key_len, aria_int value);
aria_int _aria_map_get(aria_int map_ptr, aria_int key_ptr, aria_int key_len);
aria_int _aria_list_dir(char *path_ptr, aria_int path_len);
aria_int _aria_is_dir(char *path_ptr, aria_int path_len);

// --- Exit ---

void _aria_exit(aria_int code) {
    exit((int)code);
}

// --- I/O ---

aria_int _aria_write(aria_int fd, char *ptr, aria_int len) {
    return (aria_int)write((int)fd, ptr, (size_t)len);
}

void _aria_println_str(char *ptr, aria_int len) {
    write(1, ptr, (size_t)len);
    write(1, "\n", 1);
}

void _aria_print_str(char *ptr, aria_int len) {
    write(1, ptr, (size_t)len);
}

void _aria_eprintln_str(char *ptr, aria_int len) {
    write(2, ptr, (size_t)len);
    write(2, "\n", 1);
}

// --- Memory: GC-tracked allocator ---
// Simple mark-sweep GC: track all allocations, sweep when threshold exceeded.

#define GC_INITIAL_CAPACITY 4096
#define GC_GROWTH_FACTOR 2

static struct {
    void **ptrs;
    aria_int *sizes;
    int *marks;
    aria_int count;
    aria_int capacity;
    aria_int total_bytes;
    aria_int threshold;
    int enabled;
} _gc = {NULL, NULL, NULL, 0, 0, 0, 1048576, 0};  // 1MB threshold

static void _gc_init(void) {
    if (_gc.ptrs) return;
    _gc.capacity = GC_INITIAL_CAPACITY;
    _gc.ptrs = (void **)calloc((size_t)_gc.capacity, sizeof(void *));
    _gc.sizes = (aria_int *)calloc((size_t)_gc.capacity, sizeof(aria_int));
    _gc.marks = (int *)calloc((size_t)_gc.capacity, sizeof(int));
    _gc.count = 0;
    _gc.total_bytes = 0;
    _gc.enabled = 1;
}

static void _gc_track(void *ptr, aria_int size) {
    if (!_gc.ptrs) _gc_init();
    if (_gc.count >= _gc.capacity) {
        _gc.capacity *= GC_GROWTH_FACTOR;
        _gc.ptrs = (void **)realloc(_gc.ptrs, (size_t)_gc.capacity * sizeof(void *));
        _gc.sizes = (aria_int *)realloc(_gc.sizes, (size_t)_gc.capacity * sizeof(aria_int));
        _gc.marks = (int *)realloc(_gc.marks, (size_t)_gc.capacity * sizeof(int));
    }
    _gc.ptrs[_gc.count] = ptr;
    _gc.sizes[_gc.count] = size;
    _gc.marks[_gc.count] = 0;
    _gc.count++;
    _gc.total_bytes += size;
}

// GC sweep: free all unmarked allocations (called explicitly or at threshold)
aria_int _aria_gc_collect(void) {
    aria_int freed = 0;
    aria_int new_count = 0;
    for (aria_int i = 0; i < _gc.count; i++) {
        if (_gc.marks[i] == 0 && _gc.ptrs[i] != NULL) {
            _gc.total_bytes -= _gc.sizes[i];
            free(_gc.ptrs[i]);
            freed++;
        } else {
            _gc.ptrs[new_count] = _gc.ptrs[i];
            _gc.sizes[new_count] = _gc.sizes[i];
            _gc.marks[new_count] = 0;  // reset marks for next cycle
            new_count++;
        }
    }
    _gc.count = new_count;
    return freed;
}

// GC stats
aria_int _aria_gc_total_bytes(void) { return _gc.total_bytes; }
aria_int _aria_gc_allocation_count(void) { return _gc.count; }

char *_aria_alloc(aria_int size) {
    if (!_gc.ptrs) _gc_init();
    char *ptr = (char *)calloc(1, (size_t)size);
    _gc_track(ptr, size);
    return ptr;
}

// Stack allocation: returns pointer to alloca'd memory (caller provides buffer)
// This is a no-op at runtime — @stack is handled at LLVM level via alloca
char *_aria_stack_alloc(aria_int size) {
    return (char *)calloc(1, (size_t)size);  // fallback if LLVM alloca unavailable
}

void _aria_memcpy(char *dst, char *src, aria_int len) {
    memcpy(dst, src, (size_t)len);
}

// --- Arena allocator ---
// Bulk allocator: allocate many objects, free all at once.

struct _aria_arena {
    char *buf;
    aria_int capacity;
    aria_int used;
};

aria_int _aria_arena_new(aria_int capacity) {
    if (capacity < 4096) capacity = 4096;
    struct _aria_arena *a = (struct _aria_arena *)malloc(sizeof(struct _aria_arena));
    a->buf = (char *)calloc(1, (size_t)capacity);
    a->capacity = capacity;
    a->used = 0;
    return (aria_int)a;
}

char *_aria_arena_alloc(aria_int arena_handle, aria_int size) {
    struct _aria_arena *a = (struct _aria_arena *)arena_handle;
    // Align to 8 bytes
    size = (size + 7) & ~7;
    if (a->used + size > a->capacity) {
        // Grow arena
        aria_int new_cap = a->capacity * 2;
        while (a->used + size > new_cap) new_cap *= 2;
        a->buf = (char *)realloc(a->buf, (size_t)new_cap);
        memset(a->buf + a->capacity, 0, (size_t)(new_cap - a->capacity));
        a->capacity = new_cap;
    }
    char *ptr = a->buf + a->used;
    a->used += size;
    return ptr;
}

void _aria_arena_reset(aria_int arena_handle) {
    struct _aria_arena *a = (struct _aria_arena *)arena_handle;
    memset(a->buf, 0, (size_t)a->used);
    a->used = 0;
}

void _aria_arena_free(aria_int arena_handle) {
    struct _aria_arena *a = (struct _aria_arena *)arena_handle;
    free(a->buf);
    free(a);
}

aria_int _aria_arena_allocated(aria_int arena_handle) {
    struct _aria_arena *a = (struct _aria_arena *)arena_handle;
    return a->used;
}

aria_int _aria_arena_capacity(aria_int arena_handle) {
    struct _aria_arena *a = (struct _aria_arena *)arena_handle;
    return a->capacity;
}

// --- Object Pool ---
// Pre-allocated pool of reusable objects.

struct _aria_pool {
    aria_int *objects;
    int *in_use;
    aria_int capacity;
    aria_int obj_size;
};

aria_int _aria_pool_new(aria_int capacity, aria_int obj_size) {
    struct _aria_pool *p = (struct _aria_pool *)malloc(sizeof(struct _aria_pool));
    p->capacity = capacity;
    p->obj_size = obj_size;
    p->objects = (aria_int *)calloc((size_t)capacity, sizeof(aria_int));
    p->in_use = (int *)calloc((size_t)capacity, sizeof(int));
    // Pre-allocate all objects
    for (aria_int i = 0; i < capacity; i++) {
        p->objects[i] = (aria_int)calloc(1, (size_t)obj_size);
        p->in_use[i] = 0;
    }
    return (aria_int)p;
}

aria_int _aria_pool_get(aria_int pool_handle) {
    struct _aria_pool *p = (struct _aria_pool *)pool_handle;
    for (aria_int i = 0; i < p->capacity; i++) {
        if (!p->in_use[i]) {
            p->in_use[i] = 1;
            return p->objects[i];
        }
    }
    // Pool exhausted — allocate new (not pooled)
    return (aria_int)calloc(1, (size_t)p->obj_size);
}

void _aria_pool_put(aria_int pool_handle, aria_int obj) {
    struct _aria_pool *p = (struct _aria_pool *)pool_handle;
    for (aria_int i = 0; i < p->capacity; i++) {
        if (p->objects[i] == obj) {
            p->in_use[i] = 0;
            memset((void *)obj, 0, (size_t)p->obj_size);
            return;
        }
    }
    // Not from this pool — just free
    free((void *)obj);
}

// --- File I/O ---

struct _aria_str _aria_read_file(char *path_ptr, aria_int path_len) {
    // Null-terminate the path
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    int fd = open(path, O_RDONLY);
    free(path);
    if (fd < 0) {
        struct _aria_str result = {NULL, 0};
        return result;
    }

    struct stat st;
    fstat(fd, &st);
    aria_int size = (aria_int)st.st_size;

    char *buf = (char *)malloc((size_t)(size + 1));
    read(fd, buf, (size_t)size);
    buf[size] = '\0';
    close(fd);

    struct _aria_str result = {buf, size};
    return result;
}

void _aria_write_file(char *path_ptr, aria_int path_len, char *data_ptr, aria_int data_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    free(path);
    if (fd < 0) return;
    write(fd, data_ptr, (size_t)data_len);
    close(fd);
}

void _aria_write_binary_file(char *path_ptr, aria_int path_len, aria_int *data_arr, aria_int data_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    free(path);
    if (fd < 0) return;

    // data_arr is a sentinel array: element 0 is dummy, real data starts at index 1
    // Each i64 element holds one byte in its low 8 bits
    char *bytes = (char *)malloc((size_t)data_len);
    for (aria_int i = 0; i < data_len; i++) {
        bytes[i] = (char)(data_arr[i + 1] & 0xFF);
    }
    write(fd, bytes, (size_t)data_len);
    free(bytes);
    close(fd);
}

// --- Integer to string ---

struct _aria_str _aria_int_to_str(aria_int value) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%ld", value);
    char *result = (char *)malloc((size_t)(len + 1));
    memcpy(result, buf, (size_t)(len + 1));
    struct _aria_str s = {result, (aria_int)len};
    return s;
}

// Parse string to integer. Returns {value, 1} on success, {0, 0} on failure.
struct _aria_str _aria_str_to_int(char *ptr, aria_int len) {
    if (len == 0) {
        struct _aria_str s = {0, 0};
        return s;
    }
    aria_int result = 0;
    aria_int i = 0;
    aria_int neg = 0;
    if (ptr[0] == '-') { neg = 1; i = 1; }
    if (ptr[0] == '+') { i = 1; }
    if (i >= len) {
        struct _aria_str s = {0, 0};
        return s;
    }
    while (i < len) {
        char ch = ptr[i];
        if (ch < '0' || ch > '9') {
            struct _aria_str s = {0, 0};
            return s;
        }
        result = result * 10 + (ch - '0');
        i++;
    }
    if (neg) result = -result;
    struct _aria_str s = {(char *)result, 1};
    return s;
}

// Parse string to float. Returns the float bits as i64. Returns 0 on failure.
aria_int _aria_str_to_float(char *ptr, aria_int len) {
    if (len == 0) return 0;
    char *buf = (char *)malloc((size_t)(len + 1));
    memcpy(buf, ptr, (size_t)len);
    buf[len] = '\0';
    char *end;
    double val = strtod(buf, &end);
    free(buf);
    if (end == buf) return 0;  // no conversion
    aria_int bits;
    memcpy(&bits, &val, sizeof(double));
    return bits;
}

// --- Float to string ---

struct _aria_str _aria_float_to_str(aria_int bits) {
    double value;
    memcpy(&value, &bits, sizeof(double));
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", value);
    char *result = (char *)malloc((size_t)(len + 1));
    memcpy(result, buf, (size_t)(len + 1));
    struct _aria_str s = {result, (aria_int)len};
    return s;
}

// --- String operations ---

struct _aria_str _aria_str_concat(char *a_ptr, aria_int a_len, char *b_ptr, aria_int b_len) {
    aria_int total = a_len + b_len;
    char *result = (char *)malloc((size_t)(total + 1));
    memcpy(result, a_ptr, (size_t)a_len);
    memcpy(result + a_len, b_ptr, (size_t)b_len);
    result[total] = '\0';
    struct _aria_str s = {result, total};
    return s;
}

aria_int _aria_str_eq(char *a_ptr, aria_int a_len, char *b_ptr, aria_int b_len) {
    if (a_len != b_len) return 0;
    if (a_len == 0) return 1;
    return memcmp(a_ptr, b_ptr, (size_t)a_len) == 0 ? 1 : 0;
}

struct _aria_str _aria_str_charAt(char *ptr, aria_int len, aria_int index) {
    if (index < 0 || index >= len) {
        struct _aria_str s = {"", 0};
        return s;
    }
    if (ptr == NULL) {
        fprintf(stderr, "FATAL charAt: ptr=NULL len=%ld idx=%ld\n", len, index);
        exit(97);
    }
    char *result = (char *)malloc(2);
    result[0] = ptr[index];
    result[1] = '\0';
    struct _aria_str s = {result, 1};
    return s;
}

struct _aria_str _aria_str_substring(char *ptr, aria_int len, aria_int start, aria_int end) {
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) {
        struct _aria_str s = {"", 0};
        return s;
    }
    aria_int sub_len = end - start;
    char *result = (char *)malloc((size_t)(sub_len + 1));
    memcpy(result, ptr + start, (size_t)sub_len);
    result[sub_len] = '\0';
    struct _aria_str s = {result, sub_len};
    return s;
}

aria_int _aria_str_contains(char *haystack_ptr, aria_int haystack_len,
                        char *needle_ptr, aria_int needle_len) {
    if (needle_len == 0) return 1;
    if (needle_len > haystack_len) return 0;
    for (aria_int i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack_ptr + i, needle_ptr, (size_t)needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

aria_int _aria_str_startsWith(char *str_ptr, aria_int str_len,
                          char *prefix_ptr, aria_int prefix_len) {
    if (prefix_len > str_len) return 0;
    return memcmp(str_ptr, prefix_ptr, (size_t)prefix_len) == 0 ? 1 : 0;
}

aria_int _aria_str_endsWith(char *str_ptr, aria_int str_len,
                        char *suffix_ptr, aria_int suffix_len) {
    if (suffix_len > str_len) return 0;
    return memcmp(str_ptr + str_len - suffix_len, suffix_ptr, (size_t)suffix_len) == 0 ? 1 : 0;
}

aria_int _aria_str_indexOf(char *str_ptr, aria_int str_len,
                       char *sub_ptr, aria_int sub_len) {
    if (sub_len == 0) return 0;
    if (sub_len > str_len) return -1;
    for (aria_int i = 0; i <= str_len - sub_len; i++) {
        if (memcmp(str_ptr + i, sub_ptr, (size_t)sub_len) == 0) {
            return i;
        }
    }
    return -1;
}

struct _aria_str _aria_str_trim(char *ptr, aria_int len) {
    aria_int start = 0;
    aria_int end = len;
    while (start < end && (ptr[start] == ' ' || ptr[start] == '\t' || ptr[start] == '\n' || ptr[start] == '\r')) {
        start++;
    }
    while (end > start && (ptr[end - 1] == ' ' || ptr[end - 1] == '\t' || ptr[end - 1] == '\n' || ptr[end - 1] == '\r')) {
        end--;
    }
    aria_int new_len = end - start;
    if (new_len == 0) {
        struct _aria_str s = {"", 0};
        return s;
    }
    char *result = (char *)malloc((size_t)(new_len + 1));
    memcpy(result, ptr + start, (size_t)new_len);
    result[new_len] = '\0';
    struct _aria_str s = {result, new_len};
    return s;
}

struct _aria_str _aria_str_replace(char *ptr, aria_int len,
                                   char *old_ptr, aria_int old_len,
                                   char *new_ptr, aria_int new_len) {
    if (old_len == 0) {
        char *result = (char *)malloc((size_t)(len + 1));
        memcpy(result, ptr, (size_t)len);
        result[len] = '\0';
        struct _aria_str s = {result, len};
        return s;
    }
    aria_int count = 0;
    for (aria_int i = 0; i <= len - old_len; i++) {
        if (memcmp(ptr + i, old_ptr, (size_t)old_len) == 0) {
            count++;
            i += old_len - 1;
        }
    }
    if (count == 0) {
        char *result = (char *)malloc((size_t)(len + 1));
        memcpy(result, ptr, (size_t)len);
        result[len] = '\0';
        struct _aria_str s = {result, len};
        return s;
    }
    aria_int result_len = len + count * (new_len - old_len);
    char *result = (char *)malloc((size_t)(result_len + 1));
    aria_int ri = 0;
    aria_int i = 0;
    while (i < len) {
        if (i <= len - old_len && memcmp(ptr + i, old_ptr, (size_t)old_len) == 0) {
            memcpy(result + ri, new_ptr, (size_t)new_len);
            ri += new_len;
            i += old_len;
        } else {
            result[ri++] = ptr[i++];
        }
    }
    result[result_len] = '\0';
    struct _aria_str s = {result, result_len};
    return s;
}

struct _aria_str _aria_str_toLower(char *ptr, aria_int len) {
    char *result = (char *)malloc((size_t)(len + 1));
    for (aria_int i = 0; i < len; i++) {
        char c = ptr[i];
        if (c >= 'A' && c <= 'Z') c = c + 32;
        result[i] = c;
    }
    result[len] = '\0';
    struct _aria_str s = {result, len};
    return s;
}

struct _aria_str _aria_str_toUpper(char *ptr, aria_int len) {
    char *result = (char *)malloc((size_t)(len + 1));
    for (aria_int i = 0; i < len; i++) {
        char c = ptr[i];
        if (c >= 'a' && c <= 'z') c = c - 32;
        result[i] = c;
    }
    result[len] = '\0';
    struct _aria_str s = {result, len};
    return s;
}

aria_int _aria_str_split(char *ptr, aria_int len, char *delim_ptr, aria_int delim_len) {
    aria_int arr = _aria_array_new(8);
    // Sentinel: empty string struct (safe to dereference)
    aria_int *sentinel_str = (aria_int *)malloc(16);
    sentinel_str[0] = (aria_int)"";
    sentinel_str[1] = 0;
    arr = _aria_array_append(arr,(aria_int)sentinel_str);

    if (delim_len == 0) {
        aria_int *str_struct = (aria_int *)malloc(16);
        str_struct[0] = (aria_int)ptr;
        str_struct[1] = len;
        arr = _aria_array_append(arr,(aria_int)str_struct);
        return arr;
    }

    aria_int start = 0;
    aria_int i = 0;
    while (i <= len - delim_len) {
        if (memcmp(ptr + i, delim_ptr, (size_t)delim_len) == 0) {
            aria_int sub_len = i - start;
            char *sub = (char *)malloc((size_t)(sub_len + 1));
            memcpy(sub, ptr + start, (size_t)sub_len);
            sub[sub_len] = '\0';
            aria_int *str_struct = (aria_int *)malloc(16);
            str_struct[0] = (aria_int)sub;
            str_struct[1] = sub_len;
            arr = _aria_array_append(arr,(aria_int)str_struct);
            i += delim_len;
            start = i;
        } else {
            i++;
        }
    }
    aria_int sub_len = len - start;
    char *sub = (char *)malloc((size_t)(sub_len + 1));
    memcpy(sub, ptr + start, (size_t)sub_len);
    sub[sub_len] = '\0';
    aria_int *str_struct = (aria_int *)malloc(16);
    str_struct[0] = (aria_int)sub;
    str_struct[1] = sub_len;
    arr = _aria_array_append(arr,(aria_int)str_struct);
    return arr;
}

void _aria_map_set_str(aria_int map_ptr, aria_int key_ptr, aria_int key_len, aria_int val_ptr, aria_int val_len) {
    aria_int *pair = (aria_int *)malloc(16);
    pair[0] = val_ptr;
    pair[1] = val_len;
    _aria_map_set(map_ptr, key_ptr, key_len, (aria_int)pair);
}

struct _aria_str _aria_map_get_str(aria_int map_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int val = _aria_map_get(map_ptr, key_ptr, key_len);
    if (val == 0) {
        struct _aria_str s = {NULL, 0};
        return s;
    }
    aria_int *pair = (aria_int *)val;
    struct _aria_str s = {(char *)pair[0], pair[1]};
    return s;
}

// --- Array operations ---
// Array layout: [header_ptr] -> { capacity: i64, length: i64, data_ptr: i8* }
// data_ptr -> contiguous i64 elements (sentinel at index 0)

// Allocate a new array with given capacity
// Returns pointer to header as i64
aria_int _aria_array_new(aria_int capacity) {
    // Header: 3 i64s = 24 bytes: [length, capacity, data_ptr]
    aria_int *header = (aria_int *)calloc(1, 24);
    header[0] = 0;         // length (0 = empty)
    header[1] = capacity;  // capacity
    // Allocate data: capacity * 8 bytes for i64 elements
    aria_int *data = (aria_int *)calloc((size_t)(capacity), 8);
    header[2] = (aria_int)data;
    return (aria_int)header;
}

aria_int _aria_array_len(aria_int arr_ptr) {
    if (arr_ptr == 0) return 0;
    aria_int *header = (aria_int *)arr_ptr;
    return header[0];  // length
}

aria_int _aria_array_get(aria_int arr_ptr, aria_int index) {
    aria_int *header = (aria_int *)arr_ptr;
    aria_int length = header[0];
    if (index < 0 || index >= length) {
        fprintf(stderr, "FATAL array_get: index=%ld len=%ld arr=%p\n", index, length, (void *)arr_ptr);
        exit(98);
    }
    aria_int *data = (aria_int *)header[2];
    aria_int val = data[index];
    return val;
}

void _aria_array_set(aria_int arr_ptr, aria_int index, aria_int value) {
    aria_int *header = (aria_int *)arr_ptr;
    aria_int *data = (aria_int *)header[2];
    data[index] = value;
}

// Array append with value semantics: always returns a NEW array.
// Aria code assumes arrays are immutable values — appending to a copy
// must not mutate the original.
aria_int _aria_array_append(aria_int arr_ptr, aria_int value) {
    aria_int *header = (aria_int *)arr_ptr;
    aria_int length = header[0];

    // Always create a new array (copy-on-write semantics)
    aria_int new_cap = length + 1;
    if (new_cap < 8) new_cap = 8;
    aria_int *new_header = (aria_int *)calloc(3, 8);
    aria_int *new_data = (aria_int *)calloc((size_t)new_cap, 8);
    aria_int *old_data = (aria_int *)header[2];
    if (length > 0) {
        memcpy(new_data, old_data, (size_t)(length * 8));
    }
    new_data[length] = value;
    new_header[0] = length + 1;
    new_header[1] = new_cap;
    new_header[2] = (aria_int)new_data;
    return (aria_int)new_header;
}

aria_int _aria_array_slice(aria_int arr_ptr, aria_int start) {
    aria_int *header = (aria_int *)arr_ptr;
    aria_int length = header[0];
    aria_int *old_data = (aria_int *)header[2];
    aria_int new_len = length - start;
    if (new_len < 0) new_len = 0;
    aria_int new_cap = new_len < 4 ? 4 : new_len;
    aria_int *new_header = (aria_int *)calloc(3, 8);
    aria_int *new_data = (aria_int *)calloc((size_t)new_cap, 8);
    if (new_len > 0) {
        memcpy(new_data, old_data + start, (size_t)(new_len * 8));
    }
    new_header[0] = new_len;
    new_header[1] = new_cap;
    new_header[2] = (aria_int)new_data;
    return (aria_int)new_header;
}

// --- Filesystem ---

aria_int _aria_list_dir(char *path_ptr, aria_int path_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    aria_int arr = _aria_array_new(16);
    aria_int *sentinel_str = (aria_int *)malloc(16);
    sentinel_str[0] = (aria_int)"";
    sentinel_str[1] = 0;
    arr = _aria_array_append(arr,(aria_int)sentinel_str);

#ifdef _WIN32
    char search[MAX_PATH];
    snprintf(search, MAX_PATH, "%s\\*", path);
    free(path);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return arr;
    do {
        if (fd.cFileName[0] == '.' && (fd.cFileName[1] == '\0' ||
            (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0'))) continue;
        aria_int name_len = (aria_int)strlen(fd.cFileName);
        char *name = (char *)malloc((size_t)(name_len + 1));
        memcpy(name, fd.cFileName, (size_t)name_len);
        name[name_len] = '\0';
        aria_int *str_struct = (aria_int *)malloc(16);
        str_struct[0] = (aria_int)name;
        str_struct[1] = name_len;
        arr = _aria_array_append(arr,(aria_int)str_struct);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR *dir = opendir(path);
    free(path);
    if (!dir) return arr;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
            (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;
        aria_int name_len = (aria_int)strlen(entry->d_name);
        char *name = (char *)malloc((size_t)(name_len + 1));
        memcpy(name, entry->d_name, (size_t)name_len);
        name[name_len] = '\0';
        aria_int *str_struct = (aria_int *)malloc(16);
        str_struct[0] = (aria_int)name;
        str_struct[1] = name_len;
        arr = _aria_array_append(arr,(aria_int)str_struct);
    }
    closedir(dir);
#endif
    return arr;
}

aria_int _aria_is_dir(char *path_ptr, aria_int path_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    struct stat st;
    int result = stat(path, &st);
    free(path);
    if (result != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

// --- Map (hash table) ---
// Header: [size, capacity, keys_ptr, values_ptr, states_ptr]
// states: 0=empty, 1=occupied, 2=deleted

static unsigned long _fnv_hash_str(char *ptr, aria_int len) {
    unsigned long h = 14695981039346656037ULL;
    for (aria_int i = 0; i < len; i++) {
        h ^= (unsigned char)ptr[i];
        h *= 1099511628211ULL;
    }
    return h;
}

aria_int _aria_map_new(aria_int capacity) {
    if (capacity < 8) capacity = 8;
    aria_int *header = (aria_int *)calloc(1, 40);  // 5 * 8 bytes
    header[0] = 0;          // size
    header[1] = capacity;   // capacity
    header[2] = (aria_int)calloc((size_t)capacity, 8);  // keys
    header[3] = (aria_int)calloc((size_t)capacity, 8);  // values
    header[4] = (aria_int)calloc((size_t)capacity, 1);  // states (1 byte each)
    return (aria_int)header;
}

void _aria_map_set(aria_int map_ptr, aria_int key_ptr, aria_int key_len, aria_int value) {
    aria_int *header = (aria_int *)map_ptr;
    aria_int size = header[0];
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    aria_int *values = (aria_int *)header[3];
    char *states = (char *)header[4];

    // Grow if load factor > 0.7
    if (size * 10 > capacity * 7) {
        aria_int new_cap = capacity * 2;
        aria_int *new_keys = (aria_int *)calloc((size_t)new_cap, 8);
        aria_int *new_values = (aria_int *)calloc((size_t)new_cap, 8);
        char *new_states = (char *)calloc((size_t)new_cap, 1);
        // Rehash
        for (aria_int i = 0; i < capacity; i++) {
            if (states[i] == 1) {
                // Recompute hash for this key
                // Key is stored as [ptr, len] pair: keys[i*2], keys[i*2+1]
                aria_int kp = keys[i * 2];
                aria_int kl = keys[i * 2 + 1];
                unsigned long h = _fnv_hash_str((char *)kp, kl) % (unsigned long)new_cap;
                while (new_states[h] == 1) { h = (h + 1) % (unsigned long)new_cap; }
                new_keys[h * 2] = kp;
                new_keys[h * 2 + 1] = kl;
                new_values[h] = values[i];
                new_states[h] = 1;
            }
        }
        free(keys); free(values); free(states);
        header[1] = new_cap;
        header[2] = (aria_int)new_keys;
        header[3] = (aria_int)new_values;
        header[4] = (aria_int)new_states;
        capacity = new_cap;
        keys = new_keys;
        values = new_values;
        states = new_states;
    }

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    while (states[h] == 1) {
        // Check if same key
        if (keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            // Update existing
            values[h] = value;
            return;
        }
        h = (h + 1) % (unsigned long)capacity;
    }
    keys[h * 2] = key_ptr;
    keys[h * 2 + 1] = key_len;
    values[h] = value;
    states[h] = 1;
    header[0] = size + 1;
}

// Returns value, or 0 if not found. Use _aria_map_contains to check existence.
aria_int _aria_map_get(aria_int map_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int *header = (aria_int *)map_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    aria_int *values = (aria_int *)header[3];
    char *states = (char *)header[4];

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    aria_int probes = 0;
    while (states[h] != 0 && probes < capacity) {
        if (states[h] == 1 && keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            return values[h];
        }
        h = (h + 1) % (unsigned long)capacity;
        probes++;
    }
    return 0;
}

aria_int _aria_map_contains(aria_int map_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int *header = (aria_int *)map_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[4];

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    aria_int probes = 0;
    while (states[h] != 0 && probes < capacity) {
        if (states[h] == 1 && keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            return 1;
        }
        h = (h + 1) % (unsigned long)capacity;
        probes++;
    }
    return 0;
}

aria_int _aria_map_len(aria_int map_ptr) {
    if (map_ptr == 0) return 0;
    aria_int *header = (aria_int *)map_ptr;
    return header[0];
}

// Return array of keys (each key is [ptr, len] pair stored as string array)
aria_int _aria_map_keys(aria_int map_ptr) {
    aria_int *header = (aria_int *)map_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[4];

    aria_int arr = _aria_array_new(header[0] * 2 + 2);
    arr = _aria_array_append(arr,0);  // sentinel
    for (aria_int i = 0; i < capacity; i++) {
        if (states[i] == 1) {
            // Pack key as string: append ptr then len
            aria_int *str_struct = (aria_int *)malloc(16);
            str_struct[0] = keys[i * 2];      // ptr
            str_struct[1] = keys[i * 2 + 1];  // len
            arr = _aria_array_append(arr,(aria_int)str_struct);
        }
    }
    return arr;
}

// --- Set (hash set using same structure as Map, no values) ---
// Header: [size, capacity, keys_ptr, states_ptr]

aria_int _aria_set_new(aria_int capacity) {
    if (capacity < 8) capacity = 8;
    aria_int *header = (aria_int *)calloc(1, 32);  // 4 * 8 bytes
    header[0] = 0;          // size
    header[1] = capacity;   // capacity
    header[2] = (aria_int)calloc((size_t)capacity, 8);  // keys (ptr+len pairs)
    header[3] = (aria_int)calloc((size_t)capacity, 1);  // states
    return (aria_int)header;
}

void _aria_set_add(aria_int set_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int *header = (aria_int *)set_ptr;
    aria_int size = header[0];
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[3];

    if (size * 10 > capacity * 7) {
        aria_int new_cap = capacity * 2;
        aria_int *new_keys = (aria_int *)calloc((size_t)new_cap, 8);
        char *new_states = (char *)calloc((size_t)new_cap, 1);
        for (aria_int i = 0; i < capacity; i++) {
            if (states[i] == 1) {
                aria_int kp = keys[i * 2];
                aria_int kl = keys[i * 2 + 1];
                unsigned long h = _fnv_hash_str((char *)kp, kl) % (unsigned long)new_cap;
                while (new_states[h] == 1) { h = (h + 1) % (unsigned long)new_cap; }
                new_keys[h * 2] = kp;
                new_keys[h * 2 + 1] = kl;
                new_states[h] = 1;
            }
        }
        free(keys); free(states);
        header[1] = new_cap;
        header[2] = (aria_int)new_keys;
        header[3] = (aria_int)new_states;
        capacity = new_cap;
        keys = new_keys;
        states = new_states;
    }

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    while (states[h] == 1) {
        if (keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            return;  // already in set
        }
        h = (h + 1) % (unsigned long)capacity;
    }
    keys[h * 2] = key_ptr;
    keys[h * 2 + 1] = key_len;
    states[h] = 1;
    header[0] = size + 1;
}

aria_int _aria_set_contains(aria_int set_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int *header = (aria_int *)set_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[3];

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    aria_int probes = 0;
    while (states[h] != 0 && probes < capacity) {
        if (states[h] == 1 && keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            return 1;
        }
        h = (h + 1) % (unsigned long)capacity;
        probes++;
    }
    return 0;
}

aria_int _aria_set_len(aria_int set_ptr) {
    if (set_ptr == 0) return 0;
    aria_int *header = (aria_int *)set_ptr;
    return header[0];
}

void _aria_set_remove(aria_int set_ptr, aria_int key_ptr, aria_int key_len) {
    aria_int *header = (aria_int *)set_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[3];

    unsigned long h = _fnv_hash_str((char *)key_ptr, key_len) % (unsigned long)capacity;
    aria_int probes = 0;
    while (states[h] != 0 && probes < capacity) {
        if (states[h] == 1 && keys[h * 2 + 1] == key_len &&
            memcmp((char *)keys[h * 2], (char *)key_ptr, (size_t)key_len) == 0) {
            states[h] = 2;  // tombstone
            header[0] = header[0] - 1;
            return;
        }
        h = (h + 1) % (unsigned long)capacity;
        probes++;
    }
}

aria_int _aria_set_values(aria_int set_ptr) {
    aria_int *header = (aria_int *)set_ptr;
    aria_int capacity = header[1];
    aria_int *keys = (aria_int *)header[2];
    char *states = (char *)header[3];

    aria_int arr = _aria_array_new(header[0] * 2 + 2);
    arr = _aria_array_append(arr,0);  // sentinel
    for (aria_int i = 0; i < capacity; i++) {
        if (states[i] == 1) {
            aria_int *str_struct = (aria_int *)malloc(16);
            str_struct[0] = keys[i * 2];
            str_struct[1] = keys[i * 2 + 1];
            arr = _aria_array_append(arr,(aria_int)str_struct);
        }
    }
    return arr;
}

// --- Command line args ---

static aria_int _aria_args_array = 0;

aria_int _aria_args_get(void) {
    return _aria_args_array;
}

void _aria_args_init(int argc, char **argv) {
    // Build an array of strings matching Aria's [str] layout
    // Each string is represented as two i64s: ptr, len
    // But in the current IR, args are stored as an array of str
    // which the lowerer treats as pairs of (ptr, len) in fields
    //
    // For simplicity, build a simple [str] array where each element
    // is a pair of i64 values (ptr, len) stored in the array's data
    aria_int arr = _aria_array_new(argc * 2 + 2);

    // Sentinel element (index 0) - empty string (use append to update length)
    arr = _aria_array_append(arr,0);  // sentinel

    // For each argv entry, store two elements: ptr and len
    // Actually, looking at how the bootstrap handles _ariaArgs:
    // it returns []string with sentinel at index 0
    // The IR represents this as an array of i64 where each i64
    // is packed or is a pointer to a str struct.
    //
    // Let's match the bootstrap: build array with string representations
    // Each element is a pointer to a 2-word struct {ptr, len}
    for (int i = 0; i < argc; i++) {
        aria_int slen = (aria_int)strlen(argv[i]);
        char *sptr = (char *)malloc((size_t)(slen + 1));
        memcpy(sptr, argv[i], (size_t)(slen + 1));

        // Allocate a 2-word struct for the string
        aria_int *str_struct = (aria_int *)malloc(16);
        str_struct[0] = (aria_int)sptr;
        str_struct[1] = slen;

        // Append the pointer to the struct as the element
        arr = _aria_array_append(arr,(aria_int)str_struct);
    }

    _aria_args_array = arr;
}

// --- Exec ---

aria_int _aria_exec(char *cmd_ptr, aria_int cmd_len) {
    char *cmd = (char *)malloc((size_t)(cmd_len + 1));
    memcpy(cmd, cmd_ptr, (size_t)cmd_len);
    cmd[cmd_len] = '\0';
    int ret = system(cmd);
    free(cmd);
    if (ret == -1) return 1;
    // system() returns the exit status shifted
    return (aria_int)(ret >> 8);  // WEXITSTATUS equivalent
}

// --- Environment ---

struct _aria_str _aria_getenv(char *name_ptr, aria_int name_len) {
    char *name = (char *)malloc((size_t)(name_len + 1));
    memcpy(name, name_ptr, (size_t)name_len);
    name[name_len] = '\0';
    char *val = getenv(name);
    free(name);
    if (val == NULL) {
        struct _aria_str s = {"", 0};
        return s;
    }
    aria_int vlen = (aria_int)strlen(val);
    char *result = (char *)malloc((size_t)(vlen + 1));
    memcpy(result, val, (size_t)(vlen + 1));
    struct _aria_str s = {result, vlen};
    return s;
}

// --- TCP Networking ---

#ifdef _WIN32
// winsock2.h and ws2tcpip.h already included at top of file
#pragma comment(lib, "ws2_32.lib")
#undef close
#define close closesocket
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <errno.h>

// Create a TCP socket. Returns fd or -1 on error.
aria_int _aria_tcp_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    // Enable SO_REUSEADDR to avoid "address already in use"
    int opt = 1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    return (aria_int)fd;
}

// Bind socket to address:port. Returns 0 on success, -1 on error.
aria_int _aria_tcp_bind(aria_int fd, char *addr_ptr, aria_int addr_len, aria_int port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons((uint16_t)port);

    if (addr_len == 0 || (addr_len == 7 && memcmp(addr_ptr, "0.0.0.0", 7) == 0)) {
        sa.sin_addr.s_addr = INADDR_ANY;
    } else {
        char *addr = (char *)malloc((size_t)(addr_len + 1));
        memcpy(addr, addr_ptr, (size_t)addr_len);
        addr[addr_len] = '\0';
        if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
            free(addr);
            return -1;
        }
        free(addr);
    }

    if (bind((int)fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) return -1;
    return 0;
}

// Listen for connections. Returns 0 on success, -1 on error.
aria_int _aria_tcp_listen(aria_int fd, aria_int backlog) {
    if (listen((int)fd, (int)backlog) < 0) return -1;
    return 0;
}

// Accept a connection. Returns new client fd or -1 on error.
aria_int _aria_tcp_accept(aria_int fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept((int)fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) return -1;
    return (aria_int)client_fd;
}

// Read from socket. Returns {ptr, len} or {NULL, 0} on error/closed.
struct _aria_str _aria_tcp_read(aria_int fd, aria_int max_len) {
    char *buf = (char *)malloc((size_t)(max_len + 1));
    ssize_t n = read((int)fd, buf, (size_t)max_len);
    if (n <= 0) {
        free(buf);
        struct _aria_str s = {"", 0};
        return s;
    }
    buf[n] = '\0';
    struct _aria_str s = {buf, (aria_int)n};
    return s;
}

// Write to socket. Returns bytes written or -1 on error.
aria_int _aria_tcp_write(aria_int fd, char *ptr, aria_int len) {
    ssize_t total = 0;
    while (total < len) {
        ssize_t n = write((int)fd, ptr + total, (size_t)(len - total));
        if (n <= 0) return -1;
        total += n;
    }
    return (aria_int)total;
}

// Close socket.
void _aria_tcp_close(aria_int fd) {
    close((int)fd);
}

// Get peer address as string. Returns "ip:port".
struct _aria_str _aria_tcp_peer_addr(aria_int fd) {
    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(sa);
    if (getpeername((int)fd, (struct sockaddr *)&sa, &sa_len) < 0) {
        struct _aria_str s = {"unknown", 7};
        return s;
    }
    char buf[64];
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sa.sin_addr, ip, sizeof(ip));
    int len = snprintf(buf, sizeof(buf), "%s:%d", ip, ntohs(sa.sin_port));
    char *result = (char *)malloc((size_t)(len + 1));
    memcpy(result, buf, (size_t)(len + 1));
    struct _aria_str s = {result, (aria_int)len};
    return s;
}

// Set socket timeout in milliseconds. kind: 0=recv, 1=send
aria_int _aria_tcp_set_timeout(aria_int fd, aria_int kind, aria_int ms) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    int opt = (kind == 0) ? SO_RCVTIMEO : SO_SNDTIMEO;
    if (setsockopt((int)fd, SOL_SOCKET, opt, &tv, sizeof(tv)) < 0) return -1;
    return 0;
}

// --- PostgreSQL (libpq) ---
// Only compiled when ARIA_HAS_LIBPQ is defined (optional dependency)

#ifdef ARIA_HAS_LIBPQ
#include <libpq-fe.h>

// Connect to PostgreSQL. Returns connection handle (cast PGconn* to aria_int).
aria_int _aria_pg_connect(char *ptr, aria_int len) {
    char *connstr = (char *)malloc((size_t)(len + 1));
    memcpy(connstr, ptr, (size_t)len);
    connstr[len] = '\0';
    PGconn *conn = PQconnectdb(connstr);
    free(connstr);
    return (aria_int)conn;
}

// Close connection.
void _aria_pg_close(aria_int conn) {
    if (conn != 0) PQfinish((PGconn *)conn);
}

// Check connection status. Returns 1 if OK, 0 if bad.
aria_int _aria_pg_status(aria_int conn) {
    if (conn == 0) return 0;
    return PQstatus((PGconn *)conn) == CONNECTION_OK ? 1 : 0;
}

// Get error message from connection.
struct _aria_str _aria_pg_error(aria_int conn) {
    if (conn == 0) {
        struct _aria_str s = {"no connection", 13};
        return s;
    }
    char *msg = PQerrorMessage((PGconn *)conn);
    aria_int mlen = (aria_int)strlen(msg);
    char *result = (char *)malloc((size_t)(mlen + 1));
    memcpy(result, msg, (size_t)(mlen + 1));
    struct _aria_str s = {result, mlen};
    return s;
}

// Execute a simple query. Returns result handle.
aria_int _aria_pg_exec(aria_int conn, char *qptr, aria_int qlen) {
    char *query = (char *)malloc((size_t)(qlen + 1));
    memcpy(query, qptr, (size_t)qlen);
    query[qlen] = '\0';
    PGresult *res = PQexec((PGconn *)conn, query);
    free(query);
    return (aria_int)res;
}

// Execute parameterized query. params_arr is an Aria [str] array handle.
aria_int _aria_pg_exec_params(aria_int conn, char *qptr, aria_int qlen, aria_int params_arr) {
    char *query = (char *)malloc((size_t)(qlen + 1));
    memcpy(query, qptr, (size_t)qlen);
    query[qlen] = '\0';

    // Read Aria array: header is [length, capacity, data_ptr]
    aria_int *header = (aria_int *)params_arr;
    aria_int arr_len = header[0];  // includes sentinel at index 0
    aria_int *data = (aria_int *)header[2];

    // Real params start at index 1 (skip sentinel)
    int nparams = (int)(arr_len - 1);
    if (nparams < 0) nparams = 0;

    const char **paramValues = NULL;
    if (nparams > 0) {
        paramValues = (const char **)malloc((size_t)nparams * sizeof(char *));
        for (int i = 0; i < nparams; i++) {
            // Each element is a pointer to a 2-word struct {ptr, len}
            aria_int *str_struct = (aria_int *)data[i + 1];
            char *sptr = (char *)str_struct[0];
            aria_int slen = str_struct[1];
            // Null-terminate for libpq
            char *param = (char *)malloc((size_t)(slen + 1));
            memcpy(param, sptr, (size_t)slen);
            param[slen] = '\0';
            paramValues[i] = param;
        }
    }

    PGresult *res = PQexecParams((PGconn *)conn, query, nparams,
                                  NULL, paramValues, NULL, NULL, 0);
    free(query);
    if (paramValues) {
        for (int i = 0; i < nparams; i++) free((void *)paramValues[i]);
        free(paramValues);
    }
    return (aria_int)res;
}

// Check result status. Returns 0 for PGRES_COMMAND_OK or PGRES_TUPLES_OK.
aria_int _aria_pg_result_status(aria_int result) {
    if (result == 0) return -1;
    ExecStatusType st = PQresultStatus((PGresult *)result);
    if (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK) return 0;
    return (aria_int)st;
}

// Get result error message.
struct _aria_str _aria_pg_result_error(aria_int result) {
    if (result == 0) {
        struct _aria_str s = {"no result", 9};
        return s;
    }
    char *msg = PQresultErrorMessage((PGresult *)result);
    aria_int mlen = (aria_int)strlen(msg);
    char *r = (char *)malloc((size_t)(mlen + 1));
    memcpy(r, msg, (size_t)(mlen + 1));
    struct _aria_str s = {r, mlen};
    return s;
}

// Row count.
aria_int _aria_pg_nrows(aria_int result) {
    if (result == 0) return 0;
    return (aria_int)PQntuples((PGresult *)result);
}

// Column count.
aria_int _aria_pg_ncols(aria_int result) {
    if (result == 0) return 0;
    return (aria_int)PQnfields((PGresult *)result);
}

// Column name.
struct _aria_str _aria_pg_field_name(aria_int result, aria_int col) {
    if (result == 0) {
        struct _aria_str s = {"", 0};
        return s;
    }
    char *name = PQfname((PGresult *)result, (int)col);
    if (name == NULL) {
        struct _aria_str s = {"", 0};
        return s;
    }
    aria_int nlen = (aria_int)strlen(name);
    char *r = (char *)malloc((size_t)(nlen + 1));
    memcpy(r, name, (size_t)(nlen + 1));
    struct _aria_str s = {r, nlen};
    return s;
}

// Get cell value as string.
struct _aria_str _aria_pg_get_value(aria_int result, aria_int row, aria_int col) {
    if (result == 0) {
        struct _aria_str s = {"", 0};
        return s;
    }
    char *val = PQgetvalue((PGresult *)result, (int)row, (int)col);
    if (val == NULL) {
        struct _aria_str s = {"", 0};
        return s;
    }
    aria_int vlen = (aria_int)strlen(val);
    char *r = (char *)malloc((size_t)(vlen + 1));
    memcpy(r, val, (size_t)(vlen + 1));
    struct _aria_str s = {r, vlen};
    return s;
}

// NULL check. Returns 1 if NULL, 0 otherwise.
aria_int _aria_pg_is_null(aria_int result, aria_int row, aria_int col) {
    if (result == 0) return 1;
    return PQgetisnull((PGresult *)result, (int)row, (int)col) ? 1 : 0;
}

// Free result.
void _aria_pg_clear(aria_int result) {
    if (result != 0) PQclear((PGresult *)result);
}

#else  // !ARIA_HAS_LIBPQ — provide stubs so linker doesn't fail
aria_int _aria_pg_connect(char *s, aria_int l) { return 0; }
void _aria_pg_close(aria_int c) {}
aria_int _aria_pg_status(aria_int c) { return -1; }
struct _aria_str _aria_pg_error(aria_int c) { struct _aria_str s = {"no libpq", 8}; return s; }
aria_int _aria_pg_exec(aria_int c, char *q, aria_int ql) { return 0; }
aria_int _aria_pg_exec_params(aria_int c, char *q, aria_int ql, aria_int p) { return 0; }
aria_int _aria_pg_result_status(aria_int r) { return -1; }
struct _aria_str _aria_pg_result_error(aria_int r) { struct _aria_str s = {"no libpq", 8}; return s; }
aria_int _aria_pg_nrows(aria_int r) { return 0; }
aria_int _aria_pg_ncols(aria_int r) { return 0; }
struct _aria_str _aria_pg_field_name(aria_int r, aria_int c) { struct _aria_str s = {"", 0}; return s; }
struct _aria_str _aria_pg_get_value(aria_int r, aria_int row, aria_int col) { struct _aria_str s = {"", 0}; return s; }
aria_int _aria_pg_is_null(aria_int r, aria_int row, aria_int col) { return 1; }
void _aria_pg_clear(aria_int r) {}
#endif  // ARIA_HAS_LIBPQ

// --- Concurrency ---

#ifdef _WIN32
#include <process.h>

struct _aria_spawn_arg {
    aria_int (*fn_ptr)(aria_int);
    aria_int env_ptr;
};

static unsigned __stdcall _aria_spawn_trampoline(void *arg) {
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)arg;
    aria_int result = sa->fn_ptr(sa->env_ptr);
    free(sa);
    return (unsigned)result;
}

aria_int _aria_spawn(aria_int fn_ptr, aria_int env_ptr) {
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)malloc(sizeof(struct _aria_spawn_arg));
    sa->fn_ptr = (aria_int (*)(aria_int))fn_ptr;
    sa->env_ptr = env_ptr;
    uintptr_t th = _beginthreadex(NULL, 0, _aria_spawn_trampoline, sa, 0, NULL);
    if (th == 0) { free(sa); return -1; }
    return (aria_int)th;
}

aria_int _aria_task_await(aria_int handle) {
    if (handle <= 0) return -1;
    WaitForSingleObject((HANDLE)handle, INFINITE);
    DWORD result = 0;
    GetExitCodeThread((HANDLE)handle, &result);
    CloseHandle((HANDLE)handle);
    return (aria_int)result;
}

// Windows channel using CRITICAL_SECTION + CONDITION_VARIABLE
struct _aria_chan {
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv_send;
    CONDITION_VARIABLE cv_recv;
    aria_int *buf;
    aria_int capacity;
    aria_int head;
    aria_int tail;
    aria_int count;
    int closed;
};

aria_int _aria_chan_new(aria_int capacity) {
    if (capacity < 1) capacity = 1;
    struct _aria_chan *ch = (struct _aria_chan *)calloc(1, sizeof(struct _aria_chan));
    InitializeCriticalSection(&ch->cs);
    InitializeConditionVariable(&ch->cv_send);
    InitializeConditionVariable(&ch->cv_recv);
    ch->buf = (aria_int *)calloc((size_t)capacity, sizeof(aria_int));
    ch->capacity = capacity;
    return (aria_int)ch;
}

aria_int _aria_chan_send(aria_int handle, aria_int value) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    EnterCriticalSection(&ch->cs);
    while (ch->count == ch->capacity && !ch->closed)
        SleepConditionVariableCS(&ch->cv_send, &ch->cs, INFINITE);
    if (ch->closed) { LeaveCriticalSection(&ch->cs); return -1; }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    WakeConditionVariable(&ch->cv_recv);
    LeaveCriticalSection(&ch->cs);
    return 0;
}

aria_int _aria_chan_recv(aria_int handle) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    EnterCriticalSection(&ch->cs);
    while (ch->count == 0 && !ch->closed)
        SleepConditionVariableCS(&ch->cv_recv, &ch->cs, INFINITE);
    if (ch->count == 0 && ch->closed) { LeaveCriticalSection(&ch->cs); return 0; }
    aria_int val = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    WakeConditionVariable(&ch->cv_send);
    LeaveCriticalSection(&ch->cs);
    return val;
}

void _aria_chan_close(aria_int handle) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    EnterCriticalSection(&ch->cs);
    ch->closed = 1;
    WakeAllConditionVariable(&ch->cv_send);
    WakeAllConditionVariable(&ch->cv_recv);
    LeaveCriticalSection(&ch->cs);
}

aria_int _aria_mutex_new(void) {
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(cs);
    return (aria_int)cs;
}
void _aria_mutex_lock(aria_int handle) { EnterCriticalSection((CRITICAL_SECTION *)handle); }
void _aria_mutex_unlock(aria_int handle) { LeaveCriticalSection((CRITICAL_SECTION *)handle); }

#else  // POSIX

#include <pthread.h>

// Spawn thread argument struct
struct _aria_spawn_arg {
    aria_int (*fn_ptr)(aria_int);
    aria_int env_ptr;
};

static void *_aria_spawn_trampoline(void *arg) {
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)arg;
    aria_int result = sa->fn_ptr(sa->env_ptr);
    free(sa);
    return (void *)result;
}

// Spawn a new thread running closure (fn_ptr, env_ptr). Returns task handle.
aria_int _aria_spawn(aria_int fn_ptr, aria_int env_ptr) {
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)malloc(sizeof(struct _aria_spawn_arg));
    sa->fn_ptr = (aria_int (*)(aria_int))fn_ptr;
    sa->env_ptr = env_ptr;
    pthread_t *th = (pthread_t *)malloc(sizeof(pthread_t));
    if (pthread_create(th, NULL, _aria_spawn_trampoline, sa) != 0) {
        free(sa);
        free(th);
        return -1;
    }
    return (aria_int)th;
}

// Wait for task to finish, return its result.
aria_int _aria_task_await(aria_int handle) {
    if (handle <= 0) return -1;
    pthread_t *th = (pthread_t *)handle;
    void *retval = NULL;
    pthread_join(*th, &retval);
    free(th);
    return (aria_int)retval;
}

// --- Channel (mutex-protected ring buffer) ---
// Layout: [mutex, cond_send, cond_recv, buf_ptr, capacity, head, tail, count, closed]

struct _aria_chan {
    pthread_mutex_t mutex;
    pthread_cond_t cond_send;
    pthread_cond_t cond_recv;
    aria_int *buf;
    aria_int capacity;
    aria_int head;
    aria_int tail;
    aria_int count;
    int closed;
};

aria_int _aria_chan_new(aria_int capacity) {
    if (capacity < 1) capacity = 1;
    struct _aria_chan *ch = (struct _aria_chan *)calloc(1, sizeof(struct _aria_chan));
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_send, NULL);
    pthread_cond_init(&ch->cond_recv, NULL);
    ch->buf = (aria_int *)calloc((size_t)capacity, sizeof(aria_int));
    ch->capacity = capacity;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = 0;
    return (aria_int)ch;
}

// Send value to channel. Returns 0 on success, -1 if closed.
aria_int _aria_chan_send(aria_int handle, aria_int value) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == ch->capacity && !ch->closed) {
        pthread_cond_wait(&ch->cond_send, &ch->mutex);
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return -1;
    }
    ch->buf[ch->tail] = value;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
    return 0;
}

// Receive value from channel. Returns value, or 0 if closed+empty.
aria_int _aria_chan_recv(aria_int handle) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        pthread_cond_wait(&ch->cond_recv, &ch->mutex);
    }
    if (ch->count == 0 && ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return 0;
    }
    aria_int value = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_send);
    pthread_mutex_unlock(&ch->mutex);
    return value;
}

void _aria_chan_close(aria_int handle) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = 1;
    pthread_cond_broadcast(&ch->cond_send);
    pthread_cond_broadcast(&ch->cond_recv);
    pthread_mutex_unlock(&ch->mutex);
}

// --- Mutex ---

aria_int _aria_mutex_new(void) {
    pthread_mutex_t *m = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    return (aria_int)m;
}

void _aria_mutex_lock(aria_int handle) {
    pthread_mutex_lock((pthread_mutex_t *)handle);
}

void _aria_mutex_unlock(aria_int handle) {
    pthread_mutex_unlock((pthread_mutex_t *)handle);
}

// --- RWMutex ---

aria_int _aria_rwmutex_new(void) {
    pthread_rwlock_t *rw = (pthread_rwlock_t *)malloc(sizeof(pthread_rwlock_t));
    pthread_rwlock_init(rw, NULL);
    return (aria_int)rw;
}

void _aria_rwmutex_rlock(aria_int handle) {
    pthread_rwlock_rdlock((pthread_rwlock_t *)handle);
}

void _aria_rwmutex_runlock(aria_int handle) {
    pthread_rwlock_unlock((pthread_rwlock_t *)handle);
}

void _aria_rwmutex_wlock(aria_int handle) {
    pthread_rwlock_wrlock((pthread_rwlock_t *)handle);
}

void _aria_rwmutex_wunlock(aria_int handle) {
    pthread_rwlock_unlock((pthread_rwlock_t *)handle);
}

// --- WaitGroup (atomic counter + condvar) ---

struct _aria_wg {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    aria_int count;
};

aria_int _aria_wg_new(void) {
    struct _aria_wg *wg = (struct _aria_wg *)calloc(1, sizeof(struct _aria_wg));
    pthread_mutex_init(&wg->mutex, NULL);
    pthread_cond_init(&wg->cond, NULL);
    wg->count = 0;
    return (aria_int)wg;
}

void _aria_wg_add(aria_int handle, aria_int delta) {
    struct _aria_wg *wg = (struct _aria_wg *)handle;
    pthread_mutex_lock(&wg->mutex);
    wg->count += delta;
    if (wg->count <= 0) pthread_cond_broadcast(&wg->cond);
    pthread_mutex_unlock(&wg->mutex);
}

void _aria_wg_done(aria_int handle) {
    _aria_wg_add(handle, -1);
}

void _aria_wg_wait(aria_int handle) {
    struct _aria_wg *wg = (struct _aria_wg *)handle;
    pthread_mutex_lock(&wg->mutex);
    while (wg->count > 0) {
        pthread_cond_wait(&wg->cond, &wg->mutex);
    }
    pthread_mutex_unlock(&wg->mutex);
}

// --- Once ---

struct _aria_once {
    pthread_once_t once;
    aria_int (*fn_ptr)(aria_int);
    aria_int env_ptr;
    aria_int result;
};

static struct _aria_once *_once_current = NULL;

static void _aria_once_trampoline(void) {
    if (_once_current) {
        _once_current->result = _once_current->fn_ptr(_once_current->env_ptr);
    }
}

aria_int _aria_once_new(void) {
    struct _aria_once *o = (struct _aria_once *)calloc(1, sizeof(struct _aria_once));
    pthread_once_t init = PTHREAD_ONCE_INIT;
    o->once = init;
    o->fn_ptr = NULL;
    o->env_ptr = 0;
    o->result = 0;
    return (aria_int)o;
}

aria_int _aria_once_call(aria_int handle, aria_int fn_ptr, aria_int env_ptr) {
    struct _aria_once *o = (struct _aria_once *)handle;
    o->fn_ptr = (aria_int (*)(aria_int))fn_ptr;
    o->env_ptr = env_ptr;
    _once_current = o;
    pthread_once(&o->once, _aria_once_trampoline);
    return o->result;
}

// --- Channel: try_recv (non-blocking) ---
// Returns {value, status} packed as 2-word struct.
// status=1: got value, status=0: channel closed+empty or empty

struct _aria_recv_result {
    aria_int value;
    aria_int status;
};

struct _aria_recv_result _aria_chan_try_recv(aria_int handle) {
    struct _aria_chan *ch = (struct _aria_chan *)handle;
    struct _aria_recv_result r = {0, 0};
    pthread_mutex_lock(&ch->mutex);
    if (ch->count > 0) {
        r.value = ch->buf[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        r.status = 1;
        pthread_cond_signal(&ch->cond_send);
    }
    pthread_mutex_unlock(&ch->mutex);
    return r;
}

// --- Channel: select (poll multiple channels) ---
// Polls n channels, returns index of first ready one + received value.
// channels_arr is an Aria array handle containing channel handles.
// timeout_ms: -1=block forever, 0=non-blocking

struct _aria_select_result {
    aria_int index;
    aria_int value;
};

struct _aria_select_result _aria_chan_select(aria_int channels_arr, aria_int timeout_ms) {
    struct _aria_select_result r = {-1, 0};
    aria_int *arr_header = (aria_int *)channels_arr;
    aria_int n = arr_header[0];
    aria_int *data = (aria_int *)arr_header[2];

    // Non-blocking mode
    if (timeout_ms == 0) {
        for (aria_int i = 0; i < n; i++) {
            struct _aria_recv_result rr = _aria_chan_try_recv(data[i]);
            if (rr.status) {
                r.index = i;
                r.value = rr.value;
                return r;
            }
        }
        return r;  // index=-1 means default arm
    }

    // Blocking mode: poll with short sleeps
    // (A proper implementation would use a shared condvar across channels)
    aria_int max_iters = (timeout_ms < 0) ? 1000000000L : (timeout_ms * 1000);
    aria_int iter = 0;
    while (iter < max_iters) {
        for (aria_int i = 0; i < n; i++) {
            struct _aria_recv_result rr = _aria_chan_try_recv(data[i]);
            if (rr.status) {
                r.index = i;
                r.value = rr.value;
                return r;
            }
        }
        // Check if all channels are closed
        int all_closed = 1;
        for (aria_int i = 0; i < n; i++) {
            struct _aria_chan *ch = (struct _aria_chan *)data[i];
            if (!ch->closed || ch->count > 0) {
                all_closed = 0;
                break;
            }
        }
        if (all_closed) return r;
        usleep(100);  // 100us between polls
        iter += 100;
    }
    return r;  // timeout
}

// --- Task: done check (non-blocking) ---
// Since macOS lacks pthread_tryjoin_np, we use a wrapper struct approach.

struct _aria_task {
    pthread_t thread;
    aria_int result;
    volatile int done;
    volatile int cancelled;
};

static void *_aria_task_trampoline(void *arg) {
    struct _aria_task *task = (struct _aria_task *)arg;
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)task->result;  // reuse result field temporarily
    aria_int (*fn_ptr)(aria_int) = sa->fn_ptr;
    aria_int env = sa->env_ptr;
    free(sa);
    task->result = fn_ptr(env);
    task->done = 1;
    return (void *)task->result;
}

aria_int _aria_spawn2(aria_int fn_ptr, aria_int env_ptr) {
    struct _aria_task *task = (struct _aria_task *)calloc(1, sizeof(struct _aria_task));
    struct _aria_spawn_arg *sa = (struct _aria_spawn_arg *)malloc(sizeof(struct _aria_spawn_arg));
    sa->fn_ptr = (aria_int (*)(aria_int))fn_ptr;
    sa->env_ptr = env_ptr;
    task->result = (aria_int)sa;
    task->done = 0;
    task->cancelled = 0;
    if (pthread_create(&task->thread, NULL, _aria_task_trampoline, task) != 0) {
        free(sa);
        free(task);
        return -1;
    }
    return (aria_int)task;
}

aria_int _aria_task_await2(aria_int handle) {
    if (handle <= 0) return -1;
    struct _aria_task *task = (struct _aria_task *)handle;
    pthread_join(task->thread, NULL);
    aria_int r = task->result;
    free(task);
    return r;
}

aria_int _aria_task_done(aria_int handle) {
    if (handle <= 0) return 1;
    struct _aria_task *task = (struct _aria_task *)handle;
    return task->done ? 1 : 0;
}

void _aria_task_cancel(aria_int handle) {
    if (handle <= 0) return;
    struct _aria_task *task = (struct _aria_task *)handle;
    task->cancelled = 1;
}

aria_int _aria_task_result(aria_int handle) {
    if (handle <= 0) return 0;
    struct _aria_task *task = (struct _aria_task *)handle;
    if (!task->done) return 0;
    return task->result;
}

aria_int _aria_cancel_check(aria_int handle) {
    if (handle <= 0) return 0;
    struct _aria_task *task = (struct _aria_task *)handle;
    return task->cancelled ? 1 : 0;
}

// --- String: charCount (count UTF-8 codepoints) ---

aria_int _aria_str_char_count(char *ptr, aria_int len) {
    aria_int count = 0;
    aria_int i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)ptr[i];
        if (c < 0x80) { i += 1; }
        else if ((c & 0xE0) == 0xC0) { i += 2; }
        else if ((c & 0xF0) == 0xE0) { i += 3; }
        else { i += 4; }
        count++;
    }
    return count;
}

// --- String: chars (return array of codepoint integers) ---

aria_int _aria_str_chars(char *ptr, aria_int len) {
    aria_int arr = _aria_array_new(len < 8 ? 8 : len);
    // sentinel
    arr = _aria_array_append(arr,0);
    aria_int i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)ptr[i];
        aria_int codepoint = 0;
        aria_int bytes = 1;
        if (c < 0x80) {
            codepoint = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = c & 0x0F;
            bytes = 3;
        } else {
            codepoint = c & 0x07;
            bytes = 4;
        }
        for (aria_int j = 1; j < bytes && (i + j) < len; j++) {
            codepoint = (codepoint << 6) | ((unsigned char)ptr[i + j] & 0x3F);
        }
        arr = _aria_array_append(arr,codepoint);
        i += bytes;
    }
    return arr;
}

// --- String: graphemes (simplified — split on codepoint boundaries) ---
// Full grapheme clustering requires UAX#29. This returns codepoints as single-char strings.

aria_int _aria_str_graphemes(char *ptr, aria_int len) {
    aria_int arr = _aria_array_new(len < 8 ? 8 : len);
    // sentinel
    aria_int *sentinel = (aria_int *)malloc(16);
    sentinel[0] = (aria_int)"";
    sentinel[1] = 0;
    arr = _aria_array_append(arr,(aria_int)sentinel);
    aria_int i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)ptr[i];
        aria_int bytes = 1;
        if (c < 0x80) bytes = 1;
        else if ((c & 0xE0) == 0xC0) bytes = 2;
        else if ((c & 0xF0) == 0xE0) bytes = 3;
        else bytes = 4;
        if (i + bytes > len) bytes = len - i;
        char *gc = (char *)malloc((size_t)(bytes + 1));
        memcpy(gc, ptr + i, (size_t)bytes);
        gc[bytes] = '\0';
        aria_int *str_struct = (aria_int *)malloc(16);
        str_struct[0] = (aria_int)gc;
        str_struct[1] = bytes;
        arr = _aria_array_append(arr,(aria_int)str_struct);
        i += bytes;
    }
    return arr;
}

// --- StringBuilder ---

struct _aria_sb {
    char *buf;
    aria_int len;
    aria_int cap;
};

aria_int _aria_sb_new(void) {
    struct _aria_sb *sb = (struct _aria_sb *)malloc(sizeof(struct _aria_sb));
    sb->cap = 64;
    sb->buf = (char *)malloc((size_t)sb->cap);
    sb->len = 0;
    return (aria_int)sb;
}

aria_int _aria_sb_with_capacity(aria_int cap) {
    struct _aria_sb *sb = (struct _aria_sb *)malloc(sizeof(struct _aria_sb));
    sb->cap = cap < 16 ? 16 : cap;
    sb->buf = (char *)malloc((size_t)sb->cap);
    sb->len = 0;
    return (aria_int)sb;
}

void _aria_sb_append(aria_int handle, char *ptr, aria_int len) {
    struct _aria_sb *sb = (struct _aria_sb *)handle;
    while (sb->len + len > sb->cap) {
        sb->cap *= 2;
        sb->buf = (char *)realloc(sb->buf, (size_t)sb->cap);
    }
    memcpy(sb->buf + sb->len, ptr, (size_t)len);
    sb->len += len;
}

void _aria_sb_append_char(aria_int handle, aria_int codepoint) {
    char buf[4];
    aria_int bytes = 0;
    if (codepoint < 0x80) { buf[0] = (char)codepoint; bytes = 1; }
    else if (codepoint < 0x800) { buf[0] = (char)(0xC0 | (codepoint >> 6)); buf[1] = (char)(0x80 | (codepoint & 0x3F)); bytes = 2; }
    else if (codepoint < 0x10000) { buf[0] = (char)(0xE0 | (codepoint >> 12)); buf[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F)); buf[2] = (char)(0x80 | (codepoint & 0x3F)); bytes = 3; }
    else { buf[0] = (char)(0xF0 | (codepoint >> 18)); buf[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F)); buf[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F)); buf[3] = (char)(0x80 | (codepoint & 0x3F)); bytes = 4; }
    _aria_sb_append(handle, buf, bytes);
}

aria_int _aria_sb_len(aria_int handle) {
    struct _aria_sb *sb = (struct _aria_sb *)handle;
    return sb->len;
}

struct _aria_str _aria_sb_build(aria_int handle) {
    struct _aria_sb *sb = (struct _aria_sb *)handle;
    char *result = (char *)malloc((size_t)(sb->len + 1));
    memcpy(result, sb->buf, (size_t)sb->len);
    result[sb->len] = '\0';
    struct _aria_str s = {result, sb->len};
    // Reset builder
    sb->len = 0;
    return s;
}

void _aria_sb_clear(aria_int handle) {
    struct _aria_sb *sb = (struct _aria_sb *)handle;
    sb->len = 0;
}

// --- String: format specifiers ---
// Simple implementation: handles precision, width, hex, padding

struct _aria_str _aria_format_int(aria_int value, char *spec_ptr, aria_int spec_len) {
    char buf[128];
    // Parse spec: [fill][align][sign][#][0][width][.precision][type]
    // For MVP: support #x (hex), #b (binary), #o (octal), width, 0-padding
    char fmt_type = 'd';
    int width = 0;
    int use_alt = 0;
    char fill = ' ';
    for (aria_int i = 0; i < spec_len; i++) {
        char c = spec_ptr[i];
        if (c == '#') use_alt = 1;
        else if (c == '0' && width == 0) fill = '0';
        else if (c >= '1' && c <= '9') width = width * 10 + (c - '0');
        else if (c == 'x' || c == 'X' || c == 'b' || c == 'o' || c == 'd') fmt_type = c;
    }
    int len = 0;
    if (fmt_type == 'x') {
        if (use_alt) len = snprintf(buf, 128, "0x%lx", (unsigned long)value);
        else len = snprintf(buf, 128, "%lx", (unsigned long)value);
    } else if (fmt_type == 'X') {
        if (use_alt) len = snprintf(buf, 128, "0x%lX", (unsigned long)value);
        else len = snprintf(buf, 128, "%lX", (unsigned long)value);
    } else if (fmt_type == 'o') {
        if (use_alt) len = snprintf(buf, 128, "0o%lo", (unsigned long)value);
        else len = snprintf(buf, 128, "%lo", (unsigned long)value);
    } else if (fmt_type == 'b') {
        // Binary formatting
        char bbuf[66];
        int bi = 0;
        unsigned long uv = (unsigned long)value;
        if (uv == 0) { bbuf[bi++] = '0'; }
        else { while (uv > 0) { bbuf[bi++] = (uv & 1) ? '1' : '0'; uv >>= 1; } }
        // Reverse
        for (int j = 0; j < bi / 2; j++) { char t = bbuf[j]; bbuf[j] = bbuf[bi - 1 - j]; bbuf[bi - 1 - j] = t; }
        bbuf[bi] = '\0';
        if (use_alt) len = snprintf(buf, 128, "0b%s", bbuf);
        else len = snprintf(buf, 128, "%s", bbuf);
    } else {
        len = snprintf(buf, 128, "%ld", value);
    }
    // Apply width padding
    if (width > len) {
        char padded[128];
        int pad = width - len;
        for (int j = 0; j < pad; j++) padded[j] = fill;
        memcpy(padded + pad, buf, (size_t)len);
        padded[width] = '\0';
        char *r = (char *)malloc((size_t)(width + 1));
        memcpy(r, padded, (size_t)(width + 1));
        struct _aria_str s = {r, width};
        return s;
    }
    char *r = (char *)malloc((size_t)(len + 1));
    memcpy(r, buf, (size_t)(len + 1));
    struct _aria_str s = {r, len};
    return s;
}

struct _aria_str _aria_format_float(aria_int bits, char *spec_ptr, aria_int spec_len) {
    double value;
    memcpy(&value, &bits, sizeof(double));
    char buf[128];
    int precision = -1;
    char fmt_type = 'f';
    for (aria_int i = 0; i < spec_len; i++) {
        char c = spec_ptr[i];
        if (c == '.') {
            precision = 0;
            for (aria_int j = i + 1; j < spec_len && spec_ptr[j] >= '0' && spec_ptr[j] <= '9'; j++) {
                precision = precision * 10 + (spec_ptr[j] - '0');
            }
        }
        if (c == 'e' || c == 'E') fmt_type = c;
        if (c == '%') fmt_type = '%';
    }
    int len = 0;
    if (fmt_type == '%') {
        if (precision >= 0) len = snprintf(buf, 128, "%.*f%%", precision, value * 100.0);
        else len = snprintf(buf, 128, "%.1f%%", value * 100.0);
    } else if (fmt_type == 'e') {
        if (precision >= 0) len = snprintf(buf, 128, "%.*e", precision, value);
        else len = snprintf(buf, 128, "%e", value);
    } else if (fmt_type == 'E') {
        if (precision >= 0) len = snprintf(buf, 128, "%.*E", precision, value);
        else len = snprintf(buf, 128, "%E", value);
    } else {
        if (precision >= 0) len = snprintf(buf, 128, "%.*f", precision, value);
        else len = snprintf(buf, 128, "%g", value);
    }
    char *r = (char *)malloc((size_t)(len + 1));
    memcpy(r, buf, (size_t)(len + 1));
    struct _aria_str s = {r, len};
    return s;
}

// --- Cancellation Token ---
// Hierarchical: child tokens are triggered when parent is triggered.

struct _aria_cancel_token {
    volatile int triggered;
    struct _aria_cancel_token *parent;
};

aria_int _aria_cancel_new(void) {
    struct _aria_cancel_token *ct = (struct _aria_cancel_token *)calloc(1, sizeof(struct _aria_cancel_token));
    ct->triggered = 0;
    ct->parent = NULL;
    return (aria_int)ct;
}

aria_int _aria_cancel_child(aria_int parent_handle) {
    struct _aria_cancel_token *child = (struct _aria_cancel_token *)calloc(1, sizeof(struct _aria_cancel_token));
    child->triggered = 0;
    child->parent = (struct _aria_cancel_token *)parent_handle;
    return (aria_int)child;
}

void _aria_cancel_trigger(aria_int handle) {
    struct _aria_cancel_token *ct = (struct _aria_cancel_token *)handle;
    ct->triggered = 1;
}

aria_int _aria_cancel_is_triggered(aria_int handle) {
    struct _aria_cancel_token *ct = (struct _aria_cancel_token *)handle;
    // Walk up parent chain — if any ancestor is triggered, we're triggered
    while (ct != NULL) {
        if (ct->triggered) return 1;
        ct = ct->parent;
    }
    return 0;
}

#endif  // _WIN32 / POSIX concurrency

// --- Entry point ---

extern void _aria_entry(void);

int main(int argc, char **argv) {
    _aria_args_init(argc, argv);
    _aria_entry();
    return 0;
}
