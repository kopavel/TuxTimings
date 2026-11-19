#include "backend.h"
#include "pm_table.h"
#include "dram.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fnmatch.h>
#include <glob.h>
#include <math.h>

#define SMU_PATH "/sys/kernel/ryzen_smu_drv"

/* GCC conservatively warns about format truncation for sysfs path buffers
 * because it can't statically bound hwmon directory name lengths. All path
 * buffers here are 640 bytes — more than sufficient for any sysfs path. */
#pragma GCC diagnostic ignored "-Wformat-truncation"

/* system() is marked warn_unused_result in glibc; we genuinely don't care
 * about the exit code for best-effort modprobe/shell calls. */
static void run_shell(const char *cmd) { int r = system(cmd); (void)r; }

/* ── Cached static data ─────────────────────────────────────────────── */
static int  s_cached_static = 0;
static int  s_physical_cores = -1;
static int  s_loaded_aod_voltages = 0;
static int  s_loaded_ryzen_smu    = 0;
static int  s_loaded_tuxbench     = 0;
static char s_processor_name[STR_LEN];
static char s_board_product[STR_LEN];
static char s_bios_version[STR_LEN];
static char s_bios_date[STR_SHORT];
static char s_agesa_version[STR_LEN];
static memory_module_t s_modules[MAX_MODULES];
static int  s_module_count;

/* Previous /proc/stat per-logical-cpu times for usage delta */
#define MAX_LOGICAL_CPUS 256
static uint64_t s_prev_idle[MAX_LOGICAL_CPUS];
static uint64_t s_prev_total[MAX_LOGICAL_CPUS];
static int      s_prev_valid[MAX_LOGICAL_CPUS];

/* ── Helpers ────────────────────────────────────────────────────────── */

static int read_smu_string(const char *name, char *buf, size_t sz)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", SMU_PATH, name);
    return read_file_string(path, buf, sz);
}

static uint32_t read_smu_uint32(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", SMU_PATH, name);
    uint8_t buf[4];
    if (read_file_bytes(path, buf, 4) < 4) return 0;
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

static int read_codename_index(void)
{
    char buf[32];
    if (!read_smu_string("codename", buf, sizeof(buf))) return -1;
    return atoi(buf);
}

static const char *map_codename(int idx)
{
    switch (idx) {
    case 1:  return "Colfax";
    case 2:  return "Renoir";
    case 3:  return "Picasso";
    case 4:  return "Matisse";
    case 5:  return "Threadripper";
    case 6:  return "Castle Peak";
    case 7:  return "Raven Ridge";
    case 8:  return "Raven Ridge 2";
    case 9:  return "Summit Ridge";
    case 10: return "Pinnacle Ridge";
    case 11: return "Rembrandt";
    case 12: return "Vermeer";
    case 13: return "Vangogh";
    case 14: return "Cezanne";
    case 15: return "Milan";
    case 16: return "Dali";
    case 17: return "Lucienne";
    case 18: return "Naples";
    case 19: return "Chagall";
    case 20: return "Raphael";
    case 21: return "Phoenix";
    case 22: return "Strix Point";
    case 23: return "Granite Ridge";
    case 24: return "Hawk Point";
    case 25: return "Storm Peak";
    default: return "Unknown";
    }
}

/* ── dmidecode parsing ──────────────────────────────────────────────── */

static void parse_dmidecode_processor(void)
{
    char *out = run_command("dmidecode -t processor");
    if (!out) return;

    int in_proc = 0;
    char *line = strtok(out, "\n");
    while (line) {
        if (strstr(line, "Processor Information"))
            in_proc = 1;
        else if (in_proc && strstr(line, "Version:")) {
            const char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ') colon++;
                snprintf(s_processor_name, STR_LEN, "%s", colon);
            }
            break;
        }
        line = strtok(NULL, "\n");
    }
    free(out);
}

static void parse_dmidecode_board(void)
{
    char *out = run_command("dmidecode -s baseboard-product-name");
    if (out) { snprintf(s_board_product, STR_LEN, "%s", out); free(out); }
    /* trim trailing newline */
    char *nl = strchr(s_board_product, '\n');
    if (nl) *nl = '\0';

    out = run_command("dmidecode -s bios-version");
    if (out) { snprintf(s_bios_version, STR_LEN, "%s", out); free(out); }
    nl = strchr(s_bios_version, '\n');
    if (nl) *nl = '\0';

    out = run_command("dmidecode -s bios-release-date");
    if (out) { snprintf(s_bios_date, STR_SHORT, "%s", out); free(out); }
    nl = strchr(s_bios_date, '\n');
    if (nl) *nl = '\0';
}

static uint64_t parse_capacity(const char *val)
{
    if (!val) return 0;
    if (strstr(val, "No Module") || strstr(val, "Not Installed"))
        return 0;
    uint64_t size = 0;
    char unit[16] = {0};
    if (sscanf(val, "%lu %15s", &size, unit) < 2) return 0;
    if (size > 0x100000) return 0; /* sanity: reject anything over ~1 TB */
    /* upper-case unit */
    for (char *p = unit; *p; p++) *p = (char)toupper((unsigned char)*p);
    if (strcmp(unit, "GB") == 0 || strcmp(unit, "GIB") == 0)
        return size * 1024ULL * 1024ULL * 1024ULL;
    if (strcmp(unit, "MB") == 0 || strcmp(unit, "MIB") == 0)
        return size * 1024ULL * 1024ULL;
    if (strcmp(unit, "KB") == 0 || strcmp(unit, "KIB") == 0)
        return size * 1024ULL;
    return size;
}

static void build_module_display(memory_module_t *m, int index)
{
    /* slot label from bank locator */
    int bank = -1;
    char channel = 0;
    if (sscanf(m->bank_label, "BANK %d", &bank) == 1 || sscanf(m->bank_label, "Bank %d", &bank) == 1) {
        char ch = 'A' + (char)(bank / 2);
        int num = (bank % 2) + 1;
        snprintf(m->slot_label, sizeof(m->slot_label), "%c%d", ch, num);
    } else if (sscanf(m->bank_label, "P0 CHANNEL %c", &channel) == 1 ||
               sscanf(m->bank_label, "P0 Channel %c", &channel) == 1) {
        /* e.g. "P0 CHANNEL A" + "DIMM 1" -> "A1" */
        channel = (char)toupper((unsigned char)channel);
        int dimm_num = 1;
        sscanf(m->device_locator, "DIMM %d", &dimm_num);
        snprintf(m->slot_label, sizeof(m->slot_label), "%c%d", channel, dimm_num);
    } else if (m->device_locator[0]) {
        snprintf(m->slot_label, sizeof(m->slot_label), "%.7s", m->device_locator);
    } else if (m->bank_label[0]) {
        snprintf(m->slot_label, sizeof(m->slot_label), "%.7s", m->bank_label);
    } else {
        snprintf(m->slot_label, sizeof(m->slot_label), "Slot %d", index);
    }

    double gib = (double)m->capacity_bytes / (1024.0 * 1024.0 * 1024.0);
    snprintf(m->capacity_display, sizeof(m->capacity_display), "%.1f GiB", gib);
    snprintf(m->slot_display, sizeof(m->slot_display), "Module %d: %s - %s", index + 1, m->slot_label, m->capacity_display);
    m->channel_idx = -1; /* resolved per refresh against the UMC channels */
}

static void parse_dmidecode_memory(void)
{
    /* dmidecode is often in /usr/sbin which may not be in PATH under pkexec */
    const char *cmds[] = {
        "/usr/sbin/dmidecode -t memory",
        "/usr/bin/dmidecode -t memory",
        "dmidecode -t memory",
        NULL
    };
    char *out = NULL;
    for (int ci = 0; cmds[ci]; ci++) {
        out = run_command(cmds[ci]);
        if (out && out[0]) break;
        if (out) { free(out); out = NULL; }
    }
    if (!out) return;

    /* Parse into a temporary list; only overwrite globals on success. */
    memory_module_t tmp_modules[MAX_MODULES];
    int tmp_count = 0;

    memory_module_t cur;
    memset(&cur, 0, sizeof(cur));
    int in_device = 0;
    uint64_t cap = 0;

    char *line = strtok(out, "\n");
    while (line) {
        if (strstr(line, "Memory Device") && !strstr(line, "Mapped")) {
            /* save previous */
            if (in_device && cap > 0 && tmp_count < MAX_MODULES) {
                cur.capacity_bytes = cap;
                build_module_display(&cur, tmp_count);
                tmp_modules[tmp_count++] = cur;
            }
            memset(&cur, 0, sizeof(cur));
            cap = 0;
            in_device = 1;
            line = strtok(NULL, "\n");
            continue;
        }
        if (!in_device) { line = strtok(NULL, "\n"); continue; }

        /* trim leading whitespace */
        const char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        const char *colon = strchr(trimmed, ':');
        if (!colon) { line = strtok(NULL, "\n"); continue; }

        /* key/value */
        int klen = (int)(colon - trimmed);
        const char *val = colon + 1;
        while (*val == ' ') val++;

        if (klen == 4 && strncmp(trimmed, "Size", 4) == 0)
            cap = parse_capacity(val);
        else if (klen == 7 && strncmp(trimmed, "Locator", 7) == 0)
            snprintf(cur.device_locator, STR_LEN, "%s", val);
        else if (klen == 12 && strncmp(trimmed, "Bank Locator", 12) == 0)
            snprintf(cur.bank_label, STR_LEN, "%s", val);
        else if (klen == 12 && strncmp(trimmed, "Manufacturer", 12) == 0) {
            if (strcmp(val, "Unknown") != 0 && strcmp(val, "Not Specified") != 0)
                snprintf(cur.manufacturer, STR_LEN, "%s", val);
        }
        else if (klen == 11 && strncmp(trimmed, "Part Number", 11) == 0) {
            if (strcmp(val, "Unknown") != 0 && strcmp(val, "NO DIMM") != 0 && strcmp(val, "Not Specified") != 0)
                snprintf(cur.part_number, STR_LEN, "%s", val);
        }
        else if (klen == 13 && strncmp(trimmed, "Serial Number", 13) == 0) {
            if (strcmp(val, "Unknown") != 0 && strcmp(val, "Not Specified") != 0 && strcmp(val, "00000000") != 0)
                snprintf(cur.serial_number, STR_LEN, "%s", val);
        }
        else if (klen == 4 && strncmp(trimmed, "Rank", 4) == 0) {
            int r = atoi(val);
            cur.rank = (r == 4) ? RANK_QR : (r == 2) ? RANK_DR : RANK_SR;
        }
        else if (strstr(trimmed, "Configured Memory Speed") || strstr(trimmed, "Configured Clock Speed")) {
            uint32_t mhz = 0;
            sscanf(val, "%u", &mhz);
            cur.clock_speed_mhz = mhz;
        }

        line = strtok(NULL, "\n");
    }
    /* last device */
    if (in_device && cap > 0 && tmp_count < MAX_MODULES) {
        cur.capacity_bytes = cap;
        build_module_display(&cur, tmp_count);
        tmp_modules[tmp_count++] = cur;
    }
    free(out);

    if (tmp_count > 0) {
        s_module_count = tmp_count;
        memcpy(s_modules, tmp_modules, tmp_count * sizeof(memory_module_t));
    }
}

