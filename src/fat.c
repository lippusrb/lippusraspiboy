#include "fat.h"

#include "sd.h"

#include <string.h>

typedef struct {
    uint32_t part_lba;
    uint32_t fat_lba;
    uint32_t fat_size;
    uint32_t root_lba;
    uint32_t root_sectors;
    uint32_t data_lba;
    uint32_t root_cluster;
    uint16_t root_entries;
    uint8_t spc;
    uint8_t fat32;
    uint8_t fat_cache[512];
    uint32_t fat_lba_cached;
    uint8_t io[512];
    uint32_t io_lba;
} fat_fs_t;

static fat_fs_t fs;
static const char *s_err = "FAT";

const char *fat_error(void) { return s_err; }

static uint16_t r16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t r32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool parse_bpb(const uint8_t *sec, uint32_t lba) {
    if (r16(sec + 510) != 0xAA55) {
        s_err = "boot non valido";
        return false;
    }
    if (r16(sec + 11) != 512) {
        s_err = "settore non da 512";
        return false;
    }
    uint8_t spc = sec[13];
    if (spc == 0 || (spc & (uint8_t)(spc - 1u)) != 0) {
        s_err = "cluster non valido";
        return false;
    }
    uint16_t reserved = r16(sec + 14);
    uint8_t fats = sec[16];
    uint16_t root_entries = r16(sec + 17);
    uint16_t tot16 = r16(sec + 19);
    uint16_t fatsz16 = r16(sec + 22);
    uint32_t tot32 = r32(sec + 32);
    uint32_t fatsz32 = r32(sec + 36);
    uint32_t root_cluster = r32(sec + 44);
    uint32_t total = tot16 ? tot16 : tot32;
    uint32_t fatsz = fatsz16 ? fatsz16 : fatsz32;
    if (reserved == 0 || fats == 0 || fatsz == 0 || total == 0) {
        s_err = "BPB non valido";
        return false;
    }
    uint32_t root_secs = ((uint32_t)root_entries * 32u + 511u) / 512u;
    uint32_t used = (uint32_t)reserved + (uint32_t)fats * fatsz + root_secs;
    if (used >= total) {
        s_err = "BPB non valido";
        return false;
    }
    uint32_t clusters = (total - used) / spc;
    if (clusters < 4085u) {
        s_err = "FAT12 non supportata";
        return false;
    }
    bool fat32 = clusters >= 65525u;
    fs.part_lba = lba;
    fs.spc = spc;
    fs.fat_lba = lba + reserved;
    fs.fat_size = fatsz;
    fs.root_lba = fs.fat_lba + (uint32_t)fats * fatsz;
    fs.root_sectors = root_secs;
    fs.root_entries = root_entries;
    fs.data_lba = fs.root_lba + root_secs;
    fs.root_cluster = fat32 ? root_cluster : 0;
    fs.fat32 = fat32 ? 1u : 0u;
    fs.fat_lba_cached = 0xFFFFFFFFu;
    fs.io_lba = 0xFFFFFFFFu;
    if (fat32 && root_cluster < 2u) {
        s_err = "root FAT32";
        return false;
    }
    s_err = "";
    return true;
}

bool fat_mount(void) {
    uint8_t sec[512];
    s_err = "FAT";
    if (!sd_read_block(0, sec)) {
        s_err = sd_error();
        return false;
    }
    if (sec[510] != 0x55 || sec[511] != 0xAA) {
        s_err = "SD non formattata";
        return false;
    }
    if (sec[0] == 0xEB || sec[0] == 0xE9) {
        return parse_bpb(sec, 0);
    }
    for (int i = 0; i < 4; i++) {
        const uint8_t *part = sec + 446 + i * 16;
        uint8_t type = part[4];
        uint32_t start = r32(part + 8);
        bool fat = type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0B ||
                   type == 0x0C || type == 0x0E || type == 0x0F;
        if (!fat || start == 0) {
            continue;
        }
        if (!sd_read_block(start, sec)) {
            s_err = sd_error();
            return false;
        }
        return parse_bpb(sec, start);
    }
    s_err = "partizione FAT assente";
    return false;
}

