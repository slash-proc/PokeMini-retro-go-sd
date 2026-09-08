# Changelog

## [v0.0.2] - 2026-09-08

### Added

- Published under the [GWRG distribution
  spec](https://github.com/slash-proc/gwrg-dist-spec): a `manifest.json`
  describing this core and the system it provides, an offline bundle, and a
  GitHub Pages mirror of `dist/` that a web installer can read without a human
  in the loop.
- `symbols[]` publishes the linked ELF so a crash address from a device can be
  resolved back to a function. It is named by the manifest and mirrored, but is
  not part of the install set and never reaches the card.
- `gwrg.json`, the hand-written half of the manifest: the short console name
  and whether compressed ROMs work. Everything else -- the system, its folder,
  extensions and browse mode, the firmware ABI, sizes and hashes -- is derived
  from the packed binary at release time, so the manifest and the firmware
  cannot disagree about which folder the system reads.
- The Pokémon Mini boot ROM is declared as a `bios[]` entry, `bios.min`, with
  the SHA-1 a consumer can check a user-supplied copy against. The ROM folder
  `mini` is also the BIOS folder, so no `biosDir` is needed. It is optional:
  `src/main.c` points the core at `/bios/mini/bios.min` and the bundled
  FreeBIOS covers the case where the file is absent, so declaring it required
  would warn every user about a file they do not need.

### Changed

- `scripts/make_manifest.py`, `build_dist.py`, `make_bundle.py` and
  `stage_release.py` are now the shared copies, byte-identical across every
  project. A script that has to be edited on the way in is a script that
  drifts.
- The Makefile answers `print-SIDECARS` and `print-RO_BIN`. The shared
  `stage_release.py` reads Makefile variables positionally, so a missing
  `print-` target does not degrade gracefully -- it fails the release outright.
  This core installs no file beside the packed binary, so both are empty.

## [v0.0.1]

Initial standalone Retro-Go SD dynamic core for Pokémon Mini (PokeMini).

### Added

- Nothing

### Changed

- Hot Minx CPU / timers / IRQ / IO / PRC / LCD / audio / video `.text` in ITCM (`pokemini_core.ld`); working buffers on DTCM via `dtc_malloc` (no ITCM data).

### Fixed

- Nothing

### Install

**Core**

- Unzip the release archive onto the SD card root (`cores/PokeMini.bin`).
- Place ROMs under `/roms/mini/` (extension `.min`).
- Optional BIOS: `/bios/mini/bios.min` (FreeBIOS fallback otherwise).
- Requires firmware whose ABI matches `SDK_VERSION` in this repository.
