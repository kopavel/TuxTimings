#ifndef BACKEND_H
#define BACKEND_H

#include "types.h"
#include "spd_decode.h"

/* Returns true if the ryzen_smu driver is loaded and accessible. */
int backend_is_supported(void);

/* Read all system data into out. Call every ~1 second for live refresh.
 * Static data (dmidecode, AGESA) is cached after first call. */
void backend_read_summary(system_summary_t *out);

/* Unload any kernel modules that were loaded by backend_read_summary().
 * Call once on application exit. */
void backend_cleanup(void);

/* Returns a malloc'd string with a raw PM table + AOD sysfs debug dump.
 * Caller must free(). Returns NULL on failure. */
char *backend_read_debug_dump(void);

/* Live decoded PM table, including unverified fields; caller frees. */
char *backend_read_pm_debug(void);

/* ── SPD (JESD400-5) profile access ─────────────────────────────────────
 * Enumerates DDR5 SPD EEPROMs exposed by the kernel (spd5118/at24
 * sysfs `eeprom` files under /sys/bus/i2c/devices/), decodes each and
 * pairs it to a dmidecode module by serial number (module_idx). Static
 * data — loaded and cached on first call. */
typedef struct {
    char         source[256];   /* sysfs eeprom path                   */
    char         slot_label[24];/* display name (dmidecode slot or i2c addr) */
    int          module_idx;    /* index into summary.modules, else -1 */
    int          image_len;     /* raw bytes read                      */
    bool         decode_ok;     /* image accepted (DDR5, big enough)   */
    spd_info_t   info;          /* decoded content (valid if decode_ok)*/
} backend_spd_entry_t;

/* Load/cached enumeration of live SPD EEPROMs. Returns count (may be 0). */
int backend_spd_load(void);
const backend_spd_entry_t *backend_spd_get(int i);   /* NULL if out of range */

#endif
