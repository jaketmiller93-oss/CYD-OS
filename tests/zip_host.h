#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define CYD_MAX_PACKAGE (512 * 1024)
#define CYD_MAX_SCRIPT (64 * 1024)
#define CYD_MAX_FILES 48
#define CYD_MAX_PATH 96
typedef struct {
    char name[CYD_MAX_PATH];
    const uint8_t *data;
    size_t size;
} cyd_zip_file_t;
typedef struct {
    cyd_zip_file_t files[CYD_MAX_FILES];
    unsigned count;
} cyd_zip_t;
