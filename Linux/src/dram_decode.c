/*
 * dram_decode.c — pure UMC / AOD register decoding for TuxTimings.
 *
 * Register maps, bit fields and value encodings ported from ZenStates-Core
 * (ZenTimings), MIT License, (c) Ilian Rusanov:
 *   Hardware/DRAM/BaseDramTimings.cs, Hardware/DRAM/Ddr5Timings.cs,
 *   Dictionaries/DDR5Dictionary.cs, Hardware/DRAM/MemoryConfig.cs,
 *   Hardware/Aod/AodData.cs, Dictionaries/AodDictionaries.cs,
 *   Dictionaries/EncodedValueDictionaries.cs.
 * No hardware access happens here — see dram.c for the SMN layer.
 */

#include "dram_decode.h"
#include "util.h"
#include <string.h>
#include <stdio.h>

#define NAFMT "%s"
static const char NA[] = "N/A";

float clocks_to_ns(uint32_t clocks, float mtaps)
{
    if (mtaps <= 0.0f) return 0.0f;
    return (float)clocks * 2.0f * 1000.0f / mtaps;
}

float umc_ratio_to_memspeed(uint32_t umcratio, bool is_ddr5, float bclk_mhz)
{
    float ratio;
    if (is_ddr5) {
        /* Ddr5Timings.ReadRatio: bits [15:0] / 100 */
        ratio = (float)bit_slice(umcratio, 15, 0) / 100.0f;
    } else {
        /* Ddr4Timings.ReadRatio: GetBits(reg, 0, 7) = bits [6:0] / 3 */
        ratio = (float)bit_slice(umcratio, 6, 0) / 3.0f;
    }
    if (ratio <= 0.0f) return 0.0f;
    if (bclk_mhz <= 0.0f) bclk_mhz = 100.0f; /* BaseDramTimings.DefaultBclk */
    return ratio * bclk_mhz * 2.0f;          /* DDR4/DDR5: 2 clocks -> MT/s */
}

void umc_refresh_mode_decode(uint32_t pwrdownctrl, char *out, size_t outsz, uint32_t *fgr_out)
{
    /* Ddr5Timings.Read(): FGR = [18:16], per-bank refresh = bit 1 */
    uint32_t fgr = bit_slice(pwrdownctrl, 18, 16);
    uint32_t per_bank = bit_slice(pwrdownctrl, 1, 1);
    if (fgr_out) *fgr_out = fgr;

    if (per_bank == 0)
        snprintf(out, outsz, "%s", fgr == 0 ? "NORMAL" : "FGR");
    else
        snprintf(out, outsz, "%s", fgr != 0 ? "MIXED" : "PBONLY");
}

/*
 * DDR5 rank: ZenStates-Core GetRank() — the DIMM's rank address-mask
 * registers; a rank is present when its mask register is programmed
 * (non-zero, bits [9:2] < 3 or the well-known 0x07FFFBFE pattern).
 */
int umc_detect_rank_ddr5(const uint32_t mask[2])
{
    int ranks = 0;
    for (int i = 0; i < 2; i++) {
        uint32_t v = mask[i];
        if (v != 0 && (v == 0x07FFFBFEu || bit_slice(v, 9, 2) < 3))
            ranks++;
    }
    return ranks;
}

/* DDR4 rank: chip-select config registers bit 0 (Linux amd64_edac method). */
int umc_detect_rank_ddr4(uint32_t cs0, uint32_t cs1, int dimm)
{
    (void)dimm;
    int ranks = 0;
    if (cs0 & 1u) ranks++;
    if (cs1 & 1u) ranks++;
    return ranks;
}

/* ── Electrical value dictionaries (EncodedValueDictionaries.cs) ─────── */

const char *proc_odt_decode(uint32_t code)
{
    switch (code) {
    case 0:  return "Hi-Z";
    case 1:  return "480.0 Ω";
    case 2:  return "240.0 Ω";
    case 3:  return "160.0 Ω";
    case 4:  return "120.0 Ω";
    case 5:  return "96.0 Ω";
    case 6:  return "80.0 Ω";
    case 7:  return "68.6 Ω";
    case 12: return "60.0 Ω";
    case 13: return "53.3 Ω";
    case 14: return "48.0 Ω";
    case 15: return "43.6 Ω";
    case 28: return "40.0 Ω";
    case 29: return "36.9 Ω";
    case 30: return "34.3 Ω";
    case 31: return "32.0 Ω";
    case 60: return "30.0 Ω";
    case 61: return "28.2 Ω";
    case 62: return "26.7 Ω";
    case 63: return "25.3 Ω";
    default: return NA;
    }
}

