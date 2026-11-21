/*
 * DDR5 SPD (JESD400-5) decoder — pure functions over a raw SPD image.
 *
 * PROVENANCE (see also Linux/licenses/ and Linux/PROVENANCE.md):
 *   - Base block, CRC, module-specific, manufacturing and vendor-profile
 *     semantics ported from the `spdr` Rust DDR5 SPD decoder
 *     (src/identity.rs, crc.rs, timing.rs, module.rs, manufacturing.rs,
 *     vendor.rs), Copyright MERT EFE ŞENSOY, Apache License 2.0.
 *     Full text: Linux/licenses/LICENSE-APACHE.
 *   - Extended XMP 3.0 / EXPO profile field maps (secondary timings +
 *     lower-limit bytes + command rate + boost bits) and the
 *     JESD400-5B-corrected nCK conversion ported from DDR5SPDEditor
 *     (ddr5spd_structs.h, xmp3.cpp, expo.cpp, utilities.cpp),
 *     Copyright edlf (Eduardo Fernandes), GPL-3.0-or-later.
 *     Full text: Linux/licenses/LICENSE-GPL-3.0.
 *
 * The combined work is distributed under GPL-3.0-or-later; the Apache-2.0
 * attribution above and the retained license texts satisfy Apache-2.0 §4.
 *
 * Layering: NO file, I2C or sysfs access here — the caller (backend.c)
 * hands us bytes; unit tests feed raw images (tests/test_spd.c).
 */

#include "spd_decode.h"

#include <string.h>
#include <stdio.h>

/* ── Bounds-checked readers (spdr reader.rs semantics) ───────────────── */

typedef struct {
    const uint8_t *b;
    size_t len;
} img_t;

static bool have(const img_t *im, size_t off, size_t n)
{
    return off + n <= im->len && off + n >= n; /* no wrap */
}

static uint8_t b1(const img_t *im, size_t off)
{
    return have(im, off, 1) ? im->b[off] : 0;
}

static uint16_t le16(const img_t *im, size_t off)
{
    if (!have(im, off, 2)) return 0;
    return (uint16_t)(im->b[off] | ((uint16_t)im->b[off + 1] << 8));
}

/* ── CRC-16/XMODEM (spdr crc.rs == DDR5SPDEditor utilities::Crc16) ───── */

uint16_t spd_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
    return crc;
}

/* spdr crc::verify_base_crc: covers 0..509, stored LE at 510..511. */
static spd_crc_t section_crc(const img_t *im, size_t start, size_t block_len)
{
    spd_crc_t s = { .has = false, .computed = 0, .stored = 0, .matches = false };
    if (!have(im, start, block_len))
        return s;
    s.has = true;
    s.computed = spd_crc16(im->b + start, block_len - 2);
    s.stored = le16(im, start + block_len - 2);
    s.matches = s.computed == s.stored;
    return s;
}

/* ── Unit conversions ────────────────────────────────────────────────── */

uint32_t spd_data_rate_mt_s(uint32_t tck_ps)
{
    if (tck_ps == 0) return 0;
    uint32_t raw = 2000000u / tck_ps;     /* truncating, then round the  */
    return ((raw + 50u) / 100u) * 100u;   /* quotient to nearest 100     */
}

/* DDR5SPDEditor utilities::TimeToTicksDDR5 (correctionFactor = 3):
 * nCK = floor(t*997/tCK)/1000 + 1, integer-exact form of the C++ double
 * arithmetic floor((t*997/tCK + 1000)/1000). */
uint32_t spd_ps_to_nck(uint32_t ps, uint32_t tck_ps)
{
    if (tck_ps == 0) return 0;
    return (uint32_t)(((uint64_t)ps * 997u) / tck_ps / 1000u) + 1u;
}

/* spdr vendor::rated_cas_latency: round-to-nearest in u64, saturating. */
uint32_t spd_cycles_round(uint32_t ps, uint32_t tck_ps)
{
    if (tck_ps == 0) return 0;
    uint64_t num = (uint64_t)ps + (uint64_t)(tck_ps / 2u);
    uint64_t cl = num / (uint64_t)tck_ps;
    return cl > 0xFFFFu ? 0xFFFFu : (uint32_t)cl;
}

/* spdr vendor::voltage (ConvertByteToVoltageDDR5): high 3 bits = volts,
 * low 5 bits = 50 mV steps. */
