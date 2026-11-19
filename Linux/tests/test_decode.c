/*
 * test_decode.c — unit tests for the pure register decoder (dram_decode.c).
 * Synthetic register values, expected ZenStates-Core semantics. Build & run:
 *   make test
 */

#include "../src/dram_decode.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

static void expect_u32(uint32_t got, uint32_t want, const char *what)
{
    EXPECT(got == want, "%s: got %u want %u", what, got, want);
}

static void test_ddr5_primary(void)
{
    umc_raw_regs_t raw;
    dram_channel_t d;
    memset(&raw, 0, sizeof(raw));
    memset(&d, 0, sizeof(d));

    /* channel enabled (bit19=0) + both DIMMs present */
    raw.enabled = 0x00000000u;
    raw.dimm0_config = 1;
    raw.dimm1_config = 0;

    /* DDR5-6000: ratio 30.00 -> bits[15:0]=3000; Cmd2T=1T, GDM=0, BGSel=1 */
    raw.umcratio = 3000u | (0u << 16) | (0u << 17) | (0u << 18) | (1u << 31);

    /* 0x50204: CL=30 [5:0], tRAS=36 [14:8], tRCDRD=36 [21:16], tRCDWR=48 [29:24] */
    raw.reg204 = 30u | (36u << 8) | (36u << 16) | (48u << 24);
    /* 0x50208: tRC=72 [7:0], tRCpb=8 [15:8], tRP=36 [21:16], tRPpb=8 [29:24] */
    raw.reg208 = 72u | (8u << 8) | (36u << 16) | (8u << 24);
    /* 0x5020C: tRRDS=4, tRRDL=8, tRTP=8 */
    raw.reg20C = 4u | (8u << 8) | (8u << 24);
    raw.reg210 = 24;                       /* tFAW */
    raw.reg214 = 14u | (2u << 8) | (16u << 16); /* CWL, tWTRS, tWTRL */
    raw.reg218 = 60;                       /* tWR */
    raw.reg21C = 5u | (1200u << 20);       /* PPD + tRCPAGE */
    raw.reg220 = 4u | (8u << 8) | (0u << 16) | (10u << 24); /* DD SD SC SCL */
    raw.reg224 = 4u | (8u << 8) | (1u << 16) | (10u << 24);
    raw.reg228 = 4u | (28u << 8) | (1u << 24); /* WRRD RDWR MW */
    raw.reg230 = 57150u | (0u << 20);      /* tREFI + OdtsReadInterval=0 */
    raw.reg234 = 4u | (16u << 8) | (4u << 16) | (8u << 24); /* MRD MOD MRDPDA MODPDA */
    raw.reg250 = 128u | (8u << 16);        /* STAGsb STAG */
    raw.reg254 = 5u | (8u << 8) | (4u << 16) | (3u << 24);  /* XP CACSH CPDED CKE */
    raw.reg258 = 32u | (8u << 8) | (32u << 16) | (1u << 24);/* PHYWRL PHYRDL PHYWRD */
    raw.reg2A4 = 1u | (2u << 4) | (3u << 8) | (1u << 12);   /* RDPRE RDPOST WRPRE WRPOST */
    /* tRFC: 0x260 default, 0x264 has tRFC=495, tRFC2=138 */
    raw.trfc[0] = 0x00C00138u;
    raw.trfc[1] = (495u) | (138u << 16);
    /* tRFCsb: first non-zero in 0x2C0.. */
    raw.rfcsb[0] = 123u;
    /* Nitro 0x284: ctrl=1 tx=2 rx=3, modes rx=1 tx=1 ctrl=0 */
    raw.reg284 = 1u | (2u << 4) | (3u << 8) | (0u << 16) | (1u << 17) | (1u << 18);
    /* refresh: FGR=2 ([18:16]), per-bank bit1=0 -> FGR mode */
    raw.pwrdownctrl = (2u << 16) | (1u << 28) | (0u << 29) | (0u << 30);
    /* ECC on, BL16 (3), burst ctrl dynamic (0) */
    raw.ecc_burst = (1u << 12) | (3u << 8) | (0u << 10);
    /* BGS: non-sentinel -> enabled */
    raw.bgs0 = 0x12345678u;
    raw.bgs1 = 0x87654321u;
    raw.bgs_alt0 = 0x000000F0u; /* bits[7:4]=0xF > 0 */
    raw.bgs_alt1 = 0;
    /* rank mask: DR per 0x07FFFBFE pattern */
    raw.rank0_mask[0] = 0x07FFFBFEu;
    raw.rank0_mask[1] = 0x07FFFBFFu;
    /* address config: 32 banks(2h<<20...), per GetAddressConfig */
    raw.addr_cfg0 = 0x0025060cu; /* numBanks=2(->32), col=5, row=6, BG=3h */

    umc_regs_decode(&raw, true, &d);

    EXPECT(d.present, "channel should be present");
    expect_u32(d.rp_pb, 8, "tRPpb");
    expect_u32(d.rc_pb, 8, "tRCpb");
    expect_u32(d.mw, 1, "MW");
    expect_u32(d.cacsh, 8, "tCACSH");
    expect_u32(d.cpded, 4, "tCPDED");
    expect_u32(d.ppd, 5, "tPPD");
    /* RDPRE raw=1 <2 -> 2; WRPRE raw=3 -> 4 */
    expect_u32(d.rdpost, 2, "tRDPOST");
    expect_u32(d.wrpost, 1, "tWRPOST");

    EXPECT(d.uclk_gt_fclk == 1, "UclkGtFclk");
    EXPECT(strcmp(d.wck_ck_ratio, "N/A") == 0, "WCK:CK N/A on DDR5");
    EXPECT(strcmp(d.refresh_mode, "FGR") == 0, "RefreshMode FGR: got %s", d.refresh_mode);
    expect_u32(d.fgr, 2, "FGR");
    EXPECT(strcmp(d.power_down_mode, "Full") == 0, "PowerDownMode Full");
    EXPECT(d.ecc_enabled, "ECC enabled");
    EXPECT(strcmp(d.burst_length, "BL16") == 0, "BL16");
    EXPECT(strcmp(d.burst_ctrl, "Dynamic BL/BC") == 0, "BurstCtrl dynamic");
    EXPECT(d.bgs, "BGS enabled (non-sentinel)");
    EXPECT(d.bgs_alt, "BGSAlt (bgs_alt0[7:4]>0)");
    EXPECT(strcmp(d.nitro_rx_tx_ctrl, "3/2/1") == 0, "Nitro rx/tx/ctrl: got %s",
           d.nitro_rx_tx_ctrl);
    EXPECT(strcmp(d.nitro_dly_mode, "rx:1 tx:1 ctrl:0") == 0, "Nitro modes: got %s",
           d.nitro_dly_mode);

    /* ranks: mask0 matches DR pattern, mask1 bits[9:2]=0xFF...>3 -> 1 rank counted?
     * 0x07FFFBFF: bits[9:2] = (0xFF>>2)&0xFF = 0x3F = 63 -> not <3, not sentinel
     * So only rank0 counted -> 1. */
    expect_u32(d.ranks[0], 1, "rank count DIMM0");

    /* frequency: ratio 30 * 100MHz BCLK * 2 = 6000 MT/s */
    /* tREFI ns = 57150 * 2000 / 6000 = 19050 */
    /* FGR mode -> displayed tRFC(ns) is tRFC2: 138*2000/6000 = 46 */

    /* address config */
    EXPECT(d.has_addr_cfg, "addr cfg present");
    expect_u32(d.num_bank_groups, 3, "num bank groups field");
    expect_u32(d.num_col, 5 + 5, "num col");
    expect_u32(d.num_row, 10 + 6, "num row");
}