/* ── AGESA version ──────────────────────────────────────────────────── */

static int agesa_allowed(unsigned char c)
{
    return isalnum(c) || c == ' ' || c == '.' || c == '-';
}

static int find_agesa_in_buf(const uint8_t *buf, size_t len, char *out, size_t outsz)
{
    const char marker[] = "AGESA!V9";
    size_t mlen = strlen(marker);
    if (len < mlen) return 0;

    for (size_t i = 0; i <= len - mlen; i++) {
        if (memcmp(buf + i, marker, mlen) != 0) continue;
        size_t start = i + mlen;
        /* skip non-allowed */
        while (start < len && !agesa_allowed(buf[start])) start++;
        size_t end = start;
        while (end < len && agesa_allowed(buf[end])) end++;
        if (end > start) {
            size_t n = end - start;
            if (n >= outsz) n = outsz - 1;
            memcpy(out, buf + start, n);
            out[n] = '\0';
            return 1;
        }
        return 0;
    }
    return 0;
}

static void read_agesa_version(void)
{
    /* 1) /dev/mem BIOS region 0xE0000–0xFFFFF */
    FILE *f = fopen("/dev/mem", "rb");
    if (f) {
        const long base = 0xE0000;
        const int len = 0x100000 - 0xE0000;
        uint8_t *buf = malloc(len);
        if (buf) {
            if (fseek(f, base, SEEK_SET) == 0) {
                size_t rd = fread(buf, 1, len, f);
                if (rd > 0 && find_agesa_in_buf(buf, rd, s_agesa_version, STR_LEN)) {
                    free(buf);
                    fclose(f);
                    return;
                }
            }
            free(buf);
        }
        fclose(f);
    }

    /* 2) ACPI tables */
    const char *acpi_paths[] = {
        "/sys/firmware/acpi/tables/DSDT",
        "/sys/firmware/acpi/tables/FACP",
        "/sys/firmware/acpi/tables/XSDT",
        "/sys/firmware/acpi/tables/RSDT",
        NULL
    };
    for (int i = 0; acpi_paths[i]; i++) {
        FILE *af = fopen(acpi_paths[i], "rb");
        if (!af) continue;
        fseek(af, 0, SEEK_END);
        long sz = ftell(af);
        if (sz <= 0) { fclose(af); continue; }
        rewind(af);
        uint8_t *buf = malloc(sz);
        if (!buf) { fclose(af); continue; }
        size_t rd = fread(buf, 1, sz, af);
        fclose(af);
        if (rd > 0 && find_agesa_in_buf(buf, rd, s_agesa_version, STR_LEN)) {
            free(buf);
            return;
        }
        free(buf);
    }

    /* 3) dmidecode -t bios output (contains AGESA in BIOS version or ROM info) */
    char *out = run_command("dmidecode -t bios");
    if (out) {
        if (find_agesa_in_buf((const uint8_t *)out, strlen(out), s_agesa_version, STR_LEN)) {
            free(out);
            return;
        }
        free(out);
    }

    /* 4) Scan entire SMBIOS/DMI raw blob */
    const char *dmi_paths[] = {
        "/sys/firmware/dmi/tables/DMI",
        "/sys/firmware/dmi/tables/smbios_entry_point",
        NULL
    };
    for (int i = 0; dmi_paths[i]; i++) {
        FILE *df = fopen(dmi_paths[i], "rb");
        if (!df) continue;
        fseek(df, 0, SEEK_END);
        long sz = ftell(df);
        if (sz <= 0) { fclose(df); continue; }
        rewind(df);
        uint8_t *buf = malloc(sz);
        if (!buf) { fclose(df); continue; }
        size_t rd = fread(buf, 1, sz, df);
        fclose(df);
        if (rd > 0 && find_agesa_in_buf(buf, rd, s_agesa_version, STR_LEN)) {
            free(buf);
            return;
        }
        free(buf);
    }
}

/* ── Total memory from /proc/meminfo ────────────────────────────────── */

static void read_total_memory(char *buf, size_t sz)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { buf[0] = '\0'; return; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        uint64_t kb = 0;
        if (sscanf(line, "MemTotal: %lu kB", &kb) == 1) {
            double gib = (double)kb / 1024.0 / 1024.0;
            snprintf(buf, sz, "%.1f GiB", gib);
            fclose(f);
            return;
        }
    }
    fclose(f);
    buf[0] = '\0';
}

/* ── Memory type from codename ──────────────────────────────────────── */

static mem_type_t mem_type_for_codename(int idx)
{
    switch (idx) {
    case 23: return MEM_DDR5;
    case 4: case 9: case 10: case 12: case 18: case 19:
        return MEM_DDR4;
    default: return MEM_UNKNOWN;
    }
}

/* ── hwmon helpers ──────────────────────────────────────────────────── */

/* Find a hwmon directory by name match. Returns path in buf or empty. */
static int find_hwmon_by_name(const char *match, char *buf, size_t sz)
{
    DIR *d = opendir("/sys/class/hwmon");
    if (!d) return 0;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        char namepath[512];
        snprintf(namepath, sizeof(namepath), "/sys/class/hwmon/%s/name", ent->d_name);
        char name[64];
        if (!read_file_string(namepath, name, sizeof(name))) continue;
        /* case-insensitive contains */
        char name_lower[64];
        int nli = 0;
        for (; name[nli] && nli < 63; nli++)
            name_lower[nli] = (char)tolower((unsigned char)name[nli]);
        name_lower[nli] = '\0';
        if (strstr(name_lower, match)) {
            snprintf(buf, sz, "/sys/class/hwmon/%s", ent->d_name);
            closedir(d);
            return 1;
        }
    }
    closedir(d);
    return 0;
}

static float read_temp_input(const char *hwmon_dir, int index)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/temp%d_input", hwmon_dir, index);
    int raw = read_int_file(path);
    if (raw == 0) return -1.0f;
    float c = raw / 1000.0f;
    return (c >= 0.0f && c <= 150.0f) ? c : -1.0f;
}

/* ── k10temp Tctl/Tccd overlay ──────────────────────────────────────── */

static void apply_k10temp_tctl_tccd(smu_metrics_t *m)
{
    char k10dir[640];
    if (find_hwmon_by_name("k10temp", k10dir, sizeof(k10dir))) {
        float tctl = read_temp_input(k10dir, 1);
        float tccd1 = read_temp_input(k10dir, 3);
        float tccd2 = read_temp_input(k10dir, 4);
        if (tctl >= 0) { m->tctl_c = tctl; m->has_tctl = true; }
        if (tccd1 >= 0) { m->tccd1_c = tccd1; m->has_tccd1 = true; }
        if (tccd2 >= 0) { m->tccd2_c = tccd2; m->has_tccd2 = true; }

        /* Derive Tdie from Tctl if PM table didn't provide it */
        if (!m->has_tdie && m->has_tctl) {
            m->tdie_c = m->tctl_c;
            m->has_tdie = true;
        }
        return;
    }

    /* Fallback: zenpower Tdie/Tctl/Tccd1 from labels */
    char zpdir[640];
    if (!find_hwmon_by_name("zenpower", zpdir, sizeof(zpdir))) return;

    DIR *d = opendir(zpdir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d))) {
        if (strncmp(ent->d_name, "temp", 4) != 0 || !strstr(ent->d_name, "_label"))
            continue;
        int idx = 0;
        if (sscanf(ent->d_name, "temp%d_label", &idx) != 1) continue;
        char lpath[640], label[64];
        snprintf(lpath, sizeof(lpath), "%s/temp%d_label", zpdir, idx);
        if (!read_file_string(lpath, label, sizeof(label))) continue;
        float c = read_temp_input(zpdir, idx);
        if (c < 0) continue;
        /* case insensitive match */
        char lower[64];
        int li = 0;
        for (; label[li] && li < 63; li++)
            lower[li] = (char)tolower((unsigned char)label[li]);
        lower[li] = '\0';
        if (strstr(lower, "tdie")) { m->tdie_c = c; m->has_tdie = true; }
        else if (strstr(lower, "tctl")) { m->tctl_c = c; m->has_tctl = true; }
        else if (strstr(lower, "tccd1")) { m->tccd1_c = c; m->has_tccd1 = true; }
        else if (strstr(lower, "tccd2")) { m->tccd2_c = c; m->has_tccd2 = true; }
    }
    closedir(d);

    if (!m->has_tdie && m->has_tctl) {
        m->tdie_c = m->tctl_c;
        m->has_tdie = true;
    }
    if (!m->has_tdie && m->cpu_temp_c > 0) {
        m->tdie_c = m->cpu_temp_c;
        m->has_tdie = true;
    }
}

/* ── Per-core temps from hwmon "Core N" labels ──────────────────────── */