static uint16_t volt_byte(uint8_t v)
{
    return (uint16_t)((v >> 5) * 1000u + (v & 0x1F) * 50u);
}

/* ── Enum tables (verbatim from spdr identity.rs / module.rs) ────────── */

static uint16_t enum_size_kib(uint8_t code, bool *ok)
{   /* byte0[6:4]: 1=>256 2=>512 3=>1024 4=>2048 */
    static const uint16_t t[5] = { 0, 256, 512, 1024, 2048 };
    if (code >= 1 && code <= 4) { *ok = true; return t[code]; }
    *ok = false; return 0;
}

static const char *device_type_str(uint8_t byte2)
{
    switch (byte2) {
    case 0x0B: return "DDR3 SDRAM";
    case 0x0C: return "DDR4 SDRAM";
    case 0x12: return "DDR5 SDRAM";
    case 0x13: return "LPDDR5 SDRAM";
    default:   return "unknown";
    }
}

static const char *module_type_str(uint8_t code)
{   /* byte3[3:0]; 0x00/0x09 reserved -> unknown */
    switch (code) {
    case 0x01: return "RDIMM";
    case 0x02: return "UDIMM";
    case 0x03: return "SODIMM";
    case 0x04: return "LRDIMM";
    case 0x05: return "CUDIMM";
    case 0x06: return "CSOUDIMM";
    case 0x07: return "MRDIMM";
    case 0x08: return "CAMM2";
    case 0x0A: return "DDIMM";
    case 0x0B: return "solder down";
    default:   return "unknown";
    }
}

static uint16_t density_gb(uint8_t code)
{   /* byte4[4:0]; 0 and >8 unknown */
    static const uint16_t t[9] = { 0, 4, 8, 12, 16, 24, 32, 48, 64 };
    return code <= 8 ? t[code] : 0;
}

static const char *package_type_str(uint8_t code, uint8_t *dies)
{   /* byte4[7:5] -> (type, dies per package) */
    switch (code) {
    case 0: *dies = 1;  return "monolithic";
    case 1: *dies = 2;  return "dual-die (DDP)";
    case 2: *dies = 2;  return "3DS";
    case 3: *dies = 4;  return "3DS";
    case 4: *dies = 8;  return "3DS";
    case 5: *dies = 16; return "3DS";
    default: *dies = 0; return "unknown";
    }
}

/* ── Identity and base configuration (spdr identity.rs) ──────────────── */

