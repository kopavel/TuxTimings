# AMD SMU PM Table — consolidated layout map

Generated view of the runtime registry in `Linux/src/pm_layouts.c`. That file
is the single source of truth for the decoder; this document mirrors it with
source citations and the full conflict record. Regenerate this doc from
`pm_layouts.c` when the registry changes (scripts/check-pm-map.py pattern:
parse `LAYOUT()`/`F0`/`FA` rows — keep both in sync).

## Methodology

- PM Table = contiguous float32-LE telemetry buffer DMA'd by the SMU into
  DRAM; fetched read-only via the `ryzen_smu` driver
  (`/sys/kernel/ryzen_smu_drv/pm_table{,_version}`). No SMU writes anywhere.
- **Normalization**: every index below is a float index;
  `byte_offset = float_index * 4`. The two are never mixed.
- Layout selection is EXACT version match; there is no family/size fallback.
  Unknown versions → all telemetry reported unavailable, raw dump preserved.
- Sources were cross-checked against each other; disagreements are recorded
  in the conflict registry below, never silently resolved. Where one reading
  was corroborated by ≥2 independent sources (or live measurement), it is
  used and the minority reading is noted.

## Sources

| Key | Repository / commit | License | Used for |
|---|---|---|---|
| ZS | ZenStates-Core `869b7e8257` `Hardware/PowerTable.cs` | GPL-3.0 | per-version sizes, clock/rail/SVI3 byte offsets |
| GN | gnr-smu `def79d66d5` `gnr_smu/profiles.py`, `docs/` | MIT | live-validated Granite Ridge (0x6201xx/0x6202xx) + Vermeer 0x380905, confidence semantics, fused-slot methodology |
| ZM | ZenMaster wiki “PM Table and Telemetry” | (docs) | per-arch per-core base offsets, size matrix, fused-core thresholds |
| LHM | LibreHardwareMonitor `9e2d677cef` `RyzenSMU.cs`, `Cpu/Amd17Cpu.cs` | MPL-2.0 | Raven/Matisse/Vermeer sensor indexes; 0x621202 per-core bases |
| RA | RyzenAdj `44438e1ba2` | LGPL-3.0 | cross-checks (Strix clocks, Van Gogh size) |
| TX | TuxTimings author map (`docs/PM_TABLE_INDICES.md`, git history) | GPL-3.0 | legacy zone-0 fields; live 9950X-verified Granite rows |
| LV | live DEBUG dumps on this machine (9950X, PM `0x00620205`) | — | confirmation of TX rows 2/3/8/9/10/63/71/75/79/259/261/269/271/301/317/333/349 |

## Confidence levels

- **confirmed** — live-validated on real hardware and/or cross-corroborated
  by an independent source with coherent behaviour. Shown in the GUI.
- **documented** — published for exactly this table version by a reference
  implementation, not independently re-validated. Shown in the GUI.
- **CANDIDATE** — plausible only; never in the GUI, debug output labels it.
- **CONFLICTED** — sources disagree; never in the GUI, the debug output
  names the clash.

## GUI-visible vs debug-only rules