static void apply_per_core_temps_hwmon(smu_metrics_t *m)
{
    DIR *hwmon = opendir("/sys/class/hwmon");
    if (!hwmon) return;
    struct dirent *ent;
    while ((ent = readdir(hwmon))) {
        if (ent->d_name[0] == '.') continue;
        char dir[640];
        snprintf(dir, sizeof(dir), "/sys/class/hwmon/%s", ent->d_name);

        DIR *d2 = opendir(dir);
        if (!d2) continue;
        struct dirent *e2;
        while ((e2 = readdir(d2))) {
            if (strncmp(e2->d_name, "temp", 4) != 0 || !strstr(e2->d_name, "_label"))
                continue;
            int idx = 0;
            if (sscanf(e2->d_name, "temp%d_label", &idx) != 1) continue;
            char lpath[640], label[64];
            snprintf(lpath, sizeof(lpath), "%s/temp%d_label", dir, idx);
            if (!read_file_string(lpath, label, sizeof(label))) continue;
            if (strncmp(label, "Core ", 5) != 0) continue;
            int core = atoi(label + 5);
            if (core < 0 || core >= MAX_CORES) continue;
            float c = read_temp_input(dir, idx);
            if (c >= 0) {
                m->core_temps_c[core] = c;
                if (core >= m->core_temps_count)
                    m->core_temps_count = core + 1;
            }
        }
        closedir(d2);
    }
    closedir(hwmon);
}

/* ── SPD5118 DIMM temps ─────────────────────────────────────────────── */

static void read_spd_temps(smu_metrics_t *m)
{
    /* Try hwmon entries named spd5118 */
    DIR *hwmon = opendir("/sys/class/hwmon");
    if (!hwmon) return;
    struct dirent *ent;
    m->spd_temps_count = 0;
    while ((ent = readdir(hwmon)) && m->spd_temps_count < MAX_MODULES) {
        if (ent->d_name[0] == '.') continue;
        char namepath[640], name[64];
        snprintf(namepath, sizeof(namepath), "/sys/class/hwmon/%s/name", ent->d_name);
        if (!read_file_string(namepath, name, sizeof(name))) continue;
        char lower[64];
        int li = 0;
        for (; name[li] && li < 63; li++)
            lower[li] = (char)tolower((unsigned char)name[li]);
        lower[li] = '\0';
        if (!strstr(lower, "spd5118")) continue;

        char tpath[640];
        snprintf(tpath, sizeof(tpath), "/sys/class/hwmon/%s/temp1_input", ent->d_name);
        int raw = read_int_file(tpath);
        if (raw == 0) continue;
        float c = raw / 1000.0f;
        if (c >= 0.0f && c <= 150.0f)
            m->spd_temps_c[m->spd_temps_count++] = c;
    }
    closedir(hwmon);
}

/* ── Fan names from lm-sensors config (optional) ──────────────────────
 * Board sensor packs for the CLI `sensors` tool name fan channels with
 * 'label fanN "..."' directives in /etc/sensors3.conf and
 * /etc/sensors.d/ config files (matched by a chip line such as
 * 'chip "it8696_a0040507-*"'). When the kernel driver does not provide a
 * fan<N>_label attribute, apply the same standard mapping so the GUI shows
 * connector names (CPU_FAN, SYS_FAN1, ...) instead of Fan1, Fan2, ...
 * Parsed once, at first use during this process lifetime. */

typedef struct {
    char pat[64];
    int  fan;
    char label[32];
} sensors_label_t;

#define MAX_SENSOR_LABELS 256
static sensors_label_t s_slabels[MAX_SENSOR_LABELS];
static int s_slabel_count = -1; /* -1 = not loaded yet */

static void parse_sensors_conf(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1024];
    char pats[6][64];
    int npat = 0;
    while (fgets(line, sizeof(line), f) && s_slabel_count < MAX_SENSOR_LABELS) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        if (strncmp(p, "chip", 4) == 0 && (p[4] == ' ' || p[4] == '\t')) {
            npat = 0;
            char *tok = strtok(p + 5, " \t\n\"");
            while (tok && npat < 6) {
                snprintf(pats[npat], sizeof(pats[npat]), "%.63s", tok);
                npat++;
                tok = strtok(NULL, " \t\n\"");
            }
            continue;
        }
        int fan = 0;
        char lbl[32];
        if (npat > 0 && sscanf(p, " label fan%d \"%31[^\"]\"", &fan, lbl) == 2) {
            for (int k = 0; k < npat && s_slabel_count < MAX_SENSOR_LABELS; k++) {
                snprintf(s_slabels[s_slabel_count].pat, 64, "%s", pats[k]);
                for (char *q = s_slabels[s_slabel_count].pat; *q; q++)
                    *q = (char)tolower((unsigned char)*q);
                s_slabels[s_slabel_count].fan = fan;
                snprintf(s_slabels[s_slabel_count].label, 32, "%s", lbl);
                s_slabel_count++;
            }
        }
    }
    fclose(f);
}

static const char *sensors_fan_label(const char *chip, int fan)
{
    if (s_slabel_count < 0) {
        s_slabel_count = 0;
        parse_sensors_conf("/etc/sensors3.conf");
        glob_t g;
        if (glob("/etc/sensors.d/*", 0, NULL, &g) == 0) {
            for (size_t i = 0; i < g.gl_pathc; i++)
                parse_sensors_conf(g.gl_pathv[i]); /* dirs fail fopen: skipped */
            globfree(&g);
        }
    }
    char probe[96];
    snprintf(probe, sizeof(probe), "%.79s-", chip); /* chip patterns end in "-*" */
    for (int i = 0; i < s_slabel_count; i++)
        if (s_slabels[i].fan == fan &&
            fnmatch(s_slabels[i].pat, probe, 0) == 0)
            return s_slabels[i].label;
    return NULL;
}

/* ── Fan speeds (board sensor Super I/O chips) ──────────────────────── */

static int is_fan_chip_name(const char *lower)
{
    return strncmp(lower, "nct6", 4) == 0 ||
           strstr(lower, "nuvoton") != NULL ||
           strncmp(lower, "it87", 4) == 0 ||
           strncmp(lower, "it86", 4) == 0 ||  /* dkms it87: it8696_<SIV> style names */
           strncmp(lower, "w836", 4) == 0 ||
           strncmp(lower, "f718", 4) == 0 ||
           strncmp(lower, "gigabyte", 8) == 0 ||  /* WMI fan temps, if any */
           strncmp(lower, "amdgpu", 6) == 0;   /* GPU cooler fan, if present */
}

/* Returns: bitmask — bit0 fan chip seen, bit1 RPM found */
static int read_fans(fan_reading_t *fans, int *count)
{
    *count = 0;
    int chip_seen = 0;
    DIR *hwmon = opendir("/sys/class/hwmon");
    if (!hwmon) return 0;
    struct dirent *ent;
    while ((ent = readdir(hwmon))) {
        if (ent->d_name[0] == '.') continue;
        char namepath[640], name[64];
        snprintf(namepath, sizeof(namepath), "/sys/class/hwmon/%s/name", ent->d_name);
        if (!read_file_string(namepath, name, sizeof(name))) continue;
        char lower[64];
        int li = 0;
        for (; name[li] && li < 63; li++)
            lower[li] = (char)tolower((unsigned char)name[li]);
        lower[li] = '\0';
        if (!is_fan_chip_name(lower))
            continue;

        char dir[640];
        snprintf(dir, sizeof(dir), "/sys/class/hwmon/%s", ent->d_name);
        int is_gpu = strncmp(lower, "amdgpu", 6) == 0;
        for (int i = 1; i <= 10 && *count < MAX_FANS; i++) {
            char path[640];
            snprintf(path, sizeof(path), "%s/fan%d_input", dir, i);
            if (access(path, F_OK) == 0) chip_seen = 1;
            int rpm = read_int_file(path);
            if (rpm <= 0) continue;
            fan_reading_t *fan = &fans[(*count)++];
            /* driver label when available (e.g. "CPU_FAN", "SYS_FAN1") */
            char lpath[640], label[64] = {0};
            snprintf(lpath, sizeof(lpath), "%s/fan%d_label", dir, i);
            read_file_string(lpath, label, sizeof(label));
            if (!label[0]) {
                const char *sl = sensors_fan_label(lower, i);
                if (sl)
                    snprintf(label, sizeof(label), "%s", sl);
            }
            if (is_gpu)
                snprintf(fan->label, sizeof(fan->label), "GPU");
            else if (label[0])
                snprintf(fan->label, sizeof(fan->label), "%.15s", label);
            else if (i == 7)
                snprintf(fan->label, sizeof(fan->label), "Pump");
            else
                snprintf(fan->label, sizeof(fan->label), "Fan%d", i);
            fan->rpm = rpm;
        }
    }
    closedir(hwmon);
    return chip_seen ? 1 : 0;
}

/* ── /proc/stat per-core usage ──────────────────────────────────────── */

static void read_core_usage(smu_metrics_t *m)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    float logical_usage[MAX_LOGICAL_CPUS];
    int logical_count = 0;
    memset(logical_usage, 0, sizeof(logical_usage));

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu", 3) != 0 || !isdigit((unsigned char)line[3]))
            continue;
        int cpuid = 0;
        uint64_t user, nice, sys, idle, iowait, irq, softirq, steal, guest, gnice;
        user = nice = sys = idle = iowait = irq = softirq = steal = guest = gnice = 0;
        if (sscanf(line, "cpu%d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                   &cpuid, &user, &nice, &sys, &idle, &iowait, &irq, &softirq,
                   &steal, &guest, &gnice) < 5)
            continue;
        if (cpuid < 0 || cpuid >= MAX_LOGICAL_CPUS) continue;

        uint64_t idle_all = idle + iowait;
        uint64_t total = user + nice + sys + idle + iowait + irq + softirq + steal + guest + gnice;
        if (total == 0) continue;

        float usage = 0;
        if (s_prev_valid[cpuid]) {
            uint64_t di = idle_all - s_prev_idle[cpuid];
            uint64_t dt = total - s_prev_total[cpuid];
            if (dt > 0) {
                usage = (1.0f - (float)di / (float)dt) * 100.0f;
                if (usage < 0) usage = 0;
                if (usage > 100) usage = 100;
            }
        }
        s_prev_idle[cpuid] = idle_all;
        s_prev_total[cpuid] = total;
        s_prev_valid[cpuid] = 1;

        if (cpuid < MAX_LOGICAL_CPUS) {
            logical_usage[cpuid] = usage;
            if (cpuid >= logical_count) logical_count = cpuid + 1;
        }
    }
    fclose(f);

    /* Aggregate SMT pairs: core N = avg(cpu 2N, cpu 2N+1) */
    int max_core = (logical_count + 1) / 2;
    if (max_core > MAX_CORES) max_core = MAX_CORES;
    for (int c = 0; c < max_core; c++) {
        int l0 = c * 2, l1 = c * 2 + 1;
        float sum = 0; int cnt = 0;
        if (l0 < logical_count) { sum += logical_usage[l0]; cnt++; }
        if (l1 < logical_count) { sum += logical_usage[l1]; cnt++; }
        m->core_usage_pct[c] = cnt > 0 ? sum / cnt : 0;
    }
    m->core_usage_count = max_core;
}