static void decode_identity(const img_t *im, spd_identity_t *id)
{
    memset(id, 0, sizeof(*id));
    if (!have(im, 235, 1))
        return;   /* highest required byte */

    bool ok;
    id->spd_bytes_total = enum_size_kib((b1(im, 0) >> 4) & 0x07, &ok);
    id->spd_rev_major = (uint8_t)(b1(im, 1) >> 4);
    id->spd_rev_minor = (uint8_t)(b1(im, 1) & 0x0F);
    uint8_t byte2 = b1(im, 2);
    id->device_ddr5 = byte2 == 0x12;
    id->device_type_str = device_type_str(byte2);

    uint8_t mt_code = b1(im, 3) & 0x0F;
    id->module_type_str = module_type_str(mt_code);
    id->hybrid = (b1(im, 3) & 0x80) != 0;

    id->density_per_die_gb = density_gb(b1(im, 4) & 0x1F);
    uint8_t pkg_code = (b1(im, 4) >> 5) & 0x07;
    id->package_type_str = package_type_str(pkg_code, &id->die_count);

    id->row_address_bits = (uint8_t)(16 + (b1(im, 5) & 0x1F));
    id->column_address_bits = (uint8_t)(10 + ((b1(im, 5) >> 5) & 0x07));

    static const uint8_t iow[4] = { 4, 8, 16, 32 };
    static const uint8_t bgc[4] = { 1, 2, 4, 8 };
    static const uint8_t bpg[3] = { 1, 2, 4 };
    static const uint8_t bw[4]  = { 8, 16, 32, 64 };
    static const uint8_t ch[4]  = { 1, 2, 4, 8 };
    uint8_t io_code = (b1(im, 6) >> 5) & 0x07;
    id->io_width_bits = io_code <= 3 ? iow[io_code] : 0;
    uint8_t bg_code = (b1(im, 7) >> 5) & 0x07;
    id->bank_groups = bg_code <= 3 ? bgc[bg_code] : 0;
    uint8_t bpg_code = b1(im, 7) & 0x07;
    id->banks_per_bank_group = bpg_code <= 2 ? bpg[bpg_code] : 0;

    /* spdr: no reserved-value check on byte234[5:3]; base 1 */
    id->package_ranks_per_channel = (uint8_t)(((b1(im, 234) >> 3) & 0x07) + 1);
    id->rank_mix_asymmetric = (b1(im, 234) & 0x40) != 0;

    uint8_t ch_code = (b1(im, 235) >> 5) & 0x07;
    id->channels_per_dimm = ch_code <= 3 ? ch[ch_code] : 0;
    uint8_t bw_code = b1(im, 235) & 0x07;
    id->primary_bus_width_bits = bw_code <= 3 ? bw[bw_code] : 0;
    id->ecc_width_bits = (uint8_t)(((b1(im, 235) >> 3) & 0x03) * 8);

    /* Base voltages 0x10..0x12: nominal bits[7:5] x 50 mV (JEDEC layout;
     * the DDR5SPDEditor getters decode only 0x00 — superseded here). A
     * zero byte encodes the DDR5 default rails, not 0 V (JESD400-5:
     * 1.1/1.1/1.8 V; DDR5SPDEditor getVDD/getVPP do the same; confirmed by
     * the spdr golden fixture, whose voltage bytes are all 0x00). */
    id->has_voltages = have(im, 0x12, 1);
    id->vdd_nominal_mv  = b1(im, 0x10) == 0 ? 1100 : (uint16_t)((b1(im, 0x10) >> 5) * 50u);
    id->vddq_nominal_mv = b1(im, 0x11) == 0 ? 1100 : (uint16_t)((b1(im, 0x11) >> 5) * 50u);
    id->vpp_nominal_mv  = b1(im, 0x12) == 0 ? 1800 : (uint16_t)((b1(im, 0x12) >> 5) * 50u);

    /* Capacity (spdr docs/numerical-claims Phase 8; memtest86plus formula):
     * (bus/io) * density Gb * dies * ranks * channels -> bits -> bytes */
    if (id->io_width_bits && id->primary_bus_width_bits &&
        id->primary_bus_width_bits % id->io_width_bits == 0 &&
        id->density_per_die_gb && id->die_count &&
        id->package_ranks_per_channel && id->channels_per_dimm) {
        uint64_t devices = id->primary_bus_width_bits / id->io_width_bits;
        uint64_t gbits = devices * id->density_per_die_gb * id->die_count *
                         id->package_ranks_per_channel * id->channels_per_dimm;
        id->capacity_bytes = gbits * 1000ull * 1000ull * 1000ull / 8ull;
        id->has_capacity = true;
    }

    /* spdr treats unknown enums as hard errors; here the section decodes
     * but flags itself unavailable so the UI renders N/A, never 0. */
    id->has = id->spd_bytes_total && id->density_per_die_gb && id->die_count &&
              id->io_width_bits && id->bank_groups &&
              id->banks_per_bank_group && id->channels_per_dimm &&
              id->primary_bus_width_bits &&
              strcmp(id->module_type_str, "unknown") != 0;
}

/* ── Base JEDEC timing block (spdr timing.rs + DDR5SPDEditor RFC dlr) ── */

static void cl_mask_bounds(uint64_t mask, uint16_t *lo, uint16_t *hi)
{
    *lo = 0; *hi = 0;
    for (int i = 0; i < 40; i++) {
        if ((mask >> i) & 1u) {
            uint16_t cl = (uint16_t)(20 + 2 * i);
            if (!*lo) *lo = cl;
            *hi = cl;
        }
    }
}

