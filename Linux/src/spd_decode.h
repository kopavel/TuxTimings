#ifndef SPD_DECODE_H
#define SPD_DECODE_H

/*
 * Pure DDR5 SPD (JESD400-5) decoding layer — no hardware or file access.
 *
 * PROVENANCE (see also Linux/licenses/ and Linux/PROVENANCE.md):
 *   - Identity/base-config, base-timing, module-specific, manufacturing and
 *     vendor-profile decoding ported to C from the `spdr` Rust crate
 *     (JESD400-5 decoder), Copyright MERT EFE ŞENSOY, Apache License 2.0.
 *     Field offsets, encodings and formulas reproduce that decoder's
 *     semantics; unit tests in tests/test_spd.c pin them against spdr's
 *     golden fixture values.
 *   - The extended XMP 3.0 / EXPO profile field maps (secondary timings,
 *     lower-limit bytes, command rate, boost bits) and the nCK tick
 *     conversion with the JESD400-5B 0.3% correction factor are ported from
 *     DDR5SPDEditor, Copyright edlf (Eduardo Fernandes), GPL-3.0-or-later.
 *   - Combined work: GPL-3.0-or-later (compatible superset; Apache-2.0
 *     notice texts retained under Apache-2.0 §4).
 *
 * Kept free of I/O so decoding can be unit-tested with raw SPD images
 * (tests/test_spd.c). Unavailable values use explicit has_/valid flags —
 * callers must render them as N/A, never as 0.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SPD_IMAGE_MIN     512   /* smallest image that decodes anything       */
#define SPD_IMAGE_STD     1024  /* standard DDR5 SPD size                     */
#define SPD_IMAGE_MAX     2048  /* only the first 1024 bytes are decoded      */
#define SPD_PARTNUM_LEN   31    /* 30-byte field + NUL                        */
#define SPD_NAME_LEN      17    /* 16-byte profile name + NUL               */

/* ── Small pure helpers (exported for unit tests) ────────────────────── */

/* CRC-16/XMODEM (poly 0x1021, init 0, no reflection, no final XOR).
 * crc16("123456789") == 0x31C3. Same primitive used by spdr and by
 * DDR5SPDEditor's utilities::Crc16 for every SPD/vendor-section CRC. */
uint16_t spd_crc16(const uint8_t *data, size_t len);

/* Data rate (MT/s) from a tCK period in ps: truncate 2'000'000/tCK first,
 * then round the quotient to the nearest 100 (spdr timing::data_rate_mt_s).
 * Zero period -> 0. */
uint32_t spd_data_rate_mt_s(uint32_t tck_ps);

/* nCK from ps with the JESD400-5B 0.3% correction (DDR5SPDEditor
 * utilities::TimeToTicksDDR5): floor((ps*997/tCK + 1000)/1000). tCK 0 -> 0. */
uint32_t spd_ps_to_nck(uint32_t ps, uint32_t tck_ps);

/* Round-to-nearest cycle count without the correction factor (spdr
 * vendor::rated_cas_latency): (t + tCK/2)/tCK in u64, saturated to 65535;
 * tCK 0 -> 0. */
uint32_t spd_cycles_round(uint32_t ps, uint32_t tck_ps);

/* ── CRC status (never gates decoding) ───────────────────────────────── */
typedef struct {
    bool     has;        /* image long enough to evaluate the section     */
    uint16_t computed;
    uint16_t stored;
    bool     matches;
} spd_crc_t;

/* ── Identity and base configuration (bytes 0..7, 234, 235) ──────────── */
typedef struct {
    bool     has;
    uint16_t spd_bytes_total;        /* byte0[6:4] -> 256/512/1024/2048   */
    uint8_t  spd_rev_major, spd_rev_minor; /* byte1 nibbles (hex, not BCD)*/
    bool     device_ddr5;            /* byte2 == 0x12                     */
    const char *device_type_str;     /* raw byte2, "unknown" tolerated    */
    const char *module_type_str;     /* byte3[3:0] enum; "unknown"        */
    bool     hybrid;                 /* byte3 bit7                        */
    uint16_t density_per_die_gb;     /* byte4[4:0] enum; 0 = unknown      */
    const char *package_type_str;    /* byte4[7:5]: mono/DDP/3DS          */
    uint8_t  die_count;              /* dies per package                  */
    uint8_t  row_address_bits;       /* byte5[4:0]: 16+code               */
    uint8_t  column_address_bits;    /* byte5[7:5]: 10+code               */
    uint8_t  io_width_bits;          /* byte6[7:5]: 4/8/16/32             */
    uint8_t  bank_groups;            /* byte7[7:5]: 1/2/4/8               */
    uint8_t  banks_per_bank_group;   /* byte7[2:0]: 1/2/4                 */
    uint8_t  package_ranks_per_channel; /* byte234[5:3] + 1               */
    bool     rank_mix_asymmetric;    /* byte234 bit6                      */
    uint8_t  channels_per_dimm;      /* byte235[7:5]: 1/2/4/8             */
    uint16_t primary_bus_width_bits; /* byte235[2:0]: 8/16/32/64          */
    uint8_t  ecc_width_bits;         /* byte235[4:3]: 0/8/16              */
    /* Base voltages, bytes 0x10..0x12: nominal = bits[7:5] in 50 mV steps
     * (JEDEC layout; DDR5SPDEditor's getters are degenerate — only 0x00
     * decodes — so the standard encoding is used here). */
    bool     has_voltages;
    uint16_t vdd_nominal_mv, vddq_nominal_mv, vpp_nominal_mv;
    /* Derived: (bus/io) * density Gb * dies * ranks * channels, bytes.
     * has_capacity = 0 only when the JEDEC precondition (bus % io == 0)
     * fails or a field was unknown. */
    bool     has_capacity;
    uint64_t capacity_bytes;
} spd_identity_t;