/* ── Per-core frequency from cpufreq ────────────────────────────────── */

static void read_core_freq(smu_metrics_t *m)
{
    float logical_freq[MAX_LOGICAL_CPUS];
    int logical_count = 0;
    memset(logical_freq, 0, sizeof(logical_freq));

    for (int i = 0; i < MAX_LOGICAL_CPUS; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        int khz = read_int_file(path);
        if (khz <= 0) {
            if (i > 0) break;
            continue;
        }
        logical_freq[i] = khz / 1000.0f;
        if (i >= logical_count) logical_count = i + 1;
    }

    int max_core = (logical_count + 1) / 2;
    if (max_core > MAX_CORES) max_core = MAX_CORES;
    for (int c = 0; c < max_core; c++) {
        int l0 = c * 2, l1 = c * 2 + 1;
        float sum = 0; int cnt = 0;
        if (l0 < logical_count && logical_freq[l0] > 0) { sum += logical_freq[l0]; cnt++; }
        if (l1 < logical_count && logical_freq[l1] > 0) { sum += logical_freq[l1]; cnt++; }
        m->core_freq_mhz[c] = cnt > 0 ? sum / cnt : 0;
    }
    m->core_freq_count = max_core;
}

/* ── BCLK from DMI Type 4 (External Clock field) ───────────────────── */

static float try_read_bclk_dmi(void)
{
    /*
     * DMI Type 4 (Processor Information), offset 0x12: External Clock (MHz, WORD).
     * This is the BIOS-reported BCLK — exact integer, same source CPU-X uses.
     * Requires root (binary runs via pkexec).
     */
    int fd = open("/sys/firmware/dmi/tables/DMI", O_RDONLY);
    if (fd < 0) return 0;

    uint8_t buf[65536];
    ssize_t total = 0, n;
    while (total < (ssize_t)sizeof(buf) &&
           (n = read(fd, buf + total, sizeof(buf) - total)) > 0)
        total += n;
    close(fd);
    if (total < 4) return 0;

    int i = 0;
    while (i < total - 4) {
        uint8_t type = buf[i];
        uint8_t len  = buf[i + 1];
        if (i + len > total) break;
        if (type == 127) break; /* end-of-table */

        if (type == 4 && len >= 0x14) {
            uint16_t ext_clk;
            memcpy(&ext_clk, buf + i + 0x12, sizeof(ext_clk));
            if (ext_clk >= 80 && ext_clk <= 200)
                return (float)ext_clk;
        }

        /* advance past fixed part + string table (ends with double NUL) */
        int end = i + len;
        while (end < total - 1 && !(buf[end] == 0 && buf[end + 1] == 0))
            end++;
        i = end + 2;
    }
    return 0;
}

/* ── BCLK from MSR ──────────────────────────────────────────────────── */

static int read_p0_msr_fields(uint64_t *cpuFid_out, uint64_t *cpuDfsId_out)
{
    /*
     * CONFIG_X86_MSR=y — the device is always present without modprobe.
     * Use pread directly with the MSR address as the file offset.
     */
    int fd = open("/dev/cpu/0/msr", O_RDONLY);
    if (fd < 0) return 0;
    uint64_t val = 0;
    ssize_t n = pread(fd, &val, sizeof(val), (off_t)0xC0010064);
    close(fd);
    if (n != (ssize_t)sizeof(val)) return 0;

    *cpuFid_out   = val & 0xFF;
    *cpuDfsId_out = (val >> 8) & 0x3F;
    return 1;
}

/* Read physical core count from CPU topology.
 * Count unique (physical_package_id, core_id) pairs across all logical CPUs.
 * This correctly handles multi-CCD systems where core_id is per-CCD. */
static int read_physical_core_count(void)
{
    long max_cpu = sysconf(_SC_NPROCESSORS_CONF);
    if (max_cpu <= 0) max_cpu = 256;

    /* Dedup: key = (pkg_id << 16) | core_id — one entry per physical core */
    uint32_t seen[256];
    int nseen = 0;

    for (int cpu = 0; cpu < max_cpu; cpu++) {
        char path[256];
        int pkg_id = -1, core_id = -1;

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", cpu);
        FILE *f = fopen(path, "r");
        if (f) { if (fscanf(f, "%d", &pkg_id) != 1) pkg_id = -1; fclose(f); }

        snprintf(path, sizeof(path),
                 "/sys/devices/system/cpu/cpu%d/topology/core_id", cpu);
        f = fopen(path, "r");
        if (f) { if (fscanf(f, "%d", &core_id) != 1) core_id = -1; fclose(f); }

        if (pkg_id < 0 || core_id < 0) continue;

        uint32_t key = ((uint32_t)pkg_id << 16) | (uint32_t)core_id;
        int dup = 0;
        for (int i = 0; i < nseen; i++) {
            if (seen[i] == key) { dup = 1; break; }
        }
        if (!dup && nseen < 256) seen[nseen++] = key;
    }

    return nseen > 0 ? nseen : -1;
}

static int read_cpu_family(void)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return -1;
    char line[128];
    int family = -1;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "cpu family : %d", &family) == 1)
            break;
    }
    fclose(f);
    return family;
}

static float try_read_bclk(void)
{
    /*
     * AMD Zen BCLK derivation via MSR 0xC0010064 (P-state 0):
     *   bits [7:0]  = CpuFid
     *   bits [13:8] = CpuDfsId
     *
     * Zen1–4 (family 0x17, 0x19): CoreCOF = 200 * CpuFid / CpuDfsId
     *   → P0 multiplier = 2 * CpuFid / CpuDfsId
     * Zen5   (family 0x1A):       CoreCOF = 100 * CpuFid / CpuDfsId
     *   → P0 multiplier = CpuFid / CpuDfsId
     *
     * BCLK = cpuinfo_max_freq / p0_mult
     */
    uint64_t cpuFid = 0, cpuDfsId = 0;
    if (!read_p0_msr_fields(&cpuFid, &cpuDfsId)) return 0;
    if (cpuDfsId == 0 || cpuFid == 0) return 0;

    int family = read_cpu_family();
    double mult_factor = (family == 0x1A) ? 1.0 : 2.0;
    double p0_mult = ((double)cpuFid / (double)cpuDfsId) * mult_factor;
    if (p0_mult <= 0.1 || p0_mult > 200.0) return 0;

    /* Stable reference: kernel-reported max boost frequency */
    float ref_mhz = 0;
    const char *freq_files[] = {
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
        NULL
    };
    for (int i = 0; freq_files[i]; i++) {
        int khz = read_int_file(freq_files[i]);
        if (khz > 0) { ref_mhz = khz / 1000.0f; break; }
    }
    if (ref_mhz <= 0) return 0;

    float bclk = (float)ref_mhz / (float)p0_mult;
    return (bclk >= 80.0f && bclk <= 120.0f) ? bclk : 0;
}

/* ── AOD electrical parameters (Granite Ridge / family 1Ah) ────────────
 * Offsets and encodings ported from ZenStates-Core Hardware/Aod (MIT):
 * the per-frequency block is located by finding the LAST occurrence of
 * MemClk (u16 LE) in the raw AOD region; ProcODT/RTT/drive strengths are
 * fixed deltas from it. */

static uint32_t read_msr_u32(uint64_t addr, bool *ok)
{
    int fd = open("/dev/cpu/0/msr", O_RDONLY);
    if (fd < 0) { if (ok) *ok = false; return 0; }
    uint64_t v = 0;
    ssize_t n = pread(fd, &v, sizeof(v), (off_t)addr);
    close(fd);
    if (n != (ssize_t)sizeof(v)) { if (ok) *ok = false; return 0; }
    if (ok) *ok = true;
    return (uint32_t)v; /* EAX */
}

#define AOD_REGION_BYTES 0x24BB

static int read_aod_raw_table(uint8_t **out_buf, int *out_len)
{
    FILE *f = fopen("/sys/kernel/aod_voltages/full_raw", "rb");
    if (!f) return 0;
    uint8_t *buf = malloc(AOD_REGION_BYTES);
    if (!buf) { fclose(f); return 0; }
    size_t total = 0, n;
    while (total < AOD_REGION_BYTES &&
           (n = fread(buf + total, 1, AOD_REGION_BYTES - total, f)) > 0)
        total += n;
    fclose(f);
    if (total < 1024) { free(buf); return 0; }
    *out_buf = buf;
    *out_len = (int)total;
    return 1;
}