Candidates and conflicted fields never reach `smu_metrics_t` (see
`pm_table_decode()`). Notably: current-EDC (index 64 on Granite, per GN),
the per-core clock block on dual-CCD Granite (349 — see conflict 8), CCD
L3-temperature lanes, per-core residency on 0x620105, vsoc on
Vermeer/Cezanne/Renoir/Phoenix, uclk/mclk order on Matisse and
0x540104, `vddp` on 0x620105, and the author's legacy zone-0 “PPT” on Zen 2/3
(where GN's RAPL-corroborated “socket power” occupies the same index).

## Supported layouts (85 versions)

| PM version | Size (B) | Layout | Family | Core slots/cores | Coverage |
|---|---|---|---|---|---|
| `0x001E0001` | 1392 | Raven Ridge APU (earliest) | Raven/Picasso | 4/4 | package/clocks |
| `0x001E0002` | 1408 | Raven Ridge APU (early) | Raven/Picasso | 4/4 | package/clocks |
| `0x001E0003` | 1544 | Picasso APU (3200G/3400G) | Raven/Picasso | 4/4 | package/clocks |
| `0x001E0004` | 1544 | Raven Ridge (2200G-2700U) | Raven/Picasso | 4/4 | package+per-core |
| `0x001E0005` | 1544 | Picasso APU refresh | Raven/Picasso | 4/4 | package/clocks |
| `0x001E000A` | 1544 | Picasso Embedded APU | Raven/Picasso | 4/4 | package/clocks |
| `0x001E0101` | 1544 | Picasso revision | Raven/Picasso | 4/4 | package/clocks |
| `0x00240003` | 6316 | Castle Peak / Matisse multi-CCD | Matisse | 8/16 | package/clocks |
| `0x00240503` | 3452 | Castle Peak / Matisse | Matisse | 8/16 | package+per-core |
| `0x00240603` | 2736 | Castle Peak / Matisse | Matisse | 8/16 | package+per-core |
| `0x00240703` | 2020 | Castle Peak / Matisse | Matisse | 8/16 | package+per-core |
| `0x00240802` | 2016 | Matisse 02-rev | Matisse | 8/16 | package/clocks |
| `0x00240803` | 2020 | Matisse Dual-CCD (3900X/3950X) | Matisse | 8/16 | package+per-core |
| `0x00240902` | 1300 | Matisse single-CCD 02-rev | Matisse | 8/8 | package/clocks |
| `0x00240903` | 1304 | Matisse Single-CCD (3600/3700X/3800X) | Matisse | 8/8 | package+per-core |
| `0x002D0008` | 6832 | Milan / Chagall multi-CCD | Milan | 8/16 | package/clocks |
| `0x002D0803` | 2196 | Milan / Chagall | Milan | 8/16 | package/clocks |
| `0x002D0903` | 2020 | Milan / Chagall single-CCD | Milan | 8/8 | package/clocks |
| `0x00370000` | 2180 | Renoir 00-rev | Renoir | 8/8 | package/clocks |
| `0x00370001` | 2188 | Renoir 01-rev | Renoir | 8/8 | package/clocks |
| `0x00370002` | 2220 | Renoir 02-rev | Renoir | 8/8 | package/clocks |
| `0x00370003` | 2248 | Renoir / Lucienne | Renoir | 8/8 | package+per-core |
| `0x00370004` | 2248 | Renoir 04-rev | Renoir | 8/8 | package+per-core |
| `0x00370005` | 2248 | Renoir v2 APU | Renoir | 8/8 | package+per-core |
| `0x00380005` | 7088 | Chagall / Vermeer multi-CCD | Vermeer | 8/16 | package/clocks |
| `0x00380505` | 3888 | Vermeer / Chagall | Vermeer | 8/16 | package/clocks |
| `0x00380605` | 3088 | Vermeer / Chagall | Vermeer | 8/16 | package/clocks |
| `0x00380705` | 2288 | Vermeer / Chagall | Vermeer | 8/16 | package/clocks |
| `0x00380804` | 2212 | Vermeer Dual-CCD (older BIOS) | Vermeer | 8/16 | package+per-core |
| `0x00380805` | 2288 | Vermeer Dual-CCD (newer BIOS) | Vermeer | 8/16 | package+per-core |
| `0x00380904` | 1444 | Vermeer Single-CCD (older BIOS) | Vermeer | 8/8 | package+per-core |
| `0x00380905` | 1488 | Vermeer Single-CCD (newer BIOS) | Vermeer | 8/6 (slotmap) | package+per-core |
| `0x003F0000` | 1964 | Van Gogh (Steam Deck) | Van Gogh | 2/2 | package+per-core |
| `0x00400001` | 2320 | Cezanne 01-rev | Cezanne | 8/8 | package/clocks |
| `0x00400002` | 2344 | Cezanne 02-rev | Cezanne | 8/8 | package/clocks |
| `0x00400003` | 2380 | Cezanne 03-rev | Cezanne | 8/8 | package/clocks |
| `0x00400004` | 2372 | Cezanne 04-rev | Cezanne | 8/8 | package+per-core |
| `0x00400005` | 2372 | Cezanne / Barcelo | Cezanne | 8/8 | package+per-core |
| `0x00450004` | 2724 | Rembrandt | Rembrandt | 8/8 | package+per-core |
| `0x00450005` | 2736 | Rembrandt 05-rev | Rembrandt | 8/8 | package+per-core |
| `0x004C0001` | 2840 | Phoenix 01 (ZS: placeholder, unverified) | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0003` | 2840 | Phoenix Point | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0004` | 2844 | Phoenix Point | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0005` | 2808 | Phoenix Point | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0006` | 2812 | Phoenix Point | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0007` | 2816 | Phoenix Point | Hawk/Phoenix | 8/8 | package/clocks |
| `0x004C0008` | 2800 | Phoenix Point desktop | Hawk/Phoenix | 8/8 | package+per-core |
| `0x004C0009` | 2816 | Hawk Point (Phoenix2 desktop) | Hawk/Phoenix | 8/8 | package+per-core |
| `0x00540000` | 2088 | Raphael Dual-CCD (ES/early) | Raphael | 16/16 | package+per-core |
| `0x00540001` | 2092 | Raphael Dual-CCD | Raphael | 16/16 | package+per-core |
| `0x00540002` | 2172 | Raphael Dual-CCD | Raphael | 16/16 | package+per-core |
| `0x00540003` | 2204 | Raphael Dual-CCD | Raphael | 16/16 | package+per-core |
| `0x00540004` | 2236 | Raphael Dual-CCD | Raphael | 16/16 | package+per-core |
| `0x00540005` | 2248 | Raphael Dual-CCD | Raphael | 16/16 | package+per-core |
| `0x00540100` | 1560 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540101` | 1564 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540102` | 1644 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540103` | 1676 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540104` | 1704 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540105` | 1716 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540108` | 1724 | Raphael Single-CCD / Dragon Range | Raphael | 8/8 | package+per-core |
| `0x00540208` | 2256 | Raphael Dual-CCD rev | Raphael | 16/16 | package/clocks |
| `0x005C0002` | 7740 | Storm Peak 12-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0003` | 7752 | Storm Peak 12-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0102` | 6676 | Storm Peak 8-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0103` | 6688 | Storm Peak 8-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0202` | 5612 | Storm Peak 6-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0203` | 5624 | Storm Peak 6-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0302` | 3484 | Storm Peak 4-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0303` | 3496 | Storm Peak 4-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0402` | 2420 | Storm Peak 2-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005C0403` | 2432 | Storm Peak 2-CCD | Storm Peak | 8/16 | package/clocks |
| `0x005D0008` | 3412 | Strix Point 12C | Strix Point | 12/12 | package+per-core |
| `0x005D0009` | 3416 | Strix Point 12C rev | Strix Point | 12/12 | package+per-core |
| `0x005D000A` | 3424 | Strix Point rev A | Strix Point | 12/12 | package+per-core |
| `0x005D000B` | 3424 | Strix Point rev B | Strix Point | 12/12 | package+per-core |
| `0x00620105` | 1828 | Granite Ridge Single-CCD (X3D) | Granite Ridge | 8/8 | package+per-core |
| `0x00620205` | 2452 | Granite Ridge Dual-CCD | Granite Ridge | 16/16 | package+per-core |
| `0x00621102` | 1828 | Granite Ridge Single-CCD (rev) | Granite Ridge | 8/8 | package/clocks |
| `0x00621202` | 2452 | Granite Ridge Dual-CCD (X3D rev) | Granite Ridge | 16/16 | package+per-core |
| `0x00650004` | 2932 | Krackan Point | Krackan Point | 8/8 | package/clocks |
| `0x00650005` | 2936 | Krackan Point | Krackan Point | 8/8 | package/clocks |
| `0x00650006` | 2944 | Krackan Point | Krackan Point | 8/8 | package/clocks |
| `0x00650007` | 2944 | Krackan Point | Krackan Point | 8/8 | package/clocks |
| `0x00650008` | 2944 | Krackan Point (SMU 101.72) | Krackan Point | 8/8 | package/clocks |

## Conflict registry (explicit — nothing silently chosen)

1. **Granite zone-0 pair semantics** — GN documents `(LIMIT, VALUE)` pairs at
   2/3 (PPT), 8/9 (TDC), 10/11 (thermal), 63/64 (EDC), corrected
   2026-07-30; older lore mislabeled 8/10. Adopted (live 9950X values agree).
2. **Index 64 “EDC current”** — GN: behavioural evidence only →
   CANDIDATE, debug-only.
3. **Index 11** — GN 9800X3D/9950X3D: Tctl (k10temp-matched, confirmed for
   0x6201xx/0x6202xx). TX author: “IOD hotspot” everywhere. LHM 0x1E0004:
   “EDC”. Vermeer fixture reads 1.09 at index 11 (a voltage, not °C).
   → Tctl confirmed on Granite; CONFLICTED everywhere else.
4. **Socket power** — TX used index 220 on Granite (321 W at idle — the
   plausibility failure that motivated this audit). GN: 26 on 0x620205
   (≈PPT current at idle), 20 on 0x620105, 1 on 0x380905 (RAPL-corroborated).
   220 dropped; GN indices used. On Vermeer both 1 and 29 track the same
   value in fixtures.
5. **vsoc** — TX 44/82/83/101/102 vs ZS VDDCR_SOC pointing into the SVI3
   measured-rail fields (Zen4/5: 52–54, 111) vs GN provisional (45 on Vermeer).
   Where ZS/SVI3 explains the index, both are mapped as distinct metrics;
   otherwise CONFLICTED (Vermeer 44-vs-45, Cezanne 102-vs-103, Renoir
   101-vs-102, Phoenix 101-vs-111, Matisse 44-vs-45, Raphael 82-vs-52).
6. **vddp** — TX 125 (Matisse) vs ZS 124; TX 527/534 (Renoir) vs ZS 529/543;
   TX 269 (0x540104/0x620105) vs ZS 268 / GN “vddcr_cpu_vid”. CONFLICTED
   where unresolved; documented where ZS+TX agree (0x540004, 0x620205, 400005).
7. **uclk/mclk order** — Matisse: ZS 49/50 vs TX+LHM 50/51 → resolved 2:1 to
   TX+LHM, ZS minority recorded on the field. Raphael single-CCD: ZS 74/78 vs
   TX 78/74 — indistinguishable at UCLK=MCLK 1:1 → CONFLICTED.
8. **Block at 349 on 0x620205/0x621202** — LHM Frequency=349 and ZM clock base
   0x574=349 vs GN `core_fit`. Live check on the 9950X (2026-11-24): decoded
   values disagree with the cpufreq-based per-core frequency readout →
   clock identity NOT established. Demoted to CONFLICTED: debug table only,
   excluded from `smu_metrics_t` and the GUI; the cpufreq blob is the single
   per-core frequency source. (GN's own 9800X3D profile *does* map 325 as
   frequency for single-CCD, so 0x620105/0x621102 keep 325 as documented.)
   Block 365: ZM “effective clock” vs GN “activity metric” → CANDIDATE.
9. **0x620105 residency lanes** — ZM publishes C0=357/CC1=365/CC6=341; GN
   load-validated C0=341/CC1=349/CC6=357. Unresolved → CONFLICTED (debug).
10. **Vermeer dual-CCD per-core volt/temp** — TX 185/201 (0x380804), 188/204
    (0x380805), 177/183 (0x380904) vs ZS `CoresPower` per revision (147/172/169)
    + ZM block order (volt=+8, temp=+16). Unresolved → CONFLICTED. Only
    0x380905 has a live-validated map (GN, 15-snapshot 5600X dataset).
11. **Matisse dual-CCD per-core** — TX 163/179 = ZM's TEMP base +0/+16;
    second-CCD block placement unpublished → CONFLICTED.
12. **Table sizes** — 0x1E0001: ZS 0x570 vs ZM 0x568; 0x370000-02: ZS rows
    shifted one revision vs ZM; 0x2D0903: ZS 0x7E4 vs ZM 0x594. ZS sizes used
    (they are what the reference decoder requests), conflicts recorded here.
13. **Picasso per-core (ZM)** — ZM temp base 0x624 exceeds 0x1E tables
    (0x608); treated as a ZM typo — Renoir-style per-core temp unmapped there,
    LHM's 108..111 used for 0x1E0004 instead.
14. **VID** — TX 275 (Zen4/5 “aggregate VID”) vs ZS SVI3 VDD VID 46/48.
    Different names/offsets; SVI3 mapped as its own metric; author 275 not
    carried into the new registry (unpublished for any source here).

## Unsupported (deliberately)

- **Zen 1/2 desktop (Summit/Pinnacle Ridge)** — no layout published for their
  exact versions; ZS master only offers *pseudo-version* generic fallbacks
  (0x100/0x101), which the no-fallback rule rejects. Raven/Picasso APUs (0x1E)
  are supported as published.
- **Strix Halo (0x640xxx)** — ZS lists versions+sizes but every offset is -1.
- **FireFlight (0x260001)** — ZS offsets (float index 4/10/11/12) implausible;
  excluded rather than mapped as garbage.
- **Threadripper server variants (Rome/Milan/Genoa/Turin/…), Storm Peak
  per-core** — versions known, but per-core geometry is not published;
  clocks/rails/SVI3 decoded only where ZS gives them.
- **STAPM / skin-temp limits, GFX on Renoir+, explicit throttle flags,
  boost-limit globals** — no published offsets in any listed source for the
  relevant tables; metric ids exist, no layout references them (unavailable,
  never guessed).
- Per-core blocks on down-binned parts: the fused-slot map is validated
  against the live table; a mismatch refuses per-core output (5600X fixture
  covered by unit tests; other SKUs need real dumps).

## Required real-hardware validation (promote documented → confirmed)

1. 0x620105 residency lane order (conflict #9) — pin-a-core load dataset.
2. 0x620205 index 64 = EDC current (load ramp vs d[9] correlation).
3. Vermeer 380804/380805 per-core volt/temp (conflict #10) — 5950X dataset.
4. Matisse dual-CCD per-core (conflict #11) — 3950X dataset.
5. Any 12-core (9900X/7900X/5900X) fused-slot pattern: confirm the
   present-slot-by-consensus heuristic keeps slots aligned with Linux cores.
6. Raphael vsoc/vddp family conflicts (#5/#6) — 7950X dataset with AOD and
   k10temp cross-reference.
7. Strix/Krackan/Rembrandt/Van Gogh — any owner with an `SMU Info` + PM raw
   dump (debug button) to corroborate ZS/ZM bases.

## Layout detail

### `0x001E0001` — Raven Ridge APU (earliest)
> size: ZS 0x570 vs ZM 0x568 — conflict recorded

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 280 | 0x460 | 1 | documented |  |
| UCLK | 281 | 0x464 | 1 | documented |  |
| MCLK | 282 | 0x468 | 1 | documented |  |
| VDDCR_SOC | 67 | 0x10C | 1 | documented |  |
| CLDO_VDDP | 62 | 0x0F8 | 1 | documented |  |

### `0x001E0002` — Raven Ridge APU (early)
> ZS clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 285 | 0x474 | 1 | documented |  |
| UCLK | 286 | 0x478 | 1 | documented |  |
| MCLK | 287 | 0x47C | 1 | documented |  |
| VDDCR_SOC | 67 | 0x10C | 1 | documented |  |
| CLDO_VDDP | 62 | 0x0F8 | 1 | documented |  |

### `0x001E0003` — Picasso APU (3200G/3400G)
> ZS clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 166 | 0x298 | 1 | documented |  |
| UCLK | 167 | 0x29C | 1 | documented |  |
| MCLK | 168 | 0x2A0 | 1 | documented |  |
| VDDCR_SOC | 65 | 0x104 | 1 | documented |  |
| CLDO_VDDP | 60 | 0x0F0 | 1 | documented |  |

### `0x001E0004` — Raven Ridge (2200G-2700U)
> author + LHM + ZS agree

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | LHM names 11 'EDC' current on 0x1E0004 |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VID (Aggregate) | 57 | 0x0E4 | 1 | documented |  |
| CLDO_VDDP | 60 | 0x0F0 | 1 | documented |  |
| VCORE | 61 | 0x0F4 | 1 | CONFLICTED | same index doubles as IOD temp in the author map |
| VDDCR_SOC | 65 | 0x104 | 1 | documented |  |
| Core Power | 96 | 0x180 | 4 | documented |  |
| Core Voltage | 104 | 0x1A0 | 4 | documented |  |
| Core Temp | 108 | 0x1B0 | 4 | documented |  |
| GFX Voltage | 150 | 0x258 | 1 | documented |  |
| GFX Temp | 151 | 0x25C | 1 | documented |  |
| GFX Clock | 154 | 0x268 | 1 | documented |  |
| FCLK | 166 | 0x298 | 1 | documented |  |
| UCLK | 167 | 0x29C | 1 | documented |  |
| MCLK | 168 | 0x2A0 | 1 | documented |  |

### `0x001E0005` — Picasso APU refresh
> ZS: same offsets as 0x1E0003

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 166 | 0x298 | 1 | documented |  |
| UCLK | 167 | 0x29C | 1 | documented |  |
| MCLK | 168 | 0x2A0 | 1 | documented |  |
| VDDCR_SOC | 65 | 0x104 | 1 | documented |  |
| CLDO_VDDP | 60 | 0x0F0 | 1 | documented |  |

### `0x001E000A` — Picasso Embedded APU
> ZS: same offsets as 0x1E0003

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 166 | 0x298 | 1 | documented |  |
| UCLK | 167 | 0x29C | 1 | documented |  |
| MCLK | 168 | 0x2A0 | 1 | documented |  |
| VDDCR_SOC | 65 | 0x104 | 1 | documented |  |
| CLDO_VDDP | 60 | 0x0F0 | 1 | documented |  |

### `0x001E0101` — Picasso revision
> ZS: same offsets as 0x1E0003

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 166 | 0x298 | 1 | documented |  |
| UCLK | 167 | 0x29C | 1 | documented |  |
| MCLK | 168 | 0x2A0 | 1 | documented |  |
| VDDCR_SOC | 65 | 0x104 | 1 | documented |  |
| CLDO_VDDP | 60 | 0x0F0 | 1 | documented |  |

### `0x00240003` — Castle Peak / Matisse multi-CCD
> clocks/rails only; multi-CCD per-core geometry unpublished

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 44 | 0x0B0 | 1 | documented |  |
| UCLK | 46 | 0x0B8 | 1 | documented |  |
| MCLK | 47 | 0x0BC | 1 | documented |  |
| VDDCR_SOC | 41 | 0x0A4 | 1 | documented |  |
| CLDO_VDDP | 121 | 0x1E4 | 1 | documented |  |
| CLDO_VDDG_IOD | 122 | 0x1E8 | 1 | documented |  |

### `0x00240503` — Castle Peak / Matisse
> ZS + ZM agree

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 124 | 0x1F0 | 1 | documented |  |
| CLDO_VDDG_IOD | 126 | 0x1F8 | 1 | documented |  |
| Core Power | 147 | 0x24C | 8 | documented |  |
| Core Voltage | 155 | 0x26C | 8 | documented |  |
| Core Temp | 163 | 0x28C | 8 | documented |  |
| Core Clock | 187 | 0x2EC | 8 | documented |  |

### `0x00240603` — Castle Peak / Matisse
> ZS + ZM agree

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 124 | 0x1F0 | 1 | documented |  |
| CLDO_VDDG_IOD | 126 | 0x1F8 | 1 | documented |  |
| Core Power | 147 | 0x24C | 8 | documented |  |
| Core Voltage | 155 | 0x26C | 8 | documented |  |
| Core Temp | 163 | 0x28C | 8 | documented |  |
| Core Clock | 187 | 0x2EC | 8 | documented |  |

### `0x00240703` — Castle Peak / Matisse
> ZS + ZM agree

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 124 | 0x1F0 | 1 | documented |  |
| CLDO_VDDG_IOD | 126 | 0x1F8 | 1 | documented |  |
| Core Power | 147 | 0x24C | 8 | documented |  |
| Core Voltage | 155 | 0x26C | 8 | documented |  |
| Core Temp | 163 | 0x28C | 8 | documented |  |
| Core Clock | 187 | 0x2EC | 8 | documented |  |

### `0x00240802` — Matisse 02-rev
> ZS only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 47 | 0x0BC | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | documented |  |
| CLDO_VDDP | 124 | 0x1F0 | 1 | documented |  |
| CLDO_VDDG_IOD | 125 | 0x1F4 | 1 | documented |  |

### `0x00240803` — Matisse Dual-CCD (3900X/3950X)
> per-core blocks 8 lanes/CCD — dual-CCD block stride not published

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 pair pattern (GN): 10/11 = thm/tctl |
| PPT Current | 1 | 0x004 | 1 | documented |  |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented | ZS says 49 (0xC4) — outvoted 2:1 |
| MCLK | 51 | 0x0CC | 1 | documented | ZS says 50 (0xC8) — outvoted 2:1 |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45 |
| CLDO_VDDP | 125 | 0x1F4 | 1 | CONFLICTED | ZS CLDO_VDDP=0x1F4/124 |
| CLDO_VDDG_IOD | 126 | 0x1F8 | 1 | documented |  |
| VCORE | 40 | 0x0A0 | 1 | documented |  |
| Core Power | 147 | 0x24C | 8 | documented |  |
| Core Voltage | 155 | 0x26C | 8 | CONFLICTED | author used 163/179 for voltage/temp on dual-CCD Matisse — that is ZM's TEMP base; second-CCD block placement unpublished |
| Core Temp | 163 | 0x28C | 8 | CONFLICTED | see CORE_VOLTAGE conflict |

### `0x00240902` — Matisse single-CCD 02-rev
> ZS only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 47 | 0x0BC | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | documented |  |
| CLDO_VDDP | 124 | 0x1F0 | 1 | documented |  |
| CLDO_VDDG_IOD | 125 | 0x1F4 | 1 | documented |  |

### `0x00240903` — Matisse Single-CCD (3600/3700X/3800X)
> vsoc/vddp/iod conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 pair pattern (GN): 10/11 = thm/tctl |
| PPT Current | 1 | 0x004 | 1 | documented |  |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented | ZS says 49 (0xC4) — outvoted 2:1 |
| MCLK | 51 | 0x0CC | 1 | documented | ZS says 50 (0xC8) — outvoted 2:1 |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45 |
| CLDO_VDDP | 125 | 0x1F4 | 1 | CONFLICTED | ZS CLDO_VDDP=0x1F4/124 |
| CLDO_VDDG_IOD | 126 | 0x1F8 | 1 | documented |  |
| VCORE | 39 | 0x09C | 1 | documented |  |
| Core Power | 147 | 0x24C | 8 | documented |  |
| Core Voltage | 155 | 0x26C | 8 | documented |  |
| Core Temp | 163 | 0x28C | 8 | documented |  |
| Core Clock | 187 | 0x2EC | 8 | CONFLICTED | author did not map Matisse clocks; ZM base sits past 8-lane temp block |

### `0x002D0008` — Milan / Chagall multi-CCD
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 47 | 0x0BC | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | documented |  |
| CLDO_VDDP | 136 | 0x220 | 1 | documented |  |
| CLDO_VDDG_IOD | 137 | 0x224 | 1 | documented |  |

### `0x002D0803` — Milan / Chagall
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 47 | 0x0BC | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | documented |  |
| CLDO_VDDP | 136 | 0x220 | 1 | documented |  |
| CLDO_VDDG_IOD | 137 | 0x224 | 1 | documented |  |

### `0x002D0903` — Milan / Chagall single-CCD
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 47 | 0x0BC | 1 | documented |  |
| UCLK | 49 | 0x0C4 | 1 | documented |  |
| MCLK | 50 | 0x0C8 | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | documented |  |
| CLDO_VDDP | 136 | 0x220 | 1 | documented |  |
| CLDO_VDDG_IOD | 137 | 0x224 | 1 | documented |  |

### `0x00370000` — Renoir 00-rev
> ZS size 0x884 (ZM says 0x794 — conflict)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 301 | 0x4B4 | 1 | documented |  |
| UCLK | 302 | 0x4B8 | 1 | documented |  |
| MCLK | 303 | 0x4BC | 1 | documented |  |
| VDDCR_SOC | 100 | 0x190 | 1 | documented |  |
| CLDO_VDDP | 459 | 0x72C | 1 | documented |  |

### `0x00370001` — Renoir 01-rev
> ZS size 0x88C (ZM says 0x884 — conflict)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 361 | 0x5A4 | 1 | documented |  |
| UCLK | 362 | 0x5A8 | 1 | documented |  |
| MCLK | 363 | 0x5AC | 1 | documented |  |
| VDDCR_SOC | 100 | 0x190 | 1 | documented |  |
| CLDO_VDDP | 519 | 0x81C | 1 | documented |  |

### `0x00370002` — Renoir 02-rev
> ZS size 0x8AC (ZM says 0x88C — conflict)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 363 | 0x5AC | 1 | documented |  |
| UCLK | 364 | 0x5B0 | 1 | documented |  |
| MCLK | 365 | 0x5B4 | 1 | documented |  |
| VDDCR_SOC | 102 | 0x198 | 1 | documented |  |
| CLDO_VDDP | 521 | 0x824 | 1 | documented |  |

### `0x00370003` — Renoir / Lucienne
> vsoc/vddp conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 29 | 0x074 | 1 | documented |  |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VCORE | 97 | 0x184 | 1 | documented |  |
| VDDCR_SOC | 101 | 0x194 | 1 | CONFLICTED | ZS VDDCR_SOC=0x198/102 |
| CLDO_VDDP | 527 | 0x83C | 1 | CONFLICTED | ZS CLDO_VDDP=0x844/529 |
| FCLK | 371 | 0x5CC | 1 | documented |  |
| UCLK | 372 | 0x5D0 | 1 | documented |  |
| MCLK | 373 | 0x5D4 | 1 | documented |  |
| Core Power | 192 | 0x300 | 8 | documented |  |
| Core Voltage | 200 | 0x320 | 8 | documented |  |
| Core Temp | 208 | 0x340 | 8 | documented |  |
| Core Clock | 232 | 0x3A0 | 8 | documented |  |

### `0x00370004` — Renoir 04-rev
> ZS: identical offsets to 370003

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 29 | 0x074 | 1 | documented |  |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VCORE | 97 | 0x184 | 1 | documented |  |
| VDDCR_SOC | 101 | 0x194 | 1 | CONFLICTED | ZS VDDCR_SOC=0x198/102 |
| CLDO_VDDP | 527 | 0x83C | 1 | CONFLICTED | ZS CLDO_VDDP=0x844/529 |
| FCLK | 371 | 0x5CC | 1 | documented |  |
| UCLK | 372 | 0x5D0 | 1 | documented |  |
| MCLK | 373 | 0x5D4 | 1 | documented |  |
| Core Power | 192 | 0x300 | 8 | documented |  |
| Core Voltage | 200 | 0x320 | 8 | documented |  |
| Core Temp | 208 | 0x340 | 8 | documented |  |
| Core Clock | 232 | 0x3A0 | 8 | documented |  |

### `0x00370005` — Renoir v2 APU
> vsoc/vddp conflicted; size: ZS 0x8C8 vs ZM 0x8C8, RyzenAdj 0x8C8

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 29 | 0x074 | 1 | documented |  |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VCORE | 97 | 0x184 | 1 | documented |  |
| VDDCR_SOC | 101 | 0x194 | 1 | CONFLICTED | ZS VDDCR_SOC=0x198/102 |
| CLDO_VDDP | 534 | 0x858 | 1 | CONFLICTED | ZS CLDO_VDDP=0x86C/543 |
| FCLK | 378 | 0x5E8 | 1 | documented |  |
| UCLK | 379 | 0x5EC | 1 | documented |  |
| MCLK | 380 | 0x5F0 | 1 | documented |  |
| Core Power | 199 | 0x31C | 8 | documented |  |
| Core Voltage | 207 | 0x33C | 8 | documented |  |
| Core Temp | 215 | 0x35C | 8 | documented |  |
| Core Clock | 239 | 0x3BC | 8 | documented |  |

### `0x00380005` — Chagall / Vermeer multi-CCD
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |

### `0x00380505` — Vermeer / Chagall
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |

### `0x00380605` — Vermeer / Chagall
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |

### `0x00380705` — Vermeer / Chagall
> clocks/rails only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 45 | 0x0B4 | 1 | documented |  |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |

### `0x00380804` — Vermeer Dual-CCD (older BIOS)
> per-core voltage/temp conflicted; fused-slot layout unknown

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Current | 1 | 0x004 | 1 | CONFLICTED | GN measured index 1 = socket power (RAPL-corroborated) on 0x380905 |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 (lim,val) pair pattern implies 10/11 = thm/tctl (GN) |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45; GN provisional 45; author 44 |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |
| VCORE | 40 | 0x0A0 | 1 | documented | 805 revision uses 39 |
| Core Voltage | 185 | 0x2E4 | 8 | CONFLICTED | ZS power base 0x24C/147 + ZM block order predict volt=155/temp=163; author used 185/201 — unresolved for dual-CCD parts |
| Core Temp | 201 | 0x324 | 8 | CONFLICTED | see CORE_VOLTAGE conflict |

### `0x00380805` — Vermeer Dual-CCD (newer BIOS)
> per-core voltage/temp conflicted; fused-slot layout unknown

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Current | 1 | 0x004 | 1 | CONFLICTED | GN measured index 1 = socket power (RAPL-corroborated) on 0x380905 |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 (lim,val) pair pattern implies 10/11 = thm/tctl (GN) |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45; GN provisional 45; author 44 |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |
| VCORE | 39 | 0x09C | 1 | documented |  |
| Core Voltage | 188 | 0x2F0 | 8 | CONFLICTED | ZS 0x2B0/172 power + ZM pattern predict volt=180/temp=188; author 188/204 — unresolved for dual-CCD parts |
| Core Temp | 204 | 0x330 | 8 | CONFLICTED | see CORE_VOLTAGE conflict |

### `0x00380904` — Vermeer Single-CCD (older BIOS)
> per-core voltage/temp conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Current | 1 | 0x004 | 1 | CONFLICTED | GN measured index 1 = socket power (RAPL-corroborated) on 0x380905 |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 (lim,val) pair pattern implies 10/11 = thm/tctl (GN) |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45; GN provisional 45; author 44 |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |
| VCORE | 40 | 0x0A0 | 1 | documented |  |
| Core Voltage | 177 | 0x2C4 | 8 | CONFLICTED | ZS power 0x2A4/169 + pattern predict volt=177/temp=185; author 177/183 — temp disagrees |
| Core Temp | 183 | 0x2DC | 8 | CONFLICTED | see CORE_VOLTAGE conflict |

### `0x00380905` — Vermeer Single-CCD (newer BIOS)
> gnr-smu 5600X live-validated; decoder must re-check the fused slot map against the live table and refuse per-core on mismatch

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Current | 1 | 0x004 | 1 | CONFLICTED | GN measured index 1 = socket power (RAPL-corroborated) on 0x380905 |
| VID (Aggregate) | 10 | 0x028 | 1 | documented |  |
| IOD Hotspot? | 11 | 0x02C | 1 | CONFLICTED | zone-0 (lim,val) pair pattern implies 10/11 = thm/tctl (GN) |
| Socket Power | 29 | 0x074 | 1 | documented |  |
| FCLK | 48 | 0x0C0 | 1 | documented |  |
| UCLK | 50 | 0x0C8 | 1 | documented |  |
| MCLK | 51 | 0x0CC | 1 | documented |  |
| VDDCR_SOC | 44 | 0x0B0 | 1 | CONFLICTED | ZS VDDCR_SOC=0xB4/45; GN provisional 45; author 44 |
| CLDO_VDDP | 137 | 0x224 | 1 | documented |  |
| CLDO_VDDG_IOD | 138 | 0x228 | 1 | documented |  |
| CLDO_VDDG_CCD | 139 | 0x22C | 1 | documented |  |
| Socket Power | 1 | 0x004 | 1 | **confirmed** | validated against RAPL package power |
| VCORE | 39 | 0x09C | 1 | documented |  |
| Core Power | 172 | 0x2B0 | 8 | documented |  |
| Core Voltage | 180 | 0x2D0 | 8 | documented |  |
| Core Temp | 188 | 0x2F0 | 8 | documented |  |
| Core Clock | 212 | 0x350 | 8 | documented |  |
| Core Eff. Clock | 220 | 0x370 | 8 | documented |  |
| C0 Residency | 228 | 0x390 | 8 | documented |  |
| CC1 Residency | 236 | 0x3B0 | 8 | documented |  |
| CC6 Residency | 244 | 0x3D0 | 8 | documented |  |

### `0x003F0000` — Van Gogh (Steam Deck)
> ZS clocks (unverified there) + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 239 | 0x3BC | 1 | documented |  |
| UCLK | 240 | 0x3C0 | 1 | documented |  |
| MCLK | 241 | 0x3C4 | 1 | documented |  |
| VDDCR_SOC | 104 | 0x1A0 | 1 | documented |  |
| Core Power | 142 | 0x238 | 2 | documented |  |
| Core Voltage | 146 | 0x248 | 2 | documented |  |
| Core Temp | 150 | 0x258 | 2 | documented |  |
| Core Clock | 162 | 0x288 | 2 | documented |  |

### `0x00400001` — Cezanne 01-rev
> clocks/vddp only; ZM 01-rev per-core (0x304/0x324/0x344/0x3A4) — single-sourced, not mapped

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 393 | 0x624 | 1 | documented |  |
| UCLK | 394 | 0x628 | 1 | documented |  |
| MCLK | 395 | 0x62C | 1 | documented |  |
| CLDO_VDDP | 551 | 0x89C | 1 | documented |  |

### `0x00400002` — Cezanne 02-rev
> clocks/vddp only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 399 | 0x63C | 1 | documented |  |
| UCLK | 400 | 0x640 | 1 | documented |  |
| MCLK | 401 | 0x644 | 1 | documented |  |
| CLDO_VDDP | 557 | 0x8B4 | 1 | documented |  |

### `0x00400003` — Cezanne 03-rev
> clocks/vddp only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 408 | 0x660 | 1 | documented |  |
| UCLK | 409 | 0x664 | 1 | documented |  |
| MCLK | 410 | 0x668 | 1 | documented |  |
| CLDO_VDDP | 564 | 0x8D0 | 1 | documented |  |

### `0x00400004` — Cezanne 04-rev
> ZS: identical offsets to 400005

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 29 | 0x074 | 1 | documented |  |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VCORE | 98 | 0x188 | 1 | documented |  |
| VDDCR_SOC | 102 | 0x198 | 1 | CONFLICTED | ZS VDDCR_SOC=0x19C/103 |
| FCLK | 409 | 0x664 | 1 | documented |  |
| UCLK | 410 | 0x668 | 1 | documented |  |
| MCLK | 411 | 0x66C | 1 | documented |  |
| CLDO_VDDP | 565 | 0x8D4 | 1 | documented |  |
| Core Power | 200 | 0x320 | 8 | documented |  |
| Core Voltage | 208 | 0x340 | 8 | documented |  |
| Core Temp | 216 | 0x360 | 8 | documented |  |
| Core Clock | 240 | 0x3C0 | 8 | documented |  |

### `0x00400005` — Cezanne / Barcelo
> ZS + ZM + author agree

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| IOD Hotspot? | 29 | 0x074 | 1 | documented |  |
| PPT Current | 5 | 0x014 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VCORE | 98 | 0x188 | 1 | documented |  |
| VDDCR_SOC | 102 | 0x198 | 1 | CONFLICTED | ZS VDDCR_SOC=0x19C/103 |
| FCLK | 409 | 0x664 | 1 | documented |  |
| UCLK | 410 | 0x668 | 1 | documented |  |
| MCLK | 411 | 0x66C | 1 | documented |  |
| CLDO_VDDP | 565 | 0x8D4 | 1 | documented |  |
| Core Power | 200 | 0x320 | 8 | documented |  |
| Core Voltage | 208 | 0x340 | 8 | documented |  |
| Core Temp | 216 | 0x360 | 8 | documented |  |
| Core Clock | 240 | 0x3C0 | 8 | documented |  |

### `0x00450004` — Rembrandt
> ZS clocks + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 409 | 0x664 | 1 | documented |  |
| UCLK | 410 | 0x668 | 1 | documented |  |
| MCLK | 411 | 0x66C | 1 | documented |  |
| CLDO_VDDP | 565 | 0x8D4 | 1 | documented |  |
| Core Power | 200 | 0x320 | 8 | documented |  |
| Core Voltage | 208 | 0x340 | 8 | documented |  |
| Core Temp | 216 | 0x360 | 8 | documented |  |
| Core Clock | 240 | 0x3C0 | 8 | documented |  |

### `0x00450005` — Rembrandt 05-rev
> ZS clocks + per-core candidates from [upstream issue #10](https://github.com/Death4two/TuxTimings/issues/10)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 428 | 0x6B0 | 1 | documented |  |
| UCLK | 429 | 0x6B4 | 1 | documented |  |
| MCLK | 430 | 0x6B8 | 1 | documented |  |
| CLDO_VDDP | 565 | 0x8D4 | 1 | candidate | Zero in issue #10 dump; unverified |
| Core Power | 219 | 0x36C | 8 | candidate | Single 6900HS dump; workload validation needed |
| Core Voltage | 227 | 0x38C | 8 | candidate | Single 6900HS dump; workload validation needed |
| Core Temp | 235 | 0x3AC | 8 | candidate | Single 6900HS dump; workload validation needed |
| Core Clock | 251 | 0x3EC | 8 | candidate | Single 6900HS dump; workload validation needed |

The 6900HS dump (SMU 4.69.53.0, 684 float32 entries) contradicts the previous generic ZenMaster per-core offsets. Additional candidate blocks: effective clock 267–274 (GHz), active residency 275–282 and complementary idle residency 283–290 (%). These are not independently validated. ZS clock offsets remain unchanged; 0x00450004 is not modified.

### `0x004C0001` — Phoenix 01 (ZS: placeholder, unverified)
> offsets copied from 4C0003 in ZS

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0003` — Phoenix Point
> ZS clocks + SVI3 (stride 4)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0004` — Phoenix Point
> ZS clocks + SVI3 (stride 4)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0005` — Phoenix Point
> ZS clocks + SVI3 (stride 4)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0006` — Phoenix Point
> ZS clocks + SVI3 (stride 4)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0007` — Phoenix Point
> ZS clocks + SVI3 (stride 4)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 93 | 0x174 | 1 | documented |  |
| UCLK | 97 | 0x184 | 1 | documented |  |
| MCLK | 101 | 0x194 | 1 | documented |  |
| VDDCR_SOC | 114 | 0x1C8 | 1 | documented |  |
| CLDO_VDDP | 474 | 0x768 | 1 | documented |  |
| SVI3 VDD VID | 109 | 0x1B4 | 1 | documented |  |
| SVI3 VDD Volt | 110 | 0x1B8 | 1 | documented |  |
| SVI3 VDD Current | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC VID | 113 | 0x1C4 | 1 | documented |  |
| SVI3 SOC Current | 115 | 0x1CC | 1 | documented |  |

### `0x004C0008` — Phoenix Point desktop
> clock offsets -4 vs 0x4C0009 per ZS — treat clocks as suspect

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 89 | 0x164 | 1 | documented |  |
| UCLK | 93 | 0x174 | 1 | documented |  |
| MCLK | 97 | 0x184 | 1 | documented |  |
| CLDO_VDDP | 477 | 0x774 | 1 | documented |  |
| VDDCR_SOC | 101 | 0x194 | 1 | CONFLICTED | ZS VDDCR_SOC=0x1BC/111 (SVI3 measured); author used 101 |
| SVI3 VDD VID | 105 | 0x1A4 | 1 | documented |  |
| SVI3 VDD Volt | 106 | 0x1A8 | 1 | documented |  |
| SVI3 VDD Current | 107 | 0x1AC | 1 | documented |  |
| SVI3 SOC VID | 110 | 0x1B8 | 1 | documented |  |
| SVI3 SOC Volt | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC Current | 112 | 0x1C0 | 1 | documented |  |
| PPT Current | 3 | 0x00C | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Core Power | 200 | 0x320 | 8 | documented |  |
| Core Voltage | 208 | 0x340 | 8 | documented |  |
| Core Temp | 216 | 0x360 | 8 | documented |  |
| Core Clock | 240 | 0x3C0 | 8 | documented |  |

### `0x004C0009` — Hawk Point (Phoenix2 desktop)
> vsoc conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 89 | 0x164 | 1 | documented |  |
| UCLK | 93 | 0x174 | 1 | documented |  |
| MCLK | 97 | 0x184 | 1 | documented |  |
| CLDO_VDDP | 477 | 0x774 | 1 | documented |  |
| VDDCR_SOC | 101 | 0x194 | 1 | CONFLICTED | ZS VDDCR_SOC=0x1BC/111 (SVI3 measured); author used 101 |
| SVI3 VDD VID | 105 | 0x1A4 | 1 | documented |  |
| SVI3 VDD Volt | 106 | 0x1A8 | 1 | documented |  |
| SVI3 VDD Current | 107 | 0x1AC | 1 | documented |  |
| SVI3 SOC VID | 110 | 0x1B8 | 1 | documented |  |
| SVI3 SOC Volt | 111 | 0x1BC | 1 | documented |  |
| SVI3 SOC Current | 112 | 0x1C0 | 1 | documented |  |
| PPT Current | 3 | 0x00C | 1 | documented |  |
| Socket Power | 38 | 0x098 | 1 | documented |  |
| VID (Aggregate) | 28 | 0x070 | 1 | documented |  |
| Core Power | 200 | 0x320 | 8 | documented |  |
| Core Voltage | 208 | 0x340 | 8 | documented |  |
| Core Temp | 216 | 0x360 | 8 | documented |  |
| Core Clock | 240 | 0x3C0 | 8 | documented |  |

### `0x00540000` — Raphael Dual-CCD (ES/early)
> ZS: same offsets as 540004

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540001` — Raphael Dual-CCD
> ZS: same offsets as 540004

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540002` — Raphael Dual-CCD
> ZS: same offsets as 540004

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540003` — Raphael Dual-CCD
> ZS: same offsets as 540004

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540004` — Raphael Dual-CCD
> ZS + ZM + author agree on per-core; vsoc conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540005` — Raphael Dual-CCD
> ZS: same offsets as 540004

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 74 | 0x128 | 1 | documented |  |
| MCLK | 78 | 0x138 | 1 | documented |  |
| CLDO_VDDP | 268 | 0x430 | 1 | documented |  |
| VDD_MISC | 56 | 0x0E0 | 1 | documented |  |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 16 | documented |  |
| Core Voltage | 309 | 0x4D4 | 16 | documented |  |
| Core Temp | 325 | 0x514 | 16 | documented |  |
| Core Clock | 341 | 0x554 | 16 | documented |  |
| Core Eff. Clock | 357 | 0x594 | 16 | documented |  |
| C0 Residency | 373 | 0x5D4 | 16 | documented |  |
| CC1 Residency | 389 | 0x614 | 16 | documented |  |
| CC6 Residency | 405 | 0x654 | 16 | documented |  |

### `0x00540100` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540101` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540102` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540103` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540104` — Raphael Single-CCD / Dragon Range
> uclk/mclk order and vddp index conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540105` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540108` — Raphael Single-CCD / Dragon Range
> ZS: same offsets as 540104

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 70 | 0x118 | 1 | documented |  |
| VDDCR_SOC | 82 | 0x148 | 1 | CONFLICTED | ZS VDDCR_SOC=0xD0/52 (SVI3 measured rail); author used 82 |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| SVI3 VDD VID | 46 | 0x0B8 | 1 | documented |  |
| SVI3 VDD Volt | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Current | 48 | 0x0C0 | 1 | documented |  |
| SVI3 SOC VID | 51 | 0x0CC | 1 | documented |  |
| SVI3 SOC Volt | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Current | 53 | 0x0D4 | 1 | documented |  |
| SVI3 MISC VID | 56 | 0x0E0 | 1 | documented |  |
| SVI3 MISC Volt | 57 | 0x0E4 | 1 | documented |  |
| UCLK | 78 | 0x138 | 1 | CONFLICTED | ZS 0x128/74=UCLK,0x138/78=MCLK; author swapped them (1:1 UCLK=2x? indistinguishable at 1:1 — unresolved) |
| MCLK | 74 | 0x128 | 1 | CONFLICTED | see UCLK conflict |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | ZS CLDO_VDDP=0x430/268 |
| VCORE | 271 | 0x43C | 1 | documented |  |
| Core Power | 293 | 0x494 | 8 | documented |  |
| Core Voltage | 301 | 0x4B4 | 8 | documented |  |
| Core Temp | 309 | 0x4D4 | 8 | documented |  |
| Core Clock | 317 | 0x4F4 | 8 | documented |  |
| Core Eff. Clock | 325 | 0x514 | 8 | documented |  |
| C0 Residency | 333 | 0x534 | 8 | documented |  |
| CC1 Residency | 341 | 0x554 | 8 | documented |  |
| CC6 Residency | 357 | 0x594 | 8 | documented |  |

### `0x00540208` — Raphael Dual-CCD rev
> ZS clocks +4 bytes vs 540004; per-core bases not published for this revision — clocks/rails/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 71 | 0x11C | 1 | documented |  |
| UCLK | 75 | 0x12C | 1 | documented |  |
| MCLK | 79 | 0x13C | 1 | documented |  |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| CLDO_VDDP | 269 | 0x434 | 1 | documented |  |
| SVI3 VDD VID | 47 | 0x0BC | 1 | documented |  |
| SVI3 VDD Volt | 48 | 0x0C0 | 1 | documented |  |
| SVI3 VDD Current | 49 | 0x0C4 | 1 | documented |  |
| SVI3 SOC VID | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC Volt | 53 | 0x0D4 | 1 | documented |  |
| SVI3 SOC Current | 54 | 0x0D8 | 1 | documented |  |
| SVI3 MISC VID | 57 | 0x0E4 | 1 | documented |  |
| SVI3 MISC Volt | 58 | 0x0E8 | 1 | documented |  |

### `0x005C0002` — Storm Peak 12-CCD
> clocks/SVI3 only; per-core not published

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0003` — Storm Peak 12-CCD
> clocks/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0102` — Storm Peak 8-CCD
> clocks/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0103` — Storm Peak 8-CCD
> clocks/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0202` — Storm Peak 6-CCD
> clocks/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0203` — Storm Peak 6-CCD
> x03 layout +8 (ZS: verified on 7985WX)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 141 | 0x234 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 181 | 0x2D4 | 1 | documented |  |
| VDDCR_SOC | 74 | 0x128 | 1 | documented |  |
| SVI3 VDD VID | 63 | 0x0FC | 1 | documented |  |
| SVI3 VDD Volt | 64 | 0x100 | 1 | documented |  |
| SVI3 VDD Current | 65 | 0x104 | 1 | documented |  |
| SVI3 SOC VID | 73 | 0x124 | 1 | documented |  |
| SVI3 SOC Current | 75 | 0x12C | 1 | documented |  |
| SVI3 MISC VID | 78 | 0x138 | 1 | documented |  |
| SVI3 MISC Volt | 79 | 0x13C | 1 | documented |  |

### `0x005C0302` — Storm Peak 4-CCD
> clocks/SVI3 only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 140 | 0x230 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 180 | 0x2D0 | 1 | documented |  |
| VDDCR_SOC | 72 | 0x120 | 1 | documented |  |
| SVI3 VDD VID | 61 | 0x0F4 | 1 | documented |  |
| SVI3 VDD Volt | 62 | 0x0F8 | 1 | documented |  |
| SVI3 VDD Current | 63 | 0x0FC | 1 | documented |  |
| SVI3 SOC VID | 71 | 0x11C | 1 | documented |  |
| SVI3 SOC Current | 73 | 0x124 | 1 | documented |  |
| SVI3 MISC VID | 76 | 0x130 | 1 | documented |  |
| SVI3 MISC Volt | 77 | 0x134 | 1 | documented |  |

### `0x005C0303` — Storm Peak 4-CCD
> x03 layout +8

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 141 | 0x234 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 181 | 0x2D4 | 1 | documented |  |
| VDDCR_SOC | 74 | 0x128 | 1 | documented |  |
| SVI3 VDD VID | 63 | 0x0FC | 1 | documented |  |
| SVI3 VDD Volt | 64 | 0x100 | 1 | documented |  |
| SVI3 VDD Current | 65 | 0x104 | 1 | documented |  |
| SVI3 SOC VID | 73 | 0x124 | 1 | documented |  |
| SVI3 SOC Current | 75 | 0x12C | 1 | documented |  |
| SVI3 MISC VID | 78 | 0x138 | 1 | documented |  |
| SVI3 MISC Volt | 79 | 0x13C | 1 | documented |  |

### `0x005C0402` — Storm Peak 2-CCD
> ZS: 2-CCD rows use the +8 layout

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 141 | 0x234 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 181 | 0x2D4 | 1 | documented |  |
| VDDCR_SOC | 74 | 0x128 | 1 | documented |  |
| SVI3 VDD VID | 63 | 0x0FC | 1 | documented |  |
| SVI3 VDD Volt | 64 | 0x100 | 1 | documented |  |
| SVI3 VDD Current | 65 | 0x104 | 1 | documented |  |
| SVI3 SOC VID | 73 | 0x124 | 1 | documented |  |
| SVI3 SOC Current | 75 | 0x12C | 1 | documented |  |
| SVI3 MISC VID | 78 | 0x138 | 1 | documented |  |
| SVI3 MISC Volt | 79 | 0x13C | 1 | documented |  |

### `0x005C0403` — Storm Peak 2-CCD
> x03 layout +8

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 141 | 0x234 | 1 | documented |  |
| UCLK | 168 | 0x2A0 | 1 | documented |  |
| MCLK | 181 | 0x2D4 | 1 | documented |  |
| VDDCR_SOC | 74 | 0x128 | 1 | documented |  |
| SVI3 VDD VID | 63 | 0x0FC | 1 | documented |  |
| SVI3 VDD Volt | 64 | 0x100 | 1 | documented |  |
| SVI3 VDD Current | 65 | 0x104 | 1 | documented |  |
| SVI3 SOC VID | 73 | 0x124 | 1 | documented |  |
| SVI3 SOC Current | 75 | 0x12C | 1 | documented |  |
| SVI3 MISC VID | 78 | 0x138 | 1 | documented |  |
| SVI3 MISC Volt | 79 | 0x13C | 1 | documented |  |

### `0x005D0008` — Strix Point 12C
> ZS clocks + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 312 | 0x4E0 | 1 | documented |  |
| UCLK | 313 | 0x4E4 | 1 | documented |  |
| MCLK | 315 | 0x4EC | 1 | documented |  |
| Core Power | 629 | 0x9D4 | 12 | documented |  |
| Core Voltage | 641 | 0xA04 | 12 | documented |  |
| Core Temp | 653 | 0xA34 | 12 | documented |  |
| Core Clock | 665 | 0xA64 | 12 | documented |  |
| Core Eff. Clock | 677 | 0xA94 | 12 | documented |  |
| C0 Residency | 685 | 0xAB4 | 12 | documented |  |
| CC1 Residency | 693 | 0xAD4 | 12 | documented |  |
| CC6 Residency | 701 | 0xAF4 | 12 | documented |  |

### `0x005D0009` — Strix Point 12C rev
> ZS clocks + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 312 | 0x4E0 | 1 | documented |  |
| UCLK | 313 | 0x4E4 | 1 | documented |  |
| MCLK | 315 | 0x4EC | 1 | documented |  |
| Core Power | 629 | 0x9D4 | 12 | documented |  |
| Core Voltage | 641 | 0xA04 | 12 | documented |  |
| Core Temp | 653 | 0xA34 | 12 | documented |  |
| Core Clock | 665 | 0xA64 | 12 | documented |  |
| Core Eff. Clock | 677 | 0xA94 | 12 | documented |  |
| C0 Residency | 685 | 0xAB4 | 12 | documented |  |
| CC1 Residency | 693 | 0xAD4 | 12 | documented |  |
| CC6 Residency | 701 | 0xAF4 | 12 | documented |  |

### `0x005D000A` — Strix Point rev A
> ZS clocks + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 312 | 0x4E0 | 1 | documented |  |
| UCLK | 313 | 0x4E4 | 1 | documented |  |
| MCLK | 315 | 0x4EC | 1 | documented |  |
| Core Power | 629 | 0x9D4 | 12 | documented |  |
| Core Voltage | 641 | 0xA04 | 12 | documented |  |
| Core Temp | 653 | 0xA34 | 12 | documented |  |
| Core Clock | 665 | 0xA64 | 12 | documented |  |
| Core Eff. Clock | 677 | 0xA94 | 12 | documented |  |
| C0 Residency | 685 | 0xAB4 | 12 | documented |  |
| CC1 Residency | 693 | 0xAD4 | 12 | documented |  |
| CC6 Residency | 701 | 0xAF4 | 12 | documented |  |

### `0x005D000B` — Strix Point rev B
> ZS clocks + ZM per-core

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 312 | 0x4E0 | 1 | documented |  |
| UCLK | 313 | 0x4E4 | 1 | documented |  |
| MCLK | 315 | 0x4EC | 1 | documented |  |
| Core Power | 629 | 0x9D4 | 12 | documented |  |
| Core Voltage | 641 | 0xA04 | 12 | documented |  |
| Core Temp | 653 | 0xA34 | 12 | documented |  |
| Core Clock | 665 | 0xA64 | 12 | documented |  |
| Core Eff. Clock | 677 | 0xA94 | 12 | documented |  |
| C0 Residency | 685 | 0xAB4 | 12 | documented |  |
| CC1 Residency | 693 | 0xAD4 | 12 | documented |  |
| CC6 Residency | 701 | 0xAF4 | 12 | documented |  |

### `0x00620105` — Granite Ridge Single-CCD (X3D)
> 9800X3D-validated (gnr-smu); residency block conflicted

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Limit | 2 | 0x008 | 1 | **confirmed** |  |
| PPT Current | 3 | 0x00C | 1 | **confirmed** |  |
| TDC Limit | 8 | 0x020 | 1 | **confirmed** |  |
| TDC Current | 9 | 0x024 | 1 | **confirmed** |  |
| Thermal Limit | 10 | 0x028 | 1 | **confirmed** |  |
| Tctl | 11 | 0x02C | 1 | **confirmed** |  |
| EDC Limit | 63 | 0x0FC | 1 | **confirmed** |  |
| FCLK | 71 | 0x11C | 1 | documented |  |
| UCLK | 75 | 0x12C | 1 | documented |  |
| MCLK | 79 | 0x13C | 1 | documented |  |
| VCORE | 18 | 0x048 | 1 | documented | GN vcore_telemetry_peak (9800X3D) |
| Socket Power | 20 | 0x050 | 1 | documented |  |
| SoC Rail Power | 21 | 0x054 | 1 | documented |  |
| VDD_MISC | 58 | 0x0E8 | 1 | documented | 9800X3D names 58 vddio_mem_voltage; meaning differs from 620205 |
| VDDCR_SOC | 83 | 0x14C | 1 | documented | GN lists it provisional (no independent measurement) |
| CLDO_VDDP | 269 | 0x434 | 1 | CONFLICTED | GN 9800X3D: 269 is vddcr_cpu_vid; ZS vddp 0x434/269 untested here |
| Core Voltage | 309 | 0x4D4 | 8 | documented | author map used 301 — rejected by GN/ZM |
| Core Temp | 317 | 0x4F4 | 8 | documented |  |
| Core Clock | 325 | 0x514 | 8 | documented |  |
| Core Power | 333 | 0x534 | 8 | documented |  |
| C0 Residency | 341 | 0x554 | 8 | CONFLICTED | ZM publishes C0=357/CC1=365/CC6=341 for single-CCD GNR; GN load-validated 341/349/357 — unresolved |
| CC1 Residency | 349 | 0x574 | 8 | CONFLICTED | see C0 conflict |
| CC6 Residency | 357 | 0x594 | 8 | CONFLICTED | see C0 conflict |
| Boost Limit | 373 | 0x5D4 | 8 | documented |  |
| CCD L3 Temp? | 448 | 0x700 | 1 | CANDIDATE | GN L3-coupling evidence for 9800X3D |

### `0x00620205` — Granite Ridge Dual-CCD
> zone0+per-core live-validated on 9950X; GN cross-corroborated

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| PPT Limit | 2 | 0x008 | 1 | **confirmed** |  |
| PPT Current | 3 | 0x00C | 1 | **confirmed** |  |
| TDC Limit | 8 | 0x020 | 1 | **confirmed** |  |
| TDC Current | 9 | 0x024 | 1 | **confirmed** |  |
| Thermal Limit | 10 | 0x028 | 1 | **confirmed** |  |
| Tctl | 11 | 0x02C | 1 | **confirmed** | author map called 11 'IOD hotspot'; GN matched it to k10temp Tctl |
| EDC Limit | 63 | 0x0FC | 1 | **confirmed** |  |
| EDC Current? | 64 | 0x100 | 1 | CANDIDATE | GN: behaviour like EDC current but identity not independently proven |
| FIT Metric | 16 | 0x040 | 1 | documented |  |
| VID Limit | 18 | 0x048 | 1 | documented |  |
| VID (Aggregate) | 19 | 0x04C | 1 | documented |  |
| CPU Rail Power | 20 | 0x050 | 1 | documented |  |
| SoC Rail Power | 21 | 0x054 | 1 | documented |  |
| VDDIO Mem Power | 22 | 0x058 | 1 | documented |  |
| VDD_18 Power | 23 | 0x05C | 1 | documented |  |
| Socket Power | 26 | 0x068 | 1 | documented | author map used index 220 for socket power (321 W at idle — rejected) |
| FCLK | 71 | 0x11C | 1 | **confirmed** |  |
| UCLK | 75 | 0x12C | 1 | **confirmed** |  |
| MCLK | 79 | 0x13C | 1 | **confirmed** |  |
| SVI3 VDD VID | 48 | 0x0C0 | 1 | documented |  |
| SVI3 VDD Volt | 49 | 0x0C4 | 1 | documented |  |
| SVI3 VDD Current | 50 | 0x0C8 | 1 | documented |  |
| SVI3 VDD VRM Temp | 52 | 0x0D0 | 1 | documented |  |
| SVI3 SOC VID | 53 | 0x0D4 | 1 | documented |  |
| SVI3 SOC Volt | 54 | 0x0D8 | 1 | documented |  |
| SVI3 SOC Current | 55 | 0x0DC | 1 | documented |  |
| SVI3 SOC VRM Temp | 57 | 0x0E4 | 1 | documented |  |
| SVI3 MISC VID | 58 | 0x0E8 | 1 | documented |  |
| SVI3 MISC Volt | 59 | 0x0EC | 1 | documented |  |
| VDD_MISC | 58 | 0x0E8 | 1 | **confirmed** | ZS SVI3 MISC_VOLT sits at 59 — 58 is the rail VID, both mapped |
| VDDCR_SOC | 83 | 0x14C | 1 | **confirmed** |  |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | **confirmed** |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | **confirmed** |  |
| CLDO_VDDP | 269 | 0x434 | 1 | **confirmed** |  |
| VCORE | 271 | 0x43C | 1 | documented | no ZS/GN field at 271; kept on author's live coherence |
| Core Power | 301 | 0x4B4 | 16 | **confirmed** |  |
| Core Voltage | 317 | 0x4F4 | 16 | **confirmed** |  |
| Core Temp | 333 | 0x534 | 16 | **confirmed** |  |
| Core Clock | 349 | 0x574 | 16 | CONFLICTED | GN names this core_fit; live check: values disagree with cpufreq per-core freq — clock identity NOT established, debug-only |
| Core Eff. Clock | 365 | 0x5B4 | 16 | CANDIDATE | GN names 365 'activity_metric' |
| C0 Residency | 381 | 0x5F4 | 16 | **confirmed** |  |
| CC1 Residency | 397 | 0x634 | 16 | **confirmed** |  |
| CC6 Residency | 413 | 0x674 | 16 | **confirmed** |  |
| Boost Limit | 445 | 0x6F4 | 16 | CANDIDATE | GN: boost_limit_confident=False |
| CCD L3 Temp? | 595 | 0x94C | 2 | CANDIDATE | GN L3-traffic coupling evidence, single-run per CCD pair |
| CCD Shared Temp? | 611 | 0x98C | 2 | CANDIDATE | GN: NOT CCD-selective — shared package/fabric heat |

### `0x00621102` — Granite Ridge Single-CCD (rev)
> clocks/rails from ZS only

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 71 | 0x11C | 1 | documented |  |
| UCLK | 75 | 0x12C | 1 | documented |  |
| MCLK | 79 | 0x13C | 1 | documented |  |
| VDDCR_SOC | 54 | 0x0D8 | 1 | documented |  |
| CLDO_VDDP | 269 | 0x434 | 1 | documented |  |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |

### `0x00621202` — Granite Ridge Dual-CCD (X3D rev)
> clocks/rails ZS; per-core LHa; zone0 limits not published

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 71 | 0x11C | 1 | documented |  |
| UCLK | 75 | 0x12C | 1 | documented |  |
| MCLK | 79 | 0x13C | 1 | documented |  |
| VDDCR_SOC | 54 | 0x0D8 | 1 | documented |  |
| CLDO_VDDP | 269 | 0x434 | 1 | documented |  |
| CLDO_VDDG_IOD | 259 | 0x40C | 1 | documented |  |
| CLDO_VDDG_CCD | 261 | 0x414 | 1 | documented |  |
| Core Power | 301 | 0x4B4 | 16 | documented |  |
| Core Voltage | 317 | 0x4F4 | 16 | documented |  |
| Core Temp | 333 | 0x534 | 16 | documented |  |
| Core Clock | 349 | 0x574 | 16 | CONFLICTED | same block as 0x620205/349 — live disagreement with cpufreq |

### `0x00650004` — Krackan Point
> ZS marks everything CANDIDATE

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 306 | 0x4C8 | 1 | CANDIDATE | ZS marks all Krackan offsets CANDIDATE |
| UCLK | 307 | 0x4CC | 1 | CANDIDATE | as above |
| MCLK | 308 | 0x4D0 | 1 | CANDIDATE | as above |
| VDDCR_SOC | 29 | 0x074 | 1 | CANDIDATE | as above |

### `0x00650005` — Krackan Point
> ZS marks everything CANDIDATE

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 306 | 0x4C8 | 1 | CANDIDATE | ZS marks all Krackan offsets CANDIDATE |
| UCLK | 307 | 0x4CC | 1 | CANDIDATE | as above |
| MCLK | 308 | 0x4D0 | 1 | CANDIDATE | as above |
| VDDCR_SOC | 29 | 0x074 | 1 | CANDIDATE | as above |

### `0x00650006` — Krackan Point
> ZS marks everything CANDIDATE

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 306 | 0x4C8 | 1 | CANDIDATE | ZS marks all Krackan offsets CANDIDATE |
| UCLK | 307 | 0x4CC | 1 | CANDIDATE | as above |
| MCLK | 308 | 0x4D0 | 1 | CANDIDATE | as above |
| VDDCR_SOC | 29 | 0x074 | 1 | CANDIDATE | as above |

### `0x00650007` — Krackan Point
> size+offsets copied from 650006 (ZS)

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 306 | 0x4C8 | 1 | CANDIDATE | ZS marks all Krackan offsets CANDIDATE |
| UCLK | 307 | 0x4CC | 1 | CANDIDATE | as above |
| MCLK | 308 | 0x4D0 | 1 | CANDIDATE | as above |
| VDDCR_SOC | 29 | 0x074 | 1 | CANDIDATE | as above |

### `0x00650008` — Krackan Point (SMU 101.72)
> placeholder row in ZS — unverified

| Metric | Index | Byte offset | Lanes | Confidence | Conflict / note |
|---|---:|---:|---:|---|---|
| FCLK | 306 | 0x4C8 | 1 | CANDIDATE | ZS marks all Krackan offsets CANDIDATE |
| UCLK | 307 | 0x4CC | 1 | CANDIDATE | as above |
| MCLK | 308 | 0x4D0 | 1 | CANDIDATE | as above |
| VDDCR_SOC | 29 | 0x074 | 1 | CANDIDATE | as above |

