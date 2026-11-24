/*
 * pm_table.c — unified AMD SMU PM table decoder (table-driven).
 *
 * Reads are pure functions over the float array handed in; all hardware
 * access stays in backend.c. Layout registry + provenance: pm_layouts.c,
 * docs/SMU_PM_TABLE_MAP.md.
 *
 * Rules enforced here:
 *  - exact PM version match, NEVER a guessed fallback;
 *  - bounds-check every scalar and array access;
 *  - NaN / ±Inf / implausible readings are never presented as valid;
 *  - CANDIDATE and CONFLICTED registry fields never reach smu_metrics_t
 *    (hence never the GUI); they exist only in the debug emission;
 *  - per-core slot lists are validated against the live table (fused
 *    down-binned parts) instead of assuming slot == Linux core.
 */

#include "pm_table.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ── plausibility ────────────────────────────────────────────────────── */

/* scalar sanity by metric; false → field stays unavailable (N/A, never 0) */
static bool scalar_ok(uint16_t m, float v)
{
    if (!isfinite(v)) return false;
    switch (m) {
    case PM_M_PPT_LIMIT: case PM_M_SOCKET_POWER: case PM_M_CPU_POWER:
    case PM_M_SOC_POWER:  case PM_M_VDDIO_POWER: case PM_M_VDD18_POWER:
    case PM_M_PPT_CURRENT:
        return v > 0.0f && v <= 5000.0f;
    case PM_M_TDC_LIMIT:  case PM_M_TDC_CURRENT:
    case PM_M_EDC_LIMIT:  case PM_M_EDC_CURRENT:
    case PM_M_EDC_CURRENT_CAND:
    case PM_M_SVI3_VDD_CURR: case PM_M_SVI3_SOC_CURR: case PM_M_SVI3_MISC_CURR:
        return v > 0.0f && v <= 1024.0f;
    case PM_M_THM_LIMIT:
        return v >= 20.0f && v <= 125.0f;
    case PM_M_TCTL: case PM_M_IOD_HOTSPOT:
    case PM_M_SVI3_VDD_TEMP: case PM_M_SVI3_SOC_TEMP:
        return v > 0.0f && v <= 125.0f;
    case PM_M_VID_AGG: case PM_M_VID_LIMIT:
        return v >= 0.2f && v <= 2.0f;
    case PM_M_VCORE: case PM_M_VSOC: case PM_M_VDDP: case PM_M_VDDG_IOD:
    case PM_M_VDDG_CCD: case PM_M_VDD_MISC:
    case PM_M_SVI3_VDD_VID: case PM_M_SVI3_VDD_VOLT:
    case PM_M_SVI3_SOC_VID: case PM_M_SVI3_SOC_VOLT:
    case PM_M_SVI3_MISC_VID: case PM_M_SVI3_MISC_VOLT:
    case PM_M_GFX_VOLT:
        return v >= 0.2f && v <= 4.0f;
    case PM_M_FCLK: case PM_M_UCLK: case PM_M_MCLK:
    case PM_M_GFX_CLK: case PM_M_GFX_TEMP: case PM_M_GFX_POWER:
    case PM_M_SOC_TEMP:
        return v > 0.0f && v <= 16000.0f;
    case PM_M_FIT_METRIC:
        return v > 0.0f && v <= 10.0f;
    default:
        return v > 0.0f;
    }
}

/* per-lane sanity for array blocks (thresholds per ZenMaster's documented
 * inactive/fused filtering: p<500 W, v<2 V, t<130 C, clk<10 GHz) */
static bool lane_ok(uint16_t m, float v)
{
    if (!isfinite(v)) return false;
    switch (m) {
    case PM_M_CORE_POWER:   return v >= 0.0f && v < 500.0f;
    case PM_M_CORE_VOLTAGE: return v >= 0.0f && v < 2.0f;
    case PM_M_CORE_TEMP:    return v >= 0.0f && v < 130.0f;
    case PM_M_CORE_CLK: case PM_M_CORE_BOOST_LIM:
        return v >= 0.0f && v < 10.0f;
    case PM_M_CORE_EFF_CLK: return v >= 0.0f && v < 10.0f;
    case PM_M_CORE_C0: case PM_M_CORE_CC1: case PM_M_CORE_CC6:
        return v >= 0.0f && v <= 100.0f;
    case PM_M_CCD_L3_TEMP: case PM_M_CCD_SHARED_TEMP:
        return v > 0.0f && v < 130.0f;
    default: return v > 0.0f;
    }
}

#define NANV (0.0f / 0.0f)

static float slot_lane(const float *t, int count, int idx)
{
    if (idx < 0 || idx >= count) return NANV;
    return t[idx];
}

/* ── per-core slot liveness ──────────────────────────────────────────── */

