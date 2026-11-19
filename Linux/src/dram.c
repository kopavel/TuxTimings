/*
 * dram.c — SMN access and per-UMC-channel probing for TuxTimings.
 *
 * Channel enumeration and register layout ported from ZenStates-Core
 * (ZenTimings), MIT License, (c) Ilian Rusanov:
 *   Hardware/DRAM/MemoryConfig.cs (ReadChannelsNoLock: offsets i<<20,
 *   0x50DF0 bit 19 channel enable, 0x50000/0x50008 DIMM presence).
 * All register decoding lives in dram_decode.c (pure, unit-testable).
 */

#include "dram.h"
#include "util.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define SMN_PATH "/sys/kernel/ryzen_smu_drv/smn"

/* Persistent SMN file handle: one open per session instead of per read.
 * The ryzen_smu sysfs file implements address-write + data-read on every
 * pair of read()/write() calls on the same fd. */
static int s_smn_fd = -1;

int dram_smn_open(void)
{
    if (s_smn_fd >= 0) return 1;
    s_smn_fd = open(SMN_PATH, O_RDWR);
    if (s_smn_fd < 0) {
        /* fall back to the old r+ stdio path semantics? no — fail cleanly */
        return 0;
    }
    return 1;
}

void dram_smn_close(void)
{
    if (s_smn_fd >= 0) { close(s_smn_fd); s_smn_fd = -1; }
}

/*
 * Read one SMN register. Returns 1 on success. On failure *ok=0 (or ok NULL)
 * and the value is 0 — the caller counts failures so the debug dump can show
 * hardware-read problems instead of silently reporting zeros.
 */
