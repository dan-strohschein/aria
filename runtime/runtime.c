// Aria Runtime — C implementation
// Replaces hand-assembled ARM64 runtime functions.
// Linked with LLVM IR output by clang.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

// --- Exit ---

void _aria_exit(long code) {
    exit((int)code);
}

// --- I/O ---

long _aria_write(long fd, char *ptr, long len) {
    return (long)write((int)fd, ptr, (size_t)len);
}

void _aria_println_str(char *ptr, long len) {
    write(1, ptr, (size_t)len);
    write(1, "\n", 1);
}

void _aria_print_str(char *ptr, long len) {
    write(1, ptr, (size_t)len);
}

void _aria_eprintln_str(char *ptr, long len) {
    write(2, ptr, (size_t)len);
    write(2, "\n", 1);
}

// --- Memory ---

char *_aria_alloc(long size) {
    return (char *)calloc(1, (size_t)size);
}

void _aria_memcpy(char *dst, char *src, long len) {
    memcpy(dst, src, (size_t)len);
}

// --- File I/O ---

// Returns {char* ptr, long len} as a struct in registers (ARM64: X0, X1)
struct _aria_str {
    char *ptr;
    long len;
};

struct _aria_str _aria_read_file(char *path_ptr, long path_len) {
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
    long size = (long)st.st_size;

    char *buf = (char *)malloc((size_t)(size + 1));
    read(fd, buf, (size_t)size);
    buf[size] = '\0';
    close(fd);

    struct _aria_str result = {buf, size};
    return result;
}

void _aria_write_file(char *path_ptr, long path_len, char *data_ptr, long data_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    free(path);
    if (fd < 0) return;
    write(fd, data_ptr, (size_t)data_len);
    close(fd);
}

void _aria_write_binary_file(char *path_ptr, long path_len, long *data_arr, long data_len) {
    char *path = (char *)malloc((size_t)(path_len + 1));
    memcpy(path, path_ptr, (size_t)path_len);
    path[path_len] = '\0';

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    free(path);
    if (fd < 0) return;

    // data_arr is a sentinel array: element 0 is dummy, real data starts at index 1
    // Each i64 element holds one byte in its low 8 bits
    char *bytes = (char *)malloc((size_t)data_len);
    for (long i = 0; i < data_len; i++) {
        bytes[i] = (char)(data_arr[i + 1] & 0xFF);
    }
    write(fd, bytes, (size_t)data_len);
    free(bytes);
    close(fd);
}

// --- Integer to string ---

struct _aria_str _aria_int_to_str(long value) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%ld", value);
    char *result = (char *)malloc((size_t)(len + 1));
    memcpy(result, buf, (size_t)(len + 1));
    struct _aria_str s = {result, (long)len};
    return s;
}

// --- String operations ---

struct _aria_str _aria_str_concat(char *a_ptr, long a_len, char *b_ptr, long b_len) {
    long total = a_len + b_len;
    char *result = (char *)malloc((size_t)(total + 1));
    memcpy(result, a_ptr, (size_t)a_len);
    memcpy(result + a_len, b_ptr, (size_t)b_len);
    result[total] = '\0';
    struct _aria_str s = {result, total};
    return s;
}

long _aria_str_eq(char *a_ptr, long a_len, char *b_ptr, long b_len) {
    if (a_len != b_len) return 0;
    return memcmp(a_ptr, b_ptr, (size_t)a_len) == 0 ? 1 : 0;
}

struct _aria_str _aria_str_charAt(char *ptr, long len, long index) {
    if (index < 0 || index >= len) {
        struct _aria_str s = {"", 0};
        return s;
    }
    char *result = (char *)malloc(2);
    result[0] = ptr[index];
    result[1] = '\0';
    struct _aria_str s = {result, 1};
    return s;
}

struct _aria_str _aria_str_substring(char *ptr, long len, long start, long end) {
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) {
        struct _aria_str s = {"", 0};
        return s;
    }
    long sub_len = end - start;
    char *result = (char *)malloc((size_t)(sub_len + 1));
    memcpy(result, ptr + start, (size_t)sub_len);
    result[sub_len] = '\0';
    struct _aria_str s = {result, sub_len};
    return s;
}

