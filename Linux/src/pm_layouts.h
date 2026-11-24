#ifndef PM_LAYOUTS_H
#define PM_LAYOUTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Unified AMD SMU PM Table layout registry — pure data, no hardware access.
 *
 * Every entry is sourced from a published reference implementation or from
 * live-validated dumps on this repository's hardware. byte_offset of a field
 * is always float_index * sizeof(float); we store the FLOAT INDEX and never
 * mix the two. Sources are cited as <repo>@<sha> <file>[:<lines>].
 *
 * Confidence semantics (see docs/SMU_PM_TABLE_MAP.md):
 *   PM_CONFIRMED          live-validated (local dumps and/or independent
 *                         cross-sensor corroboration in a reference impl)
 *   PM_SOURCE_DOCUMENTED  published in a reference impl for exactly this
 *                         table version, not independently re-validated here
 *   PM_CANDIDATE          plausible correlation only; NEVER shown in the GUI,
 *                         debug output must label it as candidate
 *   PM_CONFLICTED         sources disagree on this field's meaning; NEVER
 *                         shown in the GUI, debug output must name the clash
 */

typedef enum {
    PM_CONFIRMED,
    PM_SOURCE_DOCUMENTED,
    PM_CANDIDATE,
    PM_CONFLICTED
} pm_confidence_t;

typedef enum {
    PM_U_NONE,
    PM_U_W,     /* watts                      */
    PM_U_A,     /* amperes                    */
    PM_U_V,     /* volts                      */
    PM_U_C,     /* degrees Celsius            */
    PM_U_MHZ,   /* megahertz                  */
    PM_U_GHZ,   /* gigahertz                  */
    PM_U_PCT    /* percent                    */
} pm_unit_t;

/* Architecture-independent metric identifiers. A layout simply does not
 * carry ids it cannot prove; the decoder leaves them unavailable. */
typedef enum {
    /* package */
    PM_M_PPT_LIMIT = 0,
    PM_M_PPT_CURRENT,
    PM_M_TDC_LIMIT,
    PM_M_TDC_CURRENT,
    PM_M_EDC_LIMIT,
    PM_M_EDC_CURRENT,
    PM_M_EDC_CURRENT_CAND,   /* candidate identity, debug-only            */
    PM_M_THM_LIMIT,
    PM_M_TCTL,
    PM_M_IOD_HOTSPOT,
    PM_M_SOCKET_POWER,
    PM_M_CPU_POWER,          /* VDDCR_CPU rail power                      */
    PM_M_SOC_POWER,          /* VDDCR_SOC rail power                      */
    PM_M_VDDIO_POWER,
    PM_M_VDD18_POWER,
    PM_M_FIT_METRIC,
    PM_M_VID_AGG,            /* aggregate core VID                        */
    PM_M_VID_LIMIT,
    PM_M_VCORE,
    PM_M_VSOC,
    PM_M_VDDP,
    PM_M_VDDG_IOD,
    PM_M_VDDG_CCD,
    PM_M_VDD_MISC,
    PM_M_SVI3_VDD_VID,       /* SVI3 telemetry block (Zen4/5)             */
    PM_M_SVI3_VDD_VOLT,
    PM_M_SVI3_VDD_CURR,
    PM_M_SVI3_VDD_TEMP,
    PM_M_SVI3_SOC_VID,
    PM_M_SVI3_SOC_VOLT,
    PM_M_SVI3_SOC_CURR,
    PM_M_SVI3_SOC_TEMP,
    PM_M_SVI3_MISC_VID,
    PM_M_SVI3_MISC_VOLT,
    PM_M_SVI3_MISC_CURR,
    PM_M_FCLK,
    PM_M_UCLK,
    PM_M_MCLK,
    /* per-CCD */
    PM_M_CCD_L3_TEMP,        /* candidate L3-traffic temperature lanes    */
    PM_M_CCD_SHARED_TEMP,
    /* per-core blocks (index = first lane; lanes = index + slot) */
    PM_M_CORE_POWER,
    PM_M_CORE_VOLTAGE,
    PM_M_CORE_TEMP,
    PM_M_CORE_CLK,
    PM_M_CORE_EFF_CLK,
    PM_M_CORE_C0,
    PM_M_CORE_CC1,
    PM_M_CORE_CC6,
    PM_M_CORE_FIT,
    PM_M_CORE_ACTIVITY,
    PM_M_CORE_BOOST_LIM,
    /* APU (Raven/Picasso, LHM-published) */
    PM_M_GFX_VOLT,
    PM_M_GFX_TEMP,
    PM_M_GFX_CLK,
    PM_M_GFX_POWER,
    PM_M_SOC_TEMP,
    PM_M__COUNT
} pm_metric_id_t;

#define PM_SRC_MAX 96

typedef struct {
    uint16_t metric;          /* pm_metric_id_t            */
    uint16_t index;           /* float index (not bytes!)  */
    uint16_t count;           /* 1 = scalar; N = per-core/per-CCD lanes */
    uint16_t stride;          /* floats between lanes (usually 1 = 4 bytes) */
    float    scale;           /* display = raw * scale     */
    uint8_t  unit;            /* pm_unit_t                 */
    uint8_t  confidence;      /* pm_confidence_t           */
    const char *source;       /* "repo@sha file" citation  */
    const char *conflict;     /* NULL or what clashes      */
} pm_field_t;

/* Per-core slot width: per-core blocks have `core_slots` lanes, which may
 * exceed physical core count (fused parts) and may not map 1:1 to Linux
 * cores (down-binned parts). See core_slots remapping below. */
typedef struct {
    uint32_t      version;        /* exact PM table version, no fallbacks */
    uint32_t      size_bytes;     /* expected size (0 = not published)    */
    const char   *name;           /* human layout name                    */
    const char   *family;         /* codename family for reports          */
    int8_t        smu_codename;   /* ryzen_smu 'codename' value, -1 = any */
    uint8_t       core_slots;     /* lanes in per-core blocks (8/16)      */
    uint8_t       max_cores;      /* physical cores this layout serves    */
    const uint8_t *core_slot_map; /* core_slots remap or NULL = identity  */
    const pm_field_t *fields;
    size_t        field_count;
    const char   *note;           /* provenance / restriction notes       */
} pm_layout_t;

/* Registry access. Returns NULL when the exact version has no layout —
 * callers MUST then report unsupported telemetry, never fall back. */
const pm_layout_t *pm_layout_lookup(uint32_t version);
int                pm_layout_count(void);
const pm_layout_t *pm_layout_at(int i);

/* Display name for a metric id (debug tables). */
const char *pm_metric_name(uint16_t metric);
/* Short unit suffix for a pm_unit_t. */
const char *pm_unit_name(uint8_t unit);
/* Confidence label text. */
const char *pm_confidence_name(uint8_t c);

#endif /* PM_LAYOUTS_H */
