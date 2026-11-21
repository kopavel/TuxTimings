# Third-party provenance of ported decoding logic

This document records the origin of algorithms ported into TuxTimings'
DDR5 SPD decoding, as required by the applicable licenses. The combined
work is distributed under **GPL-3.0-or-later**; the full license texts of
all sources are retained in this directory.

## `Linux/src/spd_decode.c` / `Linux/src/spd_decode.h`

Ported to native C (no Rust, no C++ runtime — plain C11) from two
reference implementations:

### 1. `spdr` — JESD400-5 DDR5 SPD decoder (Rust)

- Upstream: https://github.com/The-Open-Memory-Initiative-OMI/spdr (crate `spdr`)
- Copyright: MERT EFE ŞENSOY <sensoymertefe@gmail.com>
  (contributor: Dan Alec)
- License: **Apache License 2.0** — full text retained at
  `Linux/licenses/LICENSE-APACHE` (satisfies Apache-2.0 §4(a),(c),(d)).
- Ported content (semantics, field offsets and encodings, reimplemented
  in C, unit-pinned in `Linux/tests/test_spd.c` against spdr's golden
  fixture values):
  - `crc.rs`  → `spd_crc16()` (CRC-16/XMODEM, check vector 0x31C3), base
    configuration CRC (bytes 0–509, stored LE at 510).
  - `identity.rs` → identity/base-config block (bytes 0–7, 234, 235) and
    all enum tables (module type, density, package/die-count, widths,
    bank groups).
  - `timing.rs` → base JEDEC timing block (0x12–0x5D), CL bit-map
    (CL = 20 + 2·bit), tRFC ns→ps normalisation, `data_rate_mt_s()`
    trunc-then-round-to-100 rule.
  - `module.rs` → unbuffered (UDIMM) module-specific block (230–233),
    reference-raw-card letter alphabet `ABCDEFGHJKLMNPRTUVWY`.
  - `manufacturing.rs` → manufacturing block (512–554), JEP-106 decode
    (parity stripped, bank = continuation+1, embedded name table sourced
    by spdr from the freeipmi JEDEC list), BCD date, big-endian serial.
  - `vendor.rs` → XMP 3.0 / EXPO section location, magics, per-block CRC
    ranges, voltage byte encoding, rated-CL round-to-nearest-in-u64 with
    saturating narrowing, profile-name trim/printability rules.
  - Capacity formula from `spdr/docs/numerical-claims.md` (Phase 8,
    attributed upstream to memtest86plus `parse_spd_ddr5`).
- Deliberate deviations from spdr (documented in code): unknown enum
  values do not abort the decode — the section flags itself unavailable
  so the UI renders N/A (never 0); profile presence combines spdr's
  tCK≠0 gate with DDR5SPDEditor's enable-bit gate.

### 2. `DDR5SPDEditor` — DDR5 SPD/XMP/EXPO editor (C++/Qt)

- Upstream: https://github.com/edlf/DDR5SPDEditor
- Copyright: edlf (Eduardo Fernandes), GPL-3.0-or-later
  (upstream carries no per-file copyright headers; attribution taken from
  the repository LICENSE and git history).
- License: **GPL-3.0-or-later** — full text retained at
  `Linux/licenses/LICENSE-GPL-3.0`.
- Ported content (field maps and conversions from `ddr5spd_structs.h`,
  `ddr5spd.cpp`, `xmp3.cpp`, `expo.cpp`, `utilities.cpp`):
  - Full `XMP_ProfileStruct` map (64-byte blocks at 0x2C0/0x300/0x340):
    secondary timings tRC/tWR/tRFC1/tRFC2/tRFCsb, tRRD_L/tCCD_L/
    tCCD_L_WR/tCCD_L_WR2/tCCD_L_WTR/tCCD_S_WTR/tRTP/tFAW with their
    LCLK lower-limit bytes, vMemCtrl, boost/RTMC bits (0x3B), command
    rate (0x3C).
  - Full `EXPO_ProfileStruct` map (40-byte blocks at 0x34A/0x372,
    vdd-first voltage order — unlike XMP's vpp-first) and EXPO enable-bit
    semantics (byte +0x05, bit0/bit4).
  - XMP header layout: magic 0x0C 0x4A at 0x280, version byte (0x30 =
    XMP 3.0), enable byte 0x283, 3×16-byte profile names at 0x28E.
  - EXPO block at 0x340 overlaying XMP slots 3/user1 (hybrid mode).
  - `TimeToTicksDDR5()` nCK conversion with the JESD400-5B 0.3 %
    correction factor → `spd_ps_to_nck()`.
  - Base-block RFC slr/dlr field positions (0x2A–0x35).
- Deliberate deviations from DDR5SPDEditor (documented in code): its
  degenerate base-voltage getters (only 0x00 decodes) are replaced by
  the JEDEC bit-field decode with 0x00 mapped to the DDR5 default rails;
  its `gettCCD_L_WR_tick()` bug (computes tCCD_L ticks) was not carried
  over; its frequency truncation rule was superseded by spdr's
  round-to-nearest-100 rule (both agree on all vendor samples).

## `Linux/tests/test_spd.c`

Embeds (base64) the 1024-byte real-world SPD fixture
`teamgroup-ud5-6000_0104eef6.spd` shipped in the spdr repository
(SHA-256 `cecfa75eb704272ad5b135e77a534cc416aec55a8daea54823b5dbf6d7761c98`;
fixture originally from the public community dump set
`ubihazard/ddr5-spd-recovery`). All golden expectations in the test are
spdr's committed, validated decode values (snapshots + unit/integration
test assertions).

## License-compatibility note

Apache-2.0-derived code is relicensed under GPL-3.0-or-later for this
combined work (the FSF lists GPL-3.0 as a compatible later version for
Apache-2.0 code when the distributor chooses it); the required Apache-2.0
notices are preserved above and in the source headers of every file that
contains ported logic, and the unmodified Apache-2.0 text is shipped in
`Linux/licenses/`.
