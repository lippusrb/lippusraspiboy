#pragma once

#include <stdbool.h>
#include <stdint.h>

bool fat_mount(void);
const char *fat_error(void);

typedef struct {
    char name[64];
    uint8_t attr;
    uint8_t ext[3];
    uint32_t cluster;
    uint32_t size;
} fat_dirent_t;

typedef struct {
    uint8_t buf[512];
    uint32_t cluster;
    uint32_t root_index;
    uint16_t entry;
    uint8_t sec_in_cluster;
    uint8_t fat16_root;
    uint8_t error;
    uint8_t lfn_expect;
    uint8_t lfn_ok;
    char lfn[64];
    uint32_t steps;
} fat_dir_t;

typedef struct {
    uint32_t start;
    uint32_t size;
    uint32_t pos;
    uint32_t cluster;
    uint32_t index;
} fat_file_t;

bool fat_dir_open_root(fat_dir_t *dir);
bool fat_dir_open_cluster(fat_dir_t *dir, uint32_t cluster);
bool fat_dir_next(fat_dir_t *dir, fat_dirent_t *out);
bool fat_dir_ok(const fat_dir_t *dir);

bool fat_file_open(fat_file_t *file, uint32_t cluster, uint32_t size);
int fat_file_read(fat_file_t *file, void *dst, uint32_t len);
bool fat_file_seek(fat_file_t *file, uint32_t pos);

/* /record/<nome>, nome 8.3 senza estensione. Vedi sys_record_*. */
int fat_record_read(const char *name, void *dst, uint32_t cap);
int fat_record_write(const char *name, const void *src, uint32_t len);
