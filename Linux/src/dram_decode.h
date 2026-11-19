#ifndef DRAM_DECODE_H
#define DRAM_DECODE_H

/*
 * Pure register-decoding layer (no hardware access).
 *
 * Register maps and bit fields ported from ZenStates-Core (ZenTimings),
 * MIT licensed — see Hardware/DRAM/Ddr5Timings.cs, Hardware/DRAM/BaseDramTimings.cs,
 * Dictionaries/DDR5Dictionary.cs, Hardware/DRAM/MemoryConfig.cs,
 * Hardware/Aod/AodData.cs and Dictionaries/EncodedValueDictionaries.cs.
 *
 * Kept free of I/O so decoding can be unit-tested with synthetic register
 * dumps (tests/test_decode.c).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_UMC_CHANNELS 8   /* UMC0..UMC7 probed, per ZenStates-Core MAX_CHANNELS loop */

/* ── Raw UMC register set for one channel ───────────────────────────────
 * One field per UMC register touched by the decoder; raw values are kept
 * so the debug dump can print them for hardware validation. */
typedef struct {
    uint32_t enabled;        /* 0x50DF0: UMC_D_INTERLEAVE_RDMAP_CHANNEL0 (+ DdrMaxRate [7:0]) */
    uint32_t regDF4;         /* 0x50DF4: DdrMaxRateEnf [7:0] */
    uint32_t dimm0_config;   /* UMC_D_CHIP_SEL_DDR_CHANNEL + 0x00 */
    uint32_t dimm1_config;   /* UMC_D_CHIP_SEL_DDR_CHANNEL + 0x08 */
    uint32_t ecc_burst;      /* UMC_ECCCFG0 + 0x100 */
    uint32_t pwrdownctrl;    /* UMC_PWRDOWNCTRL + 0x12C */
    uint32_t odtsctrl;       /* UMC_ODTSCTRL + 0x130 */
    uint32_t brstgap;        /* + 0x198 */
    uint32_t umcratio;       /* UMC_U_MEMCFG0 + 0x200 */
    uint32_t reg204;         /* primary timings */
    uint32_t reg208;
    uint32_t reg20C;
    uint32_t reg210;
    uint32_t reg214;
    uint32_t reg218;
    uint32_t reg21C;
    uint32_t reg220;
    uint32_t reg224;
    uint32_t reg228;
    uint32_t reg22C;
    uint32_t reg230;
    uint32_t reg234;
    uint32_t reg238;
    uint32_t reg23C;
    uint32_t reg240;
    uint32_t reg244;
    uint32_t reg248;
    uint32_t reg24C;
    uint32_t reg250;
    uint32_t reg254;
    uint32_t reg258;
    uint32_t reg25C;
    uint32_t trfc[4];        /* 0x260, 0x264, 0x268, 0x26C */
    uint32_t reg284;         /* nitro (DDR5) */
    uint32_t reg288;
    uint32_t reg28C;
    uint32_t reg2A4;
    uint32_t reg2A8;         /* ECSc */
    uint32_t rfcsb[4];       /* 0x2C0, 0x2C4, 0x2C8, 0x2CC */
    uint32_t reg2E0;         /* CCDL WR2 */
    uint32_t bgs0;           /* UMC_BGS + 0x050 */
    uint32_t bgs1;           /* UMC_BGS + 0x058 */
    uint32_t bgs_alt0;       /* UMC_D_INIT_DDR_CH_P0 + 0x0D0 */
    uint32_t bgs_alt1;       /* UMC_D_INIT_DDR_CH_P0 + 0x0D4 */
    uint32_t cs0_cfg;        /* chip select config 0..3 (0x50000 + 4*n) */
    uint32_t cs1_cfg;
    uint32_t cs2_cfg;
    uint32_t cs3_cfg;
    uint32_t rank0_mask[2];  /* DDR5 rank address mask regs (0x50020/4, 0x50028/4) */
    uint32_t rank1_mask[2];
    uint32_t addr_cfg0;      /* 0x50040 */
    uint32_t addr_cfg1;      /* 0x50048 */
} umc_raw_regs_t;

/* ── Per-channel decoded DRAM state ─────────────────────────────────────
 * A value is meaningful only when its 'has_*' flag is set; absent channels
 * and unsupported fields must display as N/A, never as 0. */
/* Author's timing struct, moved verbatim from types.h so the per-channel
 * struct below can embed it as member `t` (his decoders fill it directly). */