long _aria_str_contains(char *haystack_ptr, long haystack_len,
                        char *needle_ptr, long needle_len) {
    if (needle_len == 0) return 1;
    if (needle_len > haystack_len) return 0;
    for (long i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(haystack_ptr + i, needle_ptr, (size_t)needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

long _aria_str_startsWith(char *str_ptr, long str_len,
                          char *prefix_ptr, long prefix_len) {
    if (prefix_len > str_len) return 0;
    return memcmp(str_ptr, prefix_ptr, (size_t)prefix_len) == 0 ? 1 : 0;
}

// --- Array operations ---
// Array layout: [header_ptr] -> { capacity: i64, length: i64, data_ptr: i8* }
// data_ptr -> contiguous i64 elements (sentinel at index 0)

// Allocate a new array with given capacity
// Returns pointer to header as i64
long _aria_array_new(long capacity) {
    // Header: 3 i64s = 24 bytes: [length, capacity, data_ptr]
    long *header = (long *)calloc(1, 24);
    header[0] = 0;         // length (0 = empty)
    header[1] = capacity;  // capacity
    // Allocate data: capacity * 8 bytes for i64 elements
    long *data = (long *)calloc((size_t)(capacity), 8);
    header[2] = (long)data;
    return (long)header;
}

long _aria_array_len(long arr_ptr) {
    if (arr_ptr == 0) return 0;
    long *header = (long *)arr_ptr;
    return header[0];  // length
}

long _aria_array_get(long arr_ptr, long index) {
    long *header = (long *)arr_ptr;
    long *data = (long *)header[2];
    return data[index];
}

void _aria_array_set(long arr_ptr, long index, long value) {
    long *header = (long *)arr_ptr;
    long *data = (long *)header[2];
    data[index] = value;
}

// Append returns the array pointer (may reallocate data)
long _aria_array_append(long arr_ptr, long value) {
    long *header = (long *)arr_ptr;
    long length = header[0];
    long capacity = header[1];

    if (length >= capacity) {
        // Grow: double capacity
        long new_cap = capacity < 8 ? 8 : capacity * 2;
        long *new_data = (long *)calloc((size_t)(new_cap), 8);
        long *old_data = (long *)header[2];
        memcpy(new_data, old_data, (size_t)(length * 8));
        free(old_data);
        header[1] = new_cap;
        header[2] = (long)new_data;
    }

    long *data = (long *)header[2];
    data[length] = value;
    header[0] = length + 1;
    return arr_ptr;
}

// --- Command line args ---

static long _aria_args_array = 0;

long _aria_args_get(void) {
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
    long arr = _aria_array_new(argc * 2 + 2);

    // Sentinel element (index 0) - empty string
    _aria_array_set(arr, 0, 0);  // sentinel

    // For each argv entry, store two elements: ptr and len
    // Actually, looking at how the bootstrap handles _ariaArgs:
    // it returns []string with sentinel at index 0
    // The IR represents this as an array of i64 where each i64
    // is packed or is a pointer to a str struct.
    //
    // Let's match the bootstrap: build array with string representations
    // Each element is a pointer to a 2-word struct {ptr, len}
    for (int i = 0; i < argc; i++) {
        long slen = (long)strlen(argv[i]);
        char *sptr = (char *)malloc((size_t)(slen + 1));
        memcpy(sptr, argv[i], (size_t)(slen + 1));

        // Allocate a 2-word struct for the string
        long *str_struct = (long *)malloc(16);
        str_struct[0] = (long)sptr;
        str_struct[1] = slen;

        // Append the pointer to the struct as the element
        _aria_array_append(arr, (long)str_struct);
    }

    _aria_args_array = arr;
}

// --- Exec ---

long _aria_exec(char *cmd_ptr, long cmd_len) {
    char *cmd = (char *)malloc((size_t)(cmd_len + 1));
    memcpy(cmd, cmd_ptr, (size_t)cmd_len);
    cmd[cmd_len] = '\0';
    int ret = system(cmd);
    free(cmd);
    if (ret == -1) return 1;
    // system() returns the exit status shifted
    return (long)(ret >> 8);  // WEXITSTATUS equivalent
}

// --- Entry point ---

extern void _aria_entry(void);

int main(int argc, char **argv) {
    _aria_args_init(argc, argv);
    _aria_entry();
    return 0;
}