static uint32_t smn_read(uint32_t address, bool *ok)
{
    uint8_t buf[4];

    if (s_smn_fd < 0 && !dram_smn_open()) {
        if (ok) *ok = false;
        return 0;
    }

    /* write address register */
    buf[0] = (uint8_t)(address);
    buf[1] = (uint8_t)(address >> 8);
    buf[2] = (uint8_t)(address >> 16);
    buf[3] = (uint8_t)(address >> 24);
    off_t pos = 0;
    if (pwrite(s_smn_fd, buf, 4, pos) != 4) {
        if (ok) *ok = false;
        return 0;
    }
    if (pread(s_smn_fd, buf, 4, pos) != 4) {
        if (ok) *ok = false;
        return 0;
    }

    if (ok) *ok = true;
    return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

/* Upstream author's 1-arg reader API (his decoders below call it verbatim). */
static uint32_t read_smn(uint32_t address)
{
    bool ok;
    return smn_read(address, &ok);
}

/* ── The original TuxTimings decoders, kept as the single source for the
 * fields they already covered (read_common_timings / read_ddr5_timings /
 * read_ddr4_timings, verbatim except taking the channel SMN offset so they
 * run per UMC instance). Anything newer lives in dram_decode.c and only
 * adds fields. ───────────────────────────────────────────────────────── */

static float to_nanoseconds(uint32_t cycles, float freq_mhz)
{
    if (freq_mhz <= 0) return 0;
    float ns = (float)cycles * 2000.0f / freq_mhz;
    if (ns > (float)cycles) ns /= 2.0f;
    return ns;
}

/* Common timing extraction from SMN registers — shared between DDR4 and DDR5 */
static void read_common_timings(uint32_t offset, dram_timings_t *d)
{
    uint32_t reg50204 = read_smn(offset | 0x50204);
    uint32_t reg50208 = read_smn(offset | 0x50208);
    uint32_t reg5020C = read_smn(offset | 0x5020C);
    uint32_t reg50210 = read_smn(offset | 0x50210);
    uint32_t reg50214 = read_smn(offset | 0x50214);
    uint32_t reg50218 = read_smn(offset | 0x50218);
    uint32_t reg5021C = read_smn(offset | 0x5021C);
    uint32_t reg50220 = read_smn(offset | 0x50220);
    uint32_t reg50224 = read_smn(offset | 0x50224);
    uint32_t reg50228 = read_smn(offset | 0x50228);
    uint32_t reg50230 = read_smn(offset | 0x50230);
    uint32_t reg50234 = read_smn(offset | 0x50234);
    uint32_t reg50250 = read_smn(offset | 0x50250);
    uint32_t reg50254 = read_smn(offset | 0x50254);
    uint32_t reg50258 = read_smn(offset | 0x50258);
    uint32_t reg502A4 = read_smn(offset | 0x502A4);

    d->tcl     = bit_slice(reg50204, 5, 0);
    d->trcd_rd = bit_slice(reg50204, 21, 16);
    d->trcd_wr = bit_slice(reg50204, 29, 24);
    if (d->trcd_wr == 0) d->trcd_wr = d->trcd_rd;
    d->tras    = bit_slice(reg50204, 14, 8);
    d->trp     = bit_slice(reg50208, 21, 16);
    d->trc     = bit_slice(reg50208, 7, 0);

    d->trrds   = bit_slice(reg5020C, 4, 0);
    d->trrdl   = bit_slice(reg5020C, 12, 8);
    d->tfaw    = bit_slice(reg50210, 7, 0);
    d->rtp     = bit_slice(reg5020C, 28, 24);
    d->wtrs    = bit_slice(reg50214, 12, 8);
    d->wtrl    = bit_slice(reg50214, 22, 16);
    d->tcwl    = bit_slice(reg50214, 5, 0);
    d->twr     = bit_slice(reg50218, 7, 0);
    if (d->twr == 0) d->twr = d->wtrs;

    d->trc_page = bit_slice(reg5021C, 31, 20);

    d->rdrd_scl = bit_slice(reg50220, 29, 24);
    d->rdrd_sc  = bit_slice(reg50220, 19, 16);
    d->rdrd_sd  = bit_slice(reg50220, 11, 8);
    d->rdrd_dd  = bit_slice(reg50220, 3, 0);

    d->wrwr_scl = bit_slice(reg50224, 29, 24);
    d->wrwr_sc  = bit_slice(reg50224, 19, 16);
    d->wrwr_sd  = bit_slice(reg50224, 11, 8);
    d->wrwr_dd  = bit_slice(reg50224, 3, 0);

    d->rdwr = bit_slice(reg50228, 13, 8);
    d->wrrd = bit_slice(reg50228, 3, 0);
    d->refi = bit_slice(reg50230, 15, 0);

    d->mod_pda = bit_slice(reg50234, 29, 24);
    d->mrd_pda = bit_slice(reg50234, 21, 16);
    d->mod     = bit_slice(reg50234, 13, 8);
    d->mrd     = bit_slice(reg50234, 5, 0);

    d->stag    = bit_slice(reg50250, 26, 16);
    d->stag_sb = bit_slice(reg50250, 8, 0);

    d->cke = bit_slice(reg50254, 28, 24);
    d->xp  = bit_slice(reg50254, 5, 0);

    d->phy_wrd = bit_slice(reg50258, 26, 24);
    d->phy_rdl = bit_slice(reg50258, 23, 16);
    d->phy_wrl = bit_slice(reg50258, 15, 8);

    d->wrpre = bit_slice(reg502A4, 10, 8);
    d->rdpre = bit_slice(reg502A4, 2, 0);
}

static void read_ddr5_timings(uint32_t offset, dram_timings_t *d)
{

    /* Ratio -> frequency */
    uint32_t ratio_reg = read_smn(offset | 0x50200);
    float ratio = bit_slice(ratio_reg, 15, 0) / 100.0f;
    float mem_freq = ratio * 200.0f;
    d->frequency_hint_mhz = mem_freq;

    /* GDM, Cmd2T, PowerDown */
    d->gdm_enabled = bit_slice(ratio_reg, 18, 18) == 1;
    uint32_t cmd2t_bit = bit_slice(ratio_reg, 17, 17);
    snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    uint32_t refresh_reg = read_smn(offset | 0x5012C);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);

    /* RFC - DDR5: choose first non-default from 4 registers */
    uint32_t trfc_regs[4];
    trfc_regs[0] = read_smn(offset | 0x50260);
    trfc_regs[1] = read_smn(offset | 0x50264);
    trfc_regs[2] = read_smn(offset | 0x50268);
    trfc_regs[3] = read_smn(offset | 0x5026C);
    uint32_t trfc_reg = 0;
    for (int i = 0; i < 4; i++) {
        if (trfc_regs[i] != 0x00C00138) { trfc_reg = trfc_regs[i]; break; }
    }
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 15, 0);
        d->rfc2 = bit_slice(trfc_reg, 31, 16);
    }

    /* RFCsb */
    uint32_t rfcsb_regs[4];
    rfcsb_regs[0] = bit_slice(read_smn(offset | 0x502C0), 10, 0);
    rfcsb_regs[1] = bit_slice(read_smn(offset | 0x502C4), 10, 0);
    rfcsb_regs[2] = bit_slice(read_smn(offset | 0x502C8), 10, 0);
    rfcsb_regs[3] = bit_slice(read_smn(offset | 0x502CC), 10, 0);
    for (int i = 0; i < 4; i++) {
        if (rfcsb_regs[i] != 0) { d->rfcsb = rfcsb_regs[i]; break; }
    }

    /* Nanosecond conversions */
    d->trefi_ns  = to_nanoseconds(d->refi, mem_freq);
    d->trfc_ns   = to_nanoseconds(d->rfc, mem_freq);
    d->trfc2_ns  = to_nanoseconds(d->rfc2, mem_freq);
    d->trfcsb_ns = to_nanoseconds(d->rfcsb, mem_freq);
}