typedef struct {
    /* Primary */
    uint32_t tcl, trcd_rd, trcd_wr, trp, tras, trc;
    /* Secondary */
    uint32_t trrds, trrdl, tfaw, twr, tcwl;
    uint32_t rtp, wtrs, wtrl, rdwr, wrrd;
    uint32_t rdrd_scl, wrwr_scl;
    uint32_t rdrd_sc, rdrd_sd, rdrd_dd;
    uint32_t wrwr_sc, wrwr_sd, wrwr_dd;
    uint32_t refi, wrpre, rdpre;
    /* Tertiary */
    uint32_t trc_page, mod, mod_pda, mrd, mrd_pda;
    uint32_t stag, stag_sb, cke, xp;
    uint32_t phy_wrd, phy_wrl, phy_rdl;
    /* RFC */
    uint32_t rfc, rfc2, rfcsb;
    /* Nanoseconds */
    float trefi_ns, trfc_ns;
    float trfc2_ns, trfcsb_ns; /* reserved — computed but not displayed */
    /* Flags */
    bool gdm_enabled, power_down_enabled;
    char cmd2t[4];
    float frequency_hint_mhz;
} dram_timings_t;

typedef struct {
    bool present;            /* UMC instance exists and channel is enabled */
    char label[24];          /* "Channel A (UMC0)" */
    uint32_t smn_offset;     /* channel SMN offset (i << 20) */
    int    read_failures;    /* SMN reads that failed for this channel */
    bool   is_ddr5;

    /* Topology */
    bool   has_dimm[2];
    int    ranks[2];         /* 1 = SR, 2 = DR, 0 = unknown */
    bool   has_ranks;
    uint32_t num_banks, num_bank_groups, num_row, num_col;
    bool   has_addr_cfg;

    /* Everything upstream dram.c already decodes lives in the author's
     * struct, filled by the author's own read_common/ddr5/ddr4 functions. */
    dram_timings_t t;
    /* Extra DDR5 fields (bit-exact from ZenStates-Core, not decoded upstream): */
    uint32_t rp_pb, rc_pb;            /* DDR5 */
    uint32_t rdpost, wrpost;
    uint32_t rdrd_ban, wrwr_ban;
    /* DDR5 extras */
    uint32_t ppd, mw, odts_read_interval;
    uint32_t short_init, zqcs_interval, tzq_oper_cal, tzqcs;
    uint32_t dllk, xs;
    uint32_t rank_busy_dly, cmd_par_latency, alert_par_dly, alert_crc_dly;
    uint32_t mrri, mrr, mrw, ctrl_switch_clks;
    uint32_t csh, srx2srx, tstab, alert_crc_pulse, alert_par_pulse;
    uint32_t cacsh, cpded, parinl, rddataen, lp_exit_dly, lp_dly;
    uint32_t aggr_pwr_down_dly, pwr_down_dly, pd;
    uint32_t phyupd_cmd_dly, phyupd_wrdat_dly, phyupd_resp;
    uint32_t wrbrst_gap, wr_ph_chk, rdbrst_gap;
    uint32_t wrmpr, cmd_stg_cnt, rcvr_wait;
    uint32_t ecsc;
    uint32_t ccdl_wr2;
    bool   ccdl_wr2_valid;
    uint32_t ddr_max_rate, ddr_max_rate_enf;
    /* Config / flags (decoded strings, N/A when has_* is false) */
    bool   has_config;
    char   refresh_mode[24];          /* NORMAL / FGR / PBONLY / MIXED */
    uint32_t fgr;
    bool   bgs, bgs_alt;              /* Bank Group Swap (incl. alternate CS map) */
    bool   bank_group_en;
    bool   ecc_enabled;               /* UMC_ECCCFG0.DimmEccEn */
    char   burst_length[8];           /* BL8 / BL16 */
    char   burst_ctrl[24];            /* Dynamic BL/BC / Fixed BL */
    bool   odts_en, odts_inc_ref_en, odts_cmd_throt_en, force_pwrdown_throt_en;
    uint32_t odts_cmd_throt_cyc, roll_window_depth;
    bool   i2c_therm_event;
    char   power_down_mode[16];       /* Full / Partial */
    bool   aggressive_power_down;
    uint32_t uclk_gt_fclk;            /* 1 = FCLK below UCLK */
    char   wck_ck_ratio[8];           /* "N/A" for DDR5, "2:1"/"4:1" LPDDR5 */
    /* Nitro (DDR5 latency/power optimization knobs), shown as "rx/tx/ctrl" */
    char   nitro_rx_tx_ctrl[16];
    char   nitro_dly_mode[24];        /* e.g. "rx:1 tx:0 ctrl:1" */
    /* Frequency */
    float  ratio;                     /* MEMCLK ratio (x BCLK) */

    /* Raw dump for diagnostics */
    umc_raw_regs_t raw;
} dram_channel_t;