static void test_absent_channel(void)
{
    umc_raw_regs_t raw;
    dram_channel_t d;
    memset(&raw, 0, sizeof(raw));
    memset(&d, 0, sizeof(d));

    /* channel disabled: bit 19 set */
    raw.enabled = 1u << 19;
    raw.dimm0_config = 0;
    raw.dimm1_config = 0;

    umc_regs_decode(&raw, true, &d);
    EXPECT(!d.present, "channel must be absent");
    EXPECT(!d.has_config, "no config decoded for absent channel");
}

static void test_bgs_sentinel(void)
{
    umc_raw_regs_t raw;
    dram_channel_t d;
    memset(&raw, 0, sizeof(raw));
    memset(&d, 0, sizeof(d));
    raw.enabled = 0;
    raw.dimm0_config = 1;
    raw.bgs0 = 0x87654321u;
    raw.bgs1 = 0x87654321u;
    umc_regs_decode(&raw, true, &d);
    EXPECT(!d.bgs, "BGS off with both sentinels");
    EXPECT(!d.bgs_alt, "BGSAlt off");
}

static void test_ddr4_path(void)
{
    umc_raw_regs_t raw;
    dram_channel_t d;
    memset(&raw, 0, sizeof(raw));
    memset(&d, 0, sizeof(d));
    raw.enabled = 0;
    raw.cs0_cfg = 1;  /* DIMM0 CS0 enabled */
    raw.cs1_cfg = 1;  /* DIMM0 CS1 enabled -> DR */
    raw.dimm0_config = raw.cs0_cfg;
    raw.umcratio = (9u << 10) | (1u << 11) | (12u); /* 2T, GDM, ratio bits[7:0]=12 */
    /* DDR4 tRFC: 0x260 != 0x21060138 -> [10:0] / [21:11] */
    raw.trfc[0] = 0x00A30157u; /* RFC=0x157=343? bits10:0=0x157, bits21:11=0x14 */
    umc_regs_decode(&raw, false, &d);
    EXPECT(d.present, "DDR4 channel present via CS0");
    expect_u32(d.ranks[0], 2, "DDR4 DR via two chip selects");
}

