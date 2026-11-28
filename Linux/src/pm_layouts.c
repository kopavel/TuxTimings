/*
 * pm_layouts.c — consolidated AMD SMU PM Table layout registry (pure data).
 *
 * Provenance and conflicts: docs/SMU_PM_TABLE_MAP.md. Every row cites its
 * source; byte_offset == float_index * 4 is assumed everywhere (we store
 * float indices only). Never extend an unlisted version by copying a
 * neighbour: unlisted == unsupported.
 *
 * Source keys (full citations in docs/SMU_PM_TABLE_MAP.md):
 *   ZS  ZenStates-Core@869b7e8257 Hardware/PowerTable.cs (GPL-3.0)
 *   ZSp ZenStates-Core@869b7e8257 PawnIo/RyzenSmu.cs
 *   GN  gnr-smu@def79d66d5 gnr_smu/profiles.py + docs (MIT)
 *   ZM  HorizonUnix/ZenMaster wiki "PM-Table-and-Monitoring"
 *   LH  LibreHardwareMonitor@9e2d677cef Hardware/RyzenSMU.cs (MPL-2.0)
 *   LHa LibreHardwareMonitor@9e2d677cef Hardware/Cpu/Amd17Cpu.cs
 *   TX  TuxTimings author map (Linux/docs/PM_TABLE_INDICES.md,
 *       live 9950X validation noted there)
 *   LV  Live DEBUG dumps on this repository's hardware (9950X, PM 0x620205)
 */

#include "pm_layouts.h"

#define SRC_ZS  "ZenStates-Core@869b7e8257 Hardware/PowerTable.cs"
#define SRC_GN  "gnr-smu@def79d66d5 gnr_smu/profiles.py"
#define SRC_GND "gnr-smu@def79d66d5 docs"
#define SRC_ZM  "ZenMaster wiki PM-Table-and-Monitoring"
#define SRC_LH  "LibreHardwareMonitor@9e2d677cef RyzenSMU.cs"
#define SRC_LHa "LibreHardwareMonitor@9e2d677cef Amd17Cpu.cs"
#define SRC_TX  "TuxTimings author (docs/PM_TABLE_INDICES.md)"
#define SRC_LV  "live 9950X DEBUG dump (this repo)"

/* scalar field */
#define F0(m, i, u, sc, cf, src)     { (m), (i), 1, 1, (sc), (u), (cf), (src), NULL }
#define F0C(m, i, u, sc, cf, src, c) { (m), (i), 1, 1, (sc), (u), (cf), (src), (c) }
/* per-core / multi-lane array */
#define FA(m, i, n, u, sc, cf, src)     { (m), (i), (n), 1, (sc), (u), (cf), (src), NULL }
#define FAC(m, i, n, u, sc, cf, src, c) { (m), (i), (n), 1, (sc), (u), (cf), (src), (c) }

/* ─────────────────────────────────────────────────────────────────────
 * Zen 5 — Granite Ridge / Fire Range
 * ───────────────────────────────────────────────────────────────────── */

/* Dual-CCD desktop, 0x620205 & 0x621202 share the zone-0/per-core geometry
 * (ZS lists both with identical offsets; LHa publishes 621202's per-core
 * bases identical to our live-validated 620205 ones). */