typedef struct {
    int present[2 * MAX_CORES];    /* per SMU slot: 1 = core present        */
    int linux_core[2 * MAX_CORES]; /* slot → Linux core idx, -1 = absent    */
    int slots;                     /* number of SMU slots inspected         */
    int cores;                     /* number of present cores               */
} core_map_t;

static const pm_field_t *find_field(const pm_layout_t *L, uint16_t m)
{
    for (size_t i = 0; i < L->field_count; i++)
        if (L->fields[i].metric == m)
            return &L->fields[i];
    return NULL;
}

/*
 * Build the slot → core map.
 *  - With an explicit slot map (5600X-style down-binned parts): verify the
 *    live table matches — fused slots must read exactly 0.0 in the
 *    power/voltage/clock blocks and mapped slots non-zero (gnr-smu's
 *    empirically stable invariant). Mismatch → refuse per-core entirely.
 *  - Without one: a slot counts as present unless any decoded liveness
 *    block (power/voltage/clock) reads exactly 0 there.
 * Never equates SMU slots with logical CPU numbers: Linux cores are
 * numbered by ascending present-slot order (validated on 5600X by gnr-smu).
 */
static bool build_core_map(const pm_layout_t *L, const float *t, int count,
                           int physical_cores, core_map_t *cm)
{
    int slots = L->core_slots;
    if (slots > 2 * MAX_CORES) slots = 2 * MAX_CORES;
    if (slots <= 0) slots = L->max_cores;
    cm->slots = slots;
    cm->cores = 0;
    int any_live = 0;

    const pm_field_t *f_p = find_field(L, PM_M_CORE_POWER);
    const pm_field_t *f_v = find_field(L, PM_M_CORE_VOLTAGE);
    const pm_field_t *f_c = find_field(L, PM_M_CORE_CLK);

    for (int s = 0; s < slots; s++) {
        int in_map = 1;
        if (L->core_slot_map) {
            in_map = 0;
            for (int i = 0; i < slots; i++)
                if (L->core_slot_map[i] == (uint8_t)s) { in_map = 1; break; }
        }
        float p = f_p ? slot_lane(t, count, f_p->index + s * f_p->stride) : NANV;
        float v = f_v ? slot_lane(t, count, f_v->index + s * f_v->stride) : NANV;
        float c = f_c ? slot_lane(t, count, f_c->index + s * f_c->stride) : NANV;

        if (L->core_slot_map) {
            if (in_map) {
                if ((f_p && !(isfinite(p) && p != 0.0f))) return false;
                if ((f_v && !(isfinite(v) && v != 0.0f))) return false;
                if ((f_c && !(isfinite(c) && c != 0.0f))) return false;
            } else {
                if ((f_p && isfinite(p) && p != 0.0f))  return false;
                if ((f_v && isfinite(v) && v != 0.0f))  return false;
                if ((f_c && isfinite(c) && c != 0.0f))  return false;
            }
        }
        int present = in_map;
        if (present && !L->core_slot_map) {
            if (f_p && isfinite(p) && p == 0.0f) present = 0;
            if (f_v && isfinite(v) && v == 0.0f) present = 0;
            if (f_c && isfinite(c) && c == 0.0f) present = 0;
        }
        cm->present[s] = present;
        if (present) {
            if ((f_p && isfinite(p) && p != 0.0f) ||
                (f_v && isfinite(v) && v != 0.0f) ||
                (f_c && isfinite(c) && c != 0.0f))
                any_live = 1;
            cm->linux_core[s] = cm->cores++;
        } else {
            cm->linux_core[s] = -1;
        }
    }
    if (physical_cores > 0 && cm->cores > physical_cores)
        cm->cores = physical_cores;
    /* No liveness signal at all (e.g. truncated table → all-NaN lanes):
     * refuse per-core rather than inventing present cores. */
    if (!any_live) return false;
    return true;
}

/* ── scalar dispatch ─────────────────────────────────────────────────── */