static void read_ddr4_timings(uint32_t offset, dram_timings_t *d)
{

    /*
     * DDR4 frequency ratio / flags (aligned with ZenStates-Core):
     *   - ratio: bits 0..7, scaled by /3.0, then *200 -> effective MT/s
     *   - Cmd2T: bit 10
     *   - GDM:   bit 11
     */
    uint32_t ratio_reg = read_smn(offset | 0x50200);
    float ratio = (float)bit_slice(ratio_reg, 7, 0) / 3.0f;
    float mem_freq = ratio * 200.0f;
    d->frequency_hint_mhz = mem_freq;

    uint32_t cmd2t_bit = bit_slice(ratio_reg, 10, 10);
    snprintf(d->cmd2t, sizeof(d->cmd2t), "%s", cmd2t_bit ? "2T" : "1T");
    d->gdm_enabled = bit_slice(ratio_reg, 11, 11) == 1;
    uint32_t refresh_reg = read_smn(offset | 0x5012C);
    d->power_down_enabled = bit_slice(refresh_reg, 28, 28) == 1;

    read_common_timings(offset, d);

    /* RFC - DDR4: first non-default from 2 registers */
    uint32_t trfc0 = read_smn(offset | 0x50260);
    uint32_t trfc1 = read_smn(offset | 0x50264);
    uint32_t trfc_reg = (trfc0 != trfc1) ? ((trfc0 != 0x21060138) ? trfc0 : trfc1) : trfc0;
    if (trfc_reg) {
        d->rfc  = bit_slice(trfc_reg, 10, 0);
        d->rfc2 = bit_slice(trfc_reg, 21, 11);
    }

    /* Nanosecond conversions (DDR4 path) */
    d->trefi_ns  = to_nanoseconds(d->refi, mem_freq);
    d->trfc_ns   = to_nanoseconds(d->rfc, mem_freq);
    d->trfc2_ns  = to_nanoseconds(d->rfc2, mem_freq);
}

#define SMN_READ(dst, off, reg)                                   \
    do {                                                          \
        bool ok_;                                                  \
        (dst) = smn_read((off) | (reg), &ok_);                     \
        if (!ok_) fails++;                                         \
    } while (0)