const char *proc_data_ds_decode(uint32_t code)
{
    switch (code) {
    case 0:  return "Hi-Z";
    case 2:  return "240.0 Ω";
    case 4:  return "120.0 Ω";
    case 6:  return "80.0 Ω";
    case 12: return "60.0 Ω";
    case 14: return "48.0 Ω";
    case 28: return "40.0 Ω";
    case 30: return "34.3 Ω";
    default: return NA;
    }
}

const char *dram_data_ds_decode(uint32_t code)
{
    switch (code) {
    case 0:  return "34.0 Ω";
    case 1:  return "40.0 Ω";
    case 2:  return "48.0 Ω";
    default: return NA;
    }
}

const char *cad_bus_ds_decode(uint32_t code)
{
    switch (code) {
    case 0:   return "Hi-Z";
    case 30:  return "30.0 Ω";
    case 40:  return "40.0 Ω";
    case 60:  return "60.0 Ω";
    case 120: return "120.0 Ω";
    default:  return NA;
    }
}

const char *proc_impedance_decode(uint32_t code)
{
    switch (code) {
    case 0:   return "Off";
    case 30:  return "30.0 Ω";
    case 40:  return "40.0 Ω";
    case 60:  return "60.0 Ω";
    case 120: return "120.0 Ω";
    default:  return NA;
    }
}

/* Rtt class: "RZQ/n" plus the resulting ohms (RZQ = 240 Ω). */
void rtt_decode(uint32_t code, char *out, size_t outsz)
{
    if (code == 0) { snprintf(out, outsz, "Off"); return; }
    if (code >= 1 && code <= 7) {
        snprintf(out, outsz, "RZQ/%u (%d Ω)", code, 240 / (int)code);
        return;
    }
    snprintf(out, outsz, "%s", NA);
}

/* ── UMC register block decoding ─────────────────────────────────────── */

/* 0x502A4: WRPRE/RDPRE carry zero-based quirks (BaseDramTimings.WRPRE/RDPRE). */