static void decode_timings(const img_t *im, spd_base_timings_t *t)
{
    memset(t, 0, sizeof(*t));
    t->has = have(im, 0x5D, 1);          /* block extends through 93 */
    if (!t->has) return;

    t->tckavg_min_ps = le16(im, 0x14);
    t->tckavg_max_ps = le16(im, 0x16);

    uint64_t mask = 0;
    for (int i = 0; i < 5; i++)          /* spdr: 5 bytes LE -> 40 bits  */
        mask |= (uint64_t)b1(im, 0x18 + (size_t)i) << (8 * i);
    t->cl_mask = mask;
    cl_mask_bounds(mask, &t->cl_min, &t->cl_max);

    t->taa_ps  = le16(im, 0x1E);
    t->trcd_ps = le16(im, 0x20);
    t->trp_ps  = le16(im, 0x22);
    t->tras_ps = le16(im, 0x24);
    t->trc_ps  = le16(im, 0x26);
    t->twr_ps  = le16(im, 0x28);

    /* tRFC family stored in ns (x1000 -> ps) — spdr ns_units() */
    t->trfc1_ps   = (uint32_t)le16(im, 0x2A) * 1000u;
    t->trfc2_ps   = (uint32_t)le16(im, 0x2C) * 1000u;
    t->trfcsb_ps  = (uint32_t)le16(im, 0x2E) * 1000u;
    /* dlr variants: DDR5SPDEditor SPD_Struct @0x30..0x35 (same ns units) */
    t->trfc1_dlr_ps  = (uint32_t)le16(im, 0x30) * 1000u;
    t->trfc2_dlr_ps  = (uint32_t)le16(im, 0x32) * 1000u;
    t->trfcsb_dlr_ps = (uint32_t)le16(im, 0x34) * 1000u;

    struct { size_t off; uint32_t *ps; uint32_t *nck; } pairs[] = {
        { 0x46, &t->trrd_l_ps,     &t->trrd_l_nck },
        { 0x49, &t->tccd_l_ps,     &t->tccd_l_nck },
        { 0x4C, &t->tccd_l_wr_ps,  &t->tccd_l_wr_nck },
        { 0x4F, &t->tccd_l_wr2_ps, &t->tccd_l_wr2_nck },
        { 0x52, &t->tfaw_ps,       &t->tfaw_nck },
        { 0x55, &t->tccd_l_wtr_ps, &t->tccd_l_wtr_nck },
        { 0x58, &t->tccd_s_wtr_ps, &t->tccd_s_wtr_nck },
        { 0x5B, &t->trtp_ps,       &t->trtp_nck },
    };
    for (size_t i = 0; i < sizeof(pairs)/sizeof(pairs[0]); i++) {
        *pairs[i].ps  = le16(im, pairs[i].off);      /* ps, 1-unit      */
        *pairs[i].nck = b1(im, pairs[i].off + 2);    /* nCK lower limit */
    }

    t->base_data_rate_mt_s = spd_data_rate_mt_s(t->tckavg_min_ps);
}

/* ── Module-specific block (spdr module.rs, UDIMM only) ──────────────── */

static void decode_module(const img_t *im, const spd_identity_t *id,
                          spd_module_t *m)
{
    memset(m, 0, sizeof(*m));
    if (!id->has) return;

    if (strcmp(id->module_type_str, "UDIMM") != 0) {
        snprintf(m->not_decoded_reason, sizeof(m->not_decoded_reason),
                 "%s module block: decoder not implemented",
                 id->module_type_str);
        return;                       /* spdr: read no further bytes */
    }
    if (!have(im, 233, 1)) return;
    m->has = true;

    m->nominal_height_mm = (uint8_t)((b1(im, 230) & 0x1F) + 15);
    m->max_thickness_front_mm = (uint8_t)((b1(im, 231) & 0x0F) + 1);
    m->max_thickness_back_mm  = (uint8_t)(((b1(im, 231) >> 4) & 0x0F) + 1);

    /* Reference raw card (spdr decode_reference_raw_card; JEDEC letter
     * alphabet skips I O Q S X Z) */
    uint8_t byte232 = b1(im, 232);
    uint8_t code = byte232 & 0x1F;
    uint8_t revision = (uint8_t)((byte232 >> 5) & 0x03);
    static const char alphabet[21] = "ABCDEFGHJKLMNPRTUVWY";
    if (code == 0x1F) {
        snprintf(m->raw_card, sizeof(m->raw_card), "ZZ (no ref. raw card)");
    } else {
        unsigned index = (byte232 & 0x80) ? code + 0x1Fu : code;
        if (index < 20u)
            snprintf(m->raw_card, sizeof(m->raw_card), "%c revision %u",
                     alphabet[index], revision);
        else
            snprintf(m->raw_card, sizeof(m->raw_card), "%c%c revision %u",
                     alphabet[index / 20u], alphabet[index % 20u], revision);
    }

    uint8_t attrs = b1(im, 233);
    m->rank1_address_mirrored = (attrs & 0x01) != 0;
    m->module_attributes_raw = attrs;
}