static void test_electrical_decoders(void)
{
    char buf[48];
    EXPECT(strcmp(proc_odt_decode(12), "60.0 Ω") == 0, "ProcODT 12");
    EXPECT(strcmp(proc_odt_decode(0), "Hi-Z") == 0, "ProcODT 0");
    EXPECT(strcmp(proc_odt_decode(45), "N/A") == 0, "ProcODT invalid -> N/A");
    EXPECT(strcmp(dram_data_ds_decode(1), "40.0 Ω") == 0, "DramDS 1");
    EXPECT(strcmp(cad_bus_ds_decode(40), "40.0 Ω") == 0, "CADBUS 40");
    EXPECT(strcmp(proc_impedance_decode(0), "Off") == 0, "impedance 0");
    rtt_decode(0, buf, sizeof(buf));
    EXPECT(strcmp(buf, "Off") == 0, "RTT 0");
    rtt_decode(4, buf, sizeof(buf));
    EXPECT(strcmp(buf, "RZQ/4 (60 Ω)") == 0, "RTT 4: got %s", buf);
    rtt_decode(3, buf, sizeof(buf));
    EXPECT(strcmp(buf, "RZQ/3 (80 Ω)") == 0, "RTT 3: got %s", buf);
    rtt_decode(9, buf, sizeof(buf));
    EXPECT(strcmp(buf, "N/A") == 0, "RTT invalid");
}

static void test_aod_parse(void)
{
    /* Synthesize a Granite Ridge AOD table: zero 0x24BB bytes, plant the
     * MemClk u16 (3000 = 0x0BB8) at two places (last must win), then the
     * new-layout fields at last+4.. per the ported dictionary. */
    int len = 0x24BB;
    uint8_t *t = calloc(1, len);
    aod_mem_info_t info;

    int wrong = 4000;
    t[wrong] = 0xB8; t[wrong + 1] = 0x0B;
    int base = 8900;
    t[base] = 0xB8; t[base + 1] = 0x0B;
    int last = base + 104;
    /* cadbus at base+104: 40 ohm code 40 */
    uint32_t v;
    v = 40; memcpy(t + base + 104, &v, 4);
    v = 4;  memcpy(t + last + 4, &v, 4);   /* RttNomWr = RZQ/4 */
    v = 3;  memcpy(t + last + 8, &v, 4);   /* RttNomRd */
    v = 0;  memcpy(t + last + 12, &v, 4);  /* RttWr */
    v = 5;  memcpy(t + last + 16, &v, 4);  /* RttPark */
    v = 6;  memcpy(t + last + 20, &v, 4);  /* RttParkDqs */
    v = 14; memcpy(t + last + 132, &v, 4); /* ProcOdt (no-RMP layout!) */
    v = 30; memcpy(t + last + 136, &v, 4); /* ProcOdtPullDown */
    v = 1;  memcpy(t + last + 140, &v, 4); /* DramDataDrvStren */
    v = 30; memcpy(t + last + 144, &v, 4); /* ProcCsDs */
    v = 30; memcpy(t + last + 148, &v, 4); /* ProcCkDs */
    v = 30; memcpy(t + last + 152, &v, 4); /* ProcDataDrvStren */
    v = 28; memcpy(t + last + 156, &v, 4); /* ProcDqDsPullDown */

    /* has_rmp=true must NOT match these offsets (planted at no-RMP positions),
     * sanity should prefer the no-RMP candidate when both decode. */
    aod_mem_parse_granite_ridge(t, len, 3000, true /*patch*/, false /*no RMP*/, &info);
    EXPECT(info.available, "AOD parse available");
    EXPECT(info.base == base, "found LAST MCLK occurrence: base=%d", info.base);
    EXPECT(info.has_rtt_nom_wr && info.rtt_nom_wr_raw == 4, "RttNomWr=4");
    EXPECT(info.has_rtt_nom_rd && info.rtt_nom_rd_raw == 3, "RttNomRd=3");
    EXPECT(info.has_rtt_wr && info.rtt_wr_raw == 0, "RttWr=0");
    EXPECT(info.has_proc_odt && info.proc_odt_raw == 14, "ProcOdt=14");
    EXPECT(info.has_proc_odt_pd && info.proc_odt_pd_raw == 30, "ProcOdtPD=30");
    EXPECT(info.has_dram_data_ds && info.dram_data_ds_raw == 1, "DramDS=1");
    EXPECT(info.has_cad_bus_ds && info.cad_bus_ds_raw == 40, "CADBUS=40");
    EXPECT(aod_mem_info_sanity(&info), "sanity passes for planted table");

    /* Wrong MCLK hint must fail cleanly, not crash */
    aod_mem_info_t bad;
    aod_mem_parse_granite_ridge(t, len, 9999, true, false, &bad);
    EXPECT(!bad.available, "wrong MCLK -> unavailable");

    /* Zero MCLK hint -> unavailable with layout note */
    aod_mem_parse_granite_ridge(t, len, 0, true, false, &bad);
    EXPECT(!bad.available && strstr(bad.layout, "no MCLK") != NULL,
           "zero MCLK -> 'no MCLK' layout: %s", bad.layout);

    free(t);
}

int main(void)
{
    test_ddr5_primary();
    test_absent_channel();
    test_bgs_sentinel();
    test_ddr4_path();
    test_electrical_decoders();
    test_aod_parse();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