static void apply_scalar(smu_metrics_t *m, uint16_t metric, float v)
{
    switch (metric) {
    case PM_M_PPT_LIMIT:    m->ppt_limit_w = v;      m->has_ppt_limit = true; break;
    case PM_M_PPT_CURRENT:  m->ppt_w = v;                                    break;
    case PM_M_TDC_LIMIT:    m->tdc_limit_a = v;      m->has_tdc_limit = true; break;
    case PM_M_TDC_CURRENT:  m->tdc_current_a = v;    m->has_tdc_current = true; break;
    case PM_M_EDC_LIMIT:    m->edc_limit_a = v;      m->has_edc_limit = true; break;
    case PM_M_THM_LIMIT:    m->thermal_limit_c = v;  m->has_thermal_limit = true; break;
    case PM_M_TCTL:
        if (!m->has_tctl) { m->tctl_c = v; m->has_tctl = true; }
        if (m->cpu_temp_c <= 0.0f) m->cpu_temp_c = v;
        break;
    case PM_M_SOCKET_POWER:
        m->package_power_w = v; m->has_socket_power = true; break;
    case PM_M_VCORE:        m->vcore = v;                                    break;
    case PM_M_VSOC:         m->vsoc = v;                                     break;
    case PM_M_VDDP:         m->vddp = v;                                     break;
    case PM_M_VDDG_IOD:     m->vddg_iod = v;                                 break;
    case PM_M_VDDG_CCD:     m->vddg_ccd = v;                                 break;
    case PM_M_VDD_MISC:     m->vdd_misc = v;                                 break;
    case PM_M_FCLK:         m->fclk_mhz = v;                                 break;
    case PM_M_UCLK:         m->uclk_mhz = v;                                 break;
    case PM_M_MCLK:         m->mclk_mhz = v;                                 break;
    case PM_M_VID_AGG:      m->vid = v;                                      break;
    case PM_M_IOD_HOTSPOT:
        m->iod_hotspot_c = v; m->has_iod_hotspot = true; break;
    default: break; /* SVI3 rails, powers, FIT etc.: debug-table only */
    }
}

/* Derive aggregate core clock in MHz from per-core clocks in GHz */
static void compute_core_clock_from_clocks(smu_metrics_t *m)
{
    if (m->core_clocks_count <= 0) return;
    float max_ghz = 0.0f;
    for (int i = 0; i < m->core_clocks_count; i++)
        if (m->core_clocks_ghz[i] > max_ghz)
            max_ghz = m->core_clocks_ghz[i];
    if (max_ghz >= 0.5f && max_ghz <= 6.5f)
        m->core_clock_mhz = max_ghz * 1000.0f;
}

/* ── main decode ─────────────────────────────────────────────────────── */

const pm_layout_t *pm_table_decode(uint32_t version, const float *table, int count,
                                   int physical_cores, smu_metrics_t *out,
                                   int *core_refused)
{
    if (core_refused) *core_refused = 0;
    memset(out, 0, sizeof(*out));

    const pm_layout_t *L = pm_layout_lookup(version);
    if (!L || !table || count < 1)
        return NULL;

    core_map_t cm = { 0 };
    bool core_map_built = false;

    for (size_t i = 0; i < L->field_count && !core_map_built; i++) {
        const pm_field_t *f = &L->fields[i];
        if (f->count > 1 &&
            f->confidence != PM_CANDIDATE && f->confidence != PM_CONFLICTED)
            core_map_built = build_core_map(L, table, count, physical_cores, &cm);
    }
    if (!core_map_built) {
        for (size_t i = 0; i < L->field_count; i++) {
            const pm_field_t *f = &L->fields[i];
            if (f->count > 1 &&
                f->confidence != PM_CANDIDATE && f->confidence != PM_CONFLICTED) {
                if (core_refused) *core_refused = 1;
                break;
            }
        }
    }

    for (size_t i = 0; i < L->field_count; i++) {
        const pm_field_t *f = &L->fields[i];

        /* candidates/conflicted: never in metrics, debug table only */
        if (f->confidence == PM_CANDIDATE || f->confidence == PM_CONFLICTED)
            continue;

        if (f->count == 1) {
            if (f->index >= (uint32_t)count) continue;      /* bounds */
            float v = table[f->index] * f->scale;
            if (scalar_ok(f->metric, v))
                apply_scalar(out, f->metric, v);
            continue;
        }

        if (!core_map_built) continue;   /* per-core refused → stay N/A */

        float *dst = NULL;
        int *dst_count = NULL;
        switch (f->metric) {
        case PM_M_CORE_POWER:   dst = out->core_power_w;    dst_count = &out->core_power_count; break;
        case PM_M_CORE_VOLTAGE: dst = out->core_voltages;   dst_count = &out->core_voltages_count; break;
        case PM_M_CORE_TEMP:    dst = out->core_temps_c;    dst_count = &out->core_temps_count; break;
        case PM_M_CORE_CLK:     dst = out->core_clocks_ghz; dst_count = &out->core_clocks_count; break;
        case PM_M_CORE_C0:      dst = out->core_c0_pct;     dst_count = &out->core_res_count; break;
        case PM_M_CORE_CC1:     dst = out->core_cc1_pct;    dst_count = &out->core_res_count; break;
        case PM_M_CORE_CC6:     dst = out->core_cc6_pct;    dst_count = &out->core_res_count; break;
        default: continue; /* array metric not surfaced (EFF_CLK: debug) */
        }

        int lanes = (int)f->count;
        if (lanes > cm.slots) lanes = cm.slots;
        int filled = 0;
        for (int s = 0; s < lanes; s++) {
            int core = cm.linux_core[s];
            if (core < 0 || core >= MAX_CORES) continue;
            if (physical_cores > 0 && core >= physical_cores) continue;
            float v = slot_lane(table, count, f->index + s * f->stride) * f->scale;
            if (!cm.present[s] || !lane_ok(f->metric, v))
                v = 0.0f;             /* absent/invalid → 0, UI renders N/A */
            dst[core] = v;
            if (core + 1 > filled) filled = core + 1;
        }
        if (filled > cm.cores && cm.cores > 0) filled = cm.cores;
        if (*dst_count < filled) *dst_count = filled;
    }

    compute_core_clock_from_clocks(out);
    return L;
}

