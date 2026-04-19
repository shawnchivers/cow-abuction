# Cow Abduction

A Sega Genesis / Mega Drive light-gun arcade game built with SGDK.

Shoot UFOs before they abduct cows. Supports Konami Justifier input on port 2, with gamepad fallback aiming.

## Features

- 256px wide H32 game mode
- Multi-point Justifier calibration
- Multi-round UFO attack waves
- Debug build + BlastEm GDB debugging support

## Requirements

For local builds:

- SGDK installed at `/opt/sgdk` (or set `GDK=/path/to/sgdk`)
- `blastem` in `PATH` for `make run`

For container builds:

- Docker
- Image: `schiv-genesis-build`

## Build

Local SGDK:

```bash
make release
make debug
```

Docker (recommended if local SGDK is not installed):

```bash
make docker-release
make docker-debug
```

## Run

```bash
make run
```

This builds release ROM and launches BlastEm with `out/rom.bin`.

## Debug

Build debug ROM first:

```bash
make debug
```

Then launch debugger:

```bash
gdb-multiarch ./out/debug/rom.out -ex "set architecture m68k" -ex "target remote | blastem -D ./out/debug/rom.bin"
```

## Gameplay Flow

```mermaid
flowchart TD
	A[Game Boot] --> B[Init Video Sprites Palettes]
	B --> C[Detect Justifier on Port 2]
	C --> D[Calibration Phase]
	D -->|Start pressed| E[Use default lookup]
	D -->|5 shots recorded| F[Build calibrated X and Y mapping]
	E --> G[Start Round]
	F --> G

	G --> H[Spawn cows]
	G --> I[Prepare UFO entries]
	H --> J[Main Update Loop]
	I --> J

	J --> K[Read input and update reticle]
	K --> L[Shooting check]
	L -->|Hit UFO| M[UFO HIT state + explosion]
	L -->|Miss or no shot| N[Continue]

	J --> O[Update UFO state machine]
	O --> O1[WAITING]
	O --> O2[DESCENDING]
	O --> O3[ABDUCTING]
	O --> O4[HIT]
	O --> O5[RETREATING]
	O5 --> O6[INACTIVE]

	O3 --> P[Advance cow abduction frames]
	P -->|Cow fully abducted| Q[Mark cow gone]
	M --> R[Start cow rescue reverse animation]

	J --> S[Check round end]
	S -->|All UFOs inactive| T[Round End screen]
	T --> U[Increment round]
	U --> G
```

## Controls

- Gun trigger: shoot
- Start: skip calibration / reload
- Controller D-pad: move reticle
- Controller A/B: shoot
- Controller C: reload

## Project Layout

- `src/main.c`: initialization, input, frame loop
- `src/game_phases.h`: gameplay state machine and round logic
- `src/game.h`: constants and state structs
- `res/`: sprites, palettes, and generated SGDK resources

## Release Notes

ROM metadata is configured in `src/rom_header.c`.
Update publisher/title/serial before publishing final cartridges or ROM releases.
