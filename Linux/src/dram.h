#ifndef DRAM_H
#define DRAM_H

#include "types.h"
#include "dram_decode.h"

/*
 * SMN access + per-UMC-channel enumeration for DRAM reporting.
 * The UMC register block of channel i is read at SMN offset (i << 20);
 * presence/absence is decided from the hardware registers themselves
 * (see dram_decode.c). Hardware access lives here, decoding lives there.
 */

/* Original single-channel entry (UMC0), author's API — kept. */
void dram_read_timings(int codename_index, dram_timings_t *out);

/* Open/close the SMN interface (idempotent). Returns 1 on success. */
int  dram_smn_open(void);
void dram_smn_close(void);

/*
 * Probe UMC0..UMC7 and decode every channel (present flag distinguishes
 * populated ones).
 * mem_speed_hint_mhz: effective MT/s from an authoritative source (PM table),
 *   0 to derive it from the UMC ratio register.
 * bclk_mhz: base clock (0 -> default 100 MHz).
 * Fills chans[0..MAX_UMC_CHANNELS-1] and *chan_count = probed count.
 */
void dram_read_channels(bool is_ddr5, float mem_speed_hint_mhz, float bclk_mhz,
                        dram_channel_t *chans, int *chan_count);

/* Recompute ns conversions with an authoritative MT/s value. */
void dram_rescale_ns(dram_channel_t *chans, int chan_count, float mtaps);

#endif /* DRAM_H */