/* ── Base JEDEC timing block (bytes 0x12..0x5D) ──────────────────────── */
typedef struct {
    bool     has;
    uint32_t tckavg_min_ps, tckavg_max_ps;   /* 0x14 / 0x16               */
    uint64_t cl_mask;                        /* 5 bytes LE; bit i -> CL 20+2i */
    uint16_t cl_min, cl_max;                 /* 0 when mask empty         */
    uint32_t taa_ps, trcd_ps, trp_ps, tras_ps, trc_ps, twr_ps; /* 0x1E..  */
    uint32_t trfc1_ps, trfc2_ps, trfcsb_ps;  /* 0x2A.. stored ns *1000    */
    /* slr/dlr pair: single/different logical rank RFC (DDR5SPDEditor map) */
    uint32_t trfc1_dlr_ps, trfc2_dlr_ps, trfcsb_dlr_ps; /* 0x30..         */
    /* ps + nCK pairs (0x46..): time, and its lower-limit byte (LCLK)     */
    uint32_t trrd_l_ps,      trrd_l_nck;
    uint32_t tccd_l_ps,      tccd_l_nck;
    uint32_t tccd_l_wr_ps,   tccd_l_wr_nck;
    uint32_t tccd_l_wr2_ps,  tccd_l_wr2_nck;
    uint32_t tfaw_ps,        tfaw_nck;
    uint32_t tccd_l_wtr_ps,  tccd_l_wtr_nck;   /* 0x55: tCCD_L_WTR (XMP name) */
    uint32_t tccd_s_wtr_ps,  tccd_s_wtr_nck;   /* 0x58                      */
    uint32_t trtp_ps,        trtp_nck;         /* 0x5B                      */
    uint32_t base_data_rate_mt_s;              /* derived from tckavg_min   */
} spd_base_timings_t;

/* ── Module-specific block, unbuffered only (bytes 230..233) ─────────── */
typedef struct {
    bool     has;                /* false for non-UDIMM module types      */
    char     not_decoded_reason[80]; /* set when type has no decoder      */
    uint8_t  nominal_height_mm;  /* 230[4:0] + 15                         */
    uint8_t  max_thickness_front_mm; /* 231[3:0] + 1                      */
    uint8_t  max_thickness_back_mm;  /* 231[7:4] + 1                      */
    char     raw_card[32];       /* e.g. "A revision 0", "ZZ (no ref. raw card)" */
    bool     rank1_address_mirrored; /* 233 bit0                          */
    uint8_t  module_attributes_raw;  /* byte233 raw                       */
} spd_module_t;

/* ── Manufacturing block (bytes 512..554) ────────────────────────────── */
typedef struct {
    bool     has;
    uint8_t  module_mfr_bank;    /* JEP-106: (512&0x7F)+1                 */
    uint8_t  module_mfr_code;    /* 513 & 0x7F                            */
    char     module_mfr[40];     /* resolved name or "" (never guessed)   */
    uint8_t  location;           /* 514 raw                               */
    uint16_t year;               /* 515 BCD + 2000                        */
    uint8_t  week;               /* 516 BCD                               */
    uint32_t serial;             /* 517..520 BIG-endian                   */
    char     part_number[SPD_PARTNUM_LEN]; /* 521..550, trailing trim     */
    uint8_t  revision_code;      /* 551                                   */
    uint8_t  dram_mfr_bank;      /* 552/553 same rule                     */
    uint8_t  dram_mfr_code;
    char     dram_mfr[40];
    uint8_t  dram_stepping;      /* 554; 0xFF = not specified             */
} spd_manufacturing_t;

