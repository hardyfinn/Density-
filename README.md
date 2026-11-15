# Density

A VCV Rack 2 plugin of eight modules inspired by three Destiny+ instruments (Programma 900 2.0, entanglement space, model-Q-2). **Inspired by, not a clone:** independent designs, not emulations; Destiny+ is not involved.

| Module | Search | What it does |
|---|---|---|
| HARMONIA | HRM | Six additive harmonic voices: odd, even, partials, tilt, morph banks, harmonic lock, three pan locations |
| TRACE | TRC | Eight hand-played CV recorders: hold to record, release to loop, slow-down, random phase, sub-loop |
| NONET | NON | Stereo effect with nine algorithms (long delay, phasers, FDN reverbs, stretch flanger, grains, scanners) |
| TANGLE | TGL | One effects core, 64 programs; cores side by side entangle |
| WEAVE | WVE | Twelve coupled complex modulators, four VCA/LPG gates, four-input pan mixer |
| AXIOM | AXM | Ten equations making four polyrhythmic pulse channels with accent and pitch |
| TETRA | TET | Four two-operator PM voices with a cross-modulation matrix |
| CHEB | CHB | Stereo Chebyshev (second kind) waveshaper, 40 scannable shapers |

**Manual:** [docs/MANUAL.md](docs/MANUAL.md) and [docs/Density-Manual.pdf](docs/Density-Manual.pdf), with patch recipes.

## Install (Apple silicon Mac)

1. Open the latest successful run of the **Build** workflow under the repository's Actions tab and download the `Density-mac-arm64` artifact (or the `.vcvplugin` from a release).
2. Unzip it to get `Density-2.0.0-mac-arm64.vcvplugin`.
3. Put that file in Rack's user folder, in `plugins-mac-arm64` (Rack: Help > Open user folder), and restart Rack.

## Build

```
export RACK_DIR=/path/to/Rack-SDK
make && make install      # or make dist
make test                 # host tests of the DSP, no Rack SDK needed
make panels               # regenerate res/*.svg and src/generated/Layout.hpp (Python 3 + fonttools)
```

The build spec is in `docs/BUILD-INSTRUCTIONS.md`, the research in `docs/RESEARCH.md`, and build-time choices in `docs/DECISIONS.md`.

Licence: GPL-3.0-or-later.
