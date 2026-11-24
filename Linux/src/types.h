#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "dram_decode.h"

#define MAX_CORES       16
#define MAX_MODULES     4
#define MAX_FANS        8
#define STR_LEN         256
#define STR_SHORT       64

typedef enum { MEM_UNKNOWN, MEM_DDR4, MEM_DDR5, MEM_LPDDR4, MEM_LPDDR5 } mem_type_t;
typedef enum { RANK_SR, RANK_DR, RANK_QR } mem_rank_t;

typedef struct {
    char bank_label[STR_LEN];
    char device_locator[STR_LEN];
    char manufacturer[STR_LEN];
    char part_number[STR_LEN];
    char serial_number[STR_LEN];
    uint64_t capacity_bytes;
    uint32_t clock_speed_mhz;
    mem_rank_t rank;
    /* Derived */
    char slot_label[24];   /* e.g. "A1", "Slot 0" */
    char slot_display[64]; /* e.g. "A1 - 16.0 GiB" */
    char capacity_display[32]; /* e.g. "16.0 GiB" */
    int16_t channel_idx;   /* entry in system_summary_t.channels, -1 = unknown */
} memory_module_t;

typedef struct {
    float frequency;       /* MT/s */
    mem_type_t type;
    char total_capacity[32];
    char part_number[STR_LEN];
} memory_config_t;

typedef struct {
    char name[STR_LEN];
    char processor_name[STR_LEN];
    char codename[STR_SHORT];
    char smu_version[STR_SHORT];
    char pm_table_version[STR_SHORT];
} cpu_info_t;

typedef struct {
    char motherboard[STR_LEN];
    char bios_version[STR_LEN];
    char bios_date[STR_SHORT];
    char agesa_version[STR_LEN];
    char display_line[1024];
} board_info_t;

typedef struct {
    /* Power */
    float package_power_w; /* reserved — populated but not yet displayed */
    float ppt_w;
    float package_current_a; /* reserved — populated but not yet displayed */
    /* PBO limits (has_* false → value unavailable, render N/A) */
    float ppt_limit_w;     bool has_ppt_limit;
    float tdc_limit_a;     bool has_tdc_limit;
    float tdc_current_a;   bool has_tdc_current;
    float edc_limit_a;     bool has_edc_limit;
    float thermal_limit_c; bool has_thermal_limit;
    bool  has_socket_power;
    /* Voltages */
    float vcore, vsoc, vddp, vddg_ccd, vddg_iod, vdd_misc;
    float cpu_vddio, mem_vdd, mem_vddq, mem_vpp;
    float vid;
    /* Clocks */
    float core_clock_mhz, bclk_mhz; /* reserved */
    float fclk_mhz, uclk_mhz, mclk_mhz;
    float memory_clock_mhz; /* reserved */
    float core_clocks_ghz[MAX_CORES]; /* reserved */
    int   core_clocks_count; /* reserved */
    /* Temps */
    float cpu_temp_c;
    float core_temps_c[MAX_CORES];
    int   core_temps_count;
    float tdie_c;   bool has_tdie;
    float tctl_c;   bool has_tctl;
    float tccd1_c;  bool has_tccd1;
    float tccd2_c;  bool has_tccd2;
    float iod_hotspot_c; bool has_iod_hotspot;
    /* Per-core */
    float core_voltages[MAX_CORES];
    int   core_voltages_count;
    float core_power_w[MAX_CORES];   int core_power_count;
    float core_c0_pct[MAX_CORES];
    float core_cc1_pct[MAX_CORES];
    float core_cc6_pct[MAX_CORES];   int core_res_count;
    float core_usage_pct[MAX_CORES];
    int   core_usage_count;
    float core_freq_mhz[MAX_CORES];
    int   core_freq_count;
    /* SPD temps */
    float spd_temps_c[MAX_MODULES];
    int   spd_temps_count;
} smu_metrics_t;


/* dram_timings_t moved verbatim to dram_decode.h (per-channel structs embed
 * it); that header is included at the top. */

typedef struct {
    char label[16];
    int rpm;
} fan_reading_t;

typedef struct {
    cpu_info_t cpu;
    memory_config_t memory;
    board_info_t board;
    memory_module_t modules[MAX_MODULES];
    int module_count;
    smu_metrics_t metrics;
    /* Per-UMC-instance memory channels (decoded in dram_decode.c) */
    dram_channel_t channels[MAX_UMC_CHANNELS];
    int channel_count;      /* probed instances (present flag per entry) */
    int present_channels;   /* how many report DIMMs */
    aod_mem_info_t aod;     /* electrical parameters from ACPI AOD table */
    fan_reading_t fans[MAX_FANS];
    int fan_count;
} system_summary_t;

#endif /* TYPES_H */
