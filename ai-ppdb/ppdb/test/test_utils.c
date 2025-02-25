#include "test_utils.h"
#include "internal/infra/infra_core.h"

void test_init_logger(void) {
    ppdb_logger_init(PPDB_LOG_DEBUG);
}

void test_cleanup_logger(void) {
    ppdb_logger_cleanup();
}

char* test_create_temp_dir(void) {
    char template[] = "ppdb_test_XXXXXX";
    char* dir_path = _mktemp(template);
    if (!dir_path) {
        return NULL;
    }
    if (_mkdir(dir_path) != 0) {
        return NULL;
    }
    return infra_strdup(dir_path);
}

void test_remove_dir(const char* dir_path) {
    char search_path[PATH_MAX];
    infra_snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (infra_strcmp(find_data.cFileName, ".") == 0 || infra_strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        char path[PATH_MAX];
        infra_snprintf(path, sizeof(path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            test_remove_dir(path);
        } else {
            DeleteFile(path);
        }
    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);
    RemoveDirectory(dir_path);
}

void test_generate_random_data(void* buffer, size_t size) {
    unsigned char* p = buffer;
    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)(infra_random() % 256);
    }
}

void test_generate_random_string(char* buffer, size_t size) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const size_t charset_size = sizeof(charset) - 1;

    if (size > 0) {
        size_t i;
        for (i = 0; i < size - 1; i++) {
            buffer[i] = charset[infra_random() % charset_size];
        }
        buffer[i] = '\0';
    }
}

bool test_compare_memory(const void* a, const void* b, size_t size) {
    return infra_memcmp(a, b, size) == 0;
}

bool test_file_exists(const char* path) {
    DWORD attrs = GetFileAttributes(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
}

size_t test_file_size(const char* path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesEx(path, GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    LARGE_INTEGER size;
    size.HighPart = attrs.nFileSizeHigh;
    size.LowPart = attrs.nFileSizeLow;
    return (size_t)size.QuadPart;
}

bool test_is_directory(const char* path) {
    DWORD attrs = GetFileAttributes(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

uint64_t test_get_current_time_us(void) {
    return infra_get_time_us();
}

void test_sleep_us(uint64_t microseconds) {
    infra_sleep_ms((uint32_t)((microseconds + 999) / 1000));
} 