static void read_aod_electrical(int codename_idx, uint32_t mclk_mhz,
                                aod_mem_info_t *info)
{
    memset(info, 0, sizeof(*info));
    snprintf(info->layout, sizeof(info->layout), "n/a");

    /* The relative Granite Ridge layout is only verified for 23 (Granite
     * Ridge); other Zen5 parts would need their own layouts. */
    if (codename_idx != 23) {
        snprintf(info->layout, sizeof(info->layout),
                 "no AOD electrical layout for this CPU");
        return;
    }

    uint8_t *table = NULL;
    int len = 0;
    if (!read_aod_raw_table(&table, &len)) {
        snprintf(info->layout, sizeof(info->layout),
                 "AOD region unavailable (aod_voltages module?)");
        return;
    }

    /* CPU patch level (MSR 0x8B, ZenStates-Core GetPatchLevel) selects the
     * AGESA layout variant. Unknown → assume the newer one (0x1Bxxxxxx). */
    bool patch_ok = false;
    uint32_t patch = read_msr_u32(0x8B, &patch_ok);
    bool patch_gt = !patch_ok || patch > 0x0B404022u;

    /*
     * hasRMP is a Windows-WMI-only fact in ZenStates-Core and unknowable on
     * Linux; the two variants shift the ProcOdt/DS block by 12 bytes. Decode
     * both and keep whichever yields plausible codes. Override with
     * TUXTIMINGS_AOD_RMP=0/1 for validation.
     */
    const char *env = getenv("TUXTIMINGS_AOD_RMP");
    aod_mem_info_t cand[2];
    aod_mem_parse_granite_ridge(table, len, mclk_mhz, patch_gt, true,  &cand[0]);
    aod_mem_parse_granite_ridge(table, len, mclk_mhz, patch_gt, false, &cand[1]);
    free(table);

    int pick;
    if (env && (strcmp(env, "0") == 0 || strcmp(env, "1") == 0))
        pick = (env[0] == '0') ? 1 : 0;   /* cand[0]=RMP, cand[1]=no-RMP */
    else if (aod_mem_info_sanity(&cand[0]))
        pick = 0;                        /* prefer RMP layout when plausible */
    else if (aod_mem_info_sanity(&cand[1]))
        pick = 1;
    else
        pick = 0;                        /* nothing plausible: report cand[0] */

    *info = cand[pick];
    if (patch_ok) {
        char full[96];
        snprintf(full, sizeof(full), "%s patch=0x%08X", info->layout, patch);
        snprintf(info->layout, sizeof(info->layout), "%s", full);
    }
}

/* ── Read PM table binary ───────────────────────────────────────────── */

static int read_pm_table_raw(float **out_floats, int *out_count)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/pm_table", SMU_PATH);

    FILE *f = fopen(path, "rb");
    if (!f) { *out_floats = NULL; *out_count = 0; return 0; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 4) { fclose(f); *out_floats = NULL; *out_count = 0; return 0; }
    rewind(f);

    uint8_t *raw = malloc(sz);
    if (!raw) { fclose(f); *out_floats = NULL; *out_count = 0; return 0; }
    size_t rd = fread(raw, 1, sz, f);
    fclose(f);
    if ((int)rd < 4) { free(raw); *out_floats = NULL; *out_count = 0; return 0; }

    int count = (int)rd / 4;
    float *floats = malloc(count * sizeof(float));
    if (!floats) { free(raw); *out_floats = NULL; *out_count = 0; return 0; }
    memcpy(floats, raw, count * sizeof(float));
    free(raw);

    *out_floats = floats;
    *out_count = count;
    return 1;
}


/* ── Public API ─────────────────────────────────────────────────────── */

int backend_is_supported(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/version", SMU_PATH);
    if (file_exists(path)) return 1;

    /* Module may have been unloaded — try loading it now */
    const char *mp = access("/usr/bin/modprobe", X_OK) == 0 ? "/usr/bin/modprobe" :
                     access("/sbin/modprobe",    X_OK) == 0 ? "/sbin/modprobe"    :
                                                               "modprobe";
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s ryzen_smu 2>/dev/null", mp);
    run_shell(cmd);
    return file_exists(path);
}

void backend_read_summary(system_summary_t *out)
{
    memset(out, 0, sizeof(*out));

    /* Load kernel modules and cache static data on first call */
    if (!s_cached_static) {
        /* Use absolute path — pkexec strips PATH */
        const char *mp = access("/usr/bin/modprobe", X_OK) == 0 ? "/usr/bin/modprobe" :
                         access("/sbin/modprobe",    X_OK) == 0 ? "/sbin/modprobe"    :
                                                                   "modprobe";
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s msr 2>/dev/null", mp);
        run_shell(cmd);
        snprintf(cmd, sizeof(cmd), "%s ryzen_smu 2>/dev/null", mp);
        run_shell(cmd);

        /* Load aod_voltages — kernel module auto-detects offsets via SMU codename */
        if (!file_exists("/sys/kernel/aod_voltages/mem_vddio")) {
            snprintf(cmd, sizeof(cmd), "%s aod_voltages 2>/dev/null", mp);
            run_shell(cmd);
        }
        /* Always unload on exit — these modules are only useful while the app runs */
        if (file_exists("/sys/kernel/aod_voltages/mem_vddio"))
            s_loaded_aod_voltages = 1;
        if (file_exists("/sys/kernel/ryzen_smu_drv/version"))
            s_loaded_ryzen_smu = 1;
        if (access("/sys/module/tuxbench", F_OK) != 0) {
            snprintf(cmd, sizeof(cmd), "%s tuxbench 2>/dev/null", mp);
            run_shell(cmd);
        }
        if (access("/sys/module/tuxbench", F_OK) == 0)
            s_loaded_tuxbench = 1;

        /* Load a board-sensor Super I/O driver if no fan-capable hwmon chip
         * is active yet. All attempts are auto-detecting loads (read-only
         * ID probes; no-op when the chip is absent). On some kernels nct6775
         * is split into _core + _platform, so try both.
         * Gigabyte AM5 boards sometimes hide the chip's ID register behind
         * an unlock sequence — pass e.g. TUXTIMINGS_NCT_FORCE_ID=0xc7
         * (NCT6796D), 0xd4 (NCT6798D) or 0x8686/0x8696 (IT8686E/IT8696E
         * for the it87 driver) as a second step. */
        {
            char tmp_path[640];
            int found = 0;
            const char *chips[] = {"nct6", "it87", "w836", "f718", "nuvoton", NULL};
            for (int i = 0; chips[i] && !found; i++)
                found = find_hwmon_by_name(chips[i], tmp_path, sizeof(tmp_path));
            if (!found) {
                const char *mods[] = {"nct6775", "nct6775_platform", "it87",
                                      "f71882fg", "w83627hf", NULL};
                for (int i = 0; mods[i] && !found; i++) {
                    snprintf(cmd, sizeof(cmd), "%s %s 2>/dev/null", mp, mods[i]);
                    run_shell(cmd);
                    char p2[640];
                    if (find_hwmon_by_name("nct6", p2, sizeof(p2)) ||
                        find_hwmon_by_name("it87", p2, sizeof(p2)) ||
                        find_hwmon_by_name("w836", p2, sizeof(p2)) ||
                        find_hwmon_by_name("f718", p2, sizeof(p2)))
                        found = 1;
                }
                const char *fid = getenv("TUXTIMINGS_NCT_FORCE_ID");
                if (fid && !found) {
                    snprintf(cmd, sizeof(cmd),
                             "%s nct6775 force_id=%s 2>/dev/null", mp, fid);
                    run_shell(cmd);
                    snprintf(cmd, sizeof(cmd),
                             "%s it87 force_id=%s 2>/dev/null", mp, fid);
                    run_shell(cmd);
                }
            }
        }

        parse_dmidecode_processor();
        parse_dmidecode_board();
        parse_dmidecode_memory();
        read_agesa_version();
        s_physical_cores = read_physical_core_count();
        s_cached_static = 1;
    }

    /* CPU info */
    int codename_idx = read_codename_index();
    snprintf(out->cpu.name, STR_LEN, "AMD Ryzen (from ryzen_smu)");
    snprintf(out->cpu.processor_name, STR_LEN, "%s", s_processor_name);
    snprintf(out->cpu.codename, STR_SHORT, "%s", map_codename(codename_idx));
    read_smu_string("version", out->cpu.smu_version, STR_SHORT);

    uint32_t pm_ver = read_smu_uint32("pm_table_version");
    if (pm_ver)
        snprintf(out->cpu.pm_table_version, STR_SHORT, "PM table 0x%08X", pm_ver);

    /* Board info */
    snprintf(out->board.motherboard, STR_LEN, "%s", s_board_product);
    snprintf(out->board.bios_version, STR_LEN, "%s", s_bios_version);
    snprintf(out->board.bios_date, STR_SHORT, "%s", s_bios_date);
    snprintf(out->board.agesa_version, STR_LEN, "%s", s_agesa_version);
    snprintf(out->board.display_line, sizeof(out->board.display_line),
             "%s | BIOS %s (%s) | AGESA %s",
             s_board_product, s_bios_version, s_bios_date,
             s_agesa_version[0] ? s_agesa_version : "N/A");

    /* Modules (cached) */
    out->module_count = s_module_count;
    memcpy(out->modules, s_modules, s_module_count * sizeof(memory_module_t));
    /* PM table → metrics */
    float *pm_floats = NULL;
    int pm_count = 0;
    if (read_pm_table_raw(&pm_floats, &pm_count)) {
        pm_table_read(pm_ver, pm_floats, pm_count, s_physical_cores, &out->metrics);
        free(pm_floats);
    }

    /* BCLK: DMI (exact BIOS value) with MSR as fallback */
    out->metrics.bclk_mhz = try_read_bclk_dmi();
    if (out->metrics.bclk_mhz <= 0.0f)
        out->metrics.bclk_mhz = try_read_bclk();

    /* Memory voltages from aod_voltages kernel module sysfs */
    {
        int mv;
        mv = read_int_file("/sys/kernel/aod_voltages/mem_vddio");
        if (mv > 500 && mv < 3000) out->metrics.mem_vdd    = mv / 1000.0f;
        mv = read_int_file("/sys/kernel/aod_voltages/mem_vddq");
        if (mv > 500 && mv < 3000) out->metrics.mem_vddq   = mv / 1000.0f;
        mv = read_int_file("/sys/kernel/aod_voltages/mem_vpp");
        if (mv > 500 && mv < 3000) out->metrics.mem_vpp    = mv / 1000.0f;
        mv = read_int_file("/sys/kernel/aod_voltages/cpu_vddio");
        if (mv > 500 && mv < 3000) out->metrics.cpu_vddio  = mv / 1000.0f;
    }

    /* hwmon overlays */
    apply_per_core_temps_hwmon(&out->metrics);
    apply_k10temp_tctl_tccd(&out->metrics);
    read_spd_temps(&out->metrics);

    /* Per-core usage and frequency */
    read_core_usage(&out->metrics);
    read_core_freq(&out->metrics);

    /* DRAM timings — one entry per probed UMC instance (Zen 5: UMC0/UMC1) */
    {
        bool is_ddr5 = (codename_idx >= 20 && codename_idx <= 24);
        float mem_hint = out->metrics.mclk_mhz > 0.0f ? out->metrics.mclk_mhz * 2.0f : 0.0f;
        dram_read_channels(is_ddr5, mem_hint, out->metrics.bclk_mhz,
                           out->channels, &out->channel_count);
        out->present_channels = 0;
        for (int i = 0; i < out->channel_count; i++)
            if (out->channels[i].present) out->present_channels++;

        /* Relate SMBIOS modules to UMC channels: the slot-label letter
         * (A1/A2...) matches UMC instance i where offset(i<<20), the same
         * convention ZenStates-Core's ReadChannelsNoLock uses for slot
         * naming. */
        for (int i = 0; i < s_module_count; i++) {
            out->modules[i].channel_idx = -1;
            char l = out->modules[i].slot_label[0];
            if (!isalpha((unsigned char)l)) continue;
            int letter = toupper((unsigned char)l) - 'A';
            for (int c = 0; c < out->channel_count; c++) {
                if (!out->channels[c].present) continue;
                if ((int)(out->channels[c].smn_offset >> 20) == letter) {
                    out->modules[i].channel_idx = (int16_t)c;
                    break;
                }
            }
        }
    }

    /* Electrical parameters from the AOD ACPI table (Granite Ridge) */
    read_aod_electrical(codename_idx,
                        (uint32_t)(out->metrics.mclk_mhz > 0.0f ? out->metrics.mclk_mhz : 0.0f),
                        &out->aod);

    /* Memory config */
    out->memory.type = mem_type_for_codename(codename_idx);
    float mem_freq = 0.0f;
    for (int i = 0; i < out->channel_count; i++) {
        if (out->channels[i].present && out->channels[i].t.frequency_hint_mhz > 0) {
            mem_freq = out->channels[i].t.frequency_hint_mhz;
            break;
        }
    }

    /* dmidecode "Configured Memory Speed" (MT/s) */
    uint32_t max_cfg = 0;
    for (int i = 0; i < s_module_count; i++) {
        if (s_modules[i].clock_speed_mhz > max_cfg)
            max_cfg = s_modules[i].clock_speed_mhz;
    }
    if (out->memory.type == MEM_DDR4) {
        /* For DDR4, prefer SMN-derived effective MT/s; fall back to dmidecode. */
        if (mem_freq <= 0.0f && max_cfg > 0)
            mem_freq = (float)max_cfg;
    } else {
        /* Non-DDR4: prefer timing-derived hint, else fall back to MCLK MHz. */
        if (mem_freq == 0 && out->metrics.mclk_mhz > 0)
            mem_freq = out->metrics.mclk_mhz;
        else if (mem_freq <= 0.0f && max_cfg > 0)
            mem_freq = (float)max_cfg;
    }
    out->memory.frequency = mem_freq;
    read_total_memory(out->memory.total_capacity, sizeof(out->memory.total_capacity));

    /* Build part number string from unique module part numbers */
    char *pn_buf = out->memory.part_number;
    pn_buf[0] = '\0';
    for (int i = 0; i < s_module_count; i++) {
        if (s_modules[i].part_number[0] == '\0') continue;
        /* check duplicate */
        if (strstr(pn_buf, s_modules[i].part_number)) continue;
        if (pn_buf[0] != '\0') strncat(pn_buf, ", ", STR_LEN - strlen(pn_buf) - 1);
        strncat(pn_buf, s_modules[i].part_number, STR_LEN - strlen(pn_buf) - 1);
    }

    /* Fans */
    read_fans(out->fans, &out->fan_count);
}