/* ── Manufacturing block (spdr manufacturing.rs) ─────────────────────── */

typedef struct { uint8_t bank, code; const char *name; } jep106_t;
static const jep106_t jep106[] = {
    { 1, 0x2C, "Micron Technology" },
    { 1, 0x2D, "SK Hynix" },
    { 1, 0x4E, "Samsung" },
    { 1, 0x5A, "Winbond Electronic" },
    { 5, 0x6F, "Team Group Inc." },
};

static void jep106_resolve(uint8_t raw_bank, uint8_t raw_code,
                           uint8_t *bank, uint8_t *code, char *name, size_t nsz)
{
    *bank = (uint8_t)((raw_bank & 0x7F) + 1);   /* parity stripped */
    *code = (uint8_t)(raw_code & 0x7F);
    name[0] = '\0';
    for (size_t i = 0; i < sizeof(jep106)/sizeof(jep106[0]); i++)
        if (jep106[i].bank == *bank && jep106[i].code == *code) {
            snprintf(name, nsz, "%s", jep106[i].name);
            return;
        }
    /* spdr: unknown (bank, code) -> no name, NEVER a guessed name */
}

static uint8_t bcd2(uint8_t byte)   /* spdr bcd(): no nibble validation */
{
    return (uint8_t)(((byte >> 4) * 10) + (byte & 0x0F));
}

static void decode_manufacturing(const img_t *im, spd_manufacturing_t *mf)
{
    memset(mf, 0, sizeof(*mf));
    if (!have(im, 554, 1)) return;
    mf->has = true;

    jep106_resolve(b1(im, 512), b1(im, 513),
                   &mf->module_mfr_bank, &mf->module_mfr_code,
                   mf->module_mfr, sizeof(mf->module_mfr));
    mf->location = b1(im, 514);
    mf->year  = (uint16_t)(2000 + bcd2(b1(im, 515)));
    mf->week  = bcd2(b1(im, 516));
    /* serial: the ONLY big-endian field in the image */
    mf->serial = ((uint32_t)b1(im, 517) << 24) | ((uint32_t)b1(im, 518) << 16) |
                 ((uint32_t)b1(im, 519) << 8)  |  (uint32_t)b1(im, 520);

    /* Part number: any byte >= 0x80 -> field unusable (spdr NonAscii);
     * trim trailing ' ' and '\0' in any mix; may be empty. */
    bool nonascii = false;
    size_t end = 30;
    for (size_t i = 0; i < 30; i++)
        if (im->b[521 + i] >= 0x80) { nonascii = true; break; }
    if (!nonascii) {
        while (end > 0) {
            uint8_t c = b1(im, 521 + end - 1);
            if (c != ' ' && c != '\0') break;
            end--;
        }
        for (size_t i = 0; i < end; i++)
            mf->part_number[i] = (char)b1(im, 521 + i);
    }

    mf->revision_code = b1(im, 551);
    jep106_resolve(b1(im, 552), b1(im, 553),
                   &mf->dram_mfr_bank, &mf->dram_mfr_code,
                   mf->dram_mfr, sizeof(mf->dram_mfr));
    mf->dram_stepping = b1(im, 554);
}

/* ── Vendor profiles (spdr vendor.rs layout + DDR5SPDEditor field maps) ─ */

/* XMP profile-name read (spdr read_name): trim trailing ' '/NUL, reject
 * empty or any non-printable byte in the trimmed span -> no name. */
static void read_xmp_name(const img_t *im, size_t off, spd_profile_t *p)
{
    if (!have(im, off, 16)) return;
    size_t end = 16;
    while (end > 0 && (im->b[off + end - 1] == ' ' || im->b[off + end - 1] == 0))
        end--;
    for (size_t i = 0; i < end; i++)
        if (im->b[off + i] < 0x20 || im->b[off + i] > 0x7E)
            return;                   /* non-printable -> no name */
    if (end == 0) return;             /* empty -> no name, never "" */
    memcpy(p->name, im->b + off, end);
    p->name[end] = '\0';
    p->has_name = true;
}