void umc_regs_decode(const umc_raw_regs_t *raw, bool is_ddr5, dram_channel_t *d)
{
    d->is_ddr5 = is_ddr5;
    d->raw = *raw;

    /* ── Topology: DIMM presence + rank (MemoryConfig.ReadChannelsNoLock) ──
     * UMC_D_INTERLEAVE_RDMAP_CHANNEL0 + 0x1C0 bit 19 == 0 -> channel enabled;
     * UMC_D_CHIP_SEL_DDR_CHANNEL + 0x00/0x08 bit 0 -> DIMM0/DIMM1 installed. */
    bool channel_en = (bit_slice(raw->enabled, 19, 19) == 0);
    d->has_dimm[0] = (raw->dimm0_config & 1u) != 0;
    d->has_dimm[1] = (raw->dimm1_config & 1u) != 0;
    d->present = channel_en && (d->has_dimm[0] || d->has_dimm[1]);

    if (is_ddr5) {
        d->ranks[0] = umc_detect_rank_ddr5(raw->rank0_mask);
        d->ranks[1] = umc_detect_rank_ddr5(raw->rank1_mask);
    } else {
        d->ranks[0] = umc_detect_rank_ddr4(raw->cs0_cfg, raw->cs1_cfg, 0);
        d->ranks[1] = umc_detect_rank_ddr4(raw->cs2_cfg, raw->cs3_cfg, 1);
    }
    d->has_ranks = d->present;

    /* Address config (0x50040/0x50048), per GetAddressConfig:
     * numBanks=[21:20], numCol=[19:16]+5, numRow=[11:8]+10, bankGroups=[3:2] */
    if (d->has_dimm[0] && raw->addr_cfg0 != 0) {
        d->has_addr_cfg = true;
        d->num_banks       = bit_slice(raw->addr_cfg0, 21, 20);
        d->num_col         = 5 + bit_slice(raw->addr_cfg0, 19, 16);
        d->num_row         = 10 + bit_slice(raw->addr_cfg0, 11, 8);
        d->num_bank_groups = bit_slice(raw->addr_cfg0, 3, 2);
    }

    if (!d->present)
        return;

    /* ── Ratio / frequency + config flags (0x50200) ── */
    d->ratio = is_ddr5 ? (float)bit_slice(raw->umcratio, 15, 0) / 100.0f
                       : (float)bit_slice(raw->umcratio, 6, 0) / 3.0f;
    if (is_ddr5) {
        d->uclk_gt_fclk    = bit_slice(raw->umcratio, 31, 31);
        uint32_t wck_ratio = bit_slice(raw->umcratio, 27, 26);
        snprintf(d->wck_ck_ratio, sizeof(d->wck_ck_ratio), "%s",
                 wck_ratio == 1 ? "2:1" : wck_ratio == 2 ? "4:1" : "N/A");
        d->bank_group_en = bit_slice(raw->umcratio, 16, 16) == 1;
    } else {
        snprintf(d->wck_ck_ratio, sizeof(d->wck_ck_ratio), "N/A");
    }

    /* ── ECC / burst (0x50100, DDR5 dict) ── */
    if (is_ddr5) {
        d->ecc_enabled = bit_slice(raw->ecc_burst, 12, 12) == 1;
        uint32_t bl = bit_slice(raw->ecc_burst, 9, 8);
        static const char *bl_names[4] = { "BL2", "BL4", "BL8", "BL16" };
        snprintf(d->burst_length, sizeof(d->burst_length), "%s", bl_names[bl & 3]);
        uint32_t bc = bit_slice(raw->ecc_burst, 11, 10);
        snprintf(d->burst_ctrl, sizeof(d->burst_ctrl), "%s",
                 bc == 0 ? "Dynamic BL/BC" : bc == 1 ? "Fixed BL" : "Reserved");
    }

    /* ── Power down (0x5012C) ── */
    d->aggressive_power_down = bit_slice(raw->pwrdownctrl, 30, 30) == 1;
    uint32_t pd_mode = bit_slice(raw->pwrdownctrl, 29, 29);
    snprintf(d->power_down_mode, sizeof(d->power_down_mode), "%s",
             pd_mode ? "Partial" : "Full");
    if (is_ddr5)
        umc_refresh_mode_decode(raw->pwrdownctrl, d->refresh_mode,
                                sizeof(d->refresh_mode), &d->fgr);

    /* ── ODTS (0x50130) ── */
    d->odts_inc_ref_en       = bit_slice(raw->odtsctrl, 28, 28) == 1;
    d->odts_en               = bit_slice(raw->odtsctrl, 27, 27) == 1;
    d->force_pwrdown_throt_en= bit_slice(raw->odtsctrl, 26, 26) == 1;
    d->odts_cmd_throt_en     = bit_slice(raw->odtsctrl, 25, 25) == 1;
    d->i2c_therm_event       = bit_slice(raw->odtsctrl, 24, 24) == 1;
    d->odts_cmd_throt_cyc    = bit_slice(raw->odtsctrl, 19, 10);
    d->roll_window_depth     = bit_slice(raw->odtsctrl, 9, 0);

    /* ── Burst gaps (0x50198) ── */
    d->wrbrst_gap = bit_slice(raw->brstgap, 13, 9);
    d->wr_ph_chk  = bit_slice(raw->brstgap, 8, 8);
    d->rdbrst_gap = bit_slice(raw->brstgap, 7, 3);

    /* ── Primary timings ── */
    d->rp_pb   = is_ddr5 ? bit_slice(raw->reg208, 29, 24) : 0;  /* DDR5: precharge pb */
    d->rc_pb   = is_ddr5 ? bit_slice(raw->reg208, 15, 8) : 0;   /* DDR5: rc pb */


    d->ppd      = is_ddr5 ? bit_slice(raw->reg21C, 2, 0) : 0;

    d->rdrd_ban = is_ddr5 ? bit_slice(raw->reg220, 31, 30) : 0;

    d->wrwr_ban = is_ddr5 ? bit_slice(raw->reg224, 31, 30) : 0;

    d->mw   = is_ddr5 ? bit_slice(raw->reg228, 28, 24) : 0;     /* MW (DDR5) */

    /* 0x5022C: ZQ calibration (DDR5 dict) */
    d->short_init     = bit_slice(raw->reg22C, 31, 31);
    d->zqcs_interval  = bit_slice(raw->reg22C, 29, 20);
    d->tzq_oper_cal   = bit_slice(raw->reg22C, 19, 8);
    d->tzqcs          = bit_slice(raw->reg22C, 7, 0);

    /* 0x50230: ODtsReadInterval (DDR5) + REFI */
    if (is_ddr5)
        d->odts_read_interval = bit_slice(raw->reg230, 29, 20);


    /* 0x50238 / 0x5023C (DDR5) */
    d->dllk           = bit_slice(raw->reg238, 27, 16);
    d->xs             = bit_slice(raw->reg238, 11, 0);
    d->rank_busy_dly  = bit_slice(raw->reg23C, 31, 24);
    d->cmd_par_latency= bit_slice(raw->reg23C, 19, 16);
    d->alert_par_dly  = bit_slice(raw->reg23C, 14, 8);
    d->alert_crc_dly  = bit_slice(raw->reg23C, 7, 0);

    /* 0x50240 */
    d->mrri            = bit_slice(raw->reg240, 26, 24);
    d->mrr             = bit_slice(raw->reg240, 21, 16);
    d->mrw             = bit_slice(raw->reg240, 13, 8);
    d->ctrl_switch_clks= bit_slice(raw->reg240, 7, 0);

    /* 0x50244 */
    d->aggr_pwr_down_dly = bit_slice(raw->reg244, 31, 26);
    d->csh        = bit_slice(raw->reg244, 25, 20);
    d->pwr_down_dly = bit_slice(raw->reg244, 19, 8);
    d->pd         = bit_slice(raw->reg244, 4, 0);

    /* 0x50248 / 0x5024C */
    d->srx2srx        = bit_slice(raw->reg248, 15, 10);
    d->tstab          = bit_slice(raw->reg248, 9, 0);  /* DDR5 dict 0x50248; author's stab is 0x50250 */
    d->alert_crc_pulse= bit_slice(raw->reg24C, 10, 8);
    d->alert_par_pulse= bit_slice(raw->reg24C, 6, 0);


    /* 0x50254 */
    d->cpded  = bit_slice(raw->reg254, 20, 16);
    d->cacsh  = bit_slice(raw->reg254, 13, 8);

    /* 0x50258 / 0x5025C */
    d->parinl    = bit_slice(raw->reg258, 29, 28);
    d->rddataen  = bit_slice(raw->reg258, 6, 0);
    d->lp_exit_dly = bit_slice(raw->reg25C, 13, 8);
    d->lp_dly      = bit_slice(raw->reg25C, 5, 0);

    /* ── Nitro (0x50284, DDR5 only, Ddr5Timings.NitroSettings) ── */
    if (is_ddr5) {
        uint32_t v = raw->reg284;
        uint32_t ctrl_line = v & 0x3;
        uint32_t tx_data   = (v >> 4) & 0x3;
        uint32_t rx_data   = (v >> 8) & 0x3;
        uint32_t ctrl_dm   = (v >> 16) & 0x1;
        uint32_t tx_dm     = (v >> 17) & 0x1;
        uint32_t rx_dm     = (v >> 18) & 0x1;
        snprintf(d->nitro_rx_tx_ctrl, sizeof(d->nitro_rx_tx_ctrl),
                 "%u/%u/%u", rx_data, tx_data, ctrl_line);
        snprintf(d->nitro_dly_mode, sizeof(d->nitro_dly_mode),
                 "rx:%u tx:%u ctrl:%u", rx_dm, tx_dm, ctrl_dm);
    }

    /* ── 0x50288 / 0x5028C ── */
    d->phyupd_cmd_dly   = bit_slice(raw->reg288, 11, 8);
    d->phyupd_wrdat_dly = bit_slice(raw->reg288, 7, 4);
    d->phyupd_resp      = bit_slice(raw->reg288, 3, 0);
    d->wrmpr       = bit_slice(raw->reg28C, 29, 24);
    d->cmd_stg_cnt = bit_slice(raw->reg28C, 21, 11);
    d->rcvr_wait   = bit_slice(raw->reg28C, 10, 0);

    /* ── RD/WR pre/post (0x502A4) ── */
    d->wrpost = bit_slice(raw->reg2A4, 14, 12);
    d->rdpost = bit_slice(raw->reg2A4, 6, 4);

    /* ── ECSc (0x502A8), CCDL (0x502E0) ── */
    d->ecsc = bit_slice(raw->reg2A8, 8, 0);
    if (is_ddr5) {
        d->ccdl_wr2 = bit_slice(raw->reg2E0, 5, 0);
        d->ccdl_wr2_valid = raw->reg2E0 != 0xFFFFFFFFu && d->ccdl_wr2 != 0;
    }

    /* ── Bank Group Swap (ReadBankGroupSwap) ── */
    d->bgs = !(raw->bgs0 == 0x87654321u && raw->bgs1 == 0x87654321u);
    d->bgs_alt = (bit_slice(raw->bgs_alt0, 10, 4) > 0 || bit_slice(raw->bgs_alt1, 10, 4) > 0);

    /* DDR max rate (0x50DF0 [7:0], 0x50DF4 [7:0]) */
    d->ddr_max_rate     = bit_slice(raw->enabled, 7, 0);
    d->ddr_max_rate_enf = bit_slice(raw->regDF4, 7, 0);

    d->has_config = true;

}