static uint32_t cluster_lba(uint32_t cluster) {
    return fs.data_lba + (cluster - 2u) * fs.spc;
}

/* 1 = cluster successivo, 0 = fine catena, -1 = errore di lettura. */
static int fat_next(uint32_t cluster, uint32_t *out) {
    if (cluster < 2u) {
        return 0;
    }
    uint32_t offset = fs.fat32 ? cluster * 4u : cluster * 2u;
    uint32_t lba = fs.fat_lba + offset / 512u;
    uint32_t index = offset % 512u;
    if (lba < fs.fat_lba || lba >= fs.fat_lba + fs.fat_size) {
        return 0;
    }
    if (fs.fat_lba_cached != lba) {
        if (!sd_read_block(lba, fs.fat_cache)) {
            return -1;
        }
        fs.fat_lba_cached = lba;
    }
    if (fs.fat32) {
        uint32_t value = r32(fs.fat_cache + index) & 0x0FFFFFFFu;
        if (value < 2u || value >= 0x0FFFFFF8u) {
            return 0;
        }
        *out = value;
        return 1;
    }
    uint16_t value = r16(fs.fat_cache + index);
    if (value < 2u || value >= 0xFFF8u) {
        return 0;
    }
    *out = value;
    return 1;
}

static void lfn_reset(fat_dir_t *dir) {
    dir->lfn[0] = 0;
    dir->lfn_expect = 0;
    dir->lfn_ok = 0;
}