/* Read the whole UMC register block of one channel. */
static void read_umc_block(uint32_t off, umc_raw_regs_t *r, int *fails_out)
{
    int fails = 0;

    memset(r, 0, sizeof(*r));

    /* Channel enable + DDR max rate (0x50DF0), enable rate (0x50DF4) */
    SMN_READ(r->enabled, off, 0x50DF0);
    SMN_READ(r->regDF4,  off, 0x50DF4);

    /* DIMM / chip-select config (0x50000..0x5000C)
     * bit 0 of 0x50000 / 0x50008: DIMM0 / DIMM1 installed (ZenStates-Core)
     * bit 0 of 0x50000+4*cs: chip select cs enabled (DDR4 rank method) */
    SMN_READ(r->dimm0_config, off, 0x50000);
    SMN_READ(r->cs0_cfg,      off, 0x50000);
    SMN_READ(r->cs1_cfg,      off, 0x50004);
    SMN_READ(r->dimm1_config, off, 0x50008);
    SMN_READ(r->cs2_cfg,      off, 0x50008);
    SMN_READ(r->cs3_cfg,      off, 0x5000C);

    /* DDR5 rank address masks: DIMM0 regs 0x50020/0x50024, DIMM1 0x50028/0x5002C */
    SMN_READ(r->rank0_mask[0], off, 0x50020);
    SMN_READ(r->rank0_mask[1], off, 0x50024);
    SMN_READ(r->rank1_mask[0], off, 0x50028);
    SMN_READ(r->rank1_mask[1], off, 0x5002C);

    /* Address config per DIMM */
    SMN_READ(r->addr_cfg0, off, 0x50040);
    SMN_READ(r->addr_cfg1, off, 0x50048);

    /* Bank group swap maps */
    SMN_READ(r->bgs0,     off, 0x50050);
    SMN_READ(r->bgs1,     off, 0x50058);
    SMN_READ(r->bgs_alt0, off, 0x500D0);
    SMN_READ(r->bgs_alt1, off, 0x500D4);

    /* Config registers */
    SMN_READ(r->ecc_burst,   off, 0x50100);
    SMN_READ(r->pwrdownctrl, off, 0x5012C);
    SMN_READ(r->odtsctrl,    off, 0x50130);
    SMN_READ(r->brstgap,     off, 0x50198);

    /* Timings */
    SMN_READ(r->umcratio, off, 0x50200);
    SMN_READ(r->reg204,   off, 0x50204);
    SMN_READ(r->reg208,   off, 0x50208);
    SMN_READ(r->reg20C,   off, 0x5020C);
    SMN_READ(r->reg210,   off, 0x50210);
    SMN_READ(r->reg214,   off, 0x50214);
    SMN_READ(r->reg218,   off, 0x50218);
    SMN_READ(r->reg21C,   off, 0x5021C);
    SMN_READ(r->reg220,   off, 0x50220);
    SMN_READ(r->reg224,   off, 0x50224);
    SMN_READ(r->reg228,   off, 0x50228);
    SMN_READ(r->reg22C,   off, 0x5022C);
    SMN_READ(r->reg230,   off, 0x50230);
    SMN_READ(r->reg234,   off, 0x50234);
    SMN_READ(r->reg238,   off, 0x50238);
    SMN_READ(r->reg23C,   off, 0x5023C);
    SMN_READ(r->reg240,   off, 0x50240);
    SMN_READ(r->reg244,   off, 0x50244);
    SMN_READ(r->reg248,   off, 0x50248);
    SMN_READ(r->reg24C,   off, 0x5024C);
    SMN_READ(r->reg250,   off, 0x50250);
    SMN_READ(r->reg254,   off, 0x50254);
    SMN_READ(r->reg258,   off, 0x50258);
    SMN_READ(r->reg25C,   off, 0x5025C);
    SMN_READ(r->trfc[0],  off, 0x50260);
    SMN_READ(r->trfc[1],  off, 0x50264);
    SMN_READ(r->trfc[2],  off, 0x50268);
    SMN_READ(r->trfc[3],  off, 0x5026C);
    SMN_READ(r->reg284,   off, 0x50284);
    SMN_READ(r->reg288,   off, 0x50288);
    SMN_READ(r->reg28C,   off, 0x5028C);
    SMN_READ(r->reg2A4,   off, 0x502A4);
    SMN_READ(r->reg2A8,   off, 0x502A8);
    SMN_READ(r->rfcsb[0], off, 0x502C0);
    SMN_READ(r->rfcsb[1], off, 0x502C4);
    SMN_READ(r->rfcsb[2], off, 0x502C8);
    SMN_READ(r->rfcsb[3], off, 0x502CC);
    SMN_READ(r->reg2E0,   off, 0x502E0);

    *fails_out = fails;
}

