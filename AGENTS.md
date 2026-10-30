# CoralFlower + SHAYPE (WaveLoop fork)

Preserve CoralFlower's interface, existing arp modes, native storage formats and credits.
Local Lucian Labs agents also follow ../lucian-utils/AGENTS.md.
Push only this fork (origin: ELI7VH/CoralFlower), never upstream.

- `docs/SHAYPE.md`: integration, controls and storage contract.
- `arp_shape.c` is the pure WaveLoop donor core. Keep its parity fixtures in sync.
- SHAYPE storage: A0000/A1000, unused CZ pair; CZ must remain disabled. Backup object 68.
- `sh tools/test_shape.sh` runs core parity and native player/UI/storage/backup tests after header generation.
- Build with the existing build.sh/JieLi toolchain. Keep native build guards enabled.
- Identify and back up a physical unit before installing. Never flash while playing.
- TEST is authorized for this port. SHOW and GHOUL are excluded from this task.
