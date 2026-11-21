/*
 * test_spd.c — unit tests for the pure DDR5 SPD decoder (spd_decode.c).
 *
 * Acceptance target: the golden decode of the TEAMGROUP UD5-6000 fixture
 * shipped and validated by the `spdr` crate (Copyright MERT EFE ŞENSOY,
 * Apache-2.0; see src/spd_decode.c header and Linux/PROVENANCE.md). The
 * 1024-byte fixture is embedded base64 so the test is self-contained;
 * its SHA-256 is cecfa75eb704272ad5b135e77a534cc416aec55a8daea54823b5dbf6d7761c98.
 *
 * Optional runtime cross-checks run only when the machine has live SPD
 * dumps (sysfs eeprom) or the DDR5SPDEditor sample files present.
 * Build & run:  make test
 */

#include "../src/spd_decode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
static int checks = 0;

#define EXPECT(cond, fmt, ...)                                        \
    do {                                                               \
        checks++;                                                      \
        if (!(cond)) {                                                 \
            failures++;                                                \
            printf("FAIL %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                              \
    } while (0)

static void expect_u32(uint64_t got, uint64_t want, const char *what)
{
    EXPECT(got == want, "%s: got %llu want %llu", what,
           (unsigned long long)got, (unsigned long long)want);
}

/* ── base64 (stdlib-only decoder, just enough for the fixture) ───────── */

static size_t b64decode(const char *in, unsigned char *out, size_t outsz)
{
    static const char *T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    unsigned long acc = 0;
    int bits = 0;
    for (const char *p = in; *p; p++) {
        const char *q = strchr(T, *p);
        if (!q) { if (*p == '=') break; continue; }
        acc = (acc << 6) | (unsigned)(q - T);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= outsz) return o;
            out[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    return o;
}

static const char FIXTURE_B64[] =
    "MBASAgQAIGIAAAAAIAIAAAAAAACgAegD/gcAAAAAAEEAQQBBAH0AvjB1JwGgAIIAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAIgTCIgTCCBOIBAnEBU0IBAnEMQJBEwdDAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAEACAs4ASioyCEgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAARAQCBACIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIYAE"
    "7wAjNwEE7vZVRDUtNjAwMCAgICAgICAgICAgICAgICAgICAgICAAgK3/AAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAECMJBQGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAxKMAMAiowBAxIAAAAAVEctNjAwMC0zOC0zOC03OFRHLTU2MDAtNDAtNDAt"
    "ODQgICAgICAgICAgICAgICAgLCUwJSUAIk0BAAIAAAAAbjFuMW4xdmXkljB1JwGgAIIAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAF8KMCQkACJlAQAEAAAAAMg3yDfINyR17KwwdScBoACC"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgDECkVYUE8QMxEAAAAlJTAATQFuMW4xbjF2"
    "ZeSWMHUnAaAAggCIE4gTIE4QJ6opECfECUwdJCQwAGUByDfIN8g3JHXsrDB1JwGgAIIAiBOIEyBO"
    "ECekLBAnxAlMHQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOKfAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAoOSVAGQsFA2Y0NUgyAQAAQUI2NTAAAF8nAAVXASIBAA==";

static unsigned char fixture[1024];

/* ── helper unit tests ──────────────────────────────────────────────── */

static void test_helpers(void)
{
    expect_u32(spd_crc16((const unsigned char *)"123456789", 9), 0x31C3,
               "CRC-16/XMODEM check vector");

    expect_u32(spd_data_rate_mt_s(416), 4800, "rate 416ps->4800");
    expect_u32(spd_data_rate_mt_s(333), 6000, "rate 333ps->6000");
    expect_u32(spd_data_rate_mt_s(357), 5600, "rate 357ps->5600");
    expect_u32(spd_data_rate_mt_s(0), 0, "rate tCK0->0");

    /* spdr rated_cas_latency: round-to-nearest in u64, saturating */
    expect_u32(spd_cycles_round(12654, 333), 38, "CL 12654/333");
    expect_u32(spd_cycles_round(14280, 357), 40, "CL 14280/357");
    expect_u32(spd_cycles_round(0, 0), 0, "CL tCK0");
    expect_u32(spd_cycles_round(65541, 1), 65535, "CL saturating");
    expect_u32(spd_cycles_round(65535, 1), 65535, "CL exact 65535");
    /* (u32::MAX, 2^31): u64 rounding gives 2; a u32 add would misround to 1 */
    expect_u32(spd_cycles_round(0xFFFFFFFFu, 1u << 31), 2, "CL u64 rounding");

    /* DDR5SPDEditor TimeToTicksDDR5: floor(t*997/tCK)/1000 + 1 */
    expect_u32(spd_ps_to_nck(12654, 333), 38, "tick 12654@333");
    expect_u32(spd_ps_to_nck(14280, 357), 40, "tick 14280@357");
    expect_u32(spd_ps_to_nck(25974, 333), 78, "tick 25974@333");
    expect_u32(spd_ps_to_nck(0, 333), 1, "tick 0 -> 1 (C++ formula)");
    expect_u32(spd_ps_to_nck(100, 0), 0, "tick tCK0 -> 0");
}

/* ── golden fixture decode (spdr acceptance target) ─────────────────── */

static void test_fixture(void)
{
    spd_info_t s;
    EXPECT(spd_decode_ddr5(fixture, sizeof(fixture), &s), "fixture decodes");
    if (!s.valid) return;

    expect_u32(s.image_size, 1024, "image size");

    /* CRC (spdr tests/crc.rs): computed == stored == 0x8021 */
    EXPECT(s.base_crc.has, "base crc has");
    expect_u32(s.base_crc.computed, 0x8021, "base crc computed");
    expect_u32(s.base_crc.stored, 0x8021, "base crc stored");
    EXPECT(s.base_crc.matches, "base crc matches");

    /* identity (snapshot 1.1) */
    EXPECT(s.identity.has, "identity has");
    expect_u32(s.identity.spd_bytes_total, 1024, "spd_bytes_total");
    expect_u32(s.identity.spd_rev_major, 1, "rev major");
    expect_u32(s.identity.spd_rev_minor, 0, "rev minor");
    EXPECT(s.identity.device_ddr5, "device ddr5");
    EXPECT(strcmp(s.identity.module_type_str, "UDIMM") == 0, "module type UDIMM");
    EXPECT(!s.identity.hybrid, "hybrid false");
    expect_u32(s.identity.density_per_die_gb, 16, "density");
    EXPECT(strcmp(s.identity.package_type_str, "monolithic") == 0, "package");
    expect_u32(s.identity.die_count, 1, "die count");
    expect_u32(s.identity.row_address_bits, 16, "row bits");
    expect_u32(s.identity.column_address_bits, 10, "col bits");
    expect_u32(s.identity.io_width_bits, 8, "io width");
    expect_u32(s.identity.bank_groups, 8, "bank groups");
    expect_u32(s.identity.banks_per_bank_group, 4, "banks per BG");
    expect_u32(s.identity.package_ranks_per_channel, 1, "ranks/channel");
    EXPECT(!s.identity.rank_mix_asymmetric, "rank mix");
    expect_u32(s.identity.channels_per_dimm, 2, "channels per DIMM");
    expect_u32(s.identity.primary_bus_width_bits, 32, "bus width");
    expect_u32(s.identity.ecc_width_bits, 0, "ecc width");
    EXPECT(s.identity.has_capacity, "capacity available");
    expect_u32(s.identity.capacity_bytes, 16ull * 1000 * 1000 * 1000, "capacity 16 GB");
    /* base voltages: bytes 0x10..0x12 are 0x00 on the fixture -> DDR5
     * default rails (1.1/1.1/1.8 V), per JESD400-5 / DDR5SPDEditor */
    EXPECT(s.identity.has_voltages, "has voltages");
    expect_u32(s.identity.vdd_nominal_mv, 1100, "VDD nominal");
    expect_u32(s.identity.vddq_nominal_mv, 1100, "VDDQ nominal");
    expect_u32(s.identity.vpp_nominal_mv, 1800, "VPP nominal");

    /* base timings (snapshot 1.2) */
    EXPECT(s.timings.has, "timings has");
    expect_u32(s.timings.tckavg_min_ps, 416, "tCKAVGmin");
    expect_u32(s.timings.tckavg_max_ps, 1000, "tCKAVGmax");
    expect_u32(s.timings.cl_mask, 0x7FEull, "CL mask (FE 07)");
    expect_u32(s.timings.cl_min, 22, "CL min");
    expect_u32(s.timings.cl_max, 40, "CL max");
    expect_u32(s.timings.taa_ps, 16640, "tAA");
    expect_u32(s.timings.trcd_ps, 16640, "tRCD");
    expect_u32(s.timings.trp_ps, 16640, "tRP");
    expect_u32(s.timings.tras_ps, 32000, "tRAS");
    expect_u32(s.timings.trc_ps, 48640, "tRC");
    expect_u32(s.timings.twr_ps, 30000, "tWR");
    expect_u32(s.timings.trfc1_ps, 295000, "tRFC1");
    expect_u32(s.timings.trfc2_ps, 160000, "tRFC2");
    expect_u32(s.timings.trfcsb_ps, 130000, "tRFCsb");
    expect_u32(s.timings.trrd_l_ps, 5000,  "tRRD_L ps");
    expect_u32(s.timings.trrd_l_nck, 8,    "tRRD_L nCK");
    expect_u32(s.timings.tccd_l_ps, 5000,  "tCCD_L ps");
    expect_u32(s.timings.tccd_l_nck, 8,    "tCCD_L nCK");
    expect_u32(s.timings.tccd_l_wr_ps, 20000, "tCCD_L_WR ps");
    expect_u32(s.timings.tccd_l_wr_nck, 32,    "tCCD_L_WR nCK");
    expect_u32(s.timings.tccd_l_wr2_ps, 10000, "tCCD_L_WR2 ps");
    expect_u32(s.timings.tccd_l_wr2_nck, 16,    "tCCD_L_WR2 nCK");
    expect_u32(s.timings.tfaw_ps, 13333, "tFAW ps");
    expect_u32(s.timings.tfaw_nck, 32,    "tFAW nCK");
    expect_u32(s.timings.tccd_l_wtr_ps, 10000, "tCCD_L_WTR ps");
    expect_u32(s.timings.tccd_l_wtr_nck, 16,    "tCCD_L_WTR nCK");
    expect_u32(s.timings.tccd_s_wtr_ps, 2500, "tCCD_S_WTR ps");
    expect_u32(s.timings.tccd_s_wtr_nck, 4,    "tCCD_S_WTR nCK");
    expect_u32(s.timings.trtp_ps, 7500, "tRTP ps");
    expect_u32(s.timings.trtp_nck, 12,   "tRTP nCK");
    expect_u32(s.timings.base_data_rate_mt_s, 4800, "base rate");

    /* module-specific (snapshot 1.3) */
    EXPECT(s.module.has, "module block has");
    expect_u32(s.module.nominal_height_mm, 32, "height");
    expect_u32(s.module.max_thickness_front_mm, 2, "thickness front");
    expect_u32(s.module.max_thickness_back_mm, 1, "thickness back");
    EXPECT(strcmp(s.module.raw_card, "A revision 0") == 0, "raw card: got %s",
           s.module.raw_card);
    EXPECT(s.module.rank1_address_mirrored, "rank1 mirrored");
    expect_u32(s.module.module_attributes_raw, 0x81, "attrs raw");

    /* manufacturing (snapshot 1.4) */
    EXPECT(s.manufacturing.has, "manufacturing has");
    expect_u32(s.manufacturing.module_mfr_bank, 5, "mfr bank");
    expect_u32(s.manufacturing.module_mfr_code, 0x6F, "mfr code");
    EXPECT(strcmp(s.manufacturing.module_mfr, "Team Group Inc.") == 0,
           "mfr name: got %s", s.manufacturing.module_mfr);
    expect_u32(s.manufacturing.location, 0, "location");
    expect_u32(s.manufacturing.year, 2023, "year");
    expect_u32(s.manufacturing.week, 37, "week");
    expect_u32(s.manufacturing.serial, 0x0104EEF6u, "serial");
    EXPECT(strcmp(s.manufacturing.part_number, "UD5-6000") == 0,
           "part: got %s", s.manufacturing.part_number);
    expect_u32(s.manufacturing.revision_code, 0, "revision code");
    expect_u32(s.manufacturing.dram_mfr_bank, 1, "dram mfr bank");
    expect_u32(s.manufacturing.dram_mfr_code, 0x2D, "dram mfr code");
    EXPECT(strcmp(s.manufacturing.dram_mfr, "SK Hynix") == 0,
           "dram mfr: got %s", s.manufacturing.dram_mfr);
    expect_u32(s.manufacturing.dram_stepping, 0xFF, "stepping");

    /* XMP (tests/vendor.rs oracles) */
    EXPECT(s.xmp.present, "xmp present");
    expect_u32(s.xmp.version, 0x30, "xmp version");
    EXPECT(s.xmp.version_known, "xmp version known");
    expect_u32(s.xmp.enable_bits, 0x03, "xmp enable");
    expect_u32(s.xmp.header_crc.computed, 0x252C, "xmp hdr crc");
    EXPECT(s.xmp.header_crc.matches, "xmp hdr crc match");

    const spd_profile_t *x1 = &s.xmp.profile[0];
    const spd_profile_t *x2 = &s.xmp.profile[1];
    EXPECT(x1->present && x2->present, "xmp p1/p2 present");
    EXPECT(x1->has_name && strcmp(x1->name, "TG-6000-38-38-78") == 0,
           "xmp p1 name: got %s", x1->name);
    EXPECT(x2->has_name && strcmp(x2->name, "TG-5600-40-40-84") == 0,
           "xmp p2 name: got %s", x2->name);
    expect_u32(x1->crc_valid, 1, "xmp p1 crc valid");
    expect_u32(x2->crc_valid, 1, "xmp p2 crc valid");
    expect_u32(x1->tck_ps, 333, "xmp p1 tCK");
    expect_u32(x1->data_rate_mt_s, 6000, "xmp p1 rate");
    expect_u32(x1->cl, 38, "xmp p1 CL");
    expect_u32(x1->trcd_ps, 12654, "xmp p1 tRCD");
    expect_u32(x1->trp_ps, 12654, "xmp p1 tRP");
    expect_u32(x1->tras_ps, 25974, "xmp p1 tRAS");
    expect_u32(x1->vdd_mv, 1250, "xmp p1 VDD");
    expect_u32(x1->vddq_mv, 1250, "xmp p1 VDDQ");
    expect_u32(x1->vpp_mv, 1800, "xmp p1 VPP");
    expect_u32(x2->data_rate_mt_s, 5600, "xmp p2 rate");
    expect_u32(x2->cl, 40, "xmp p2 CL");
    expect_u32(x2->trcd_ps, 14280, "xmp p2 tRCD");
    expect_u32(x2->tras_ps, 29988, "xmp p2 tRAS");
    expect_u32(x2->vdd_mv, 1200, "xmp p2 VDD");

    /* EXPO (block CRC + same rated oracle) */
    EXPECT(s.expo.present, "expo present");
    EXPECT(s.xmp.hybrid_expo, "xmp knows about EXPO overlay at 0x340");
    expect_u32(s.expo.revision, 0x10, "expo revision");
    expect_u32(s.expo.enable_bits, 0x33, "expo enable");
    expect_u32(s.expo.block_crc.computed, 0x9FE2, "expo block crc");
    EXPECT(s.expo.block_crc.matches, "expo block crc match");
    EXPECT(s.expo.profile[0].present && s.expo.profile[1].present,
           "expo p1/p2 present");
    const spd_profile_t *e1 = &s.expo.profile[0];
    expect_u32(e1->tck_ps, 333, "expo p1 tCK");
    expect_u32(e1->data_rate_mt_s, 6000, "expo p1 rate");
    expect_u32(e1->cl, 38, "expo p1 CL");
    expect_u32(e1->trcd_ps, 12654, "expo p1 tRCD");
    expect_u32(e1->tras_ps, 25974, "expo p1 tRAS");
    expect_u32(e1->vdd_mv, 1250, "expo p1 VDD");
    expect_u32(e1->vpp_mv, 1800, "expo p1 VPP");
    expect_u32(s.expo.profile[1].data_rate_mt_s, 5600, "expo p2 rate");
    expect_u32(s.expo.profile[1].cl, 40, "expo p2 CL");

    /* XMP P3 slot must NOT be decoded as XMP here (EXPO overlay) */
    EXPECT(!s.xmp.profile[2].present, "xmp p3 suppressed under EXPO");
}

/* ── robustness / negative cases (spdr contract) ────────────────────── */

static void test_negative(void)
{
    spd_info_t s;
    unsigned char zero[1024];
    memset(zero, 0, sizeof(zero));

    EXPECT(!spd_decode_ddr5(zero, 511, &s), "511B image rejected");
    EXPECT(!spd_decode_ddr5(zero, 512, &s), "byte2 gate: 0x00 rejected");
    zero[2] = 0x12;
    EXPECT(spd_decode_ddr5(zero, 1024, &s), "zeroed DDR5 image decodes tolerantly");
    EXPECT(s.valid, "valid");
    EXPECT(!s.xmp.present, "absent XMP magic -> no XMP");
    EXPECT(!s.expo.present, "absent EXPO magic -> no EXPO");
    EXPECT(!s.base_crc.matches, "zero image: crc mismatch reported");
    EXPECT(!s.identity.has, "zero image: reserved enums -> identity N/A");

    /* mutation: flip one bit -> CRC must fail but decode still completes */
    unsigned char mut[1024];
    memcpy(mut, fixture, sizeof(mut));
    mut[2] ^= 0x01;   /* device type byte: keep decode by fixing gate byte */
    mut[2] = 0x12;
    mut[30] ^= 0x01;
    EXPECT(spd_decode_ddr5(mut, sizeof(mut), &s), "mutated decodes");
    EXPECT(!s.base_crc.matches, "mutation breaks crc");
    EXPECT(s.manufacturing.has, "sections still decode");

    /* truncated to 512: base CRC ok, later sections unavailable */
    EXPECT(spd_decode_ddr5(fixture, 512, &s), "512B decodes");
    EXPECT(s.base_crc.matches, "512B base crc still valid");
    EXPECT(!s.manufacturing.has, "512B: manufacturing N/A");
    EXPECT(s.module.has, "512B: module-specific fits (bytes 230..233)");
    EXPECT(strcmp(s.module.raw_card, "A revision 0") == 0, "512B: raw card");
    EXPECT(s.timings.has, "512B: timings fit (end at byte 93)");
    EXPECT(!s.xmp.present && !s.expo.present, "512B: no vendor sections");
}

/* ── XMP3 user profile slots @0x380/0x3C0 ────────────────────── */

/* CRC-16/CCITT-FALSE-zero-init over the first 62 block bytes — identical
 * to spd_decode.c's section CRC / DDR5SPDEditor utilities::Crc16. */
static uint16_t t_crc16(const unsigned char *b, size_t n)
{
    unsigned crc = 0;
    for (size_t i = 0; i < n; i++) {
        crc ^= (unsigned)b[i] << 8;
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x8000u) ? ((crc << 1) ^ 0x1021u) & 0xFFFFu
                                  : (crc << 1) & 0xFFFFu;
    }
    return (uint16_t)crc;
}

/* Synthetic XMP-shaped 64-B profile block: DDR5-6400 CL32,
 * VPP 1.8 / VDD 1.35 / VDDQ 1.35 V, cmd rate 2n, stored CRC fixed. */
static void put_user_profile(unsigned char *img, size_t base)
{
    memset(img + base, 0, 64);
    img[base + 0x00] = 0x30;   /* vpp  1800                             */
    img[base + 0x01] = 0x27;   /* vdd  1350                             */
    img[base + 0x02] = 0x27;   /* vddq 1350                             */
    img[base + 0x04] = 0x30;   /* vmemctrl 1800                         */
    img[base + 0x05] = 0x38;   /* tCK  312 ps  (LE)                     */
    img[base + 0x06] = 0x01;
    img[base + 0x07] = 0x40;   /* CL mask: bit (32-20)/2 -> {32}        */
    img[base + 0x0D] = 0x00;   /* tAA  9984 ps -> CL 32 @312            */
    img[base + 0x0E] = 0x27;
    img[base + 0x0F] = 0x00;   /* tRCD 9984                             */
    img[base + 0x10] = 0x27;
    img[base + 0x11] = 0x00;   /* tRP  9984                             */
    img[base + 0x12] = 0x27;
    img[base + 0x3C] = 0x02;   /* command rate 2n                       */
    uint16_t crc = t_crc16(img + base, 62);
    img[base + 0x3E] = (unsigned char)(crc & 0xFF);   /* stored CRC (LE) */
    img[base + 0x3F] = (unsigned char)(crc >> 8);
}

static void expect_user_profile(const spd_profile_t *p, const char *slot)
{
    char what[64];
#define EW(cond, fmt, ...) do { snprintf(what, sizeof(what), fmt, slot); \
                                EXPECT(cond, "%s", what); } while (0)
    EW(p->present, "%s: present");
    EW(!p->has_name, "%s: user slots have no name field");
    EW(p->tck_ps == 312, "%s: tCK 312");
    EW(p->data_rate_mt_s == 6400, "%s: rate 6400");
    EW(p->has_cl && p->cl == 32, "%s: CL 32");
    EW(p->cl_mask == 0x40 && p->cl_in_mask, "%s: CL mask {32}");
    EW(p->vpp_mv == 1800 && p->vdd_mv == 1350 && p->vddq_mv == 1350,
       "%s: voltages");
    EW(p->has_vmemctrl && p->vmemctrl_mv == 1800, "%s: vmemctrl");
    EW(p->has_command_rate && strcmp(p->command_rate, "2n") == 0,
       "%s: cmd rate 2n");
    EW(p->crc_valid, "%s: stored CRC valid");
#undef EW
}

static void test_user_profiles(void)
{
    spd_info_t s;
    unsigned char img[1024];

    /* Case A — hybrid fixture (EXPO at 0x340 covers 0x340..0x3BF):
     * User1@0x380 must stay suppressed EVEN THOUGH the fixture has
     * non-zero bytes there (EXPO profile 2 data); User2@0x3C0 (outside
     * the EXPO block) must decode when populated. */
    memcpy(img, fixture, sizeof(img));
    int user1_nonzero = 0;
    for (size_t i = 0; i < 64; i++)
        if (img[0x380 + i]) user1_nonzero = 1;
    EXPECT(user1_nonzero, "fixture has data at 0x380 (suppression bites)");
    put_user_profile(img, 0x3C0);
    EXPECT(spd_decode_ddr5(img, sizeof(img), &s), "user2 image decodes");
    EXPECT(!s.xmp.profile[3].present, "user1 suppressed under EXPO");
    expect_user_profile(&s.xmp.profile[4], "user2");

    memcpy(img, fixture, sizeof(img));
    put_user_profile(img, 0x3C0);
    img[0x3C0 + 0x05] ^= 0xFF;   /* break the payload, CRC now stale */
    EXPECT(spd_decode_ddr5(img, sizeof(img), &s), "bad-crc image decodes");
    EXPECT(!s.xmp.profile[4].present, "user2 with stale CRC not presented");

    /* Case B — non-hybrid (EXPO magic erased): User1@0x380 gated on its
     * valid block CRC; all-zero User2@0x3C0 stays absent. */
    memcpy(img, fixture, sizeof(img));
    memset(img + 0x340, 0, 4);   /* drop "EXPO" magic */
    memset(img + 0x3C0, 0, 64);  /* ensure empty user2 */
    put_user_profile(img, 0x380);
    EXPECT(spd_decode_ddr5(img, sizeof(img), &s), "user1 image decodes");
    EXPECT(!s.expo.present, "EXPO magic erased");
    expect_user_profile(&s.xmp.profile[3], "user1");
    EXPECT(!s.xmp.profile[4].present, "empty user2 block stays absent");
    /* P3 slot @0x340 is enable-bit-gated (bit2 clear) — not revived: */
    EXPECT(!s.xmp.profile[2].present, "P3 still enable-bit-gated");

    /* Case C — untouched fixture: both user slots empty -> absent. */
    EXPECT(spd_decode_ddr5(fixture, sizeof(fixture), &s), "fixture decodes");
    EXPECT(!s.xmp.profile[3].present && !s.xmp.profile[4].present,
           "fixture: no user profiles");
}

/* ── optional: live dumps and vendor samples (skip if absent) ───────── */

static int load_file(const char *path, unsigned char *buf, size_t cap, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    *len = fread(buf, 1, cap, f);
    fclose(f);
    return *len > 0;
}

static void test_external(const char *path, int expect_crc_match)
{
    static unsigned char buf[4096];
    size_t len;
    if (!load_file(path, buf, sizeof(buf), &len))
        return;   /* not present: skip silently */
    spd_info_t s;
    int ok = spd_decode_ddr5(buf, len, &s);
    EXPECT(ok, "%s: decode failed: %s", path, s.error);
    if (!ok) return;
    if (expect_crc_match) {
        EXPECT(s.base_crc.has && s.base_crc.matches,
               "%s: base crc %04X vs %04X", path, s.base_crc.computed, s.base_crc.stored);
    }
    printf("  note: %s -> %s %s, part '%s', xmp=%d expo=%d\n",
           path, s.identity.module_type_str,
           s.identity.has ? "id-ok" : "id-NA",
           s.manufacturing.part_number,
           s.xmp.present, s.expo.present);
}

static void test_external_samples(void)
{
    /* live SPD5118 sysfs dumps (present on machines with the driver) */
    test_external("/sys/bus/i2c/devices/2-0051/eeprom", 1);
    test_external("/sys/bus/i2c/devices/2-0053/eeprom", 1);
    /* reference sample dumps (dev checkout paths; skipped when absent) */
    test_external("/mnt/evo/src/timings/DDR5SPDEditor/SPD examples/DDR5 XMP+EXPO.spd", 1);
    test_external("/mnt/evo/src/timings/DDR5SPDEditor/SPD examples/Sample DDR5 5600.spd", 1);
    test_external("/mnt/evo/src/timings/DDR5SPDEditor/SPD examples/Sample DDR5 5600 - EXPO XMP Hybrid.spd", 1);
    test_external("/mnt/evo/src/timings/DDR5SPDEditor/SPD examples/Sample DDR5 5600 - XMP Only.spd", 1);
}

int main(void)
{
    size_t n = b64decode(FIXTURE_B64, fixture, sizeof(fixture));
    if (n != sizeof(fixture)) {
        printf("FAIL: fixture base64 decoded to %zu bytes (want 1024)\n", n);
        return 1;
    }

    test_helpers();
    test_fixture();
    test_negative();
    test_user_profiles();
    test_external_samples();

    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