static void cl_mask_read(const img_t *im, size_t off, spd_profile_t *p)
{
    uint64_t mask = 0;
    if (!have(im, off, 5)) return;
    for (int i = 0; i < 5; i++)
        mask |= (uint64_t)im->b[off + (size_t)i] << (8 * i);
    p->cl_mask = mask;
    cl_mask_bounds(mask, &p->cl_min, &p->cl_max);
}

static void profile_cl_finalize(spd_profile_t *p)
{
    p->has_cl = p->tck_ps != 0;
    if (!p->has_cl) { p->cl = 0; return; }
    p->cl = spd_cycles_round(p->taa_ps, p->tck_ps);
    /* spdr contains(): even CL in [20, 2+2*39], bit (cl-20)/2 */
    p->cl_in_mask = p->cl_mask != 0 && p->cl >= 20 && p->cl < 100 &&
                    (p->cl % 2 == 0) &&
                    (((p->cl_mask >> ((p->cl - 20u) / 2u)) & 1u) != 0);
}

/* XMP 3.0: header @0x280 (magic 0C 4A), five 64-B profile slots
 * @0x2C0/0x300/0x340/0x380/0x3C0, enable bits @0x283, names @0x28E/+16/+32;
 * per-block CRC over block-2 bytes (spdr + DDR5SPDEditor agree on all
 * ranges). Slot 4/5 = XMP3 user profiles (DDR5SPDEditor XMP_Struct:
 * profile1/2/3 + user_profile1/2). */
#define XMP_HDR_BASE   0x280u
#define XMP_BLOCK_LEN  64u
#define XMP_P1_BASE    0x2C0u
#define XMP_USER1_BASE 0x380u
#define XMP_USER2_BASE 0x3C0u
#define XMP_NAME1      0x28Eu

static void decode_xmp_profile(const img_t *im, size_t base, spd_profile_t *p)
{
    if (!have(im, base, XMP_BLOCK_LEN)) return;
    /* Field map: DDR5SPDEditor XMP_ProfileStruct (block-relative) */
    p->vpp_mv  = volt_byte(im->b[base + 0x00]);
    p->vdd_mv  = volt_byte(im->b[base + 0x01]);
    p->vddq_mv = volt_byte(im->b[base + 0x02]);
    p->has_vmemctrl = true;
    p->vmemctrl_mv  = volt_byte(im->b[base + 0x04]);
    p->tck_ps = le16(im, base + 0x05);
    cl_mask_read(im, base + 0x07, p);
    p->taa_ps  = le16(im, base + 0x0D);
    p->trcd_ps = le16(im, base + 0x0F);
    p->trp_ps  = le16(im, base + 0x11);
    p->tras_ps = le16(im, base + 0x13);
    p->trc_ps  = le16(im, base + 0x15);
    p->twr_ps  = le16(im, base + 0x17);
    p->trfc1_ns   = le16(im, base + 0x19);
    p->trfc2_ns   = le16(im, base + 0x1B);
    p->trfcsb_ns  = le16(im, base + 0x1D);
    p->trrd_l_ps      = le16(im, base + 0x1F); p->trrd_l_ll      = im->b[base + 0x21];
    p->tccd_l_wr_ps   = le16(im, base + 0x22); p->tccd_l_wr_ll   = im->b[base + 0x24];
    p->tccd_l_wr2_ps  = le16(im, base + 0x25); p->tccd_l_wr2_ll  = im->b[base + 0x27];
    p->tccd_l_wtr_ps  = le16(im, base + 0x28); p->tccd_l_wtr_ll  = im->b[base + 0x2A];
    p->tccd_s_wtr_ps  = le16(im, base + 0x2B); p->tccd_s_wtr_ll  = im->b[base + 0x2D];
    p->tccd_l_ps      = le16(im, base + 0x2E); p->tccd_l_ll      = im->b[base + 0x30];
    p->trtp_ps        = le16(im, base + 0x31); p->trtp_ll        = im->b[base + 0x33];
    p->tfaw_ps        = le16(im, base + 0x34); p->tfaw_ll        = im->b[base + 0x36];
    p->has_lower_limits = true;
    uint8_t boost = im->b[base + 0x3B];
    p->has_dm_boost = p->has_rtmc = true;
    p->dm_boost = (boost & 0x01) != 0;   /* IntelDynamicMemoryBoostBit   */
    p->rtmc   = (boost & 0x02) != 0;     /* RealTimeMemoryFrequencyOCBit */
    static const char *crate[] = { "undefined", "1n", "2n", "3n" };
    uint8_t cr = im->b[base + 0x3C] & 0x0F;
    p->has_command_rate = cr <= 3;
    if (p->has_command_rate)
        snprintf(p->command_rate, sizeof(p->command_rate), "%s", crate[cr]);
    p->data_rate_mt_s = spd_data_rate_mt_s(p->tck_ps);
    profile_cl_finalize(p);
    p->crc_valid = section_crc(im, base, XMP_BLOCK_LEN).matches;
}