void pm_table_read(uint32_t version, const float *table, int count,
                   int physical_cores, smu_metrics_t *out)
{
    pm_table_decode(version, table, count, physical_cores, out, NULL);
}

/* ── debug emission ──────────────────────────────────────────────────── */

static void fmt_scalar(pm_out_fn out, void *ctx, const pm_field_t *f,
                       const float *t, int count)
{
    char val[48] = "N/A";
    if (f->index < (uint32_t)count) {
        float v = t[f->index] * f->scale;
        /* candidate/conflicted rows show any finite value: the point of the
         * debug table is the raw evidence, labelled as unproven */
        bool show = (f->confidence == PM_CANDIDATE ||
                     f->confidence == PM_CONFLICTED)
                        ? isfinite(v) : scalar_ok(f->metric, v);
        if (show)
            snprintf(val, sizeof(val), "%.3f %s", (double)v,
                     pm_unit_name(f->unit));
    }
    out(ctx, "%-22s %5u  0x%04X  %12s  %-10s",
        pm_metric_name(f->metric), f->index, f->index * 4u,
        val, pm_confidence_name(f->confidence));
    if (f->conflict)
        out(ctx, "   <conflict: %s>", f->conflict);
    out(ctx, "\n");
}

static void fmt_array(pm_out_fn out, void *ctx, const pm_field_t *f,
                      const float *t, int count)
{
    int lanes = f->count;
    if (lanes > 2 * MAX_CORES) lanes = 2 * MAX_CORES;
    out(ctx, "%-22s %5u  0x%04X  %2d lanes   %-10s",
        pm_metric_name(f->metric), f->index, f->index * 4u, lanes,
        pm_confidence_name(f->confidence));
    if (f->conflict)
        out(ctx, "   <%s>", f->conflict);
    out(ctx, "\n");

    char line[512];
    int off = 0;
    line[0] = '\0';
    for (int s = 0; s < lanes; s++) {
        int idx = (int)f->index + s * f->stride;
        float v = idx < count ? t[idx] * f->scale : NANV;
        bool ok = lane_ok(f->metric, v);
        off += snprintf(line + off, sizeof(line) - (size_t)off,
                        " [%d]%s%.3f", idx, ok ? "=" : "!", (double)v);
        if ((s & 7) == 7 || s == lanes - 1) {
            out(ctx, "  %s\n", line);
            off = 0; line[0] = '\0';
        }
    }
}

void pm_debug_emit(pm_out_fn out, void *ctx, uint32_t version,
                   const float *table, int count, int physical_cores)
{
    const pm_layout_t *L = pm_layout_lookup(version);
    out(ctx, "=== SMU PM Table Decoded ===\n");
    if (!L) {
        out(ctx, "PM Table: 0x%08X\nStatus: UNSUPPORTED - no published layout "
                 "for this version; decoded telemetry suppressed (no fallback "
                 "by design). Raw dump below.\n\n", version);
        return;
    }
    out(ctx, "Layout: %s (%s)\n", L->name, L->family);
    out(ctx, "PM Table: 0x%08X\n", L->version);
    out(ctx, "Size: %u bytes expected\n", L->size_bytes);

    int refused = 0;
    if (table && count > 0) {
        smu_metrics_t probe;
        pm_table_decode(version, table, count, physical_cores, &probe, &refused);
    }
    out(ctx, "Status: supported%s\n",
        refused ? " (per-core blocks REFUSED: live table disagrees with the "
                  "published fused-slot layout)" : "");
    if (L->note)
        out(ctx, "Note: %s\n", L->note);
    out(ctx, "\n%-22s %5s  %6s  %12s  %-10s\n",
        "Metric", "Index", "Offset", "Value", "Confidence");
    out(ctx, "%-22s %5s  %6s  %12s  %-10s\n",
        "------", "-----", "------", "-----", "----------");
    if (!table || count <= 0) {
        out(ctx, "(table not available)\n\n");
        return;
    }
    for (size_t i = 0; i < L->field_count; i++) {
        const pm_field_t *f = &L->fields[i];
        if (f->count == 1)
            fmt_scalar(out, ctx, f, table, count);
        else
            fmt_array(out, ctx, f, table, count);
    }
    out(ctx, "\n");
}