static const pm_field_t gnr_dual_fields[] = {
    /* zone 0: (LIMIT, VALUE) pair layout per GN; validated live on 9950X */
    F0(PM_M_PPT_LIMIT,    2,  PM_U_W,   1.0f, PM_CONFIRMED, SRC_GN "; " SRC_LV),
    F0(PM_M_PPT_CURRENT,  3,  PM_U_W,   1.0f, PM_CONFIRMED, SRC_GN "; " SRC_LV),
    F0(PM_M_TDC_LIMIT,    8,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN "; " SRC_LV),
    F0(PM_M_TDC_CURRENT,  9,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN "; " SRC_LV),
    F0(PM_M_THM_LIMIT,   10,  PM_U_C,   1.0f, PM_CONFIRMED,
       SRC_GN "; " SRC_LV),
    F0C(PM_M_TCTL,       11,  PM_U_C,   1.0f, PM_CONFIRMED, SRC_GN,
        "author map called 11 'IOD hotspot'; GN matched it to k10temp Tctl"),
    F0(PM_M_EDC_LIMIT,   63,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN),
    F0C(PM_M_EDC_CURRENT, 64, PM_U_A, 1.0f, PM_CONFIRMED, SRC_GND "; " SRC_LV,
        "9950X 0x620205: 16-22 A video, 35-45 A single-core, 180 A all-core; behavioural validation"),
    F0(PM_M_FIT_METRIC,  16,  PM_U_NONE, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_VID_LIMIT,   18,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_VID_AGG,     19,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_CPU_POWER,   20,  PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_SOC_POWER,   21,  PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_VDDIO_POWER, 22,  PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_VDD18_POWER, 23,  PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0C(PM_M_SOCKET_POWER, 26, PM_U_W,  1.0f, PM_SOURCE_DOCUMENTED, SRC_GN,
        "author map used index 220 for socket power (321 W at idle — rejected)"),
    F0(PM_M_FCLK,        71,  PM_U_MHZ, 1.0f, PM_CONFIRMED, SRC_ZS " (0x11C); " SRC_LV),
    F0(PM_M_UCLK,        75,  PM_U_MHZ, 1.0f, PM_CONFIRMED, SRC_ZS " (0x12C); " SRC_LV),
    F0(PM_M_MCLK,        79,  PM_U_MHZ, 1.0f, PM_CONFIRMED, SRC_ZS " (0x13C); " SRC_LV),
    /* SVI3 telemetry block @0xC0, stride 5: VID, V, I, I2, VRM temp */
    F0(PM_M_SVI3_VDD_VID,   48, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_VOLT,  49, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR,  50, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_TEMP,  52, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,   53, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VOLT,  54, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR,  55, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_TEMP,  57, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VID,  58, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VOLT, 59, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0C(PM_M_VDD_MISC,   58,  PM_U_V,   1.0f, PM_CONFIRMED, SRC_TX "; " SRC_GN,
        "ZS SVI3 MISC_VOLT sits at 59 — 58 is the rail VID, both mapped"),
    F0(PM_M_VSOC,        83,  PM_U_V,   1.0f, PM_CONFIRMED, SRC_TX "; " SRC_GN),
    F0(PM_M_VDDG_IOD,   259,  PM_U_V,   1.0f, PM_CONFIRMED,
       SRC_ZS " (0x40C); " SRC_GN),
    F0(PM_M_VDDG_CCD,   261,  PM_U_V,   1.0f, PM_CONFIRMED, SRC_ZS " (0x414); " SRC_GN),
    F0(PM_M_VDDP,       269,  PM_U_V,   1.0f, PM_CONFIRMED, SRC_ZS " (0x434); " SRC_GN),
    F0C(PM_M_VCORE,     271,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX,
        "no ZS/GN field at 271; kept on author's live coherence"),
    /* per-core: 16 lanes, 4 B stride. GN+LHa+ZM+live agree. */
    FA(PM_M_CORE_POWER,   301, 16, PM_U_W,   1.0f,    PM_CONFIRMED,
       SRC_GN "; " SRC_LHa " (0x621202); " SRC_ZM " (0x4B4)"),
    FA(PM_M_CORE_VOLTAGE, 317, 16, PM_U_V,   1.0f,    PM_CONFIRMED,
       SRC_TX "; " SRC_GN "; " SRC_LHa "; " SRC_ZM " (0x4F4)"),
    FA(PM_M_CORE_TEMP,    333, 16, PM_U_C,   1.0f,    PM_CONFIRMED,
       SRC_TX "; " SRC_GN "; " SRC_LHa "; " SRC_ZM " (0x534)"),
    FAC(PM_M_CORE_CLK,    349, 16, PM_U_GHZ, 1.0f,    PM_SOURCE_DOCUMENTED,
       SRC_LHa " (Frequency=349); " SRC_ZM " (0x574); " SRC_LV,
       "9950X all-core load agrees with cpufreq within ~50 MHz; idle semantics unresolved; GN names core_fit"),
    FAC(PM_M_CORE_EFF_CLK,365, 16, PM_U_GHZ, 1.0f,    PM_CANDIDATE,
       SRC_ZM " (0x5B4)", "GN names 365 'activity_metric'"),
    FA(PM_M_CORE_C0,      381, 16, PM_U_PCT, 1.0f,    PM_CONFIRMED,
       SRC_GN "; " SRC_ZM " (0x5F4); C0+CC1+CC6 ~= 100% live check"),
    FA(PM_M_CORE_CC1,     397, 16, PM_U_PCT, 1.0f,    PM_CONFIRMED,
       SRC_GN "; " SRC_ZM " (0x634)"),
    FA(PM_M_CORE_CC6,     413, 16, PM_U_PCT, 1.0f,    PM_CONFIRMED,
       SRC_GN "; " SRC_ZM " (0x674)"),
    FAC(PM_M_CORE_BOOST_LIM, 445, 16, PM_U_GHZ, 1.0f, PM_CANDIDATE, SRC_GN,
        "GN: boost_limit_confident=False"),
    FAC(PM_M_CCD_L3_TEMP, 595, 2, PM_U_C, 1.0f, PM_CANDIDATE, SRC_GND,
        "GN L3-traffic coupling evidence, single-run per CCD pair"),
    FAC(PM_M_CCD_SHARED_TEMP, 611, 2, PM_U_C, 1.0f, PM_CANDIDATE, SRC_GND,
        "GN: NOT CCD-selective — shared package/fabric heat"),
};

/* Single-CCD desktop/X3D, 0x620105 (9800X3D-validated by GN, ZS offsets) */
static const pm_field_t gnr_single_fields[] = {
    F0(PM_M_PPT_LIMIT,    2,  PM_U_W,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_PPT_CURRENT,  3,  PM_U_W,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_TDC_LIMIT,    8,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_TDC_CURRENT,  9,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_THM_LIMIT,   10,  PM_U_C,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_TCTL,        11,  PM_U_C,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_EDC_LIMIT,   63,  PM_U_A,   1.0f, PM_CONFIRMED, SRC_GN),
    F0(PM_M_FCLK,        71,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x11C)"),
    F0(PM_M_UCLK,        75,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x12C)"),
    F0(PM_M_MCLK,        79,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x13C)"),
    F0C(PM_M_VCORE,      18,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN,
        "GN vcore_telemetry_peak (9800X3D)"),
    F0(PM_M_SOCKET_POWER, 20, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0(PM_M_SOC_POWER,    21, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    F0C(PM_M_VDD_MISC,    58, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN,
        "9800X3D names 58 vddio_mem_voltage; meaning differs from 620205"),
    F0C(PM_M_VSOC,        83, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_GN,
        "GN lists it provisional (no independent measurement)"),
    F0C(PM_M_VDDP,       269, PM_U_V,   1.0f, PM_CONFLICTED, SRC_TX " (vddp)",
        "GN 9800X3D: 269 is vddcr_cpu_vid; ZS vddp 0x434/269 untested here"),
    FAC(PM_M_CORE_VOLTAGE, 309, 8, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED,
        SRC_GN "; " SRC_ZM " (0x4D4)",
        "author map used 301 — rejected by GN/ZM"),
    FA(PM_M_CORE_TEMP,    317, 8, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_TX "; " SRC_GN "; " SRC_ZM " (0x4F4)"),
    FA(PM_M_CORE_CLK,     325, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_TX "; " SRC_GN "; " SRC_ZM " (0x514)"),
    FA(PM_M_CORE_POWER,   333, 8, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_GN "; " SRC_ZM " (0x534)"),
    FAC(PM_M_CORE_C0,     341, 8, PM_U_PCT, 1.0f, PM_CONFLICTED, SRC_GN,
        "ZM publishes C0=357/CC1=365/CC6=341 for single-CCD GNR; GN "
        "load-validated 341/349/357 — unresolved"),
    FAC(PM_M_CORE_CC1,    349, 8, PM_U_PCT, 1.0f, PM_CONFLICTED, SRC_GN,
        "see C0 conflict"),
    FAC(PM_M_CORE_CC6,    357, 8, PM_U_PCT, 1.0f, PM_CONFLICTED, SRC_GN,
        "see C0 conflict"),
    FA(PM_M_CORE_BOOST_LIM, 373, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_GN " (boost_limit_confident=True)"),
    FAC(PM_M_CCD_L3_TEMP, 448, 1, PM_U_C, 1.0f, PM_CANDIDATE, SRC_GND,
        "GN L3-coupling evidence for 9800X3D"),
};

/* 0x621102 / 0x621202 — X3D revisions. ZS publishes clocks/rails/SVI3;
 * LHa publishes 621202 per-core bases identical to 0x620205. No zone-0
 * limit fields are published for these revisions: they stay unavailable. */
static const pm_field_t gnr621202_fields[] = {
    F0(PM_M_FCLK,        71,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x11C)"),
    F0(PM_M_UCLK,        75,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x12C)"),
    F0(PM_M_MCLK,        79,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x13C)"),
    F0(PM_M_VSOC,        54,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xD8, SVI3 SOC V)"),
    F0(PM_M_VDDP,       269,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x434)"),
    F0(PM_M_VDDG_IOD,   259,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x40C)"),
    F0(PM_M_VDDG_CCD,   261,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x414)"),
    FA(PM_M_CORE_POWER,   301, 16, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LHa),
    FA(PM_M_CORE_VOLTAGE, 317, 16, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LHa),
    FA(PM_M_CORE_TEMP,    333, 16, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LHa),
    FAC(PM_M_CORE_CLK,     349, 16, PM_U_GHZ, 1.0f, PM_CONFLICTED, SRC_LHa,
        "same block as 0x620205/349 — live disagreement with cpufreq"),
};

static const pm_field_t gnr621102_fields[] = {
    F0(PM_M_FCLK,        71,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x11C)"),
    F0(PM_M_UCLK,        75,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x12C)"),
    F0(PM_M_MCLK,        79,  PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x13C)"),
    F0(PM_M_VSOC,        54,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xD8, SVI3 SOC V)"),
    F0(PM_M_VDDP,       269,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x434)"),
    F0(PM_M_VDDG_IOD,   259,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x40C)"),
    F0(PM_M_VDDG_CCD,   261,  PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x414)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Zen 4 — Raphael / Dragon Range
 * ───────────────────────────────────────────────────────────────────── */

#define RAPHAEL_GLOBALS \
    F0(PM_M_FCLK, 70, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x118); " SRC_TX), \
    F0C(PM_M_VSOC, 82, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX, \
        "ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82"), \
    F0C(PM_M_VDDG_IOD, 259, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x40C); " SRC_TX, NULL), \
    F0(PM_M_VDDG_CCD, 261, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x414); " SRC_TX), \
    F0(PM_M_SVI3_VDD_VID,  46, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB8 block)"), \
    F0(PM_M_SVI3_VDD_VOLT, 47, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_VDD_CURR, 48, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_SOC_VID,  51, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_SOC_VOLT, 52, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_SOC_CURR, 53, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_MISC_VID, 56, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS), \
    F0(PM_M_SVI3_MISC_VOLT,57, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS)

/* note: F0C with NULL conflict is allowed but pointless — use F0 there */

static const pm_field_t raphael_dual_fields[] = {
    RAPHAEL_GLOBALS,
    F0(PM_M_UCLK, 74, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x128); " SRC_TX),
    F0(PM_M_MCLK, 78, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x138); " SRC_TX),
    F0(PM_M_VDDP, 268, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x430); " SRC_TX),
    F0(PM_M_VDD_MISC, 56, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX " (56=SVI3 MISC VID)"),
    F0(PM_M_VCORE, 271, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    FA(PM_M_CORE_POWER,   293, 16, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x494)"),
    FA(PM_M_CORE_VOLTAGE, 309, 16, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x4D4)"),
    FA(PM_M_CORE_TEMP,    325, 16, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x514)"),
    FA(PM_M_CORE_CLK,     341, 16, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x554)"),
    FA(PM_M_CORE_EFF_CLK, 357, 16, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x594)"),
    FA(PM_M_CORE_C0,      373, 16, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x5D4)"),
    FA(PM_M_CORE_CC1,     389, 16, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x614)"),
    FA(PM_M_CORE_CC6,     405, 16, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x654)"),
};

static const pm_field_t raphael_single_fields[] = {
    RAPHAEL_GLOBALS,
    F0C(PM_M_UCLK, 78, PM_U_MHZ, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? "
        "indistinguishable at 1:1 — unresolved)"),
    F0C(PM_M_MCLK, 74, PM_U_MHZ, 1.0f, PM_CONFLICTED, SRC_TX,
        "see UCLK conflict"),
    F0C(PM_M_VDDP, 269, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS CLDO_VDDP=0x430/268"),
    F0(PM_M_VCORE, 271, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    FA(PM_M_CORE_POWER,   293, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x494)"),
    FA(PM_M_CORE_VOLTAGE, 301, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x4B4)"),
    FA(PM_M_CORE_TEMP,    309, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x4D4)"),
    FA(PM_M_CORE_CLK,     317, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x4F4)"),
    FA(PM_M_CORE_EFF_CLK, 325, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x514)"),
    FA(PM_M_CORE_C0,      333, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x534)"),
    FA(PM_M_CORE_CC1,     341, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x554)"),
    FA(PM_M_CORE_CC6,     357, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x574)"),
};

/* 0x540208 — ZS shifts the whole clock/rail/SVI3 zone +4 bytes vs 540004 */
static const pm_field_t raphael_540208_fields[] = {
    F0(PM_M_FCLK, 71, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x11C)"),
    F0(PM_M_UCLK, 75, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x12C)"),
    F0(PM_M_MCLK, 79, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x13C)"),
    F0(PM_M_VDDG_IOD, 259, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x40C)"),
    F0(PM_M_VDDG_CCD, 261, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x414)"),
    F0(PM_M_VDDP, 269, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x434)"),
    F0(PM_M_SVI3_VDD_VID,  47, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xBC block)"),
    F0(PM_M_SVI3_VDD_VOLT, 48, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR, 49, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,  52, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VOLT, 53, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR, 54, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VID, 57, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VOLT,58, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xE8)"),
};

/* Phoenix/Hawk Point desktop (0x4C0008/0x4C0009) — ZS clocks + SVI3 */
static const pm_field_t phoenix_fields[] = {
    F0(PM_M_FCLK, 89, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x164); " SRC_TX),
    F0(PM_M_UCLK, 93, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x174); " SRC_TX),
    F0(PM_M_MCLK, 97, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x184); " SRC_TX),
    F0(PM_M_VDDP, 477, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x774); " SRC_TX),
    F0C(PM_M_VSOC, 101, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS VDDCR_SOC=0x1BC/111 (SVI3 measured); author used 101"),
    F0(PM_M_SVI3_VDD_VID,   105, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1A4 block)"),
    F0(PM_M_SVI3_VDD_VOLT,  106, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR,  107, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,   110, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VOLT,  111, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR,  112, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_PPT_CURRENT, 3, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_SOCKET_POWER, 38, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VID_AGG, 28, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    FA(PM_M_CORE_POWER,   200, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x320)"),
    FA(PM_M_CORE_VOLTAGE, 208, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x340)"),
    FA(PM_M_CORE_TEMP,    216, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x360)"),
    FA(PM_M_CORE_CLK,     240, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x3C0)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Zen 3 — Vermeer / Cezanne
 * ───────────────────────────────────────────────────────────────────── */

/* Vermeer zone-0 + rails shared by all 0x3808/0x3809 revisions. */
#define VERMEER_GLOBALS \
    F0C(PM_M_PPT_CURRENT, 1, PM_U_W, 1.0f, PM_CONFLICTED, SRC_TX, \
        "GN measured index 1 = socket power (RAPL-corroborated) on 0x380905"), \
    F0(PM_M_VID_AGG, 10, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX " (legacy ZenStates port)"), \
    F0C(PM_M_IOD_HOTSPOT, 11, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX, \
        "zone-0 (lim,val) pair pattern implies 10/11 = thm/tctl (GN)"), \
    F0(PM_M_SOCKET_POWER, 29, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX), \
    F0(PM_M_FCLK, 48, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC0); " SRC_LH "; " SRC_TX), \
    F0(PM_M_UCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC8); " SRC_LH "; " SRC_TX), \
    F0(PM_M_MCLK, 51, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xCC); " SRC_LH "; " SRC_TX), \
    F0C(PM_M_VSOC, 44, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX, \
        "ZS VDDCR_SOC=0xB4/45; GN provisional 45; author 44"), \
    F0(PM_M_VDDP, 137, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x224); " SRC_TX), \
    F0(PM_M_VDDG_IOD, 138, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x228); " SRC_TX), \
    F0(PM_M_VDDG_CCD, 139, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x22C); " SRC_TX)

static const pm_field_t vermeer_804_fields[] = {
    VERMEER_GLOBALS,
    F0C(PM_M_VCORE, 40, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX,
        "805 revision uses 39"),
    FAC(PM_M_CORE_VOLTAGE, 185, 8, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS power base 0x24C/147 + ZM block order predict volt=155/temp=163; "
        "author used 185/201 — unresolved for dual-CCD parts"),
    FAC(PM_M_CORE_TEMP, 201, 8, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX,
        "see CORE_VOLTAGE conflict"),
};

static const pm_field_t vermeer_805_fields[] = {
    VERMEER_GLOBALS,
    F0C(PM_M_VCORE, 39, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX, NULL),
    FAC(PM_M_CORE_VOLTAGE, 188, 8, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS 0x2B0/172 power + ZM pattern predict volt=180/temp=188; author "
        "188/204 — unresolved for dual-CCD parts"),
    FAC(PM_M_CORE_TEMP, 204, 8, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX,
        "see CORE_VOLTAGE conflict"),
};

static const pm_field_t vermeer_904_fields[] = {
    VERMEER_GLOBALS,
    F0C(PM_M_VCORE, 40, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX, NULL),
    FAC(PM_M_CORE_VOLTAGE, 177, 8, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS power 0x2A4/169 + pattern predict volt=177/temp=185; author "
        "177/183 — temp disagrees"),
    FAC(PM_M_CORE_TEMP, 183, 8, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX,
        "see CORE_VOLTAGE conflict"),
};

/* 0x380905 — the only pre-Zen5 layout with a live-validated (load-pinned,
 * 15-snapshot) mapping in the wild: gnr-smu's 5600X profile. Fused-slot
 * detection is enforced by the decoder (fail closed). */
static const uint8_t vermeer_5600x_slots[] = { 0, 1, 4, 5, 6, 7 };
static const pm_field_t vermeer_905_fields[] = {
    VERMEER_GLOBALS,
    F0C(PM_M_SOCKET_POWER, 1, PM_U_W, 1.0f, PM_CONFIRMED, SRC_GN,
        "validated against RAPL package power"),
    F0C(PM_M_VCORE, 39, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX, NULL),
    FA(PM_M_CORE_POWER,   172, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED,
       SRC_GN "; " SRC_ZS " (0x2B0)"),
    FA(PM_M_CORE_VOLTAGE, 180, 8, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    FA(PM_M_CORE_TEMP,    188, 8, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_GN " (live vs k10temp Tccd1)"),
    FA(PM_M_CORE_CLK,     212, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    FA(PM_M_CORE_EFF_CLK, 220, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    FA(PM_M_CORE_C0,      228, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    FA(PM_M_CORE_CC1,     236, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
    FA(PM_M_CORE_CC6,     244, 8, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_GN),
};

static const pm_field_t cezanne_fields[] = {
    F0(PM_M_IOD_HOTSPOT, 29, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_PPT_CURRENT, 5, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VID_AGG, 28, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_SOCKET_POWER, 38, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VCORE, 98, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0C(PM_M_VSOC, 102, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS VDDCR_SOC=0x19C/103"),
    F0(PM_M_FCLK, 409, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x664); " SRC_TX),
    F0(PM_M_UCLK, 410, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x668); " SRC_TX),
    F0(PM_M_MCLK, 411, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x66C); " SRC_TX),
    F0(PM_M_VDDP, 565, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x8D4); " SRC_TX),
    FA(PM_M_CORE_POWER,   200, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x320)"),
    FA(PM_M_CORE_VOLTAGE, 208, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x340)"),
    FA(PM_M_CORE_TEMP,    216, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x360)"),
    FA(PM_M_CORE_CLK,     240, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x3C0)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Zen 2 — Matisse / Renoir
 * ───────────────────────────────────────────────────────────────────── */

#define MATISSE_GLOBALS \
    F0C(PM_M_IOD_HOTSPOT, 11, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX, \
        "zone-0 pair pattern (GN): 10/11 = thm/tctl"), \
    F0(PM_M_PPT_CURRENT, 1, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX), \
    F0(PM_M_VID_AGG, 10, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX), \
    F0(PM_M_SOCKET_POWER, 29, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX), \
    F0(PM_M_FCLK, 48, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC0); " SRC_LH "; " SRC_TX), \
    F0C(PM_M_UCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LH "; " SRC_TX, \
        "ZS says 49 (0xC4) — outvoted 2:1"), \
    F0C(PM_M_MCLK, 51, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LH "; " SRC_TX, \
        "ZS says 50 (0xC8) — outvoted 2:1"), \
    F0C(PM_M_VSOC, 44, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX, \
        "ZS VDDCR_SOC=0xB4/45"), \
    F0C(PM_M_VDDP, 125, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX, \
        "ZS CLDO_VDDP=0x1F4/124"), \
    F0(PM_M_VDDG_IOD, 126, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1F8); " SRC_TX)

static const pm_field_t matisse_8_fields[] = {
    MATISSE_GLOBALS,
    F0C(PM_M_VCORE, 39, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX, NULL),
    FA(PM_M_CORE_POWER,   147, 8, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_ZS " (0x24C); " SRC_ZM " (0x24C)"),
    FA(PM_M_CORE_VOLTAGE, 155, 8, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x26C)"),
    FA(PM_M_CORE_TEMP,    163, 8, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x28C)"),
    FAC(PM_M_CORE_CLK,    187, 8, PM_U_GHZ, 1.0f, PM_CONFLICTED, SRC_ZM " (0x2EC)",
        "author did not map Matisse clocks; ZM base sits past 8-lane temp block"),
};

static const pm_field_t matisse_16_fields[] = {
    MATISSE_GLOBALS,
    F0C(PM_M_VCORE, 40, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX, NULL),
    FA(PM_M_CORE_POWER,   147, 8, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED,
       SRC_ZS " (0x24C); " SRC_ZM " (0x24C)"),
    FAC(PM_M_CORE_VOLTAGE, 155, 8, PM_U_V, 1.0f, PM_CONFLICTED, SRC_ZM " (0x26C)",
        "author used 163/179 for voltage/temp on dual-CCD Matisse — that is "
        "ZM's TEMP base; second-CCD block placement unpublished"),
    FAC(PM_M_CORE_TEMP,    163, 8, PM_U_C, 1.0f, PM_CONFLICTED, SRC_ZM " (0x28C)",
        "see CORE_VOLTAGE conflict"),
};

static const pm_field_t renoir_370003_fields[] = {
    F0(PM_M_IOD_HOTSPOT, 29, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_PPT_CURRENT, 5, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VID_AGG, 28, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_SOCKET_POWER, 38, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VCORE, 97, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0C(PM_M_VSOC, 101, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS VDDCR_SOC=0x198/102"),
    F0C(PM_M_VDDP, 527, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS CLDO_VDDP=0x844/529"),
    F0(PM_M_FCLK, 371, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5CC); " SRC_TX),
    F0(PM_M_UCLK, 372, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5D0); " SRC_TX),
    F0(PM_M_MCLK, 373, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5D4); " SRC_TX),
    FA(PM_M_CORE_POWER,   192, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x300)"),
    FA(PM_M_CORE_VOLTAGE, 200, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x320)"),
    FA(PM_M_CORE_TEMP,    208, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x340)"),
    FA(PM_M_CORE_CLK,     232, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x3A0)"),
};

static const pm_field_t renoir_370005_fields[] = {
    F0(PM_M_IOD_HOTSPOT, 29, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_PPT_CURRENT, 5, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VID_AGG, 28, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_SOCKET_POWER, 38, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VCORE, 97, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0C(PM_M_VSOC, 101, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS VDDCR_SOC=0x198/102"),
    F0C(PM_M_VDDP, 534, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "ZS CLDO_VDDP=0x86C/543"),
    F0(PM_M_FCLK, 378, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5E8); " SRC_TX),
    F0(PM_M_UCLK, 379, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5EC); " SRC_TX),
    F0(PM_M_MCLK, 380, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5F0); " SRC_TX),
    FA(PM_M_CORE_POWER,   199, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x31C rev)"),
    FA(PM_M_CORE_VOLTAGE, 207, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x33C rev)"),
    FA(PM_M_CORE_TEMP,    215, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x35C rev)"),
    FA(PM_M_CORE_CLK,     239, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x3BC rev)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Zen/Zen+ APUs — Raven/Picasso (0x1E0004: author + LHM + ZS agree)
 * ───────────────────────────────────────────────────────────────────── */

static const pm_field_t raven_1e0004_fields[] = {
    F0C(PM_M_IOD_HOTSPOT, 11, PM_U_C, 1.0f, PM_CONFLICTED, SRC_TX,
        "LHM names 11 'EDC' current on 0x1E0004"),
    F0(PM_M_PPT_CURRENT, 5, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_SOCKET_POWER, 38, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VID_AGG, 57, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX),
    F0(PM_M_VDDP, 60, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xF0); " SRC_TX),
    F0C(PM_M_VCORE, 61, PM_U_V, 1.0f, PM_CONFLICTED, SRC_TX,
        "same index doubles as IOD temp in the author map"),
    F0(PM_M_VSOC, 65, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x104); " SRC_TX),
    FA(PM_M_CORE_POWER,   96, 4, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x180)"),
    FA(PM_M_CORE_VOLTAGE, 104, 4, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_ZM " (0x1A0)"),
    FA(PM_M_CORE_TEMP,    108, 4, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_TX "; " SRC_LH),
    F0(PM_M_GFX_VOLT,     150, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LH),
    F0(PM_M_GFX_TEMP,     151, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LH),
    F0(PM_M_GFX_CLK,      154, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_LH),
    F0(PM_M_FCLK,         166, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x298); " SRC_LH "; " SRC_TX),
    F0(PM_M_UCLK,         167, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x29C); " SRC_TX),
    F0(PM_M_MCLK,         168, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x2A0); " SRC_TX),
};

/* ZS-published 0x1E clocks/rails for the other Raven/Picasso revisions */
static const pm_field_t picasso_1e0003_fields[] = {
    F0(PM_M_FCLK, 166, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x298)"),
    F0(PM_M_UCLK, 167, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x29C)"),
    F0(PM_M_MCLK, 168, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x2A0)"),
    F0(PM_M_VSOC,  65, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x104)"),
    F0(PM_M_VDDP,  60, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xF0)"),
};
static const pm_field_t picasso_1e0002_fields[] = {
    F0(PM_M_FCLK, 285, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x474)"),
    F0(PM_M_UCLK, 286, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x478)"),
    F0(PM_M_MCLK, 287, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x47C)"),
    F0(PM_M_VSOC,  67, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x10C)"),
    F0(PM_M_VDDP,  62, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xF8)"),
};
static const pm_field_t picasso_1e0001_fields[] = {
    F0(PM_M_FCLK, 280, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x460)"),
    F0(PM_M_UCLK, 281, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x464)"),
    F0(PM_M_MCLK, 282, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x468)"),
    F0(PM_M_VSOC,  67, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x10C)"),
    F0(PM_M_VDDP,  62, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xF8)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Zen 5 APUs — Strix Point / Krackan Point (ZS + ZM published)
 * ───────────────────────────────────────────────────────────────────── */

static const pm_field_t strix_fields[] = {
    F0(PM_M_FCLK, 312, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4E0)"),
    F0(PM_M_UCLK, 313, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4E4)"),
    F0(PM_M_MCLK, 315, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4EC)"),
    FA(PM_M_CORE_POWER,   629, 12, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x9D4)"),
    FA(PM_M_CORE_VOLTAGE, 641, 12, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xA04)"),
    FA(PM_M_CORE_TEMP,    653, 12, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xA34)"),
    FA(PM_M_CORE_CLK,     665, 12, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xA64)"),
    FA(PM_M_CORE_EFF_CLK, 677, 12, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xA94)"),
    FA(PM_M_CORE_C0,      685, 12, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xAB4)"),
    FA(PM_M_CORE_CC1,     693, 12, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xAD4)"),
    FA(PM_M_CORE_CC6,     701, 12, PM_U_PCT, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0xAF4)"),
};

/* ZS publishes Krackan offsets under an explicit 'CANDIDATE, not
 * confirmed' comment — inherit that confidence verbatim. */
static const pm_field_t krackan_fields[] = {
    F0C(PM_M_FCLK, 306, PM_U_MHZ, 1.0f, PM_CANDIDATE, SRC_ZS " (0x4C8)",
        "ZS marks all Krackan offsets CANDIDATE"),
    F0C(PM_M_UCLK, 307, PM_U_MHZ, 1.0f, PM_CANDIDATE, SRC_ZS " (0x4CC)", "as above"),
    F0C(PM_M_MCLK, 308, PM_U_MHZ, 1.0f, PM_CANDIDATE, SRC_ZS " (0x4D0)", "as above"),
    F0C(PM_M_VSOC, 29, PM_U_V, 1.0f, PM_CANDIDATE, SRC_ZS " (0x74)", "as above"),
};


/* ─────────────────────────────────────────────────────────────────────
 * Additional published revisions (ZS rows; ZM per-core where listed)
 * ───────────────────────────────────────────────────────────────────── */

/* Zen2 Threadripper multi-CCD 0x240003 (ZS only) */
static const pm_field_t zen2_tr_fields[] = {
    F0(PM_M_FCLK, 44, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB0)"),
    F0(PM_M_UCLK, 46, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB8)"),
    F0(PM_M_MCLK, 47, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xBC)"),
    F0(PM_M_VSOC,  41, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xA4)"),
    F0(PM_M_VDDP, 121, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1E4)"),
    F0(PM_M_VDDG_IOD, 122, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1E8)"),
};

/* Zen2 (0x24) 503/603/703 revisions — ZS+ZM offsets, no author map */
static const pm_field_t matisse_503_fields[] = {
    F0(PM_M_FCLK, 48, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC0)"),
    F0(PM_M_UCLK, 49, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC4)"),
    F0(PM_M_MCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC8)"),
    F0(PM_M_VSOC,  45, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB4)"),
    F0(PM_M_VDDP, 124, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1F4)"),
    F0(PM_M_VDDG_IOD, 126, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1F8)"),
    FA(PM_M_CORE_POWER,   147, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED,
       SRC_ZS " (0x24C); " SRC_ZM " (0x24C)"),
    FA(PM_M_CORE_VOLTAGE, 155, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x26C)"),
    FA(PM_M_CORE_TEMP,    163, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x28C)"),
    FA(PM_M_CORE_CLK,     187, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x2EC)"),
};

/* Zen2 02-revisions (240802/240902, ZS only) */
static const pm_field_t matisse_02_fields[] = {
    F0(PM_M_FCLK, 47, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xBC)"),
    F0(PM_M_UCLK, 49, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC4)"),
    F0(PM_M_MCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC8)"),
    F0(PM_M_VSOC,  44, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB0)"),
    F0(PM_M_VDDP, 124, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1F0)"),
    F0(PM_M_VDDG_IOD, 125, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1F4)"),
};

/* Zen3 Threadripper/Milan 0x2D/0x380x05 — ZS clocks/rails, per-core unpublished */
static const pm_field_t zen3_multi_fields[] = {
    F0(PM_M_FCLK, 48, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC0)"),
    F0(PM_M_UCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC8)"),
    F0(PM_M_MCLK, 51, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xCC)"),
    F0(PM_M_VSOC,  45, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB4)"),
    F0(PM_M_VDDP, 137, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x224)"),
    F0(PM_M_VDDG_IOD, 138, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x228)"),
    F0(PM_M_VDDG_CCD, 139, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x22C)"),
};
static const pm_field_t milan_2d_fields[] = {
    F0(PM_M_FCLK, 47, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xBC)"),
    F0(PM_M_UCLK, 49, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC4)"),
    F0(PM_M_MCLK, 50, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xC8)"),
    F0(PM_M_VSOC,  44, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xB0)"),
    F0(PM_M_VDDP, 136, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x220)"),
    F0(PM_M_VDDG_IOD, 137, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x224)"),
};

/* Cezanne early revisions (ZS rows; clocks/rails only) */
static const pm_field_t cezanne_400001_fields[] = {
    F0(PM_M_FCLK, 393, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x624)"),
    F0(PM_M_UCLK, 394, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x628)"),
    F0(PM_M_MCLK, 395, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x62C)"),
    F0(PM_M_VDDP, 551, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x89C)"),
};
static const pm_field_t cezanne_400002_fields[] = {
    F0(PM_M_FCLK, 399, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x63C)"),
    F0(PM_M_UCLK, 400, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x640)"),
    F0(PM_M_MCLK, 401, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x644)"),
    F0(PM_M_VDDP, 557, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x8B4)"),
};
static const pm_field_t cezanne_400003_fields[] = {
    F0(PM_M_FCLK, 408, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x660)"),
    F0(PM_M_UCLK, 409, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x664)"),
    F0(PM_M_MCLK, 410, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x668)"),
    F0(PM_M_VDDP, 564, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x8D0)"),
};

/* Renoir early revisions */
static const pm_field_t renoir_370000_fields[] = {
    F0(PM_M_FCLK, 301, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4B4)"),
    F0(PM_M_UCLK, 302, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4B8)"),
    F0(PM_M_MCLK, 303, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x4BC)"),
    F0(PM_M_VSOC, 100, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x190)"),
    F0(PM_M_VDDP, 459, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x72C)"),
};
static const pm_field_t renoir_370001_fields[] = {
    F0(PM_M_FCLK, 361, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5A4)"),
    F0(PM_M_UCLK, 362, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5A8)"),
    F0(PM_M_MCLK, 363, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5AC)"),
    F0(PM_M_VSOC, 100, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x190)"),
    F0(PM_M_VDDP, 519, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x81C)"),
};
static const pm_field_t renoir_370002_fields[] = {
    F0(PM_M_FCLK, 363, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5AC)"),
    F0(PM_M_UCLK, 364, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5B0)"),
    F0(PM_M_MCLK, 365, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x5B4)"),
    F0(PM_M_VSOC, 102, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x198)"),
    F0(PM_M_VDDP, 521, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x824)"),
};

/* Van Gogh (ZS clocks + ZM per-core) */
static const pm_field_t vangogh_fields[] = {
    F0(PM_M_FCLK, 239, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x3BC)"),
    F0(PM_M_UCLK, 240, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x3C0)"),
    F0(PM_M_MCLK, 241, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x3C4)"),
    F0(PM_M_VSOC, 104, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1A0)"),
    FA(PM_M_CORE_POWER,   142, 2, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x238)"),
    FA(PM_M_CORE_VOLTAGE, 146, 2, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x248)"),
    FA(PM_M_CORE_TEMP,    150, 2, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x258)"),
    FA(PM_M_CORE_CLK,     162, 2, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x288)"),
};

/* Rembrandt (ZS clocks + ZM per-core; 450005 clocks shifted per ZS) */
static const pm_field_t rembrandt_fields[] = {
    F0(PM_M_FCLK, 409, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x664)"),
    F0(PM_M_UCLK, 410, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x668)"),
    F0(PM_M_MCLK, 411, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x66C)"),
    F0(PM_M_VDDP, 565, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x8D4)"),
    FA(PM_M_CORE_POWER,   200, 8, PM_U_W,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x320)"),
    FA(PM_M_CORE_VOLTAGE, 208, 8, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x340)"),
    FA(PM_M_CORE_TEMP,    216, 8, PM_U_C,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x360)"),
    FA(PM_M_CORE_CLK,     240, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZM " (0x3C0)"),
};
static const pm_field_t rembrandt_450005_fields[] = {
    F0(PM_M_FCLK, 428, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x6B0)"),
    F0(PM_M_UCLK, 429, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x6B4)"),
    F0(PM_M_MCLK, 430, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x6B8)"),
    F0C(PM_M_VDDP, 565, PM_U_V, 1.0f, PM_CANDIDATE, SRC_ZS, "Zero in issue #10 dump; unverified"),
    FAC(PM_M_CORE_POWER, 219, 8, PM_U_W, 1.0f, PM_SOURCE_DOCUMENTED, "upstream issue #10 (0x36C)", "Single 6900HS dump; needs workload validation"),
    FAC(PM_M_CORE_VOLTAGE, 227, 8, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, "upstream issue #10 (0x38C)", "Single 6900HS dump; needs workload validation"),
    FAC(PM_M_CORE_TEMP, 235, 8, PM_U_C, 1.0f, PM_SOURCE_DOCUMENTED, "upstream issue #10 (0x3AC)", "Single 6900HS dump; needs workload validation"),
    FAC(PM_M_CORE_CLK, 251, 8, PM_U_GHZ, 1.0f, PM_SOURCE_DOCUMENTED, "upstream issue #10 (0x3EC)", "Single 6900HS dump; needs workload validation"),
};

/* Phoenix mobile 0x4C0001-4C0007 (ZS; SVI3 stride 4, no VRM temp) */
static const pm_field_t phoenix_4c01_07_fields[] = {
    F0(PM_M_FCLK,  93, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x174)"),
    F0(PM_M_UCLK,  97, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x184)"),
    F0(PM_M_MCLK, 101, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x194)"),
    F0(PM_M_VSOC, 114, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1C8, SVI3 SOC V)"),
    F0(PM_M_VDDP, 474, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x768)"),
    F0(PM_M_SVI3_VDD_VID, 109, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1B4 block)"),
    F0(PM_M_SVI3_VDD_VOLT, 110, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR, 111, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,  113, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR, 115, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
};

/* Storm Peak (Threadripper 7000WX) — ZS; SVI3 4 rails incl. VDD1 */
static const pm_field_t stormpeak_fields[] = {
    F0(PM_M_FCLK, 140, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x194)"),
    F0(PM_M_UCLK, 168, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1A8)"),
    F0(PM_M_MCLK, 180, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1BC)"),
    F0(PM_M_VSOC,  72, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x120, SVI3 SOC V)"),
    F0(PM_M_SVI3_VDD_VID,  61, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xF4 block)"),
    F0(PM_M_SVI3_VDD_VOLT, 62, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR, 63, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,  71, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR, 73, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VID, 76, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VOLT,77, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x134)"),
};
static const pm_field_t stormpeak_x03_fields[] = {
    F0(PM_M_FCLK, 141, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x19C, verified on 7985WX report)"),
    F0(PM_M_UCLK, 168, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1B0)"),
    F0(PM_M_MCLK, 181, PM_U_MHZ, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x1C4)"),
    F0(PM_M_VSOC,  74, PM_U_V,   1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x128, SVI3 SOC V)"),
    F0(PM_M_SVI3_VDD_VID,  63, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0xFC block)"),
    F0(PM_M_SVI3_VDD_VOLT, 64, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_VDD_CURR, 65, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_VID,  73, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_SOC_CURR, 75, PM_U_A, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VID, 78, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS),
    F0(PM_M_SVI3_MISC_VOLT,79, PM_U_V, 1.0f, PM_SOURCE_DOCUMENTED, SRC_ZS " (0x13C)"),
};

/* ─────────────────────────────────────────────────────────────────────
 * Registry
 * ───────────────────────────────────────────────────────────────────── */

#define LAYOUT(ver, size, nm, fam, slots, cores, map, flds, note) \
    { (ver), (size), (nm), (fam), -1, (slots), (cores), (map), \
      (flds), sizeof(flds) / sizeof((flds)[0]), (note) }

static const pm_layout_t g_layouts[] = {
    LAYOUT(0x620205, 0x994, "Granite Ridge Dual-CCD", "Granite Ridge",
           16, 16, NULL, gnr_dual_fields,
           "zone0+per-core live-validated on 9950X; GN cross-corroborated"),
    LAYOUT(0x621202, 0x994, "Granite Ridge Dual-CCD (X3D rev)", "Granite Ridge",
           16, 16, NULL, gnr621202_fields,
           "clocks/rails ZS; per-core LHa; zone0 limits not published"),
    LAYOUT(0x620105, 0x724, "Granite Ridge Single-CCD (X3D)", "Granite Ridge",
           8, 8, NULL, gnr_single_fields,
           "9800X3D-validated (gnr-smu); residency block conflicted"),
    LAYOUT(0x621102, 0x724, "Granite Ridge Single-CCD (rev)", "Granite Ridge",
           8, 8, NULL, gnr621102_fields,
           "clocks/rails from ZS only"),
    LAYOUT(0x540004, 0x8BC, "Raphael Dual-CCD", "Raphael",
           16, 16, NULL, raphael_dual_fields,
           "ZS + ZM + author agree on per-core; vsoc conflicted"),
    LAYOUT(0x540104, 0x6A8, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields,
           "uclk/mclk order and vddp index conflicted"),
    LAYOUT(0x540208, 0x8D0, "Raphael Dual-CCD rev", "Raphael",
           16, 16, NULL, raphael_540208_fields,
           "ZS clocks +4 bytes vs 540004; per-core bases not published for "
           "this revision — clocks/rails/SVI3 only"),
    LAYOUT(0x4C0009, 0xB00, "Hawk Point (Phoenix2 desktop)", "Hawk/Phoenix",
           8, 8, NULL, phoenix_fields, "vsoc conflicted"),
    LAYOUT(0x4C0008, 0xAF0, "Phoenix Point desktop", "Hawk/Phoenix",
           8, 8, NULL, phoenix_fields,
           "clock offsets -4 vs 0x4C0009 per ZS — treat clocks as suspect"),
    LAYOUT(0x380804, 0x8A4, "Vermeer Dual-CCD (older BIOS)", "Vermeer",
           8, 16, NULL, vermeer_804_fields,
           "per-core voltage/temp conflicted; fused-slot layout unknown"),
    LAYOUT(0x380805, 0x8F0, "Vermeer Dual-CCD (newer BIOS)", "Vermeer",
           8, 16, NULL, vermeer_805_fields,
           "per-core voltage/temp conflicted; fused-slot layout unknown"),
    LAYOUT(0x380904, 0x5A4, "Vermeer Single-CCD (older BIOS)", "Vermeer",
           8, 8, NULL, vermeer_904_fields, "per-core voltage/temp conflicted"),
    LAYOUT(0x380905, 0x5D0, "Vermeer Single-CCD (newer BIOS)", "Vermeer",
           8, 6, vermeer_5600x_slots, vermeer_905_fields,
           "gnr-smu 5600X live-validated; decoder must re-check the fused "
           "slot map against the live table and refuse per-core on mismatch"),
    LAYOUT(0x400005, 0x944, "Cezanne / Barcelo", "Cezanne",
           8, 8, NULL, cezanne_fields, "ZS + ZM + author agree"),
    LAYOUT(0x370003, 0x8C8, "Renoir / Lucienne", "Renoir",
           8, 8, NULL, renoir_370003_fields, "vsoc/vddp conflicted"),
    LAYOUT(0x370005, 0x8C8, "Renoir v2 APU", "Renoir",
           8, 8, NULL, renoir_370005_fields,
           "vsoc/vddp conflicted; size: ZS 0x8C8 vs ZM 0x8C8, RyzenAdj 0x8C8"),
    LAYOUT(0x240903, 0x518, "Matisse Single-CCD (3600/3700X/3800X)", "Matisse",
           8, 8, NULL, matisse_8_fields, "vsoc/vddp/iod conflicted"),
    LAYOUT(0x240803, 0x7E4, "Matisse Dual-CCD (3900X/3950X)", "Matisse",
           8, 16, NULL, matisse_16_fields,
           "per-core blocks 8 lanes/CCD — dual-CCD block stride not published"),
    LAYOUT(0x1E0004, 0x608, "Raven Ridge (2200G-2700U)", "Raven/Picasso",
           4, 4, NULL, raven_1e0004_fields, "author + LHM + ZS agree"),
    LAYOUT(0x1E0003, 0x608, "Picasso APU (3200G/3400G)", "Raven/Picasso",
           4, 4, NULL, picasso_1e0003_fields, "ZS clocks/rails only"),
    LAYOUT(0x1E0005, 0x608, "Picasso APU refresh", "Raven/Picasso",
           4, 4, NULL, picasso_1e0003_fields, "ZS: same offsets as 0x1E0003"),
    LAYOUT(0x1E0101, 0x608, "Picasso revision", "Raven/Picasso",
           4, 4, NULL, picasso_1e0003_fields, "ZS: same offsets as 0x1E0003"),
    LAYOUT(0x1E0002, 0x580, "Raven Ridge APU (early)", "Raven/Picasso",
           4, 4, NULL, picasso_1e0002_fields, "ZS clocks/rails only"),
    LAYOUT(0x1E0001, 0x570, "Raven Ridge APU (earliest)", "Raven/Picasso",
           4, 4, NULL, picasso_1e0001_fields,
           "size: ZS 0x570 vs ZM 0x568 — conflict recorded"),
    LAYOUT(0x1E000A, 0x608, "Picasso Embedded APU", "Raven/Picasso",
           4, 4, NULL, picasso_1e0003_fields, "ZS: same offsets as 0x1E0003"),
    LAYOUT(0x240003, 0x18AC, "Castle Peak / Matisse multi-CCD", "Matisse",
           8, 16, NULL, zen2_tr_fields,
           "clocks/rails only; multi-CCD per-core geometry unpublished"),
    LAYOUT(0x240503, 0xD7C, "Castle Peak / Matisse", "Matisse",
           8, 16, NULL, matisse_503_fields, "ZS + ZM agree"),
    LAYOUT(0x240603, 0xAB0, "Castle Peak / Matisse", "Matisse",
           8, 16, NULL, matisse_503_fields, "ZS + ZM agree"),
    LAYOUT(0x240703, 0x7E4, "Castle Peak / Matisse", "Matisse",
           8, 16, NULL, matisse_503_fields, "ZS + ZM agree"),
    LAYOUT(0x240802, 0x7E0, "Matisse 02-rev", "Matisse",
           8, 16, NULL, matisse_02_fields, "ZS only"),
    LAYOUT(0x240902, 0x514, "Matisse single-CCD 02-rev", "Matisse",
           8, 8, NULL, matisse_02_fields, "ZS only"),
    LAYOUT(0x2D0008, 0x1AB0, "Milan / Chagall multi-CCD", "Milan",
           8, 16, NULL, milan_2d_fields, "clocks/rails only"),
    LAYOUT(0x2D0803, 0x894, "Milan / Chagall", "Milan",
           8, 16, NULL, milan_2d_fields, "clocks/rails only"),
    LAYOUT(0x2D0903, 0x7E4, "Milan / Chagall single-CCD", "Milan",
           8, 8, NULL, milan_2d_fields, "clocks/rails only"),
    LAYOUT(0x380005, 0x1BB0, "Chagall / Vermeer multi-CCD", "Vermeer",
           8, 16, NULL, zen3_multi_fields, "clocks/rails only"),
    LAYOUT(0x380505, 0xF30, "Vermeer / Chagall", "Vermeer",
           8, 16, NULL, zen3_multi_fields, "clocks/rails only"),
    LAYOUT(0x380605, 0xC10, "Vermeer / Chagall", "Vermeer",
           8, 16, NULL, zen3_multi_fields, "clocks/rails only"),
    LAYOUT(0x380705, 0x8F0, "Vermeer / Chagall", "Vermeer",
           8, 16, NULL, zen3_multi_fields, "clocks/rails only"),
    LAYOUT(0x3F0000, 0x7AC, "Van Gogh (Steam Deck)", "Van Gogh",
           2, 2, NULL, vangogh_fields, "ZS clocks (unverified there) + ZM per-core"),
    LAYOUT(0x400001, 0x910, "Cezanne 01-rev", "Cezanne",
           8, 8, NULL, cezanne_400001_fields, "clocks/vddp only; ZM 01-rev per-core "
           "(0x304/0x324/0x344/0x3A4) — single-sourced, not mapped"),
    LAYOUT(0x400002, 0x928, "Cezanne 02-rev", "Cezanne",
           8, 8, NULL, cezanne_400002_fields, "clocks/vddp only"),
    LAYOUT(0x400003, 0x94C, "Cezanne 03-rev", "Cezanne",
           8, 8, NULL, cezanne_400003_fields, "clocks/vddp only"),
    LAYOUT(0x400004, 0x944, "Cezanne 04-rev", "Cezanne",
           8, 8, NULL, cezanne_fields, "ZS: identical offsets to 400005"),
    LAYOUT(0x370000, 0x884, "Renoir 00-rev", "Renoir",
           8, 8, NULL, renoir_370000_fields, "ZS size 0x884 (ZM says 0x794 — conflict)"),
    LAYOUT(0x370001, 0x88C, "Renoir 01-rev", "Renoir",
           8, 8, NULL, renoir_370001_fields, "ZS size 0x88C (ZM says 0x884 — conflict)"),
    LAYOUT(0x370002, 0x8AC, "Renoir 02-rev", "Renoir",
           8, 8, NULL, renoir_370002_fields, "ZS size 0x8AC (ZM says 0x88C — conflict)"),
    LAYOUT(0x370004, 0x8C8, "Renoir 04-rev", "Renoir",
           8, 8, NULL, renoir_370003_fields, "ZS: identical offsets to 370003"),
    LAYOUT(0x450004, 0xAA4, "Rembrandt", "Rembrandt",
           8, 8, NULL, rembrandt_fields, "ZS clocks + ZM per-core"),
    LAYOUT(0x450005, 0xAB0, "Rembrandt 05-rev", "Rembrandt",
           8, 8, NULL, rembrandt_450005_fields, "ZS clocks + issue #10 per-core candidates"),
    LAYOUT(0x4C0001, 0xB18, "Phoenix 01 (ZS: placeholder, unverified)", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "offsets copied from 4C0003 in ZS"),
    LAYOUT(0x4C0003, 0xB18, "Phoenix Point", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "ZS clocks + SVI3 (stride 4)"),
    LAYOUT(0x4C0004, 0xB1C, "Phoenix Point", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "ZS clocks + SVI3 (stride 4)"),
    LAYOUT(0x4C0005, 0xAF8, "Phoenix Point", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "ZS clocks + SVI3 (stride 4)"),
    LAYOUT(0x4C0006, 0xAFC, "Phoenix Point", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "ZS clocks + SVI3 (stride 4)"),
    LAYOUT(0x4C0007, 0xB00, "Phoenix Point", "Hawk/Phoenix",
           8, 8, NULL, phoenix_4c01_07_fields, "ZS clocks + SVI3 (stride 4)"),
    LAYOUT(0x540000, 0x828, "Raphael Dual-CCD (ES/early)", "Raphael",
           16, 16, NULL, raphael_dual_fields, "ZS: same offsets as 540004"),
    LAYOUT(0x540001, 0x82C, "Raphael Dual-CCD", "Raphael",
           16, 16, NULL, raphael_dual_fields, "ZS: same offsets as 540004"),
    LAYOUT(0x540002, 0x87C, "Raphael Dual-CCD", "Raphael",
           16, 16, NULL, raphael_dual_fields, "ZS: same offsets as 540004"),
    LAYOUT(0x540003, 0x89C, "Raphael Dual-CCD", "Raphael",
           16, 16, NULL, raphael_dual_fields, "ZS: same offsets as 540004"),
    LAYOUT(0x540005, 0x8C8, "Raphael Dual-CCD", "Raphael",
           16, 16, NULL, raphael_dual_fields, "ZS: same offsets as 540004"),
    LAYOUT(0x540100, 0x618, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x540101, 0x61C, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x540102, 0x66C, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x540103, 0x68C, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x540105, 0x6B4, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x540108, 0x6BC, "Raphael Single-CCD / Dragon Range", "Raphael",
           8, 8, NULL, raphael_single_fields, "ZS: same offsets as 540104"),
    LAYOUT(0x5C0002, 0x1E3C, "Storm Peak 12-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only; per-core not published"),
    LAYOUT(0x5C0003, 0x1E48, "Storm Peak 12-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only"),
    LAYOUT(0x5C0102, 0x1A14, "Storm Peak 8-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only"),
    LAYOUT(0x5C0103, 0x1A20, "Storm Peak 8-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only"),
    LAYOUT(0x5C0202, 0x15EC, "Storm Peak 6-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only"),
    LAYOUT(0x5C0203, 0x15F8, "Storm Peak 6-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_x03_fields, "x03 layout +8 (ZS: verified on 7985WX)"),
    LAYOUT(0x5C0302, 0xD9C, "Storm Peak 4-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_fields, "clocks/SVI3 only"),
    LAYOUT(0x5C0303, 0xDA8, "Storm Peak 4-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_x03_fields, "x03 layout +8"),
    LAYOUT(0x5C0402, 0x974, "Storm Peak 2-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_x03_fields, "ZS: 2-CCD rows use the +8 layout"),
    LAYOUT(0x5C0403, 0x980, "Storm Peak 2-CCD", "Storm Peak",
           8, 16, NULL, stormpeak_x03_fields, "x03 layout +8"),
    LAYOUT(0x5D0008, 0xD54, "Strix Point 12C", "Strix Point",
           12, 12, NULL, strix_fields, "ZS clocks + ZM per-core"),
    LAYOUT(0x5D0009, 0xD58, "Strix Point 12C rev", "Strix Point",
           12, 12, NULL, strix_fields, "ZS clocks + ZM per-core"),
    LAYOUT(0x5D000A, 0xD60, "Strix Point rev A", "Strix Point",
           12, 12, NULL, strix_fields, "ZS clocks + ZM per-core"),
    LAYOUT(0x5D000B, 0xD60, "Strix Point rev B", "Strix Point",
           12, 12, NULL, strix_fields, "ZS clocks + ZM per-core"),
    LAYOUT(0x650004, 0xB74, "Krackan Point", "Krackan Point",
           8, 8, NULL, krackan_fields, "ZS marks everything CANDIDATE"),
    LAYOUT(0x650005, 0xB78, "Krackan Point", "Krackan Point",
           8, 8, NULL, krackan_fields, "ZS marks everything CANDIDATE"),
    LAYOUT(0x650006, 0xB80, "Krackan Point", "Krackan Point",
           8, 8, NULL, krackan_fields, "ZS marks everything CANDIDATE"),
    LAYOUT(0x650007, 0xB80, "Krackan Point", "Krackan Point",
           8, 8, NULL, krackan_fields, "size+offsets copied from 650006 (ZS)"),
    LAYOUT(0x650008, 0xB80, "Krackan Point (SMU 101.72)", "Krackan Point",
           8, 8, NULL, krackan_fields, "placeholder row in ZS — unverified"),
};

const pm_layout_t *pm_layout_lookup(uint32_t version)
{
    for (size_t i = 0; i < sizeof(g_layouts) / sizeof(g_layouts[0]); i++)
        if (g_layouts[i].version == version)
            return &g_layouts[i];
    return NULL;
}

int pm_layout_count(void) { return (int)(sizeof(g_layouts) / sizeof(g_layouts[0])); }
const pm_layout_t *pm_layout_at(int i)
{
    if (i < 0 || i >= pm_layout_count()) return NULL;
    return &g_layouts[i];
}

const char *pm_unit_name(uint8_t u)
{
    switch ((pm_unit_t)u) {
    case PM_U_W:   return "W";
    case PM_U_A:   return "A";
    case PM_U_V:   return "V";
    case PM_U_C:   return "C";
    case PM_U_MHZ: return "MHz";
    case PM_U_GHZ: return "GHz";
    case PM_U_PCT: return "%";
    default:       return "";
    }
}

const char *pm_confidence_name(uint8_t c)
{
    switch ((pm_confidence_t)c) {
    case PM_CONFIRMED:         return "confirmed";
    case PM_SOURCE_DOCUMENTED: return "documented";
    case PM_CANDIDATE:         return "CANDIDATE";
    case PM_CONFLICTED:        return "CONFLICTED";
    }
    return "?";
}

static const char *g_metric_names[PM_M__COUNT] = {
    [PM_M_PPT_LIMIT]          = "PPT Limit",
    [PM_M_PPT_CURRENT]        = "PPT Current",
    [PM_M_TDC_LIMIT]          = "TDC Limit",
    [PM_M_TDC_CURRENT]        = "TDC Current",
    [PM_M_EDC_LIMIT]          = "EDC Limit",
    [PM_M_EDC_CURRENT]        = "EDC Current",
    [PM_M_EDC_CURRENT_CAND]   = "EDC Current?",
    [PM_M_THM_LIMIT]          = "Thermal Limit",
    [PM_M_TCTL]               = "Tctl",
    [PM_M_IOD_HOTSPOT]        = "IOD Hotspot?",
    [PM_M_SOCKET_POWER]       = "Socket Power",
    [PM_M_CPU_POWER]          = "CPU Rail Power",
    [PM_M_SOC_POWER]          = "SoC Rail Power",
    [PM_M_VDDIO_POWER]        = "VDDIO Mem Power",
    [PM_M_VDD18_POWER]        = "VDD_18 Power",
    [PM_M_FIT_METRIC]         = "FIT Metric",
    [PM_M_VID_AGG]            = "VID (Aggregate)",
    [PM_M_VID_LIMIT]          = "VID Limit",
    [PM_M_VCORE]              = "VCORE",
    [PM_M_VSOC]               = "VDDCR_SOC",
    [PM_M_VDDP]               = "CLDO_VDDP",
    [PM_M_VDDG_IOD]           = "CLDO_VDDG_IOD",
    [PM_M_VDDG_CCD]           = "CLDO_VDDG_CCD",
    [PM_M_VDD_MISC]           = "VDD_MISC",
    [PM_M_SVI3_VDD_VID]       = "SVI3 VDD VID",
    [PM_M_SVI3_VDD_VOLT]      = "SVI3 VDD Volt",
    [PM_M_SVI3_VDD_CURR]      = "SVI3 VDD Current",
    [PM_M_SVI3_VDD_TEMP]      = "SVI3 VDD VRM Temp",
    [PM_M_SVI3_SOC_VID]       = "SVI3 SOC VID",
    [PM_M_SVI3_SOC_VOLT]      = "SVI3 SOC Volt",
    [PM_M_SVI3_SOC_CURR]      = "SVI3 SOC Current",
    [PM_M_SVI3_SOC_TEMP]      = "SVI3 SOC VRM Temp",
    [PM_M_SVI3_MISC_VID]      = "SVI3 MISC VID",
    [PM_M_SVI3_MISC_VOLT]     = "SVI3 MISC Volt",
    [PM_M_SVI3_MISC_CURR]     = "SVI3 MISC Current",
    [PM_M_FCLK]               = "FCLK",
    [PM_M_UCLK]               = "UCLK",
    [PM_M_MCLK]               = "MCLK",
    [PM_M_CCD_L3_TEMP]        = "CCD L3 Temp?",
    [PM_M_CCD_SHARED_TEMP]    = "CCD Shared Temp?",
    [PM_M_CORE_POWER]         = "Core Power",
    [PM_M_CORE_VOLTAGE]       = "Core Voltage",
    [PM_M_CORE_TEMP]          = "Core Temp",
    [PM_M_CORE_CLK]           = "Core Clock",
    [PM_M_CORE_EFF_CLK]       = "Core Eff. Clock",
    [PM_M_CORE_C0]            = "C0 Residency",
    [PM_M_CORE_CC1]           = "CC1 Residency",
    [PM_M_CORE_CC6]           = "CC6 Residency",
    [PM_M_CORE_FIT]           = "Core FIT",
    [PM_M_CORE_ACTIVITY]      = "Core Activity",
    [PM_M_CORE_BOOST_LIM]     = "Boost Limit",
    [PM_M_GFX_VOLT]           = "GFX Voltage",
    [PM_M_GFX_TEMP]           = "GFX Temp",
    [PM_M_GFX_CLK]            = "GFX Clock",
    [PM_M_GFX_POWER]          = "GFX Power",
    [PM_M_SOC_TEMP]           = "SoC Temp",
};

const char *pm_metric_name(uint16_t m)
{
    if (m < PM_M__COUNT && g_metric_names[m]) return g_metric_names[m];
    return "?";
}