static void decode_xmp(const img_t *im, bool expo_at_340, spd_xmp_t *x)
{
    memset(x, 0, sizeof(*x));
    if (!have(im, XMP_HDR_BASE, 2)) return;
    if (im->b[XMP_HDR_BASE] != 0x0C || im->b[XMP_HDR_BASE + 1] != 0x4A)
        return;                       /* spdr: absent magic -> read nothing */
    x->present = true;
    x->version = b1(im, XMP_HDR_BASE + 2);
    x->version_known = x->version == 0x30;    /* XMP 3.0 */
    x->enable_bits = b1(im, XMP_HDR_BASE + 3);
    x->header_crc = section_crc(im, XMP_HDR_BASE, XMP_BLOCK_LEN);
    x->hybrid_expo = expo_at_340;

    for (int i = 0; i < 3; i++) {
        if (i == 2 && expo_at_340)    /* slot belongs to the EXPO block */
            break;
        if (!(x->enable_bits & (1u << i)))
            continue;                 /* disabled slot: not parsed (spdr) */
        spd_profile_t *p = &x->profile[i];
        p->present = true;
        decode_xmp_profile(im, XMP_P1_BASE + (size_t)i * XMP_BLOCK_LEN, p);
        read_xmp_name(im, XMP_NAME1 + (size_t)i * 16, p);
    }

    /* XMP3 user profiles (DDR5SPDEditor XMP_Struct.user_profile1/2):
     * NOT gated by the enable bits. Presence gate = block CRC valid
     * (stored field != 0 AND == CRC16 over the first 62 bytes). This is
     * stricter than the editor's hasData() (any non-zero CRC16), which
     * false-positives on vendor data overlapping the slots — the
     * TEAMGROUP sample carries a vendor tail at 0x3E0 that hasData()
     * would present as "User 2". A BIOS/editor-written user profile
     * always carries a valid CRC (XMP3_Bundle::fixCRCs), so real
     * profiles still pass. User1 @0x380 sits inside the 128-B EXPO
     * block on hybrid modules (isXMPUser1Present returns false when
     * EXPO coexists); User2 @0x3C0 stays valid there
     * (isXMPUser2Present). The header has name fields for P1..P3 only,
     * so user slots carry no name. */
    static const size_t user_base[2] = { XMP_USER1_BASE, XMP_USER2_BASE };
    for (int i = 0; i < 2; i++) {
        if (i == 0 && expo_at_340)
            continue;
        size_t base = user_base[i];
        if (!have(im, base, XMP_BLOCK_LEN)) continue;
        spd_crc_t c = section_crc(im, base, XMP_BLOCK_LEN);
        if (!c.has || c.stored == 0 || !c.matches)
            continue;
        spd_profile_t *p = &x->profile[3 + i];
        p->present = true;
        decode_xmp_profile(im, base, p);
    }
}

/* EXPO: block @0x340, magic "EXPO" + revision 0x10 (DDR5SPDEditor
 * isEXPOPresentStatic requires both; spdr checks the magic only). Profiles
 * 40 B @ +0x0A/+0x32; enable byte +0x05 (bit0/bit4) with tCK != 0 fallback
 * (spdr gated on tCK only; samples always set both -> AND-free union). */
#define EXPO_BASE      0x340u
#define EXPO_BLOCK_LEN 128u
#define EXPO_P1_OFF    0x0Au
#define EXPO_P_LEN     40u

