# Changelog

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