static void lfn_take(fat_dir_t *dir, const uint8_t *entry) {
    static const uint8_t k_off[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    int seq = entry[0] & 0x1F;
    bool last = (entry[0] & 0x40) != 0;
    if (seq < 1 || seq > 4) {
        lfn_reset(dir);
        return;
    }
    if (last) {
        memset(dir->lfn, 0, sizeof dir->lfn);
        dir->lfn_expect = (uint8_t)seq;
        dir->lfn_ok = 1;
    }
    if (!dir->lfn_ok || seq != dir->lfn_expect) {
        lfn_reset(dir);
        return;
    }
    int pos = (seq - 1) * 13;
    for (int i = 0; i < 13; i++) {
        uint16_t ch = r16(entry + k_off[i]);
        if (ch == 0 || ch == 0xFFFF) {
            break;
        }
        if (pos + i < 63) {
            dir->lfn[pos + i] = (ch >= 32 && ch < 127) ? (char)ch : '?';
        }
    }
    dir->lfn_expect--;
}

static void sfn_name(const uint8_t *entry, char *dst) {
    bool lower_base = (entry[12] & 0x08) != 0;
    bool lower_ext = (entry[12] & 0x10) != 0;
    int j = 0;
    for (int i = 0; i < 8 && entry[i] != ' '; i++) {
        char ch = (char)entry[i];
        if (i == 0 && entry[0] == 0x05) {
            ch = (char)0xE5;
        }
        if (lower_base && ch >= 'A' && ch <= 'Z') {
            ch = (char)(ch + 32);
        }
        dst[j++] = ch;
    }
    if (entry[8] != ' ') {
        dst[j++] = '.';
        for (int i = 8; i < 11 && entry[i] != ' '; i++) {
            char ch = (char)entry[i];
            if (lower_ext && ch >= 'A' && ch <= 'Z') {
                ch = (char)(ch + 32);
            }
            dst[j++] = ch;
        }
    }
    dst[j] = 0;
}

static bool load_dir_sector(fat_dir_t *dir) {
    if (++dir->steps > 65536u) {
        dir->error = 1;
        return false;
    }
    if (dir->fat16_root) {
        if (dir->root_index >= fs.root_sectors) {
            return false;
        }
        if (!sd_read_block(fs.root_lba + dir->root_index, dir->buf)) {
            dir->error = 1;
            return false;
        }
        dir->root_index++;
        return true;
    }
    if (dir->sec_in_cluster >= fs.spc) {
        uint32_t next = 0;
        int step = fat_next(dir->cluster, &next);
        if (step < 0) {
            dir->error = 1;
            return false;
        }
        if (step == 0) {
            return false;
        }
        dir->cluster = next;
        dir->sec_in_cluster = 0;
    }
    if (!sd_read_block(cluster_lba(dir->cluster) + dir->sec_in_cluster, dir->buf)) {
        dir->error = 1;
        return false;
    }
    dir->sec_in_cluster++;
    return true;
}

bool fat_dir_open_root(fat_dir_t *dir) {
    memset(dir, 0, sizeof *dir);
    dir->entry = 16;
    if (!fs.fat32) {
        dir->fat16_root = 1;
        return fs.root_sectors > 0;
    }
    if (fs.root_cluster < 2u) {
        return false;
    }
    dir->cluster = fs.root_cluster;
    return true;
}

bool fat_dir_open_cluster(fat_dir_t *dir, uint32_t cluster) {
    memset(dir, 0, sizeof *dir);
    dir->entry = 16;
    dir->cluster = cluster;
    return cluster >= 2u;
}

bool fat_dir_ok(const fat_dir_t *dir) { return dir && !dir->error; }

bool fat_dir_next(fat_dir_t *dir, fat_dirent_t *out) {
    if (dir->error) {
        return false;
    }
    for (;;) {
        if (dir->entry >= 16) {
            if (!load_dir_sector(dir)) {
                return false;
            }
            dir->entry = 0;
        }
        const uint8_t *entry = dir->buf + (size_t)dir->entry * 32u;
        dir->entry++;
        if (entry[0] == 0x00) {
            return false;
        }
        if (entry[0] == 0xE5) {
            lfn_reset(dir);
            continue;
        }
        uint8_t attr = entry[11];
        if (attr == 0x0F) {
            lfn_take(dir, entry);
            continue;
        }
        if (attr & 0x08) {
            lfn_reset(dir);
            continue;
        }
        memset(out, 0, sizeof *out);
        if (dir->lfn_ok && dir->lfn_expect == 0 && dir->lfn[0]) {
            memcpy(out->name, dir->lfn, 63);
            out->name[63] = 0;
        } else {
            sfn_name(entry, out->name);
        }
        lfn_reset(dir);
        out->attr = attr;
        out->ext[0] = entry[8];
        out->ext[1] = entry[9];
        out->ext[2] = entry[10];
        out->cluster = (uint32_t)r16(entry + 26) | ((uint32_t)r16(entry + 20) << 16);
        out->size = r32(entry + 28);
        return true;
    }
}

bool fat_file_open(fat_file_t *file, uint32_t cluster, uint32_t size) {
    if (cluster < 2u) {
        return false;
    }
    file->start = cluster;
    file->size = size;
    file->pos = 0;
    file->cluster = cluster;
    file->index = 0;
    return true;
}

bool fat_file_seek(fat_file_t *file, uint32_t pos) {
    if (pos > file->size) {
        pos = file->size;
    }
    if (pos < file->pos) {
        file->pos = 0;
        file->cluster = file->start;
        file->index = 0;
    }
    file->pos = pos;
    return true;
}

int fat_file_read(fat_file_t *file, void *dst, uint32_t len) {
    uint8_t *out = dst;
    uint32_t got = 0;
    uint32_t cluster_bytes = (uint32_t)fs.spc * 512u;
    uint32_t guard = 0;
    while (len && file->pos < file->size) {
        if (++guard > 100000u) {
            return -1;
        }
        uint32_t want = file->pos / cluster_bytes;
        while (file->index < want) {
            uint32_t next = 0;
            int step = fat_next(file->cluster, &next);
            if (step != 1) {
                return -1;
            }
            file->cluster = next;
            file->index++;
        }
        uint32_t into = file->pos % cluster_bytes;
        uint32_t lba = cluster_lba(file->cluster) + into / 512u;
        if (fs.io_lba != lba) {
            if (!sd_read_block(lba, fs.io)) {
                return -1;
            }
            fs.io_lba = lba;
        }
        uint32_t off = into % 512u;
        uint32_t chunk = 512u - off;
        uint32_t left = file->size - file->pos;
        if (chunk > len) {
            chunk = len;
        }
        if (chunk > left) {
            chunk = left;
        }
        memcpy(out, fs.io + off, chunk);
        out += chunk;
        got += chunk;
        len -= chunk;
        file->pos += chunk;
    }
    return (int)got;
}