/* ── Vendor overclocking profiles ────────────────────────────────────── */
/* XMP 3.0 profile block (64 B at 0x2C0/0x300/0x340) and EXPO profile
 * block (40 B at 0x34A/0x372) share one C struct; fields absent from the
 * EXPO layout have has_* = 0 there. Units: *_ps raw stored values (RFC
 * family stored in ns -> *_ns fields), voltages in millivolts.
 * Field map: DDR5SPDEditor structs §3.2/§5.2 (offsets verified on the
 * vendor sample dumps and the spdr fixture). */
typedef struct {
    bool     present;            /* slot populated (enable bit / tCK != 0)*/
    bool     crc_valid;          /* section CRC matches (XMP per-profile) */
    char     name[SPD_NAME_LEN]; /* XMP only; "" = absent/invalid         */
    bool     has_name;

    uint16_t vpp_mv, vdd_mv, vddq_mv;
    bool     has_vmemctrl; uint16_t vmemctrl_mv; /* XMP only              */
    uint32_t tck_ps;             /* rated cycle time                      */
    uint32_t data_rate_mt_s;     /* spd_data_rate_mt_s(tck_ps)            */
    uint32_t cl;                 /* rated CL = tAA/tCK round-to-nearest   */
    uint64_t cl_mask;            /* XMP: stored 5-byte bitmap; EXPO: 0    */
    uint16_t cl_min, cl_max;     /* from mask (0 when no mask)            */
    bool     cl_in_mask;         /* cl is a supported value in the mask   */
    bool     has_cl;             /* tck != 0 -> cl computed               */
    uint32_t taa_ps, trcd_ps, trp_ps, tras_ps, trc_ps, twr_ps;
    uint32_t trfc1_ns, trfc2_ns, trfcsb_ns;
    uint32_t trrd_l_ps,      trrd_l_ll;   /* ll = lower-limit byte (XMP)  */
    uint32_t tccd_l_ps,      tccd_l_ll;
    uint32_t tccd_l_wr_ps,   tccd_l_wr_ll;
    uint32_t tccd_l_wr2_ps,  tccd_l_wr2_ll;
    uint32_t tccd_l_wtr_ps,  tccd_l_wtr_ll;
    uint32_t tccd_s_wtr_ps,  tccd_s_wtr_ll;
    uint32_t trtp_ps,        trtp_ll;
    uint32_t tfaw_ps,        tfaw_ll;
    bool     has_lower_limits;   /* false on EXPO (no LCLK bytes stored)  */
    bool     has_dm_boost, dm_boost;      /* XMP byte 0x3B bit0           */
    bool     has_rtmc, rtmc;              /* XMP byte 0x3B bit1           */
    char     command_rate[8];             /* XMP byte 0x3C[3:0]: "1n"...  */
    bool     has_command_rate;
} spd_profile_t;

typedef struct {
    bool        present;         /* magic found                            */
    spd_crc_t   header_crc;      /* 0x280..0x2BD stored 0x2BE              */
    uint8_t     version;         /* 0x282 (0x30 = XMP 3.0)                 */
    bool        version_known;
    uint8_t     enable_bits;     /* 0x283                                  */
    bool        hybrid_expo;     /* EXPO overlays slots 3/user1 (0x340)    */
    spd_profile_t profile[3];    /* [0]=P1@0x2C0 [1]=P2@0x300 [2]=P3@0x340
                                  *  (P3 invalid when hybrid_expo)         */
} spd_xmp_t;

typedef struct {
    bool        present;         /* "EXPO" magic at 0x340                  */
    spd_crc_t   block_crc;       /* 0x340..0x3BD stored 0x3BE              */
    uint8_t     revision;        /* 0x344                                  */
    uint8_t     enable_bits;     /* 0x345 (bit0 P1, bit4 P2)               */
    spd_profile_t profile[2];    /* 0x34A, 0x372                           */
} spd_expo_t;

/* ── Top-level decode ────────────────────────────────────────────────── */
typedef struct {
    bool     valid;              /* image decodable at all                */
    char     error[96];          /* when !valid                           */
    uint16_t image_size;
    spd_crc_t base_crc;          /* 0..509 stored 510..511 (LE)           */
    spd_identity_t    identity;
    spd_base_timings_t timings;
    spd_module_t      module;
    spd_manufacturing_t manufacturing;
    spd_xmp_t         xmp;
    spd_expo_t        expo;
} spd_info_t;

/* Decode a raw SPD image (>= SPD_IMAGE_MIN bytes; bytes past 1024 are
 * ignored). Never fails on bad content — per-section has/present flags
 * report what could not be decoded. Returns false only if the image is too
 * small or not DDR5 (byte 2 != 0x12); info->error is then filled. */
bool spd_decode_ddr5(const uint8_t *image, size_t len, spd_info_t *info);

#endif /* SPD_DECODE_H */
