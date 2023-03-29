#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_THREAD_COUNT 10

#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
    #include <io.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

typedef struct {
    char *buffer;
    size_t length;
    char *keyword;
    int id;
    int count;
} search_thread_arg_t;

void *search_thread(void *arg) {
    search_thread_arg_t *data = (search_thread_arg_t *) arg;
    size_t start = data->id * (data->length / data->count);
    size_t end = (data->id + 1) * (data->length / data->count);
    if (data->id == data->count - 1) {
        end = data->length;
    }

    char *buffer = data->buffer + start;
    size_t size = end - start;
    char *line_start = buffer;
    char *line_end = NULL;

    // Convert search keyword to uppercase
    size_t keyword_length = strlen(data->keyword);
    char *upper_keyword = malloc(keyword_length + 1);
    for (int i = 0; i < keyword_length; i++) {
        upper_keyword[i] = toupper(data->keyword[i]);
    }
    upper_keyword[keyword_length] = '\0';

    // Search for keyword match
    while ((line_end = memchr(line_start, '\n', buffer + size - line_start))) {
        *line_end = '\0';
        if (strstr(line_start, data->keyword)) {
            printf("%s\n", line_start);
            fflush(stdout);
        } else {
            size_t line_length = line_end - line_start;
            char *upper_line = malloc(line_length + 1);
            memcpy(upper_line, line_start, line_length);
            for (int i = 0; i < line_length; i++) {
                upper_line[i] = toupper(line_start[i]);
            }
            upper_line[line_length] = '\0';

            if (strstr(upper_line, upper_keyword)) {
                printf("%s\n", line_start);
                fflush(stdout);
            }

            free(upper_line);
        }

        line_start = line_end + 1;
    }

    free(upper_keyword);

    return NULL;
}

int main(int argc, char **argv) {
    char *filename = NULL;
    char *keyword = NULL;

    if (argc < 3) {
        printf("Usage: %s filename keyword\n", argv[0]);
        return 1;
    }

    filename = argv[1];
    keyword = argv[2];

#ifdef _WIN32
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Failed to open file: %s\n", filename);
        return 2;
    }

    // Get file size
    LARGE_INTEGER liFileSize;
    GetFileSizeEx(hFile, &liFileSize);
    size_t length = (size_t)liFileSize.QuadPart;

    // Allocate buffer and read file content in one go
    char* buffer = (char*)malloc(length + 1);
    DWORD dwBytesRead;
    ReadFile(hFile, buffer, length, &dwBytesRead, NULL);
    buffer[length] = '\0';

    // Close file handle
    CloseHandle(hFile);

    // Count the number of CPU cores available
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    int cpu_count = SystemInfo.dwNumberOfProcessors;
    if (cpu_count > MAX_THREAD_COUNT) {
        cpu_count = MAX_THREAD_COUNT;
    }

    // Spawn multiple threads to search the buffer
    HANDLE* handles = (HANDLE*)malloc(cpu_count * sizeof(HANDLE));
    search_thread_arg_t* args = (search_thread_arg_t*)malloc(cpu_count * sizeof(search_thread_arg_t));
    for (int i = 0; i < cpu_count; i++) {
        args[i].buffer = buffer;
        args[i].length = length;
        args[i].keyword = keyword;
        args[i].id = i;
        args[i].count = cpu_count;
        handles[i] = (HANDLE)_beginthread(search_thread, 0, &args[i]);
    }

    // Wait for threads to finish
    WaitForMultipleObjects(cpu_count, handles, TRUE, INFINITE);

    // Cleanup
    free(buffer);
    free(handles);
    free(args);
#else
    // Read file into memory
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        printf("Failed to open file: %s\n", filename);
        return 2;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // Allocate buffer and read file content in one go
    char *buffer = malloc(length + 1);
    fread(buffer, 1, length, fp);
    buffer[length] = '\0';

    // Close file handle
    fclose(fp);

    // Count the number of CPU cores available
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count > MAX_THREAD_COUNT) {
        cpu_count = MAX_THREAD_COUNT;
    }
    // printf("Using %d threads\n", cpu_count);

    // Spawn multiple threads to search the buffer
    pthread_t threads[cpu_count];
    search_thread_arg_t args[cpu_count];
    for (int i = 0; i < cpu_count; i++) {
        args[i].buffer = buffer;
        args[i].length = length;
        args[i].keyword = keyword;
        args[i].id = i;
        args[i].count = cpu_count;
        pthread_create(&threads[i], NULL, search_thread, &args[i]);
    }

    // Wait for threads to finish
    for (int i = 0; i < cpu_count; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    free(buffer);
#endif
    return 0;
}