static void rmmod_module(const char *rm, const char *module)
{
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execl(rm, rm, module, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

void backend_cleanup(void)
{
    const char *rm = access("/usr/bin/rmmod", X_OK) == 0 ? "/usr/bin/rmmod" :
                     access("/sbin/rmmod",    X_OK) == 0 ? "/sbin/rmmod"    :
                                                           "/usr/bin/rmmod";

    /* Use fork+execl: safe from both normal exit and signal handlers.
     * system()/popen() are not async-signal-safe and silently fail in signals. */
    if (s_loaded_aod_voltages) {
        s_loaded_aod_voltages = 0;
        rmmod_module(rm, "aod_voltages");
    }
    if (s_loaded_ryzen_smu) {
        s_loaded_ryzen_smu = 0;
        rmmod_module(rm, "ryzen_smu");
    }
    if (s_loaded_tuxbench) {
        s_loaded_tuxbench = 0;
        rmmod_module(rm, "tuxbench");
    }
}

/* ── Debug dump ─────────────────────────────────────────────────────── */

/* One-line "%s: %u" for a decoded timing (used by the channel dump). */
#define DT(lbl, val) \
    do { off += (size_t)snprintf(buf + off, cap - off, "  %-18s %u\n", lbl, (unsigned)(val)); } while (0)
#define DTS(lbl, s) \
    do { off += (size_t)snprintf(buf + off, cap - off, "  %-18s %s\n", lbl, (s)); } while (0)
#define DTF(lbl, fmt, ...) \
    do { off += (size_t)snprintf(buf + off, cap - off, "  %-18s " fmt "\n", lbl, ##__VA_ARGS__); } while (0)

static const char *YN(bool v) { return v ? "Enabled" : "Disabled"; }
static const char *rank_str(int r) { return r == 2 ? "DR" : r == 1 ? "SR" : "N/A"; }

static void dump_channel(char *buf, size_t cap, size_t *offp,
                         const dram_channel_t *d, const umc_raw_regs_t *raw)
{
    size_t off = *offp;
    DTS("Label", d->label);
    DTF ("SMN offset", "0x%08X", d->smn_offset);
    DTS("Channel present", d->present ? "yes" : "NO (absent/disabled)");
    DTF ("SMN read failures", "%d", d->read_failures);
    if (!d->present && d->read_failures == 0) {
        DTF ("0x50DF0 raw", "0x%08X (bit19=%u -> %s)", raw->enabled,
             (unsigned)bit_slice(raw->enabled, 19, 19),
             bit_slice(raw->enabled, 19, 19) ? "channel disabled" : "enabled but no DIMMs");
    }
    if (d->present) {
        DTF ("DIMM0 / DIMM1", "%s / %s", d->has_dimm[0] ? "installed" : "empty",
             d->has_dimm[1] ? "installed" : "empty");
        DTF ("Rank DIMM0/1", "%s / %s", rank_str(d->ranks[0]), rank_str(d->ranks[1]));
        if (d->has_addr_cfg) {
            DTF ("banks/BG/row/col", "%u/%u/%u/%u", d->num_banks, d->num_bank_groups,
                 d->num_row, d->num_col);
        }
        DTF ("Ratio (MEMCLK)", "%.2f", (double)d->ratio);
        DTF ("Speed", "%.0f MT/s", (double)d->t.frequency_hint_mhz);
        DT ("tCL", d->t.tcl); DT ("tRCDRD", d->t.trcd_rd); DT ("tRCDWR", d->t.trcd_wr);
        DT ("tRP", d->t.trp); DT ("tRPpb", d->rp_pb); DT ("tRAS", d->t.tras);
        DT ("tRC", d->t.trc); DT ("tRCpb", d->rc_pb);
        DT ("tRRDS", d->t.trrds); DT ("tRRDL", d->t.trrdl); DT ("tFAW", d->t.tfaw);
        DT ("tWR", d->t.twr); DT ("tCWL", d->t.tcwl); DT ("tRTP", d->t.rtp);
        DT ("tWTRS", d->t.wtrs); DT ("tWTRL", d->t.wtrl);
        DT ("tRDWR", d->t.rdwr); DT ("tWRRD", d->t.wrrd); DT ("MW", d->mw);
        DT ("tRDRDSC", d->t.rdrd_sc); DT ("tRDRDSD", d->t.rdrd_sd); DT ("tRDRDDD", d->t.rdrd_dd);
        DT ("tRDRDSCL", d->t.rdrd_scl); DT ("tRDRDBan", d->rdrd_ban);
        DT ("tWRWRSC", d->t.wrwr_sc); DT ("tWRWRSD", d->t.wrwr_sd); DT ("tWRWRDD", d->t.wrwr_dd);
        DT ("tWRWRSCL", d->t.wrwr_scl); DT ("tWRWRBan", d->wrwr_ban);
        DT ("tREFI", d->t.refi); DTF ("tREFI ns", "%.1f", (double)d->t.trefi_ns);
        DT ("tRFC", d->t.rfc); DTF ("tRFC ns", "%.1f", (double)d->t.trfc_ns);
        DT ("tRFC2", d->t.rfc2); DTF ("tRFC2 ns", "%.1f", (double)d->t.trfc2_ns);
        DT ("tRFCsb", d->t.rfcsb); DTF ("tRFCsb ns", "%.1f", (double)d->t.trfcsb_ns);
        DT ("tRDPRE", d->t.rdpre); DT ("tRDPOST", d->rdpost);
        DT ("tWRPRE", d->t.wrpre); DT ("tWRPOST", d->wrpost);
        DT ("tPPD", d->ppd); DT ("tTRCPAGE", d->t.trc_page);
        DT ("tCKE", d->t.cke); DT ("tXP", d->t.xp); DT ("tCPDED", d->cpded);
        DT ("tSTAG", d->t.stag); DT ("tSTAGsb", d->t.stag_sb);
        DT ("tMOD", d->t.mod); DT ("tMODPDA", d->t.mod_pda);
        DT ("tMRD", d->t.mrd); DT ("tMRDPDA", d->t.mrd_pda);
        DT ("tMRRI", d->mrri); DT ("tMRR", d->mrr); DT ("tMRW", d->mrw);
        DT ("CtrlSwitchClks", d->ctrl_switch_clks);
        DT ("tCSH", d->csh); DT ("tCACSH", d->cacsh);
        DT ("tSRX2SRX", d->srx2srx); DT ("tSTAB", d->tstab);
        DT ("tDLLK", d->dllk); DT ("tXS", d->xs);
        DT ("tZQCS", d->tzqcs); DT ("tZQOPER", d->tzq_oper_cal);
        DT ("ZQCSInterval", d->zqcs_interval); DT ("ShortInit", d->short_init);
        DT ("RankBusyDly", d->rank_busy_dly); DT ("CmdParLatency", d->cmd_par_latency);
        DT ("AlertParDly", d->alert_par_dly); DT ("AlertCrcDly", d->alert_crc_dly);
        DT ("AlertParPulse", d->alert_par_pulse); DT ("AlertCrcPulse", d->alert_crc_pulse);
        DT ("PARINL", d->parinl); DT ("RDDATAEN", d->rddataen);
        DT ("LpDly", d->lp_dly); DT ("LpExitDly", d->lp_exit_dly);
        DT ("WrBrstGap", d->wrbrst_gap); DT ("RdBrstGap", d->rdbrst_gap);
        DT ("WrPhChk", d->wr_ph_chk);
        DT ("WRMPR", d->wrmpr); DT ("CmdStgCnt", d->cmd_stg_cnt); DT ("RcvrWait", d->rcvr_wait);
        DT ("PHYWRD", d->t.phy_wrd); DT ("PHYWRL", d->t.phy_wrl); DT ("PHYRDL", d->t.phy_rdl);
        DTF ("PHYUPD cmd/wrdat/resp", "%u/%u/%u", d->phyupd_cmd_dly,
             d->phyupd_wrdat_dly, d->phyupd_resp);
        DT ("ECSc", d->ecsc);
        if (d->ccdl_wr2_valid) DT ("CcdlWr2", d->ccdl_wr2);
        DT ("DdrMaxRate", d->ddr_max_rate); DT ("DdrMaxRateEnf", d->ddr_max_rate_enf);
        DT ("AggrPwrDownDly", d->aggr_pwr_down_dly); DT ("PwrDownDly", d->pwr_down_dly);
        DT ("PD", d->pd);
        /* config */
        DTS("GDM", YN(d->t.gdm_enabled));
        DTS("Cmd2T", d->t.cmd2t[0] ? d->t.cmd2t : "N/A");
        DTS("PowerDown", YN(d->t.power_down_enabled));
        DTS("PowerDownMode", d->power_down_mode[0] ? d->power_down_mode : "N/A");
        DTS("AggrPowerDown", YN(d->aggressive_power_down));
        DTS("RefreshMode", d->refresh_mode[0] ? d->refresh_mode : "N/A");
        DT ("FGR", d->fgr);
        DTS("BGS", YN(d->bgs));
        DTS("BGSAlt", YN(d->bgs_alt));
        DTS("BankGroupEn", YN(d->bank_group_en));
        DTS("ECC", YN(d->ecc_enabled));
        DTS("BurstLength", d->burst_length[0] ? d->burst_length : "N/A");
        DTS("BurstCtrl", d->burst_ctrl[0] ? d->burst_ctrl : "N/A");
        DTS("ODTS", YN(d->odts_en));
        DT ("OdtsCmdThrotCyc", d->odts_cmd_throt_cyc);
        DT ("OdtsReadInterval", d->odts_read_interval);
        DT ("RollWindowDepth", d->roll_window_depth);
        DTS("WCK:CK", d->wck_ck_ratio);
        DT ("UclkGtFclk", d->uclk_gt_fclk);
        DTS("Nitro (rx/tx/ctrl)", d->nitro_rx_tx_ctrl[0] ? d->nitro_rx_tx_ctrl : "N/A");
        DTS("Nitro DlyMode", d->nitro_dly_mode[0] ? d->nitro_dly_mode : "N/A");
    }
    *offp = off;
}

static void dump_channel_raw(char *buf, size_t cap, size_t *offp,
                             uint32_t base, const umc_raw_regs_t *raw)
{
    static const struct { uint32_t reg; size_t off; const char *name; } tbl[] = {
        { 0x50DF0, offsetof(umc_raw_regs_t, enabled),       "ENABLED/DDRMAXRATE" },
        { 0x50DF4, offsetof(umc_raw_regs_t, regDF4),        "DDRRATEENF" },
        { 0x50000, offsetof(umc_raw_regs_t, dimm0_config),  "DIMM0/CS0_CFG" },
        { 0x50004, offsetof(umc_raw_regs_t, cs1_cfg),       "CS1_CFG" },
        { 0x50008, offsetof(umc_raw_regs_t, dimm1_config),  "DIMM1/CS2_CFG" },
        { 0x5000C, offsetof(umc_raw_regs_t, cs3_cfg),       "CS3_CFG" },
        { 0x50020, offsetof(umc_raw_regs_t, rank0_mask[0]), "RANK0_MASK0" },
        { 0x50024, offsetof(umc_raw_regs_t, rank0_mask[1]), "RANK0_MASK1" },
        { 0x50028, offsetof(umc_raw_regs_t, rank1_mask[0]), "RANK1_MASK0" },
        { 0x5002C, offsetof(umc_raw_regs_t, rank1_mask[1]), "RANK1_MASK1" },
        { 0x50040, offsetof(umc_raw_regs_t, addr_cfg0),     "ADDR_CFG0" },
        { 0x50048, offsetof(umc_raw_regs_t, addr_cfg1),     "ADDR_CFG1" },
        { 0x50050, offsetof(umc_raw_regs_t, bgs0),          "BGS0" },
        { 0x50058, offsetof(umc_raw_regs_t, bgs1),          "BGS1" },
        { 0x500D0, offsetof(umc_raw_regs_t, bgs_alt0),      "BGS_ALT0" },
        { 0x500D4, offsetof(umc_raw_regs_t, bgs_alt1),      "BGS_ALT1" },
        { 0x50100, offsetof(umc_raw_regs_t, ecc_burst),     "ECC/BURST" },
        { 0x5012C, offsetof(umc_raw_regs_t, pwrdownctrl),   "PWRDOWNCTRL" },
        { 0x50130, offsetof(umc_raw_regs_t, odtsctrl),      "ODTSCTRL" },
        { 0x50198, offsetof(umc_raw_regs_t, brstgap),       "BRSTGAP" },
        { 0x50200, offsetof(umc_raw_regs_t, umcratio),      "UMCRATIO" },
        { 0x50204, offsetof(umc_raw_regs_t, reg204),        "TIMING1" },
        { 0x50208, offsetof(umc_raw_regs_t, reg208),        "TIMING2" },
        { 0x5020C, offsetof(umc_raw_regs_t, reg20C),        "TIMING3" },
        { 0x50210, offsetof(umc_raw_regs_t, reg210),        "FAW" },
        { 0x50214, offsetof(umc_raw_regs_t, reg214),        "CW/WTR" },
        { 0x50218, offsetof(umc_raw_regs_t, reg218),        "WR" },
        { 0x5021C, offsetof(umc_raw_regs_t, reg21C),        "TRCPAGE/PPD" },
        { 0x50220, offsetof(umc_raw_regs_t, reg220),        "RDRD" },
        { 0x50224, offsetof(umc_raw_regs_t, reg224),        "WRWR" },
        { 0x50228, offsetof(umc_raw_regs_t, reg228),        "RDWR/WRRD/MW" },
        { 0x5022C, offsetof(umc_raw_regs_t, reg22C),        "ZQ" },
        { 0x50230, offsetof(umc_raw_regs_t, reg230),        "REFI/ODTSINT" },
        { 0x50234, offsetof(umc_raw_regs_t, reg234),        "MOD/MRD" },
        { 0x50238, offsetof(umc_raw_regs_t, reg238),        "DLLK/XS" },
        { 0x5023C, offsetof(umc_raw_regs_t, reg23C),        "RANKBUSY/ALERT" },
        { 0x50240, offsetof(umc_raw_regs_t, reg240),        "MRR/MRW" },
        { 0x50244, offsetof(umc_raw_regs_t, reg244),        "PWRDOWN DLY" },
        { 0x50248, offsetof(umc_raw_regs_t, reg248),        "SRX2SRX/STAB" },
        { 0x5024C, offsetof(umc_raw_regs_t, reg24C),        "ALERT PULSE" },
        { 0x50250, offsetof(umc_raw_regs_t, reg250),        "STAG" },
        { 0x50254, offsetof(umc_raw_regs_t, reg254),        "CKE/XP/CACSH" },
        { 0x50258, offsetof(umc_raw_regs_t, reg258),        "PHY/PARINL" },
        { 0x5025C, offsetof(umc_raw_regs_t, reg25C),        "LP DLY" },
        { 0x50260, offsetof(umc_raw_regs_t, trfc[0]),       "TRFC0" },
        { 0x50264, offsetof(umc_raw_regs_t, trfc[1]),       "TRFC1" },
        { 0x50268, offsetof(umc_raw_regs_t, trfc[2]),       "TRFC2" },
        { 0x5026C, offsetof(umc_raw_regs_t, trfc[3]),       "TRFC3" },
        { 0x50284, offsetof(umc_raw_regs_t, reg284),        "NITRO" },
        { 0x50288, offsetof(umc_raw_regs_t, reg288),        "PHYUPD" },
        { 0x5028C, offsetof(umc_raw_regs_t, reg28C),        "WRMPR/RCVR" },
        { 0x502A4, offsetof(umc_raw_regs_t, reg2A4),        "RD/WR PRE/POST" },
        { 0x502A8, offsetof(umc_raw_regs_t, reg2A8),        "ECSc" },
        { 0x502C0, offsetof(umc_raw_regs_t, rfcsb[0]),      "RFCSB0" },
        { 0x502C4, offsetof(umc_raw_regs_t, rfcsb[1]),      "RFCSB1" },
        { 0x502C8, offsetof(umc_raw_regs_t, rfcsb[2]),      "RFCSB2" },
        { 0x502CC, offsetof(umc_raw_regs_t, rfcsb[3]),      "RFCSB3" },
        { 0x502E0, offsetof(umc_raw_regs_t, reg2E0),        "CCDL WR2" },
    };
    size_t off = *offp;
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        uint32_t v;
        memcpy(&v, (const uint8_t *)raw + tbl[i].off, sizeof(v));
        off += (size_t)snprintf(buf + off, cap - off, "  0x%05X (smn 0x%06X) %-19s 0x%08X\n",
                                tbl[i].reg, base | tbl[i].reg, tbl[i].name, v);
        if (off + 256 >= cap) break;
    }
    *offp = off;
}

static void dump_electrical(char *buf, size_t cap, size_t *offp, const aod_mem_info_t *a)
{
    size_t off = *offp;
    DTS("AOD layout", a->layout);
    DTF ("AOD freq base", "%d", a->base);
    if (!a->available) {
        DTS("Electrical", "N/A — see layout line above");
        *offp = off;
        return;
    }
    char tmp[48];
#define EL(lbl, has, raw, fn) \
    do { if (has) { DTS(lbl, fn(raw)); DTF(lbl " raw", "0x%X", (unsigned)(raw)); } \
         else DTS(lbl, "N/A"); } while (0)
#define ELR(lbl, has, raw) \
    do { if (has) { rtt_decode((raw), tmp, sizeof(tmp)); DTS(lbl, tmp); \
                    DTF(lbl " raw", "0x%X", (unsigned)(raw)); } \
         else DTS(lbl, "N/A"); } while (0)
    EL("ProcODT",         a->has_proc_odt,     a->proc_odt_raw,     proc_odt_decode);
    EL("ProcODT PullUp",  a->has_proc_odt_pu,  a->proc_odt_pu_raw,  proc_odt_decode);
    EL("ProcODT PullDown",a->has_proc_odt_pd,  a->proc_odt_pd_raw,  proc_odt_decode);
    ELR("RTT Nom WR",     a->has_rtt_nom_wr,   a->rtt_nom_wr_raw);
    ELR("RTT Nom RD",     a->has_rtt_nom_rd,   a->rtt_nom_rd_raw);
    ELR("RTT WR",         a->has_rtt_wr,       a->rtt_wr_raw);
    ELR("RTT Park",       a->has_rtt_park,     a->rtt_park_raw);
    ELR("RTT Park DQS",   a->has_rtt_park_dqs, a->rtt_park_dqs_raw);
    EL("CADBUS DrvStren", a->has_cad_bus_ds,   a->cad_bus_ds_raw,   cad_bus_ds_decode);
    EL("Proc Data DQ DS", a->has_proc_data_ds, a->proc_data_ds_raw, proc_data_ds_decode);
    EL("ProcDqDs PullUp", a->has_proc_dq_ds_pu,a->proc_dq_ds_pu_raw,proc_data_ds_decode);
    EL("ProcDqDs PullDn", a->has_proc_dq_ds_pd,a->proc_dq_ds_pd_raw,proc_data_ds_decode);
    EL("ProcCsDs",        a->has_proc_cs_ds,   a->proc_cs_ds_raw,   proc_impedance_decode);
    EL("ProcCkDs",        a->has_proc_ck_ds,   a->proc_ck_ds_raw,   proc_impedance_decode);
    EL("Dram Data DS",    a->has_dram_data_ds, a->dram_data_ds_raw, dram_data_ds_decode);
    EL("DramDqDs PullUp", a->has_dram_dq_ds_pu,a->dram_dq_ds_pu_raw,dram_data_ds_decode);
    EL("DramDqDs PullDn", a->has_dram_dq_ds_pd,a->dram_dq_ds_pd_raw,dram_data_ds_decode);
#undef EL
#undef ELR
    *offp = off;
}

char *backend_read_debug_dump(void)
{
    /* Allocate a large enough buffer (PM table can be ~2000 entries * ~20 chars) */
    size_t cap = 256 * 1024;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t off = 0;

#define DUMP(fmt, ...) \
    do { off += (size_t)snprintf(buf + off, cap - off, fmt, ##__VA_ARGS__); } while (0)

    /* ── SMU sysfs values ─────────────────────────────────────────── */
    DUMP("=== SMU Info ===\n");
    {
        char tmp[256];
        if (read_smu_string("version", tmp, sizeof(tmp)))
            DUMP("SMU Version:        %s\n", tmp);
        if (read_smu_string("codename", tmp, sizeof(tmp)))
            DUMP("Codename:           %s\n", tmp);
        if (read_smu_string("mp1_if_version", tmp, sizeof(tmp)))
            DUMP("MP1 IF Version:     %s\n", tmp);
        uint32_t pm_ver = read_smu_uint32("pm_table_version");
        if (pm_ver) DUMP("PM Table Version:   0x%08X\n", pm_ver);
    }

    /* ── AOD scan ─────────────────────────────────────────────────── */
    if (file_exists("/sys/kernel/aod_voltages/scan")) {
        DUMP("\n--- AOD scan (/sys/kernel/aod_voltages/scan) ---\n");
        FILE *sf = fopen("/sys/kernel/aod_voltages/scan", "r");
        if (sf) {
            char line[512];
            while (fgets(line, sizeof(line), sf)) {
                DUMP("%s", line);
                if (off + 4096 >= cap) {
                    DUMP("\n... (AOD scan truncated, buffer full)\n");
                    break;
                }
            }
            fclose(sf);
        } else {
            DUMP("  (failed to read /sys/kernel/aod_voltages/scan)\n");
        }
    } else {
        DUMP("\n--- AOD scan: unavailable (aod_voltages module not loaded) ---\n");
    }

    /* ── PM table raw floats ──────────────────────────────────────── */
    DUMP("\n=== PM Table Raw Entries ===\n");
    {
        float *t = NULL;
        int count = 0;
        if (read_pm_table_raw(&t, &count)) {
            DUMP("Total entries: %d\n\n", count);
            DUMP("%-8s  %-14s\n", "Index", "Value");
            DUMP("%-8s  %-14s\n", "-----", "-----");
            for (int i = 0; i < count; i++) {
                DUMP("[%4d]    %14.6f\n", i, (double)t[i]);
                /* Resize guard — leave 4KB headroom */
                if (off + 4096 >= cap) {
                    DUMP("\n... (truncated at entry %d, buffer full)\n", i);
                    break;
                }
            }
            free(t);
        } else {
            DUMP("(PM table unavailable)\n");
        }
    }

    /* ── DRAM: UMC topology, per-channel decode, raw registers ──── */
    DUMP("\n=== DRAM / UMC ===\n");
    {
        int codename_idx = read_codename_index();
        bool is_ddr5 = (codename_idx >= 20 && codename_idx <= 24);
        DUMP("CPU:            %s (SMU codename %d, %s)\n",
             map_codename(codename_idx), codename_idx,
             is_ddr5 ? "DDR5 class" : "DDR4 class");
        bool patch_ok = false;
        uint32_t patch = read_msr_u32(0x8B, &patch_ok);
        if (patch_ok)
            DUMP("Patch level:    0x%08X%s\n", patch,
                 patch > 0x0B404022u ? " (> 0xB404022: new AOD layout)" : "");
        else
            DUMP("Patch level:    N/A (MSR 0x8B unreadable)\n");

        if (!dram_smn_open()) {
            DUMP("SMN:            UNAVAILABLE (%s not readable)\n", "/sys/kernel/ryzen_smu_drv/smn");
        } else {
            static dram_channel_t chans[MAX_UMC_CHANNELS];
            int chan_count = 0;
            dram_read_channels(is_ddr5, 0.0f, 0.0f, chans, &chan_count);
            DUMP("UMC instances probed: %d (SMN offsets 0x000000, 0x100000, ...)\n\n", chan_count);
            for (int i = 0; i < chan_count; i++) {
                DUMP("--- %s ---\n", chans[i].label);
                dump_channel(buf, cap, &off, &chans[i], &chans[i].raw);
                if (chans[i].present) {
                    DUMP("\n  Raw registers:\n");
                    dump_channel_raw(buf, cap, &off, chans[i].smn_offset, &chans[i].raw);
                }
                DUMP("\n");
                if (off + 8192 >= cap) break;
            }
            DUMP("SMBIOS modules (inputs to physical-slot pairing):\n");
            for (int i = 0; i < s_module_count; i++) {
                DUMP("  [%d] slot=%-6s bank='%s' locator='%s'\n", i,
                     s_modules[i].slot_label, s_modules[i].bank_label,
                     s_modules[i].device_locator);
            }
            DUMP("\n");
        }

        static aod_mem_info_t aod;
        read_aod_electrical(codename_idx, 3000 /* MCLK hint fallback */, &aod);
        /* Retry with the PM-table MCLK when it is available */
        {
            float *t = NULL; int c = 0;
            uint32_t pm_ver = read_smu_uint32("pm_table_version");
            smu_metrics_t m; memset(&m, 0, sizeof(m));
            if (read_pm_table_raw(&t, &c)) {
                pm_table_read(pm_ver, t, c, -1, &m);
                free(t);
            }
            if (m.mclk_mhz > 0)
                read_aod_electrical(codename_idx, (uint32_t)m.mclk_mhz, &aod);
        }
        DUMP("=== Electrical (AOD) ===\n");
        dump_electrical(buf, cap, &off, &aod);
    }

    /* ── hwmon inventory (fan/thermal chip visibility) ──────────── */
    DUMP("\n=== hwmon devices ===\n");
    {
        DIR *hw = opendir("/sys/class/hwmon");
        if (!hw) {
            DUMP("(cannot open /sys/class/hwmon)\n");
        } else {
            struct dirent *e;
            while ((e = readdir(hw))) {
                if (e->d_name[0] == '.') continue;
                char p[640], nm[64];
                snprintf(p, sizeof(p), "/sys/class/hwmon/%s/name", e->d_name);
                if (!read_file_string(p, nm, sizeof(nm))) continue;
                int fans = 0;
                for (int i = 1; i <= 15; i++) {
                    char fp[640];
                    snprintf(fp, sizeof(fp), "/sys/class/hwmon/%s/fan%d_input", e->d_name, i);
                    if (read_int_file(fp) > 0) fans++;
                }
                DUMP("%-8s %-24s fan_inputs=%d\n", e->d_name, nm, fans);
            }
            closedir(hw);
        }
    }

#undef DUMP
    return buf;
}