static void decode_expo_profile(const img_t *im, size_t base, spd_profile_t *p)
{
    if (!have(im, base, EXPO_P_LEN)) return;
    /* Field map: DDR5SPDEditor EXPO_ProfileStruct — vdd first (unlike XMP) */
    p->vdd_mv  = volt_byte(im->b[base + 0x00]);
    p->vddq_mv = volt_byte(im->b[base + 0x01]);
    p->vpp_mv  = volt_byte(im->b[base + 0x02]);
    p->tck_ps    = le16(im, base + 0x04);
    p->taa_ps    = le16(im, base + 0x06);
    p->trcd_ps   = le16(im, base + 0x08);
    p->trp_ps    = le16(im, base + 0x0A);
    p->tras_ps   = le16(im, base + 0x0C);
    p->trc_ps    = le16(im, base + 0x0E);
    p->twr_ps    = le16(im, base + 0x10);
    p->trfc1_ns  = le16(im, base + 0x12);
    p->trfc2_ns  = le16(im, base + 0x14);
    p->trfcsb_ns = le16(im, base + 0x16);
    p->trrd_l_ps     = le16(im, base + 0x18);
    p->tccd_l_ps     = le16(im, base + 0x1A);
    p->tccd_l_wr_ps  = le16(im, base + 0x1C);
    p->tccd_l_wr2_ps = le16(im, base + 0x1E);
    p->tfaw_ps       = le16(im, base + 0x20);
    p->tccd_l_wtr_ps = le16(im, base + 0x22);
    p->tccd_s_wtr_ps = le16(im, base + 0x24);
    p->trtp_ps       = le16(im, base + 0x26);
    p->data_rate_mt_s = spd_data_rate_mt_s(p->tck_ps);
    profile_cl_finalize(p);
}

static void decode_expo(const img_t *im, spd_expo_t *e)
{
    memset(e, 0, sizeof(*e));
    if (!have(im, EXPO_BASE, 4)) return;
    if (memcmp(im->b + EXPO_BASE, "EXPO", 4) != 0)
        return;
    e->present = true;
    e->revision = b1(im, EXPO_BASE + 4);
    e->enable_bits = b1(im, EXPO_BASE + 5);
    e->block_crc = section_crc(im, EXPO_BASE, EXPO_BLOCK_LEN);

    static const uint8_t en_bit[2] = { 0, 4 };
    for (int i = 0; i < 2; i++) {
        size_t base = EXPO_BASE + EXPO_P1_OFF + (size_t)i * EXPO_P_LEN;
        if (!have(im, base, EXPO_P_LEN)) continue;
        /* A slot is populated when its tCK is non-zero (spdr rule) AND the
         * enable bit agrees (DDR5SPDEditor rule); if the enable byte's P1/P2
         * bits carry no information (both zero), fall back to the spdr rule. */
        bool tck_set = le16(im, base + 0x04) != 0;
        bool bit_set = (e->enable_bits & (1u << en_bit[i])) != 0;
        bool no_en_info = (e->enable_bits & 0x11u) == 0;
        if (!tck_set || (!bit_set && !no_en_info))
            continue;
        spd_profile_t *p = &e->profile[i];
        p->present = true;
        decode_expo_profile(im, base, p);
    }
}

/* ── Top level ───────────────────────────────────────────────────────── */

bool spd_decode_ddr5(const uint8_t *image, size_t len, spd_info_t *info)
{
    img_t im = { image, len > SPD_IMAGE_STD ? SPD_IMAGE_STD : len };
    memset(info, 0, sizeof(*info));
    info->image_size = (uint16_t)len;

    if (len < SPD_IMAGE_MIN) {
        snprintf(info->error, sizeof(info->error),
                 "SPD image too small: %zu bytes (need >= %d)",
                 len, SPD_IMAGE_MIN);
        return false;
    }
    if (im.b[2] != 0x12) {
        snprintf(info->error, sizeof(info->error),
                 "not a DDR5 SPD (device type byte = 0x%02X)", im.b[2]);
        return false;
    }

info->valid = true;
info->base_crc = section_crc(&im, 0, SPD_IMAGE_STD / 2); /* covers 0..509 */
    decode_identity(&im, &info->identity);
    decode_timings(&im, &info->timings);
    decode_module(&im, &info->identity, &info->module);
    decode_manufacturing(&im, &info->manufacturing);
    decode_expo(&im, &info->expo);
    decode_xmp(&im, info->expo.present, &info->xmp);
    return true;
}