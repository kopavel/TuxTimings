#ifndef PM_TABLE_H
#define PM_TABLE_H

#include <stdarg.h>
#include "types.h"
#include "pm_layouts.h"

/* Read PM table binary and apply family-specific index mappings to fill metrics.
 * version:         PM table version from /sys/kernel/ryzen_smu_drv/pm_table_version
 * table:          array of floats from pm_table binary
 * count:          number of floats
 * physical_cores: number of physical cores (from topology); used to clamp per-core
 *                 reads so 12-core/6-core parts sharing tables with 16-core variants
 *                 don't read out-of-bounds PM table entries.
 */
void pm_table_read(uint32_t version, const float *table, int count,
                   int physical_cores, smu_metrics_t *out);

/* ── unified registry-driven interface (pm_layouts.c) ──────────────────
 * Layout selection is EXACT PM-table-version match; there is no fallback
 * layout — unknown versions leave every decoded field unavailable.
 * CANDIDATE and CONFLICTED registry fields are never written into `out`
 * (hence never shown in the GUI); they appear only in the debug emission.
 * core_refused (optional): 1 when per-core blocks were refused because the
 * live table disagreed with the layout's published fused-slot mapping.
 * Returns the selected layout, or NULL when the version is unsupported.
 */
const pm_layout_t *pm_table_decode(uint32_t version, const float *table, int count,
                                   int physical_cores, smu_metrics_t *out,
                                   int *core_refused);

/* DEBUG aid: human-readable decoded-telemetry table (layout header + every
 * registry field, candidates/conflicts clearly labelled) via callback. */
typedef void (*pm_out_fn)(void *ctx, const char *fmt, ...);
void pm_debug_emit(pm_out_fn out, void *ctx, uint32_t version,
                   const float *table, int count, int physical_cores);

void pm_debug_emit_unverified(pm_out_fn out, void *ctx, uint32_t version,
                              const float *table, int count, int physical_cores);

#endif