/* ── AOD (ACPI Overclocking Data) decoded electrical parameters ────────
 * Source of both offsets and encodings: ZenStates-Core Hardware/Aod.
 * Raw codes kept alongside the decoded impedance strings. */
typedef struct {
    bool   available;        /* AOD region mapped and frequency block found */
    char   layout[96];       /* layout description for diagnostics */
    uint32_t mclk;           /* MemClk the frequency block was found for */
    int    base;             /* byte offset of the frequency block */
    /* Raw AOD u32 codes (valid only when the corresponding field was read) */
    uint32_t proc_odt_raw, proc_odt_pu_raw, proc_odt_pd_raw;
    uint32_t rtt_nom_wr_raw, rtt_nom_rd_raw, rtt_wr_raw, rtt_park_raw, rtt_park_dqs_raw;
    uint32_t cad_bus_ds_raw, proc_data_ds_raw, proc_cs_ds_raw, proc_ck_ds_raw;
    uint32_t proc_dq_ds_pu_raw, proc_dq_ds_pd_raw;
    uint32_t dram_data_ds_raw, dram_dq_ds_pu_raw, dram_dq_ds_pd_raw;
    bool   has_proc_odt, has_proc_odt_pu, has_proc_odt_pd;
    bool   has_rtt_nom_wr, has_rtt_nom_rd, has_rtt_wr, has_rtt_park, has_rtt_park_dqs;
    bool   has_cad_bus_ds, has_proc_data_ds, has_proc_cs_ds, has_proc_ck_ds;
    bool   has_proc_dq_ds_pu, has_proc_dq_ds_pd;
    bool   has_dram_data_ds, has_dram_dq_ds_pu, has_dram_dq_ds_pd;
} aod_mem_info_t;

/* ── Decoder API (pure functions) ────────────────────────────────────── */

/* Decode one channel's raw register set into ddr (DDR4/DDR5 aware). */
void umc_regs_decode(const umc_raw_regs_t *raw, bool is_ddr5, dram_channel_t *d);

/* MEMCLK (MT/s) -> nanoseconds for a clock-count timing (2 clocks per cycle). */
float clocks_to_ns(uint32_t clocks, float mtaps);

/* Frequency (MT/s) from the UMC_U_MEMCFG0 ratio register, per generation. */
float umc_ratio_to_memspeed(uint32_t umcratio, bool is_ddr5, float bclk_mhz);

/* Refresh mode string from UMC_PWRDOWNCTRL (0x5012C) DDR5 fields. */
void umc_refresh_mode_decode(uint32_t pwrdownctrl, char *out, size_t outsz, uint32_t *fgr_out);

/* Rank detection from DIMM config / chip-select / address-mask regs. */
int  umc_detect_rank_ddr5(const uint32_t mask[2]);
int  umc_detect_rank_ddr4(uint32_t cs01, uint32_t cs23_unused, int dimm);

/* Electrical value decoding — return static strings, N/A-safe. */
const char *proc_odt_decode(uint32_t code);          /* "48.0 Ω" / "Hi-Z" */
const char *proc_data_ds_decode(uint32_t code);      /* drive strength */
const char *dram_data_ds_decode(uint32_t code);
const char *cad_bus_ds_decode(uint32_t code);
const char *proc_impedance_decode(uint32_t code);    /* ProcCsDs/ProcCkDs */
void rtt_decode(uint32_t code, char *out, size_t outsz); /* "RZQ/4 (60)" */

/* Find the last occurrence of a 2-byte LE pattern (ZenStates FindLastSequence). */
int aod_find_last_u16(const uint8_t *buf, int len, uint16_t value);

/* Locate the Granite Ridge/Zen5 frequency block and decode electrical fields.
 * mclk_hint: MemClk to search for (0 = try the 1Ah default base dictionary). */
void aod_mem_parse_granite_ridge(const uint8_t *table, int len, uint32_t mclk_hint,
                                 bool patch_gt_b404022, bool has_rmp,
                                 aod_mem_info_t *info);

/* True when the codes decoded into info are all plausible dictionary values
 * (RTT in 0..7, ProcODT in the ProcOdt dictionary, CADBUS in its dictionary).
 * Used to pick between layouts that cannot be told apart on Linux. */
bool aod_mem_info_sanity(const aod_mem_info_t *info);

#endif /* DRAM_DECODE_H */