/* ── AOD electrical block (Granite Ridge / 1Ah) ──────────────────────── */

int aod_find_last_u16(const uint8_t *buf, int len, uint16_t value)
{
    if (!buf || len < 2) return -1;
    uint8_t lo = (uint8_t)(value & 0xFF);
    uint8_t hi = (uint8_t)((value >> 8) & 0xFF);
    int last = -1;
    for (int i = 0; i + 1 < len; i++) {
        if (buf[i] == lo && buf[i + 1] == hi)
            last = i;
    }
    return last;
}

static uint32_t aod_u32(const uint8_t *buf, int len, int off, bool *ok)
{
    if (ok) {
        if (off < 0 || off + 4 > len) { *ok = false; return 0; }
        *ok = true;
    } else if (off < 0 || off + 4 > len) {
        return 0;
    }
    return (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
           ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
}

#define AOD_GET(field_raw, has_field, off)                                   \
    do {                                                                      \
        bool ok_;                                                              \
        info->field_raw = aod_u32(table, len, (off), &ok_);                   \
        info->has_field = ok_;                                                 \
    } while (0)

void aod_mem_parse_granite_ridge(const uint8_t *table, int len, uint32_t mclk_hint,
                                 bool patch_gt_b404022, bool has_rmp,
                                 aod_mem_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->mclk = mclk_hint;

    /*
     * Aod.GetBaseDictionaryByFrequency(): the active per-frequency block starts
     * at the LAST occurrence of MemClk (u16 LE, MCLK in MHz) in the raw table.
     * fieldIndex = base of that block. Without a frequency match we cannot use
     * the relative layout (AodDataDictionary_1Ah_B404023 has different absolute
     * offsets; the base-relative fields are not guaranteed there).
     */
    if (mclk_hint == 0 || mclk_hint > 0xFFFF) {
        snprintf(info->layout, sizeof(info->layout),
                 "no MCLK hint — electrical N/A");
        return;
    }
    int base = aod_find_last_u16(table, len, (uint16_t)mclk_hint);
    if (base < 0) {
        snprintf(info->layout, sizeof(info->layout),
                 "MCLK %u block not found", mclk_hint);
        return;
    }
    int last = base + 104; /* CadBusDrvStren sits at base+104 */
    info->base = base;

    if (patch_gt_b404022) {
        /* AodDataDictionary: new-AGESA layout */
        int o_proc_odt      = has_rmp ? last + 144 : last + 132;
        int o_proc_odt_pd   = has_rmp ? last + 148 : last + 136;
        int o_dram_ds       = has_rmp ? last + 152 : last + 140;
        int o_dram_ds_pd    = has_rmp ? last + 156 : last + 140;
        int o_proc_cs_ds    = has_rmp ? last + 160 : last + 144;
        int o_proc_ck_ds    = has_rmp ? last + 164 : last + 148;
        int o_proc_data_ds  = has_rmp ? last + 168 : last + 152;
        int o_proc_dq_ds_pd = has_rmp ? last + 172 : last + 156;

        AOD_GET(rtt_nom_wr_raw, has_rtt_nom_wr, last + 4);
        AOD_GET(rtt_nom_rd_raw, has_rtt_nom_rd, last + 8);
        AOD_GET(rtt_wr_raw,     has_rtt_wr,     last + 12);
        AOD_GET(rtt_park_raw,   has_rtt_park,   last + 16);
        AOD_GET(rtt_park_dqs_raw, has_rtt_park_dqs, last + 20);

        AOD_GET(proc_odt_raw,     has_proc_odt,     o_proc_odt);
        AOD_GET(proc_odt_pu_raw,  has_proc_odt_pu,  o_proc_odt);   /* PullUp == ProcOdt */
        AOD_GET(proc_odt_pd_raw,  has_proc_odt_pd,  o_proc_odt_pd);
        AOD_GET(dram_data_ds_raw, has_dram_data_ds, o_dram_ds);
        AOD_GET(dram_dq_ds_pu_raw, has_dram_dq_ds_pu, o_dram_ds);
        AOD_GET(dram_dq_ds_pd_raw, has_dram_dq_ds_pd, o_dram_ds_pd);
        AOD_GET(proc_cs_ds_raw,   has_proc_cs_ds,   o_proc_cs_ds);
        AOD_GET(proc_ck_ds_raw,   has_proc_ck_ds,   o_proc_ck_ds);
        AOD_GET(proc_data_ds_raw, has_proc_data_ds, o_proc_data_ds);
        AOD_GET(proc_dq_ds_pu_raw, has_proc_dq_ds_pu, o_proc_data_ds);
        AOD_GET(proc_dq_ds_pd_raw, has_proc_dq_ds_pd, o_proc_dq_ds_pd);

        snprintf(info->layout, sizeof(info->layout),
                 "1Ah base=%d new%s layout", base, has_rmp ? "+RMP" : "-RMP");
    } else {
        /* Old-AGESA (< 0xB404023) 1Ah layout */
        int o_proc_odt    = has_rmp ? last + 136 : last + 148;
        int o_proc_odt_pd = has_rmp ? last + 140 : last + 152;
        int o_dram_ds     = has_rmp ? last + 144 : last + 156;

        AOD_GET(proc_data_ds_raw, has_proc_data_ds, last + 4);
        AOD_GET(rtt_nom_wr_raw,   has_rtt_nom_wr,   last + 8);
        AOD_GET(rtt_nom_rd_raw,   has_rtt_nom_rd,   last + 12);
        AOD_GET(rtt_wr_raw,       has_rtt_wr,       last + 16);
        AOD_GET(rtt_park_raw,     has_rtt_park,     last + 20);
        AOD_GET(rtt_park_dqs_raw, has_rtt_park_dqs, last + 24);

        AOD_GET(proc_odt_raw,    has_proc_odt,     o_proc_odt);
        AOD_GET(proc_odt_pu_raw, has_proc_odt_pu,  o_proc_odt);
        AOD_GET(proc_odt_pd_raw, has_proc_odt_pd,  o_proc_odt_pd);
        AOD_GET(dram_data_ds_raw, has_dram_data_ds, o_dram_ds);
        /* old layout has no split pull-up/pull-down for DQ/CS/CK */
        AOD_GET(proc_dq_ds_pu_raw, has_proc_dq_ds_pu, last + 4);
        AOD_GET(proc_dq_ds_pd_raw, has_proc_dq_ds_pd, last + 4);
        AOD_GET(dram_dq_ds_pu_raw, has_dram_dq_ds_pu, o_dram_ds);
        AOD_GET(dram_dq_ds_pd_raw, has_dram_dq_ds_pd, o_dram_ds);

        snprintf(info->layout, sizeof(info->layout),
                 "1Ah base=%d old%s layout", base, has_rmp ? "+RMP" : "-RMP");
    }

    /* CADBUS drive strength is part of the base dictionary (base + 104) */
    AOD_GET(cad_bus_ds_raw, has_cad_bus_ds, base + 104);

    /* sanity: ProcOdt should be a known code, RTT codes 0..7 */
    info->available = info->has_proc_odt || info->has_rtt_nom_wr;
}

bool aod_mem_info_sanity(const aod_mem_info_t *info)
{
    if (!info->available) return false;

    static const uint8_t bad_odt[] = { 8,9,10,11,16,17,18,19,20,21,22,23,24,25,26,27,
                                       32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
                                       48,49,50,51,52,53,54,55,56,57,58,59 };
    if (info->has_proc_odt) {
        uint32_t c = info->proc_odt_raw;
        if (c > 63) return false;
        for (size_t i = 0; i < sizeof(bad_odt); i++)
            if (c == bad_odt[i]) return false;
    }
    if (info->has_rtt_nom_wr  && info->rtt_nom_wr_raw  > 7) return false;
    if (info->has_rtt_nom_rd  && info->rtt_nom_rd_raw  > 7) return false;
    if (info->has_rtt_wr      && info->rtt_wr_raw      > 7) return false;
    if (info->has_rtt_park    && info->rtt_park_raw    > 7) return false;
    if (info->has_rtt_park_dqs&& info->rtt_park_dqs_raw> 7) return false;
    if (info->has_cad_bus_ds) {
        uint32_t c = info->cad_bus_ds_raw;
        if (c != 0 && c != 30 && c != 40 && c != 60 && c != 120) return false;
    }
    if (info->has_dram_data_ds && info->dram_data_ds_raw > 2) return false;
    return true;
}