void dram_read_channels(bool is_ddr5, float mem_speed_hint_mhz, float bclk_mhz,
                        dram_channel_t *chans, int *chan_count)
{
    *chan_count = 0;

    if (!dram_smn_open())
        return;

    for (uint32_t i = 0; i < MAX_UMC_CHANNELS; i++) {
        uint32_t off = i << 20;   /* ZenStates-Core: channel offset = i << 20 */
        dram_channel_t *d = &chans[*chan_count];
        memset(d, 0, sizeof(*d));
        snprintf(d->label, sizeof(d->label), "Channel %c (UMC%u)", 'A' + (char)i, i);
        d->smn_offset = off;

        /* Cheap existence probe first: unimplemented SMN offsets read back 0
         * with the access itself succeeding, so also require the DIMM bits. */
        bool ok = false;
        uint32_t en = smn_read(off | 0x50DF0, &ok);
        if (!ok) {
            d->read_failures = 1;
            d->present = false;
            (*chan_count)++;
            continue;
        }
        /* bit 19 == 0 means the channel is enabled (ZenStates-Core) */
        bool chan_en = (bit_slice(en, 19, 19) == 0);

        if (!chan_en) {
            /* Still record it (disabled but present UMC) for diagnostics. */
            d->raw.enabled = en;
            d->present = false;
            (*chan_count)++;
            continue;
        }

        int fails = 0;
        read_umc_block(off, &d->raw, &fails);
        d->read_failures = fails;

        d->is_ddr5 = is_ddr5;
        umc_regs_decode(&d->raw, is_ddr5, d);

        /* Author's own decoders fill dram_timings_t (live SMN reads at this
         * channel's offset). */
        if (is_ddr5)
            read_ddr5_timings(off, &d->t);
        else
            read_ddr4_timings(off, &d->t);
        if (is_ddr5) {
            /* ZenStates-Core DDR5 dictionary displays tWRPRE as raw+1 and
             * tRDPRE as raw+1 when raw<2; the original decoder stores raw.
             * Apply the dictionary adjustment additively. */
            d->t.wrpre += 1u;
            if (d->t.rdpre < 2u) d->t.rdpre += 1u;
        }

        /* Authoritative sources win: PM-table MT/s first, else ratio x real BCLK
         * (BaseDramTimings.Frequency logic). */
        if (mem_speed_hint_mhz > 0.0f)
            d->t.frequency_hint_mhz = mem_speed_hint_mhz;
        else if (bclk_mhz > 0.0f)
            d->t.frequency_hint_mhz = umc_ratio_to_memspeed(d->raw.umcratio, is_ddr5, bclk_mhz);
        float mtaps = d->t.frequency_hint_mhz;
        if (mtaps > 0.0f) {
            d->t.trefi_ns  = clocks_to_ns(d->t.refi, mtaps);
            d->t.trfc_ns   = clocks_to_ns(d->t.rfc, mtaps);
            d->t.trfc2_ns  = clocks_to_ns(d->t.rfc2, mtaps);
            d->t.trfcsb_ns = clocks_to_ns(d->t.rfcsb, mtaps);
            /* Ddr5Timings.RFCns: non-normal refresh modes report tRFC2 */
            if (is_ddr5 && d->refresh_mode[0] &&
                strcmp(d->refresh_mode, "NORMAL") != 0)
                d->t.trfc_ns = clocks_to_ns(d->t.rfc2, mtaps);
        }
        (*chan_count)++;
    }
}

void dram_rescale_ns(dram_channel_t *chans, int chan_count, float mtaps)
{
    if (mtaps <= 0.0f) return;
    for (int i = 0; i < chan_count; i++) {
        dram_channel_t *d = &chans[i];
        if (!d->present) continue;
        d->t.frequency_hint_mhz = mtaps;
        d->t.trefi_ns  = clocks_to_ns(d->t.refi, mtaps);
        d->t.trfc_ns   = clocks_to_ns(d->t.rfc, mtaps);
        d->t.trfc2_ns  = clocks_to_ns(d->t.rfc2, mtaps);
        d->t.trfcsb_ns = clocks_to_ns(d->t.rfcsb, mtaps);
        if (d->is_ddr5 && d->refresh_mode[0] &&
            strcmp(d->refresh_mode, "NORMAL") != 0)
            d->t.trfc_ns = clocks_to_ns(d->t.rfc2, mtaps);
    }
}

/* Original public API (single-channel, UMC0): kept — fills the author's
 * struct via his own decoders. New code should use dram_read_channels(). */
void dram_read_timings(int codename_index, dram_timings_t *out)
{
    memset(out, 0, sizeof(*out));
    switch (codename_index) {
    case 20: case 21: /* Raphael (Ryzen 7000), Phoenix (Ryzen 7040/8040) — Zen4 DDR5 */
    case 22: case 24: /* Strix Point / Hawk Point — Zen5/Zen4c DDR5 */
    case 23:          /* Granite Ridge (Ryzen 9000) — Zen5 DDR5 */
        read_ddr5_timings(0, out);
        break;
    case 2:  /* Renoir (APU, DDR4/LPDDR4) */
    case 3:  /* Picasso (APU, DDR4) */
    case 7:  /* Raven Ridge (APU, DDR4) */
    case 8:  /* Raven Ridge 2 (APU, DDR4) */
    case 14: /* Cezanne (APU, DDR4) */
    case 16: /* Dali (APU, DDR4) */
    case 17: /* Lucienne (APU, DDR4/LPDDR4) */
    case 4: case 9: case 10: case 12: case 18: case 19:
        /* Matisse, Summit, Pinnacle, Vermeer, Naples, Chagall (DDR4) */
        read_ddr4_timings(0, out);
        break;
    default:
        break;
    }
}
