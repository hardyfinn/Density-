# Build instructions: DENSITY, modules inspired by three Destiny+ instruments

**Audience:** an Opus model implementing this plugin from scratch.
**Owner:** Hardy (also Haer). Fluent in VCV Rack, does not read C++. Build the whole plugin in one go, test it yourself, and hand over the finished result. No milestone check-ins.

**What it is:** a VCV Rack 2 plugin, brand and name `Density`, slug `Density`, with **8 modules** inspired by three instruments from the boutique maker Destiny+ (destiny-plus.com, designer "kirkis"):

- **Programma 900 2.0**, a "harmonic musical computer": six additive voices, 32 recordable gestural CV buffers, nine DSP effect algorithms.
- **entanglement space**, a quad-core DSP effects processor with 64 algorithms, 12 modulators, 4 voltage-controlled gates and a 4-input pan mixer.
- **model-Q-2**, a small algorithmic system: 10 equations making polyrhythmic pulses, 8 phase-modulation oscillators (four voices W, X, Y, Z), a modulation matrix, a stereo Chebyshev (2nd kind) waveshaper with 40 predefined polynomials of 10 harmonics each, and 6 complex modulation sources.

The feasibility study is `docs/RESEARCH.md` (copy of `01-feasibility.md`). Read it first.

**This is behavioural, not a clone.** Destiny+ publishes no firmware, schematics or DSP equations. Everything below is this spec's own DSP design, shaped by the Programma 900 v2 manual, the entanglement space algorithm list, the model-Q-2 product page and app labels, and demo videos. It will not sound identical to the hardware, and must not claim to.

**Repository:** https://github.com/hardyfinn/Density- (empty at the start). Build on `main`.

**Hardy's decisions (2026-11-15), which override anything else:**

| Question | Answer |
|---|---|
| Plugin name | **Density** (Hardy's repo name, "Density+"). Personal plugin, not for the VCV Library. |
| Source instruments | Programma 900 2.0, entanglement space, model-Q-2. |
| Labels | Accurate text labels on every control and jack, no symbols (as in Serge, Tudor, Silverbox). |
| Search | Short module names findable in Rack's browser (§2.1). |
| Builds | **Apple silicon Mac only** in CI (macOS arm64). Skip Intel, Windows, Linux builds. Host unit tests may run on Linux. |
| Manual | `docs/MANUAL.md` plus `docs/Density-Manual.pdf` with patch recipes. |
| Naming | Our own module, algorithm and program names. **Never** use Destiny+ product names or their algorithm names ("Programma", "Silver tulips", "xor xor", etc.) on panels, in code identifiers, or in tooltips. The manual may say "inspired by the Destiny+ Programma 900" in its introduction and credits only. No copied panel graphics. |

Defaults this spec chose (Hardy may override later; record any change in `docs/DECISIONS.md`): TANGLE implements all 64 programs (16 per bank); the camera/gesture control of the originals is replaced by TRACE (recorded hand gestures from knobs or CV); model-Q-2's six modulators are covered by WEAVE.

---

## 0. Ground rules

1. **Spirit of the sources.** Dense, drifting, self-generating material: drones built from harmonic spectra, long gestural modulation loops, deep effect chains, and equation-driven rhythms. Generate material first, perform later. Every module must be pleasant at default settings and stay stable when patched into itself.
2. **Never blow up.** Every state variable that can grow is soft-clamped (§3.2). Every `process()` ends with an `isfinite` guard that resets that module's state to zero if anything is non-finite. Feedback above unity must saturate, never produce NaN, inf or DC lock-up.
3. **DSP separate from Rack.** All models live in `src/dsp/` as plain C++17 with no Rack includes, so they can be unit tested on the host. `src/modules/*.cpp` are thin wrappers.
4. **No allocation, locks, file I/O or logging in `process()`.** Allocate buffers in the constructor or in `onSampleRateChange`. The one exception is NONET's long delay buffer, allocated by setting (§5.3.2, algorithm 1), which must be done off the audio thread: allocate in `onSampleRateChange`/constructor according to the context-menu setting, never inside `process()`.
5. **Every param has a tooltip and unit; every port has a name** (`configParam`, `configInput`, `configOutput`, `configSwitch` with labels for each position). Where a knob's meaning depends on the selected algorithm (NONET, TANGLE), update the `ParamQuantity` name and unit when the algorithm changes so the tooltip is always correct.
6. **Ambiguity:** choose what best serves the behaviour described here, write it in `docs/DECISIONS.md`, and list it in the hand-over.
7. **CPU at 48 kHz, one core:** HARMONIA under 6% with all six voices sounding and spectra being modulated, under 2% when static; NONET under 4% for its heaviest algorithm; TANGLE under 3%; TETRA under 3%; CHEB under 2%; others under 1%.
8. Licence GPL-3.0-or-later. Do not copy code from other plugins unless the licence is compatible; credit it in `docs/CREDITS.md`.
9. **Polyphony:** ports are mono unless marked "poly".

---

## 1. Setup

- **VCV Rack 2.x**, newest 2.x Rack SDK. Read https://vcvrack.com/manual/PluginDevelopmentTutorial, https://vcvrack.com/manual/Building, https://vcvrack.com/manual/Panel and https://vcvrack.com/manual/DSP first.
- `export RACK_DIR=/path/to/Rack-SDK`, then `make`, `make dist`, `make install`.
- Unit tests with **doctest** (single header) in `tests/`. `make test` builds `tests/*.cpp` with `src/dsp/*.cpp` as a host executable and runs it.
- `.github/workflows/build.yml`: one job on `macos-14` (Apple silicon) that downloads the Rack SDK for `mac-arm64`, builds, runs `make test`, and uploads the `.vcvplugin` as an artifact on every push and as a release asset on tags `v*`. In earlier projects the Rack SDK download was blocked (403) from the cloud container, so Rack builds may only be possible on GitHub Actions. Keep `make test` host-only so it runs anywhere.
- `README.md`: what the plugin is, the "inspired by, not a clone" note, and install steps (drop the `.vcvplugin` into Rack's `plugins-mac-arm64` user folder).

```
plugin.json   Makefile   README.md   LICENSE
docs/   BUILD-INSTRUCTIONS.md (this file)  RESEARCH.md  DECISIONS.md  CREDITS.md  MANUAL.md  Density-Manual.pdf
res/    <Module>.svg ×8,  components/ (jack, knob, slider, switch, button, LED SVGs)
scripts/  make_panels.py  make_manual_pdf.sh  gen_cheb_tables.py
src/
  plugin.hpp  plugin.cpp
  dsp/      Common.hpp  Filters.hpp  Hilbert.hpp  Noise.hpp  DelayLine.hpp  Oversample.hpp  Fft.hpp
            Envelope.hpp  Lfo.hpp  Smooth.hpp
            fx/  Allpass.hpp  Fdn8.hpp  Plate.hpp  Phaser.hpp  Flanger.hpp  PitchShift.hpp  FreqShift.hpp
                 Chorus.hpp  RingMod.hpp  Drive.hpp  Grains.hpp  Scanner.hpp  StretchBuffer.hpp  FxParams.hpp
            Harmonia.hpp  Trace.hpp  Nonet.hpp  TangleCore.hpp  TanglePrograms.hpp  Weave.hpp
            Axiom.hpp  Tetra.hpp  Cheb.hpp  ChebTables.hpp (generated)
  modules/  one .cpp per module
  widgets/  DensityComponents.hpp  (ports, knobs, sliders, buttons, displays)
tests/
```

---

## 2. The module set

### 2.1 Modules

| # | Panel title | Short name (search) | HP | Inspired by | What it does |
|---|---|---|---|---|---|
| 1 | HARMONIA | HRM | 44 | Programma 900 voices | Six additive harmonic voices: odd, even, partials, tilt, tune, octave; harmonic lock; morph; three pan locations; poly envelope |
| 2 | TRACE | TRC | 26 | Programma 900 CV buffers (+ Double Q loop tricks) | Eight gestural CV recorders: hold to record, release to loop, offset, attenuate, speed, slow-down, random phase, random sub-loop |
| 3 | NONET | NON | 18 | Programma 900 DSP algorithms | Stereo effect with nine algorithms, three modes each: long delay, 16-pole phaser, Hadamard FDN reverb, shifting reverb, nested phaser, stretch flanger, grains, scanner, twin scan |
| 4 | TANGLE | TGL | 16 | entanglement space core | One stereo effects core: 4 banks × 16 programs, six pots and a button; side-by-side cores entangle |
| 5 | WEAVE | WVE | 32 | entanglement space modulators, gates, mixer (+ model-Q-2 modulators) | 12 complex modulators, 4 voltage-controlled gates, 4-input pan mixer |
| 6 | AXIOM | AXM | 22 | model-Q-2 equations | Ten equation types making four polyrhythmic pulse channels with accent and pitch |
| 7 | TETRA | TET | 30 | model-Q-2 PM voices and matrix | Four 2-operator phase-modulation voices (W, X, Y, Z), drum or voice mode, cross-modulation matrix |
| 8 | CHEB | CHB | 10 | model-Q-2 waveshaper | Stereo Chebyshev (2nd kind) waveshaper, 40 predefined polynomials, scan and spread |

`plugin.json` tags: HRM `Oscillator, Polyphonic, Additive`; TRC `Recording, Sample and hold, Utility`; NON and TGL `Effect, Delay, Reverb, Phaser, Flanger, Granular`; WVE `Low-frequency oscillator, VCA, Mixer, Random`; AXM `Sequencer, Clock modulator, Random`; TET `Oscillator, Drum, Synth voice`; CHB `Waveshaper, Distortion`. Each module's `keywords` includes its short name and its source family word ("harmonic", "entangle", "equation").

### 2.2 How they fit

```
AXIOM gates/pitch ──► TETRA ──► CHEB ──► TANGLE ──► TANGLE ──► WEAVE mixer ──► out
                                          (entangled side by side)
TRACE loops ──► HARMONIA spectra / locations ──► NONET ──► out
WEAVE modulators ──► anything (TANGLE pots, CHEB scan, TETRA index, HARMONIA tilt)
```

---

## 3. System conventions

### 3.1 Signal levels (standard Rack)

| Kind | Level | Port ring colour (§4.2) |
|---|---|---|
| Audio | ±5 V nominal | grey |
| Bipolar CV | ±5 V | blue |
| Unipolar CV | 0 to 10 V | blue |
| Pitch | 1 V/oct, 0 V = C4 = 261.63 Hz | blue |
| Gate / trigger | 0 or 10 V; inputs fire on a rise through +1.0 V, re-arm below +0.1 V | red |
| Poly lists | as their kind, N channels | yellow |

Unpatched inputs read 0 V unless a section says otherwise.

**CV convention:** a CV input adds `volts × amount` to its knob, where amount is a small attenuverter trimpot beside the jack (−1 to +1, default +1 unless stated). For a knob with range [lo, hi], 10 V of CV at amount +1 sweeps the full range (so 1 V = 10% of range). Knob plus CV is clamped to the knob's range. Poly CV inputs marked "per voice" apply channel *i* to voice/lane *i*; a mono cable applies to all.

### 3.2 Shared DSP (`src/dsp/`)

- `softClamp(x, lim)`: linear to `0.9·lim`, then `0.9·lim + 0.1·lim·tanh((|x| − 0.9·lim)/(0.1·lim))` with sign. `lim = 12` V on every output and every internal state that can grow.
- `fastTanh`: rational approximation, max error 1e-4 for |x| < 5, exact clamp beyond.
- **SVF** (`Filters.hpp`): TPT state-variable filter (Zavalishin), `g = tan(π f/fs)`, `k = 1/Q`, LP/BP/HP outputs. Clamp `f` to `[5 Hz, 0.45·fs]`.
- **One-pole LP/HP**, **first-order allpass** `y = a·x + x1 − a·y1` with `a = (tan(πf/fs) − 1)/(tan(πf/fs) + 1)`, **DC blocker** (one-pole HP at 8 Hz).
- **Tilt filter** `TiltFilter(bipolar t)`: `t < 0` is a 12 dB/oct SVF lowpass with cutoff `20 kHz · 100^(t)` (t = −1 → 200 Hz); `t > 0` is a 12 dB/oct highpass with cutoff `20 Hz · 100^(t)` (t = +1 → 2 kHz); `|t| < 0.02` bypasses. Used as the FILTER knob on NONET and in programs.
- **Hilbert pair** (`Hilbert.hpp`): two chains of four second-order allpass sections, `y[n] = c·(x[n] + y[n−2]) − x[n−2]` with `c = a²`. Coefficients (Olli Niemitalo): chain I `a = 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737` then a one-sample delay; chain Q `a = 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278`.
- **Noise** (`Noise.hpp`): xoshiro128+ per instance seeded from `random::u64()`; uniform, Gaussian (sum of 4 uniforms), pink (Paul Kellet).
- **Delay line** (`DelayLine.hpp`): power-of-two ring buffer, cubic Hermite fractional read; a `DelayLine16` variant stores `int16_t` (scale ±12 V) for very long buffers.
- **Oversampling** (`Oversample.hpp`): ×2 and ×4 polyphase half-band up/down.
- **FFT** (`Fft.hpp`): real radix-2 FFT and inverse in plain C++, sizes up to 4096. Used by HARMONIA's table builder.
- **Envelope** (`Envelope.hpp`): ADSR with exponential segments (time constant = stage time / 4.6, so stage reaches 99%), plus an AD "cycle" mode.
- **LFO** (`Lfo.hpp`): phase accumulator with sine, triangle, ramp up, ramp down, square, stepped random, smooth random (cosine-interpolated random), and logistic-chaos outputs.
- **Smoothing** (`Smooth.hpp`): one-pole 10 ms on every continuous knob that scales audio or sets a frequency or delay time; delay-time changes use 50 ms smoothing to give tape-like pitch glides. Switches and program changes crossfade 10 ms (equal-power) between old and new paths; for effect programs, fade the wet output to zero, swap, fade back in (total 20 ms), and clear the new program's buffers.

### 3.3 Shared effect blocks (`src/dsp/fx/`)

NONET and TANGLE are built from the same blocks. All take smoothed parameters and are sample-rate aware; delay lengths below are given in milliseconds at any rate.

| Block | Definition |
|---|---|
| `Allpass(ms, g)` | Schroeder allpass on a delay line: `v = x − g·d; y = d + g·v; write v`. Optional modulation of the read position ± `modMs` by an LFO. |
| `NestedAllpass` | An allpass whose inner delay contains another allpass (Gardner nesting), two levels. |
| `Plate` | Dattorro-style plate: input bandwidth one-pole, predelay, four input-diffusion allpasses (4.77, 3.60, 12.73, 9.31 ms; g 0.75, 0.75, 0.625, 0.625), a two-branch tank with modulated decay-diffusion allpasses (22.58 and 30.51 ms, g 0.7, mod ±0.3 ms at 0.5 to 1.2 Hz), delays (149.6 and 125.0 ms), damping one-poles and decay gain; stereo taps as in Dattorro's 1997 paper. Params: `size` (0.25 to 2 scales all tank delays), `decay` (0 to 0.999), `damping` (LP cutoff), `diffusion` (scales input allpass g by 0.5 to 1.0), `modDepth`, `modRate`. Fully wet output. |
| `Fdn8` | 8-line feedback delay network with a normalised 8×8 Hadamard matrix. Base delays (ms): 29.7, 37.1, 41.1, 43.7, 53.0, 59.9, 67.7, 79.3, each × `size` (0.1 to 3). Per-line gain from RT60: `g_i = 10^(−3·len_i/(RT60·fs))`. Per-line one-pole damping. Each read position modulated ±`mod` samples (0 to 16) by its own slow sine (0.1 to 0.9 Hz, spread). Stereo out = even lines to L, odd to R. Optional insert per line (used for frequency shift, pitch shift or phaser inside the loop). `freeze`: gains = 1, damping off, input muted. |
| `Phaser(N)` | N first-order allpass stages (N = 4, 8, 12 or 16). Stage *k* centre `fc·(1 + spread·(k/N − 0.5))`, swept by an LFO over `depth` octaves around the centre. Feedback from the last stage to the input with bipolar `resonance` (−0.95 to +0.95; negative inverts polarity), `fastTanh` in the loop. Output = `(dry + wet)/2` notches, or wet only if a program says so. |
| `Flanger` | Through-zero flanger: two delay lines fed the same input. Line A fixed at `zeroMs` (0.5 to 10 ms); line B at `zeroMs + depthMs·lfo` where the LFO is bipolar, so B passes through A (through-zero). Output `A − B` (through-zero mode) or `A + B` (classic). Feedback from output into the input, `fastTanh` limited. |
| `PitchShift(semis)` | Two read taps on a 100 ms delay moving at rate `1 − 2^(semis/12)`, 180° apart, each with a sine-squared window; sum. Range ±24 semitones. |
| `FreqShift(hz)` | Hilbert pair × quadrature sine/cosine oscillator; up = `I·cos − Q·sin`, down = `I·cos + Q·sin`. Range ±2 kHz. |
| `Chorus` | Three taps around 7, 11, 15 ms modulated by sines 120° apart, depth 0 to 5 ms; stereo: taps 1 and 3 left, 2 and 3 right. |
| `RingMod(hz)` | Input × sine oscillator, 0.1 Hz to 5 kHz (exponential knob). `depth` blends dry and ringmod. |
| `Drive` | ×2 oversampled `fastTanh(gain·x)/fastTanh(gain)` with gain 1 to 40. |
| `Bandpass` | SVF bandpass, Q 0.5 to 20. |
| `StretchBuffer` | The "record past halfway, loop after 12 s, clear at zero" behaviour of Programma 900's algorithms 6 to 8 (§7.2). A 12 s stereo buffer with a record head and a read head moving at `speed`. |
| `Grains` | Up to 32 simultaneous grains reading a `StretchBuffer` or live 4 s buffer. Each grain: start position, length 10 to 500 ms, pitch ratio, direction, Hann window (attack/release split by a `shape` param 0.05 to 0.95). |
| `Scanner` | A read head scanning a 12 s buffer: forward, pendulum or random jumps, with `rate` and crossfaded wrap (5 ms). |
| `Looper` | A short reverse/forward loop of the last N ms of input, held while a gate is high. |

`FxParams.hpp` defines parameter **kinds** and their knob-to-value mappings, used everywhere a program table says a pot's kind:

| Kind | Range and curve |
|---|---|
| `RevTime` | Plate decay 0 to 0.98, or FDN RT60 0.2 to 30 s (exponential) |
| `Diffusion` | 0.3 to 0.8 (allpass g) |
| `LowPass` | 400 Hz to 18 kHz, exponential |
| `HighPass` | 20 Hz to 2 kHz, exponential |
| `Level` | 0 to 1 (squared for amplitude) |
| `Mix` | 0 to 1 equal-power wet/dry |
| `Rate` | 0.02 to 12 Hz, exponential |
| `Depth` | 0 to 1 |
| `Feedback` | 0 to 1.05 (above 1 saturates through `fastTanh`) |
| `DelayShort` | 1 ms to 1 s, exponential |
| `DelayLong` | 10 ms to 6 s, exponential |
| `Predelay` | 0 to 250 ms |
| `Pitch` | −12 to +12 semitones, with a 5% dead zone at 0 |
| `Shift` | −500 to +500 Hz, cubic curve so small shifts are fine near the centre |
| `RingFreq` | 0.1 Hz to 5 kHz, exponential |
| `Zero` | flanger zero point 0.5 to 10 ms |
| `Gain` | drive 1 to 40, exponential |
| `Q` | 0.5 to 20, exponential |
| `Coef` | 0 to 0.95, generic coefficient (allpass g or similar) |

### 3.4 Saved state

Non-param state (morph banks, harmonic-lock tunings, TRACE buffers, AXIOM seeds) is saved in `dataToJson`/`dataFromJson` so a patch reopens sounding the same, except where a section says otherwise.

---

## 4. Panels

### 4.1 Look

Destiny+ instruments are pale, minimal and technical. These panels are our own: a cool off-white ground with a single family colour.

- **Ground:** `#ecebe6` with a 0.4 mm `#2b2b2b` border inset 1 mm. Rack's `ScrewBlack`.
- **Ink:** `#1d1d1d`, lines 0.35 mm, section dividers 0.6 mm, section names small in capitals at the top-left of each section box.
- **Family stripe:** a 3 mm band under the title: HARMONIC family (HRM, TRC, NON) amber `#c98a1b`; ENTANGLE family (TGL, WVE) teal `#1f7a78`; EQUATION family (AXM, TET, CHB) violet `#5b4a9e`. Family word printed small in white in the band.
- **Title** in capitals, 3 mm cap height; short name small under the stripe.
- **Labels are text and exact.** Every knob, slider, switch position, button, jack, light and display gets a printed label: the **Label** column of that module's tables, verbatim, in capitals. No pictograms. Bipolar controls get `−` and `+` at the scale ends.
- **Algorithm-dependent knobs** (NONET P1 to P6, TANGLE POT 1 to POT 6) are labelled `P1` … `P6` / `POT 1` … `POT 6` on the panel; the module's display always shows what each one currently does, and tooltips are renamed (§0.5). This is the one allowed exception to fixed descriptive labels; state it in the manual.
- **Placement:** knob label centred above; jack label centred below; never overlapping or rotated. Outputs sit in a dark inset plate (`#2a2a2a`, white labels).
- **Type:** an OFL sans (e.g. "Jost"), capitals, 2.2 mm cap height, 1.8 mm minimum; convert text to paths.
- `DENSITY` small at the bottom of each panel.
- **The check:** `scripts/make_panels.py` builds every SVG and a C++ position table from one table per module, and fails if any control lacks a label or two items overlap. Render each panel to PNG and look at it before hand-over.

### 4.2 Components (`DensityComponents.hpp`)
- **Ports:** dark 8 mm jack with a coloured ring by kind (§3.1): grey `#8a8f94`, blue `#3a6ea5`, red `#b33a2e`, yellow `#d1a823`.
- **Knobs:** `DKnob` 11 mm dark grey with white line; `DKnobSmall` 8 mm; `DTrim` 6 mm trimpot.
- **Sliders:** `DSlider` vertical, 22 mm travel, for HARMONIA voice sliders and envelope.
- **Switches:** 2- and 3-position toggles; rotary snapped knobs for selectors.
- **Buttons:** momentary or latching with an RGB LED (`VCVLightBezel<RedGreenBlueLight>` style).
- **Displays:** drawn with NanoVG in `drawLayer(1)` so they glow in the dark. Ground `#15171a`, ink `#e6e2d3`, accent in the family colour.

### 4.3 Layout rules
Knobs upper, jacks lower, outputs bottom right in the dark plate. Minimum centre spacing: jack to jack 10 mm, knob to knob 14 mm (small knobs 11 mm), knob to jack 12 mm, trimpot to its jack 8 mm, slider to slider 9 mm. A CV jack sits below its trimpot, which sits beside the control it modulates.

Notation in tables: **K** knob, **k** small knob, **F** slider (fader), **T** trimpot, **S** switch, **B** button, **I** input, **O** output, **L** light, **D** display.

---
## 5. HARMONIC family (after Programma 900 2.0)

### 5.1 HARMONIA (HRM): six additive harmonic voices

Six voices, each a spectrum you shape with six sliders: how much odd harmonic content, how much even, how many partials, how the energy leans across the spectrum, and the fundamental's tuning and octave. With nothing patched they drone, so the module is a self-contained instrument; with V/OCT and GATE patched it plays notes through one of three voice-assign modes.

#### Controls (per voice *v* = 1 to 6, in six vertical strips)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| ODD_v | ODD | F | 0 to 1 / 1.0 | Weight of odd harmonics (including the fundamental) |
| EVEN_v | EVEN | F | 0 to 1 / 0.5 | Weight of even harmonics |
| PART_v | PARTIALS | F | 0 to 1 / 0.25 | Partial count `N = 1 + 639·p²` (1 to 640), fractional; display shows the integer |
| TILT_v | TILT | F | −1 to +1 / −0.2 | Spectral lean, dark to bright (§5.1.2) |
| TUNE_v | TUNE | F | −12 to +12 semitones / per voice (below) | Fine tuning of the fundamental; context menu "Snap tune to semitones" (default off) |
| OCT_v | OCTAVE | F, snapped | −3 to +3 / 0 | Octave offset |
| MULT_v | MULT | B latching, LED | off | Self-modulation on for this voice (§5.1.2) |

Default tuning per voice (an open drone): v1 TUNE 0; v2 TUNE +7; v3 TUNE +4, OCTAVE +1; v4 TUNE −5; v5 TUNE +2, OCTAVE +1; v6 TUNE 0, OCTAVE −1. All other OCTAVE defaults are 0.

#### Controls (global)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| ROOT | ROOT | K | −2 to +2 V (1 V/oct) / 0 | Base pitch added to all voices, 0 V = C3 for the drone (C4 − 1 oct) |
| MULTD | MULT DEPTH | K | 0 to 1 / 0.3 | Self-modulation index for voices with MULT on |
| LOCK | HARMONIC LOCK | B latching, LED | off | Voices 2 to 6 use voice 1's TILT (manual: "all voice tilts follow voice 1") |
| STA | STORE A | B | | Captures ODD, EVEN, PARTIALS, TILT of all six voices into morph bank A (LED lit when bank holds data) |
| STB | STORE B | B | | Same for bank B |
| MORPH_ON | MORPH | B latching, LED | off | When on, the spectral parameters are `lerp(A, B, MORPH)` instead of the sliders (requires both banks; otherwise the LED blinks and sliders stay in control) |
| MORPH | MORPH | K | 0 to 1 / 0 | Morph position |
| DICE | DICE | B | | Randomises ODD, EVEN, PARTIALS, TILT and MULT of all voices (uniform, PARTIALS biased low: `p = u²`). Never touches TUNE, OCTAVE, envelope or TRACE |
| LOC_A, LOC_B, LOC_C | LOCATION A / B / C | K | −1 to +1 / −0.6, 0, +0.6 | Pan positions. Voices 2 and 5 sit at A, voices 1 and 3 at B, voices 4 and 6 at C (as in the source manual) |
| ATK, DEC, SUS, REL | ATTACK, DECAY, SUSTAIN, RELEASE | F | 1 ms to 20 s exp / 2 s; 1 ms to 20 s / 3 s; 0 to 1 / 0.8; 1 ms to 30 s / 6 s | Envelope mode 1 |
| RATE, SPREAD, FMFADE, LEVEL | RATE, SPREAD, FM FADE, LEVEL | k | RATE 0.01 to 10 Hz exp / 0.1; SPREAD 0 to 1 / 0.3; FM FADE −1 to +1 / 0; LEVEL 0 to 1 / 0.8 | Envelope mode 2 controls, all active at once (§5.1.3) |
| ENVT | ENVELOPE | S 3-pos | DRONE / ONE SHOT / CYCLE, default DRONE | |
| ASSIGN | ASSIGN | S 3-pos | STACK / ROUND ROBIN / CHANNEL, default ROUND ROBIN | Voice assign |
| D | — | D | | Six small spectrum previews (bar graph of the first 32 harmonic amplitudes per voice) plus each voice's partial count and note name |

#### Inputs and outputs

| Id | Label | Type | Notes |
|---|---|---|---|
| VOCT | V/OCT | I poly | Up to 6 channels |
| GATE | GATE | I poly | Up to 6 channels |
| ODD_CV, EVEN_CV, PART_CV, TILT_CV | ODD CV, EVEN CV, PART CV, TILT CV | I poly, per voice | Each with a trimpot; 10 V = full range |
| MORPH_CV | MORPH CV | I | trimpot |
| LOC_A_CV, LOC_B_CV, LOC_C_CV | LOC A CV, LOC B CV, LOC C CV | I | ±5 V = ±1 pan, trimpots. TRACE's location lanes go here |
| DICE_TRIG | DICE | I | trigger |
| OUT_L, OUT_R | LEFT, RIGHT | O | stereo mix |
| VOICES | VOICES | O poly | 6 channels, each voice post-envelope, pre-pan |
| ENVS | ENVELOPES | O poly | 6 channels, 0 to 10 V |

#### 5.1.2 Voice model

**Spectrum.** For harmonic *n* = 1 … `Nmax`:
- `w(n) = ODD` if *n* odd, `EVEN` if *n* even.
- Lean: `g(n) = n^(−p)` with `p = 1.25 − 1.75·TILT` (TILT −1 → p = 3, dark; TILT 0 → p = 1.25; TILT +1 → p = −0.5, upper partials louder).
- Partial count: with `Nf = 1 + 639·PART²`, harmonics `n ≤ floor(Nf)` get factor 1, harmonic `floor(Nf) + 1` gets `frac(Nf)`, the rest 0.
- Band limit: harmonics with `n·f0 > 0.45·fs` are zero.
- Amplitude `a_n = w(n)·g(n)·partialFactor(n)`.
- Phase: Schroeder phases `φ_n = π·n²/N` (low crest factor) by default; context menu "Phases: aligned / spread" (default spread).

**Wavetable oscillator.** Each voice owns two 2048-point single-cycle tables (current and next). When the voice's effective spectral parameters change by more than 0.002, or its band-limit harmonic count changes, the voice schedules a rebuild: fill a 2048-bin spectrum with `a_n` and `φ_n` (n ≤ 1023), inverse FFT, normalise the table to peak 1. Rebuilds are rate-limited to one per voice per 256 samples and staggered so at most one voice rebuilds per 64-sample block. After a rebuild, crossfade from the old table to the new over 256 samples. Table read: linear interpolation between adjacent points (tables are smooth after band-limiting). When `Nmax` needs more than 1023 harmonics (only at very low pitches), cap at 1023.

**Self-modulation (MULT).** Phase modulation by the voice's own previous output: `phase_read = phase + MULT_DEPTH·k·y[n−1]` with `k = 0.25` cycles per unit. FM FADE (mode 2) scales this index by the envelope: `index = MULT_DEPTH·(1 + FMFADE·(env − 1))` for FMFADE ≥ 0 (index follows the envelope), and `index = MULT_DEPTH·(1 + |FMFADE|·(−env))` clamped ≥ 0 for FMFADE < 0 (index rises as the envelope falls). Voices with MULT on run at ×2 oversampling.

**Pitch.** `f0 = 261.63 · 2^(ROOT − 1 + VOCT_v + OCT_v + TUNE_v/12)` (drone root C3). Output amplitude ±5 V at LEVEL 1 for a single voice at envelope 1; the stereo mix is scaled by `1/√(active voices)`.

#### 5.1.3 Envelope and voice assign

- **DRONE:** envelope held at 1 for every voice regardless of GATE (if GATE is patched, DRONE behaves like CYCLE with the gate as hold). Mode-2 controls still act: RATE and SPREAD apply a slow amplitude breathing, `amp = 1 − 0.5·SPREAD·(1 − cos(2π·RATE·t + 2π·v/6))/2`, so voices swell in turn.
- **ONE SHOT:** gate rise starts A→D→S, gate fall starts R (a classic ADSR on the voice that took the note).
- **CYCLE:** while the gate is high (or always, if GATE is unpatched), the voice loops attack then decay to zero then repeats at period `1/RATE` (ATTACK and DECAY are rescaled proportionally to fit the period); SPREAD offsets each voice's cycle start by `SPREAD·v/6` of the period.
- **STACK:** every voice plays the lowest-channel note held, each keeping its own TUNE and OCTAVE (a chord stack). **ROUND ROBIN:** each new gate takes the next voice (1 → 6 → 1). **CHANNEL:** poly channel *i* drives voice *i*; channels beyond the cable's count leave their voices in DRONE.
- Unpatched V/OCT: 0 V. Unpatched GATE: ENVELOPE switch decides (DRONE/CYCLE run freely, ONE SHOT is silent until gated).

#### 5.1.4 Saved state
Morph banks A and B (24 floats each) and their filled flags.

#### 5.1.5 Tests
- Spectrum: ODD = 1, EVEN = 0, PART giving N = 9, TILT making p = 1: FFT of the table shows only odd harmonics 1 to 9 with amplitudes ∝ 1/n (±0.5 dB).
- Band limit: at f0 = 5 kHz and fs = 48 kHz, no partial above 21.6 kHz (aliasing below −80 dB).
- Rebuild cost stays inside the CPU budget with all six voices' TILT modulated by a 5 Hz LFO.
- Morph with both banks filled is continuous (no step larger than 0.01 in any parameter across a 1 s sweep).
- DICE leaves TUNE and OCTAVE unchanged.

---

### 5.2 TRACE (TRC): gestural CV buffers

Eight lanes, each a long CV recorder that you play by hand. Press to wake a lane, hold to record, release to loop. While a lane loops, its offset, attenuation and speed reshape it. Global controls borrowed from Destiny+'s Double Q: slow-down, random phase and random sub-loop.

#### Controls (per lane *i* = 1 to 8, eight rows)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| SRC_i | SOURCE | K | −5 to +5 V / 0 | The hand gesture: what is recorded when CV IN is unpatched |
| REC_i | REC | B, RGB LED | | State machine below |
| OFS_i | OFFSET | T | −5 to +5 V / 0 | Added on playback |
| ATT_i | ATTEN | T | −1 to +1 / 1 | Attenuverter on playback (before offset) |
| SPD_i | SPEED | T | 0.25× to 4×, exponential, detent at 1× / 1× | Playback speed |
| CV_i | IN | I | | Records this instead of SOURCE when patched |
| OUT_i | OUT | O | | |

#### Controls (global)

| Id | Label | Type | Notes |
|---|---|---|---|
| SLOW | SLOW | S 4-pos via snapped knob | 1× / 2× / 4× / 8× slow-down of all lanes |
| RPH | RANDOM PHASE | B | Each looping lane jumps to a random position (crossfaded over 5 ms) |
| SUB | SUB LOOP | B latching, LED | Each looping lane plays a random sub-section of its loop, 1/8 to 1/2 of its length, re-chosen at every wrap |
| REC_G | REC GATES | I poly | Channel *i* acts as holding lane *i*'s REC button (gate high = held) |
| CLR | CLEAR ALL | I trigger and B | Clears every lane to asleep |
| RPH_T | RANDOM PHASE | I trigger | |
| ALL | ALL | O poly | 8 channels, lanes 1 to 8 |

#### State machine (per lane, from the source manual)

| State | LED | Output |
|---|---|---|
| ASLEEP | off | 0 V |
| AWAKE | dim white | the live source (SOURCE or IN) through ATTEN and OFFSET: "a static voltage" you can still turn |
| RECORDING | red, blinking 4 Hz | the live source through ATTEN and OFFSET, while it is written to the buffer |
| LOOPING | green, brightness pulses once per loop | the recorded buffer, read at SPEED ÷ SLOW, through ATTEN and OFFSET |

- Short press (< 300 ms) in ASLEEP → AWAKE; in AWAKE → ASLEEP; in LOOPING → AWAKE (the loop stops and output returns to the live source; the buffer is discarded by the next recording or a double press).
- Hold (≥ 300 ms) in AWAKE or LOOPING → RECORDING from the start of an empty buffer (the press time counts toward the recording: start writing at the button-down moment, keep a 300 ms pre-roll so nothing is lost).
- Release in RECORDING → LOOPING; loop length = recorded length.
- Double press (two presses within 350 ms) in any state → clears the buffer and goes ASLEEP.
- Recording stops by itself at 5 minutes and the lane loops.
- Storage: 1 kHz sample rate (decimate with a one-pole at 300 Hz), 300 000 floats per lane, linear interpolation on playback, 2 ms crossfade at the loop wrap.

#### Saved state
Lane states and buffers. Buffers are stored as 16-bit integers (±12 V full scale), base64, in `dataToJson`. Context menu "Save recordings with patch" (default on); when off, lanes reopen ASLEEP.

#### Tests
- Hold for 2.000 s with SOURCE at 3 V, release: loop length 2.000 s ±1 ms, output 3 V.
- A 1 Hz sine recorded for 4 s and played at SPEED 2× and SLOW 4× has a period of 2 s.
- SUB LOOP picks windows inside the loop only; RANDOM PHASE never produces a step larger than the 5 ms crossfade allows.
- Save/load round-trip error below 0.5 mV.

---

### 5.3 NONET (NON): nine stereo effect algorithms

Programma 900's nine effects, rebuilt from their published parameter lists with our own names. One algorithm runs at a time; each has three modes. FILTER and MIX are the same on every algorithm; P1 to P6 change meaning per algorithm and the display names them.

#### Controls

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| ALG | ALGORITHM | K snapped | 1 to 9 / 3 | Changes crossfade (§3.2) |
| MODE | MODE | B, three LEDs labelled 1 2 3 | 1 | Cycles 1 → 2 → 3 |
| P1 … P6 | P1 … P6 | K | 0 to 1 / per algorithm | Meanings in §5.3.2 |
| FILT | FILTER | K | −1 to +1 / 0 | Tilt filter on the wet signal, inside the feedback path where the algorithm has one |
| MIX | MIX | K | 0 to 1 / 0.5 | Equal-power |
| RST | RESTART | B | | Clears all buffers of the current algorithm ("DSP restart") |
| D | — | D | | Algorithm name, mode name, P1 to P6 names and values with units, a buffer-state bar for algorithms 1 and 6 to 8 |

#### Inputs and outputs

| Id | Label | Notes |
|---|---|---|
| IN_L, IN_R | IN L, IN R | R normalled to L |
| P1_CV … P6_CV | P1 … P6 | with trimpots |
| MIX_CV | MIX | trimpot |
| ALG_CV | ALG | 0 to 10 V selects 1 to 9 |
| MODE_T | MODE | trigger advances mode |
| CLK | CLOCK | clock for algorithm 1 mode 2 and the LFOs' sync (rate = clock rate × ratio when patched) |
| RST_T | RESTART | trigger |
| OUT_L, OUT_R | OUT L, OUT R | |

#### 5.3.1 The stretch-buffer behaviour (algorithms 6, 7, 8)

The source manual says these record once their stretch control is past halfway, loop after 12 seconds, and clear at zero. Implement exactly that with `StretchBuffer`:
- The algorithm's **STRETCH** pot below 0.5: no recording; the algorithm processes the live input directly.
- Crossing 0.5 upward: start recording the input into the 12 s stereo buffer. Reading follows the record head at speed `s = 1 − (STRETCH − 0.5)·1.5` (1.0 at 0.5, 0.25 at 1.0), so the read head falls behind: the sound stretches in time.
- When 12 s have been recorded: stop recording and loop the buffer, still read at speed `s`.
- STRETCH returned to 0 (below 0.02): clear the buffer, back to live.
- The display shows record progress and loop position.

#### 5.3.2 Algorithms

For each: P1 to P6 as **name (kind, default)**, the topology, and the three modes. Kinds are §3.3.

**1 LONG DELAY.** Stereo regenerating delay up to 6 minutes.
P1 TIME L (custom: 10 ms to 360 s, exponential, default 0.45 → ~1.2 s), P2 TIME R (same, default 0.5), P3 FEEDBACK (Feedback, 0.5), P4 AGE (Depth, 0.2: per-repeat lowpass from 18 kHz down to 1.5 kHz plus `Drive` gain 1 to 3 in the loop), P5 DRIFT (Depth, 0.1: wow, a smooth-random modulation of read position ±0.3% of the time at 0.3 to 2 Hz), P6 CROSS (Depth, 0: feeds L repeats into R and vice versa, ping-pong at 1).
Topology: `DelayLine16` per channel, length set by the context-menu "Long delay buffer: 6 min (default) / 1 min" (6 min at 48 kHz is about 35 MB per channel in 16-bit). Allocate on module creation or when the setting or sample rate changes, never in `process()`. Above 96 kHz the maximum time halves; display says so.
Modes: **1 FREE**; **2 CLOCK**: each time snaps to the clock period × {1/16, 1/8, 1/4, 1/3, 1/2, 2/3, 1, 3/2, 2, 3, 4, 8, 16, 32, 64} (time knob picks the nearest); **3 REVERSE**: reads each repeat backwards in windows equal to the delay time, 10 ms crossfades.

**2 SIXTEEN PHASER.** 16-pole phaser.
P1 RATE (Rate, 0.3), P2 SPREAD (Depth, 0.3: per-stage coefficient offset), P3 RESONANCE (custom −0.95 to +0.95, default +0.5; negative inverts polarity), P4 SWEEP (Depth, 0.6: sweep depth 0 to 6 octaves), P5 CENTRE (custom 100 Hz to 4 kHz exponential, default 600 Hz), P6 STEREO (Depth, 0.5: right LFO phase offset 0 to 180°).
Modes: **1 SINE** LFO; **2 TRIANGLE** LFO; **3 FOLLOW**: sweep follows the input envelope (attack 5 ms, release 200 ms) instead of an LFO.

**3 HADAMARD SPACE.** 8-line Hadamard FDN reverb.
P1 SPACE (custom, `Fdn8` size 0.2 to 3, default 0.5), P2 TIME L (RevTime as RT60, 0.5), P3 TIME R (RevTime, 0.55: odd lines' RT60), P4 MOTION (Depth, 0.3: delay modulation 0 to 16 samples), P5 PREDELAY (Predelay, 0.1), P6 DIFFUSE (Depth, 0.5: four input allpasses 3 to 11 ms, g = 0.3 + 0.45·P6).
Modes: **1 ROOM** (size ×1); **2 HALL** (size ×2.2, modulation ×1.5); **3 FREEZE** (gains 1, damping off, input muted: an infinite hold; switching back restores).

**4 SHIFTING SPACE.** FDN with a frequency shifter in the loop (Hilbert), so tails climb or fall.
P1 DECAY (RevTime, 0.6), P2 BALANCE (Mix, 0.5: early reflections vs late tail), P3 SHIFT (Shift, 0.55 → small positive), P4 SIZE (custom, `Fdn8` size 0.2 to 3, 0.5), P5 SHIFT AMOUNT (Mix, 0.5: how much of the loop passes through the shifter), P6 SHIFT DRIFT (Rate, 0: slow sine on the shift frequency, ±50% of SHIFT; 0 = off).
Early reflections: 6 taps from a 120 ms delay at 7, 19, 31, 47, 67, 89 ms × SIZE, gains 0.8 decreasing.
Modes: **1 IN LOOP** (shifter inside every FDN loop: spiralling tails); **2 ON TAIL** (shifter after the FDN, once); **3 SPLIT** (L shifts up, R shifts down by the same amount, in loop).

**5 PHASER SPACE.** 16-pole phaser feeding seven nested allpasses.
P1 RATE (Rate, 0.3), P2 SPREAD (Depth, 0.4), P3 RESONANCE (as algorithm 2, +0.4), P4 RANGE (Depth, 0.5: sweep octaves 0 to 6), P5 SPACE MIX (Mix, 0.5), P6 SPACE TIME (Coef, 0.6: g of the nested allpasses; delays 5.3, 7.9, 11.3, 17.1, 23.7, 31.9, 41.3 ms in a chain, nested in pairs).
Modes: **1 PHASE THEN SPACE**; **2 SPACE THEN PHASE**; **3 PHASE IN SPACE** (phaser inside a feedback loop around the allpass chain, loop gain 0.7·P6).

**6 STRETCH FLANGER.** Through-zero flanger reading the live input or a stretched buffer (§5.3.1).
P1 FREQUENCY (Rate, 0.2), P2 STRETCH (custom 0 to 1 with §5.3.1 behaviour, default 0), P3 FEEDBACK (custom −0.95 to +0.95, +0.3), P4 DEPTH (custom 0 to 8 ms, 0.4), P5 ZERO POINT (Zero, 0.3), P6 STRETCH MIX (Mix, 1.0: blend of live and stretched source into the flanger).
Modes: **1 SINE**; **2 RANDOM** (smooth random LFO); **3 FOLLOW** (envelope sweeps the depth).

**7 GRAIN CLOUD.** Granular cloud over the live 4 s buffer or the stretched buffer.
P1 DENSITY (custom 1 to 100 grains/s exponential, 0.4), P2 STRETCH (§5.3.1, 0), P3 POSITION (Depth, 0.2: grain start offset behind the write head, 0 to full buffer), P4 SCATTER (Depth, 0.3: random timing and position jitter), P5 PITCH (custom −24 to +24 semitones, 5% dead zone, 0.5 → 0), P6 SIZE (custom 10 to 500 ms exponential, 0.5).
Grain window shape fixed by mode. Modes: **1 FORWARD** (Hann, forward); **2 REVERSE** (Hann, backward); **3 MIXED** (direction random per grain, 50/50, window attack randomised 0.1 to 0.9 of length). Grains pan randomly across ±SCATTER.

**8 SCAN SPACE.** A scanning read head feeding an FDN.
P1 SCAN RATE (custom 0.05× to 4×, exponential, 0.5 → 1×), P2 STRETCH (§5.3.1, 0), P3 ALLPASS TIME (custom 1 to 50 ms, 0.3: four allpasses before the FDN), P4 FEEDBACK (Feedback, 0.5: FDN RT60 mapped from 0.5 to 20 s), P5 DAMP (LowPass, 0.6), P6 SPACE MIX (Mix, 0.5: scanner dry vs FDN).
Modes: **1 FORWARD**; **2 PENDULUM**; **3 JUMPS** (jumps to random positions at SCAN RATE × 2 Hz, 5 ms crossfade).

**9 TWIN SCAN.** Two modulated allpass delays that modulate each other (self-modulating, through-zero scanning).
P1 BASE DELAY (custom 0.5 to 40 ms exponential, 0.3), P2 DEPTH (Depth, 0.4: modulation depth up to ±BASE), P3 FEEDBACK (custom −0.95 to +0.95, +0.4), P4 ALLPASS TIME (custom 1 to 30 ms, 0.4), P5 ALLPASS MIX (Mix, 0.5), P6 PRE-DELAY (Predelay, 0.1).
Topology: lines A and B both read the input; A's read position is modulated by `DEPTH·tanh(yB)` and B's by `−DEPTH·tanh(yA)` (each line's output is the other's LFO); outputs summed as through-zero (A − B) into an allpass with P4/P5; feedback from the sum.
Modes: **1 TWIN** (as described); **2 CROSS** (A's output feeds B's input and vice versa); **3 RUNAWAY** (adds a slow chaotic logistic LFO to both modulations at 0.1 to 1 Hz).

#### 5.3.3 Tests
- Each algorithm at defaults: 10 s of pink noise at ±5 V in, outputs finite and within ±12 V; then 30 s of silence: output decays below −80 dBFS (except FREEZE, LONG DELAY at feedback ≥ 1, and stretched loops).
- LONG DELAY: an impulse with TIME L = 2.000 s returns at 2.000 s ±1 sample at 48 kHz.
- StretchBuffer: state transitions as §5.3.1 (record starts at 0.5, loops at 12 s, clears below 0.02).
- Algorithm change mid-signal produces no click above −40 dBFS (crossfade).

---

## 6. ENTANGLE family (after entanglement space)

The source is one box with four DSP cores of 16 programs each, cores that can be "entangled", 12 modulators, 4 voltage-controlled gates and a 4-input pan mixer. In Rack this becomes two modules: **TANGLE** (one core; use up to four side by side) and **WEAVE** (modulators, gates and mixer). How the original entangles cores is not documented; the mechanism in §6.1.3 is this spec's design.

### 6.1 TANGLE (TGL): one entangled effects core

#### Controls

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| BANK | BANK | S 4-pos (snapped knob) | A / B / C / D, default A | Which 16-program bank. Each bank has its own character (§6.1.2) |
| PROG | PROGRAM | K snapped | 0 to 15 / 0 | |
| POT1 … POT6 | POT 1 … POT 6 | K | 0 to 1 / per program | Meanings per program; display names them |
| BTN | BUTTON | B, LED | | Momentary or latching per program; LED shows state; display says what it does ("—" if unused) |
| ENT | ENTANGLE | K | −1 to +1 / 0 | §6.1.3 |
| INLV | INPUT | k | 0 to 2 / 1 | Input gain |
| MIX | MIX | k | 0 to 1 / 1 | Wet/dry. Programs are mostly fully wet with their own level pots, as on the source, so default is 1 |
| D | — | D | | Bank and program number, program name, POT 1 to 6 names and values, button function and state, entangle source ("LEFT", "JACK" or "—") |

#### Inputs and outputs

| Id | Label | Notes |
|---|---|---|
| IN_L, IN_R | IN L, IN R | R normalled to L |
| POT1_CV … POT6_CV | POT 1 … POT 6 | Trimpot each, default +1 |
| PROG_CV | PROGRAM | 0 to 10 V selects 0 to 15 (adds to knob) |
| BANK_CV | BANK | 0 to 10 V selects A to D |
| BTN_G | BUTTON | Gate acts as holding the button (latching programs toggle on rise) |
| ENT_IN | ENTANGLE IN | Overrides the left neighbour (§6.1.3) |
| ENT_CV | ENTANGLE | Trimpot, adds to ENT |
| OUT_L, OUT_R | OUT L, OUT R | |
| ENT_OUT | ENTANGLE OUT | Mono sum of this core's wet output, for patching entanglement by cable |

#### 6.1.2 Programs

Each program is a short chain of §3.3 blocks. "→" is series; "‖" is parallel summed; "fb" is a feedback path (limited by `fastTanh`). Pots are **POT n: name (kind)**. Defaults: every pot 0.5 unless the program says otherwise; Level pots 0.7; Feedback pots 0.4. Program names are ours, at most 16 characters, and are what the display shows. Stereo: programs are mono-in (L+R)/2 with stereo output from the reverb or chorus blocks; programs without a stereo block decorrelate L/R with a 0.3 ms and 0.7 ms allpass pair.

Bank character: **A** mixed (reverbs with pitch and delay), **B** spaces and warped reverbs, **C** pitch, phase and modulated reverbs, **D** filters, distortion, scanners and oddities.

**Bank A**

| # | Name | Chain | POT 1 … POT 6 | BUTTON |
|---|---|---|---|---|
| A0 | VERB PITCH DLY | PitchShift → delay → Plate | RevTime; Diffusion; LowPass; reverb Level; Pitch; DelayShort | latch: delay output fed back into the pitch shifter (fb 0.5, shimmer) |
| A1 | SIGN DELAYS | two delays A and B, PitchShift in A's fb; out = A·sign(B) blended with A | DelayShort A; Pitch; Feedback A; Feedback B; DelayShort B; Mix (sign blend) | — |
| A2 | ZERO FLANGE PCH | Flanger (through-zero) → PitchShift | Rate; Depth; Feedback; Level; Zero; Pitch | — |
| A3 | PHASE DLY VERB | Phaser(12) → delay → Plate | Rate; Depth (sweep and plate decay together); Coef (resonance); Mix; DelayShort; Feedback | — |
| A4 | CHORUS SPACE | Chorus → Plate, fb from plate to chorus 0.3 | RevTime; LowPass (damping); Depth (chorus); Level (wet); Coef (input allpass g); Mix | — |
| A5 | TWO LFO VERB | Plate with its two tank modulators driven by independent LFOs | Predelay; RevTime; LowPass; Level; Rate (LFO A, mod depth fixed 1 ms); Rate (LFO B) | — |
| A6 | VERB CHORUS PCH | Plate → Chorus, PitchShift ‖ | RevTime; Diffusion; LowPass; Level (reverb); Level (pitch layer); Pitch | latch: chorus rate ×4 |
| A7 | TWIN REVERSE | two reverse delays (Looper-style, windows = delay time) A ‖ B | DelayShort A; Feedback A; DelayShort B; Feedback B; Mix; Level | — |
| A8 | DRIVE CHORUS | Drive → SVF lowpass → Chorus | Gain; LowPass; Q; Level; Rate; Depth | — |
| A9 | TWO LFO DELAY | delay, read position modulated by LFO 1 and LFO 2 (FM: LFO 2 modulates LFO 1 rate) | Rate 1; Depth 1; Feedback; Mix (FM amount); Rate 2; Depth 2 | — |
| A10 | PHASE DLY 24 | Phaser(12) → two delays, 24 allpasses (2 × 12) | Rate; Q (resonance via Coef); Feedback; Mix; DelayShort 1; DelayShort 2 | momentary: allpass coefficients jump to a random set while held |
| A11 | GLITCH VERB | Plate, with a Looper on the input that repeats 30 to 120 ms slices at random | Predelay; RevTime; LowPass; Mix; Rate (slice rate); Feedback (slice repeat probability) | — |
| A12 | RING DELAY | RingMod → delay, fb | RingFreq; Depth; Feedback; DelayShort | (POT 5, 6 unused: display "—") |
| A13 | FLANGE PCH VERB | Flanger → PitchShift → Plate | Rate; Feedback (also plate diffusion); Mix; Depth; Level (plate); Pitch | — |
| A14 | FDN CHORUS SLAP | Fdn8 (size 0.3) ‖ Chorus ‖ slap delay 80 ms | Rate; Depth; Level A (FDN); Level B (slap); Rate B (slap wow) | — |
| A15 | NESTED VERB DLY | NestedAllpass loops → delay → NestedAllpass | RevTime; Diffusion; LowPass; Level; DelayShort; Feedback | — |

**Bank B**

| # | Name | Chain | POT 1 … POT 6 | BUTTON |
|---|---|---|---|---|
| B0 | SHIFT FLANGE AIR | Flanger → two FreqShifts (0 and 1) ‖ → Plate tail | Shift 0; Feedback (buffer depth); Shift 1; Level (shift levels); Zero; Feedback | latch: slow LFO on both shifts ("air") |
| B1 | WARP VERB | Plate with large tank modulation (±2 ms) | RevTime; Diffusion; LowPass; Level; Rate (mod A); Depth (mod B) | — |
| B2 | AM TWO DELAY | sine AM on input → delays A ‖ B | Rate (AM sine); Feedback A; DelayShort A; DelayShort B; Feedback B; Level | — |
| B3 | SKY LOOPS | Plate → delay → Plate, allpass fb loops | Predelay; RevTime; LowPass (damping); Level; Coef (tail mod); Feedback | — |
| B4 | SMALL ROOM DLY | NestedAllpass small room (size 0.3) → delay | RevTime; Diffusion; LowPass; Level; DelayShort; Feedback | — |
| B5 | PHASE DELAY | Phaser(8) → delay | Rate; Depth (sweep range); Coef (resonance); Mix; DelayShort; Feedback | — |
| B6 | VERB DLY VERB | Plate → delay → Plate | RevTime; Diffusion; LowPass; Level; DelayShort; Feedback | latch: tank modulation ×3 |
| B7 | DRIVE SVF VERB | Drive → SVF (resonant) → Plate | RevTime (also drive gain at its top quarter); Diffusion (also SVF frequency fine); Q; Level; LowPass as SVF frequency; Level (output) | — |
| B8 | PITCH TAIL LOOPS | Plate, PitchShift inside the tank's feedback | Predelay; RevTime; LowPass; Level; Depth (tank mod); Pitch (tail) | — |
| B9 | AM DOUBLE DLY | delays A ‖ B, each amplitude-modulated by its own sine, PitchShift on B | DelayShort A; Feedback A; DelayShort B; Feedback B; Rate (AM); Pitch | — |
| B10 | CROSS MOD ROOM | Fdn8 whose line modulators are cross-coupled (line *i* LFO rate modulated by line *i+1* output) | Predelay; RevTime; LowPass; Level; Rate A; Rate B | — |
| B11 | SMALL PITCH TAIL | small Plate (size 0.4) → PitchShift on the tail | RevTime; Diffusion; LowPass; Level; Mix; Pitch | — |
| B12 | BANDPASS SPACE | Plate → Bandpass | RevTime; Diffusion; LowPass; Level; Q; custom 80 Hz to 8 kHz (band frequency) | — |
| B13 | WET MOD LOOPS | NestedAllpass loops, 93% wet fixed, modulated delay inside | RevTime (also mod multiplier); Diffusion; LowPass; Level; Rate; Tilt (bipolar HP/LP, uses TiltFilter) | — |
| B14 | ZERO WARP VERB | through-zero Flanger with separate out/back LFO rates → warped Plate | Rate (out); Rate (back); Feedback; Level; Level (reverb, also mod B); Zero | latch: freezes the plate |
| B15 | TWIN MID VERBS | Plate A (size 1) ‖ Plate B (size 0.6, LFO-modulated) | RevTime A; Diffusion A; LowPass A; Level (A+B); RevTime B (and its LFO rate); Diffusion B | — |

**Bank C**

| # | Name | Chain | POT 1 … POT 6 | BUTTON |
|---|---|---|---|---|
| C0 | MIRROR SHIFTS | two PitchShifts ‖ → two delays (crossed) → Plate | Pitch 0; Feedback B (and pitch level); Pitch 1; Level (shift 1, delay A fb); DelayShort A; DelayShort B | latch: slow modulation of both pitches ±20 cents |
| C1 | RING FLANGE | RingMod → Flanger | RingFreq; Depth (ring); Feedback; Level (mod A); Depth (flange); Rate (mod A) | — |
| C2 | CRYSTAL VERB | Plate, PitchShift +12 in the tank (shimmer) | RevTime; Diffusion; LowPass; Pitch; Rate (mod A coefficients); Depth (mod B) | latch: second shifter at +19 semitones |
| C3 | DOMINO DELAYS | delay A → delay B with PitchShift on B, warp mod on both | DelayShort A; Feedback A; DelayShort B; Feedback B; Pitch; Depth (warp) | — |
| C4 | PHASE VERB | Phaser(8) → Plate | Rate; Depth (sweep); Mix; Coef (resonance); RevTime; LowPass | — |
| C5 | NANO REVERSE | short reverse delay (Looper windows 5 to 120 ms) with modulation | DelayShort (5 to 120 ms mapped); Feedback; Depth (mod A); Rate (mod B) | — |
| C6 | PITCH FLANGE | Flanger → PitchShift | Rate; Feedback; Mix; Depth; Pitch | — |
| C7 | LOWPASS MID VERB | SVF lowpass → mid Plate (size 0.8), modulated | RevTime; Diffusion; LowPass; Level; Rate (mod A); Depth (mod B) | — |
| C8 | SMALL CHORUS VERB | small Plate (0.4) ‖ fb delays → Chorus | RevTime; Diffusion; LowPass; Level; Diffusion (second); Depth (delay mod) | — |
| C9 | CATHEDRAL | large Plate (size 2), long predelay | Predelay; RevTime; LowPass; Level; Rate (mod A); Diffusion (delay B rate) | — |
| C10 | PITCH PREDELAY | Predelay → PitchShift → Plate | Predelay; RevTime; LowPass; Level; Pitch; Rate (pitch wobble) | — |
| C11 | PHASE CHORUS | Phaser(8) → Chorus | Rate; Depth (sweep); Coef (resonance); Mix; Depth (chorus); Level | — |
| C12 | DOUBLE PHASE DLY | Phaser A → delay ‖ Phaser B | Rate A (and delay rate); Feedback A (and sweep range); Coef (resonance); Mix; Rate B; Feedback B | — |
| C13 | MOD DELAY CHORUS | modulated delay → Chorus | Rate; Depth; Level; Mix; DelayShort; Feedback | — |
| C14 | FLANGE VERB MOD | Flanger → Plate | Rate; Depth; Feedback; Level; Zero; RevTime | — |
| C15 | DANGER FDN | Fdn8 with allpass-diffused lines and modulated coefficients, loop gain can exceed 1 into a limiter (warn in the manual: loud) | RevTime; Coef (diffusion); LowPass; Level (k); Rate (mod A); Depth (mod B) | — |

**Bank D**

| # | Name | Chain | POT 1 … POT 6 | BUTTON |
|---|---|---|---|---|
| D0 | NESTED CHORUS | NestedAllpass FDN → Chorus | RevTime; Diffusion; LowPass; Level (chorus); Depth (chorus) | latch: coefficient and chorus rate ×3 |
| D1 | ODD FLANGE PCH | Flanger with PitchShift in its feedback | Rate; Pitch; Feedback; Level | — |
| D2 | BANDPASS DELAY | envelope-following Bandpass → delay | Depth (sensitivity: envelope to band frequency); Q; DelayShort; Rate (mod A) | — |
| D3 | CHORUS VERB FLANGE | Chorus → Plate → Flanger | Rate; Depth; Level; Level B (flange); Depth (mod A; manual note "keep this low"); Feedback | — |
| D4 | RING PHASE DLY | Phaser(8) and delay sharing one rate | Rate; Depth (sweep); Coef (resonance); Mix; Feedback (phaser); Feedback (delay) | — |
| D5 | DOUBLE DLY CHORUS | delays A ‖ B → Chorus | DelayShort A; Feedback A; DelayShort B; Feedback B; Rate (chorus); Depth (chorus) | — |
| D6 | DRIVE VERB FLANGE | Drive → Plate → through-zero Flanger with out/back rates | Rate (out); Rate (back); Feedback; Level; Zero; Coef (mod B) | latch: drive on/off |
| D7 | PINK CLOUDS | three NestedAllpass loops in series | RevTime; Coef (decay); Diffusion; Level | — |
| D8 | TIME SCANNER | Looper: reverse nano looper over the last 50 to 400 ms | Depth (loop length 50 to 400 ms); Rate (scan speed −2× to +2×); Mix | momentary: hold to capture and loop (source behaviour: "hold button to loop") |
| D9 | PHASE VERB B | Plate → Phaser(8) on the tail | Predelay; RevTime; LowPass; Level; Rate (phase); Depth (phase range) | — |
| D10 | FLANGE DLY MOD | Flanger → delay | Rate; Depth; Feedback; Level; Zero; DelayShort | — |
| D11 | NESTED LOOPS | NestedAllpass reverb with two modulators | RevTime; Diffusion; LowPass; Level; Rate (mod A); Depth (mod B) | — |
| D12 | RING CHORUS VERB | RingMod → Chorus → small Plate | RingFreq; Depth (ring); Rate; Depth (chorus); Level | — |
| D13 | BUFFER FLANGE | Scanner over a 2 s buffer with a moving pointer → Flanger | Feedback (pointer, rate back); Feedback; Depth (manual note "restraint is key"); Zero; Rate (out) | momentary: chaos modulation of the flange while held |
| D14 | TREMOLO VERB | HighPass → Plate → tremolo from two LFOs (one sine, one square) | RevTime; Diffusion; HighPass; Level; Rate 1; Rate 2 | — |
| D15 | RING PHASER | RingMod → Phaser(12) | RingFreq; Depth (ring); Rate; Feedback (phaser); Coef (resonance); Mix (phaser depth) | — |

Unused pots show "—" on the display and do nothing. Tooltips follow the program (§0.5).

#### 6.1.3 Entanglement (this spec's design)

- **Left neighbour.** If the module directly to the left is a TANGLE, it sends its mono wet output (`ENTANGLE OUT`) to this one through Rack's expander messages (one-block latency is fine). `ENTANGLE IN`, when patched, replaces the neighbour.
- **ENTANGLE > 0 (feed):** the entangle signal is added to this core's input at gain `ENT` (up to 1). Four cores side by side become one chained algorithm A → B → C → D, while each core still has its own output.
- **ENTANGLE < 0 (couple):** the entangle signal's envelope (attack 5 ms, release 120 ms, scaled 0 to 1 at 5 V) modulates this core's POT 1 and POT 2 by `±|ENT|·0.5` (POT 1 up, POT 2 down), so one core's activity reshapes another's. Nothing is added to the audio.
- **Ring:** context menu "Entangle from rightmost core" on the leftmost TANGLE closes the loop (rightmost → leftmost), with the usual one-block delay, through `softClamp`.
- Display shows the entangle source and a small level meter.

#### 6.1.4 Tests
- All 64 programs: 10 s pink noise at ±5 V, random pots, then button toggles: finite, within ±12 V. Silence after 30 s of no input falls below −80 dBFS, except C15 at high RevTime, B14 with button, D8 while held.
- Four TANGLEs side by side with ENT = 1 each: the fourth core's input contains the first core's output (correlation test).
- Program change produces no click above −40 dBFS.
- CPU: the heaviest program under the §0.7 budget.

---

### 6.2 WEAVE (WVE): modulators, gates and mixer

#### 6.2.1 Twelve complex modulators

Twelve identical modulators in a 3 × 4 grid, each with RATE and SHAPE knobs and its own output. Modulators are coupled in a chain: modulator *i* frequency-modulates modulator *i+1* by the global COUPLE amount, so they drift in related, non-repeating patterns.

| Id | Label | Type | Range / default |
|---|---|---|---|
| RATE_i | RATE | k | 0.005 to 50 Hz exponential / spread so modulator *i* defaults to `0.05·1.5^(i−1)` Hz |
| SHAPE_i | SHAPE | k | 0 to 1 / 0.2 |
| OUT_i | 1 … 12 | O | ±5 V |
| COUPLE | COUPLE | K | 0 to 1 / 0.2: FM depth of each modulator by the previous one, up to ±2 octaves |
| SPEED | SPEED | K | ×0.01 to ×100 exponential / ×1: global rate multiplier |
| UNI | UNIPOLAR | S | BIPOLAR (±5 V) / UNIPOLAR (0 to 10 V) |
| SPEED_CV | SPEED | I | 1 V/oct on all rates |
| RESET | RESET | I | trigger: all phases to 0, random states reseeded from the patch seed |
| ALL | ALL | O poly | 12 channels |

SHAPE morphs continuously through: 0.0 sine → 0.2 triangle → 0.4 ramp → 0.55 stepped random (sample and hold) → 0.7 smooth random → 0.85 logistic chaos (`x ← r·x·(1−x)` stepped at RATE, `r = 3.57 + 0.43·(SHAPE−0.85)/0.15`, cosine-smoothed) → 1.0 Lorenz x-axis (integrated at a speed set by RATE, scaled to ±5 V). Neighbouring shapes crossfade.

#### 6.2.2 Four voltage-controlled gates

| Id | Label | Type | Notes |
|---|---|---|---|
| GIN_j | IN | I | Audio |
| GCV_j | CV | I | 0 to 10 V; normalled to modulator *j·3* (3, 6, 9, 12) so the gates move with nothing patched |
| GLEV_j | LEVEL | k | 0 to 1 / 1: CV amount (with nothing in CV and normal disabled by context menu, the gate is a plain level) |
| GRESP_j | RESPONSE | S 3-pos | VCA / LPG / BOTH |
| GOUT_j | OUT | O | |

VCA: `out = in · (cv/10)²`. LPG: vactrol-like: CV drives a one-pole "vactrol" (rise 5 ms, fall 150 ms, nonlinear: fall slows as level drops) that sets both amplitude and a 2-pole lowpass from 80 Hz to 18 kHz; BOTH: LPG filtering with VCA amplitude.

#### 6.2.3 Four-input pan mixer

| Id | Label | Type | Notes |
|---|---|---|---|
| MIN_j | MIX IN | I | Normalled from GOUT_j |
| MLEV_j | LEVEL | k | 0 to 1 / 0.7 |
| MPAN_j | PAN | k | −1 to +1 / −0.6, −0.2, +0.2, +0.6 |
| MPAN_CV_j | PAN CV | I | ±5 V = ±1 |
| MOUT_L, MOUT_R | MIX L, MIX R | O | Equal-power pan, sum soft-clamped |

#### 6.2.4 Tests
Modulator rates within 1% at SHAPE 0 across sample rates; COUPLE 0 gives independent modulators; chaos and Lorenz shapes bounded ±5 V for 10 minutes of simulated time; LPG decay audibly slower than VCA (fall time to −40 dB ≥ 120 ms).

---

## 7. EQUATION family (after model-Q-2)

model-Q-2 is described as "built on 10 algorithmic equations capable of infinite variable polyrhythmic pulses", with 8 phase-modulation oscillators grouped as voices W, X, Y and Z, a "DSP algorithmic matrix", a stereo Chebyshev (2nd kind) waveshaper with 40 predefined 10-harmonic polynomials that can be interpolated and scanned, and six complex modulation sources. Its app shows modes called voice allocation, drum assign, pulse, equation, algorithm B, channel assign and complexity. None of the equations are published. AXIOM, TETRA and CHEB are this spec's design in that shape; WEAVE (§6.2) provides the modulators.

### 7.1 AXIOM (AXM): ten equations, four pulse channels

Four channels named W, X, Y and Z. Each picks one of ten equations and turns it into a gate stream, an accent CV and a pitch CV. Channels run at their own clock ratios, so they form polyrhythms against each other.

#### Controls (global)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| TEMPO | TEMPO | K | 20 to 300 BPM / 110 | Internal clock (16th-note steps) used when CLOCK is unpatched |
| RUN | RUN | B latching, LED | on | |
| COMPLEX | COMPLEXITY | K | 0 to 1 / 0 | §7.1.3 |
| LOGIC | LOGIC | S 3-pos via snapped knob | OFF / XOR / AND / OR (4 positions), default OFF | Algorithm B: each channel's gate combined with the channel to its left (W with Z) |
| RANGE | RANGE | K | 0 to 5 octaves / 2 | Pitch CV span |
| SCALE | SCALE | K snapped | CHROMATIC, MAJOR, MINOR, DORIAN, PENTATONIC, WHOLE TONE, HARMONIC (7 partials 1 to 7 as 1 V/oct log2) / PENTATONIC | |
| SEED | SEED | B | | New random seed for equations that use randomness; seed saved with the patch |

#### Controls (per channel *c* = W, X, Y, Z)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| EQ_c | EQUATION | K snapped | 1 to 10 / W 1, X 2, Y 4, Z 6 | Names on display |
| LEN_c | LENGTH | K snapped | 1 to 64, then ∞ / 16 | Steps before the equation restarts (∞ = free run) |
| A_c | A | k | 0 to 1 / per equation | Meaning per equation, named on display |
| B_c | B | k | 0 to 1 / per equation | |
| RATIO_c | RATIO | k snapped | 1/4, 1/3, 1/2, 2/3, 3/4, 1, 4/3, 3/2, 5/4, 2, 3, 4 / 1 | Channel step rate relative to the clock (fractional ratios accumulate phase, giving true polyrhythms) |
| ROT_c | ROTATE | k snapped | 0 to 63 / 0 | Shifts the pattern start |
| L_c | — | L | | Gate light |

#### Inputs and outputs

| Id | Label | Notes |
|---|---|---|
| CLK | CLOCK | External step clock (each rise = one 16th) |
| RST | RESET | All channels to step 0 and equation state reset |
| A_CV_c, B_CV_c | A, B | Per channel, trimpots |
| EQ_CV | EQUATION | Poly, per channel: 0 to 10 V adds 0 to 9 |
| GATE_c | GATE | O, 10 V, length 50% of the channel's step (or a 5 ms trigger: context menu) |
| ACC_c | ACCENT | O, 0 to 10 V |
| PITCH_c | PITCH | O, 1 V/oct |
| POLY_GATE, POLY_PITCH, POLY_ACC | GATES, PITCHES, ACCENTS | O poly, 4 channels W X Y Z |

#### 7.1.2 The ten equations

Step index `i` counts from 0 at reset, `j = (i + ROT) mod LEN` (with LEN ∞, `j = i + ROT`). Each equation yields `gate` (bool) and `v` in 0..1 (used for accent and pitch). `L = LEN` (16 when ∞, for the formulas that need a length).

| # | Name | A | B | Definition |
|---|---|---|---|---|
| 1 | EUCLID | PULSES (0..L) | ACCENT EVERY | Bjorklund: `k = round(A·L)` pulses over L steps. `v` = 1 on every `1 + round(B·7)`-th pulse, else 0.5 |
| 2 | RATIO | P (1..16) | Q (1..16) | Polyrhythm p against q: gate when `floor(j·p/q)` increments. `v = (j·p mod q)/q` |
| 3 | LOGISTIC | R (3.4..4.0) | THRESHOLD | `x ← r·x·(1−x)` per step, `x0` from seed, reset to `x0` at the start of each length (so LEN makes it repeat). Gate when `x > B`. `v = x` |
| 4 | MODULO | MULTIPLIER (1..31) | WINDOW | Gate when `(j·m) mod L < round(B·L)`, `m = 1 + round(A·30)`. `v = ((j·m) mod L)/L` |
| 5 | COLLATZ | START (1..999) | ODD/EVEN | `n` from START, `n ← n/2` if even else `3n+1`, restarting at START when `n = 1` or at the length. Gate when `n` odd (B < 0.5) or even (B ≥ 0.5). `v = log2(n)/log2(max seen)` |
| 6 | GOLDEN | ALPHA (0.01..0.99; 0.5 maps to 0.618) | OFFSET | Sturmian / Beatty word: gate when `floor((j+1)·α + B) − floor(j·α + B) = 1`. `v = frac(j·α)` |
| 7 | PRIMES | STRIDE (1..9) | OFFSET (0..L) | Gate when `j·s + o` is prime. `v` = position of that prime among the first 64 primes / 64 |
| 8 | AUTOMATON | RULE (0..255) | CELL (0..15) | Elementary cellular automaton, 16 cells in a ring, seeded from the patch seed, one generation per step, reseeded at each length. Gate = cell B. `v` = row value / 65535 |
| 9 | GRAY | BIT (0..5) | INVERT/MIX | Gray code `g = j ^ (j >> 1)`; gate = bit A of g (inverted if B > 0.5). `v` = g / 63 (masked to 6 bits) |
| 10 | LORENZ | SPEED | THRESHOLD | Lorenz system (σ 10, ρ 28, β 8/3) integrated with dt `0.002 + 0.02·A` per step (RK4, 4 substeps). Gate when x crosses `(B − 0.5)·20` upward. `v = (z − 5)/40` clamped. Reset to the seeded initial point at each length unless LEN ∞ |

Pitch: `PITCH = quantise(v·RANGE, SCALE)`. Accent: `ACC = 10·v` on gate steps, held between.

#### 7.1.3 Complexity

COMPLEXITY expands each equation (the source's "expanded algorithm set"):
- 0 to 0.33: plain equations.
- above 0.33: each channel's A parameter is modulated each step by the previous channel's `v` (Z feeds W) at depth `(COMPLEX − 0.33)·0.6`.
- above 0.66: additionally, at each length wrap, the channel's ROTATE advances by its own last `v·L` (patterns walk).
- The display shows a complexity tier "1", "2" or "3".

#### 7.1.4 Tests
- EUCLID with L 16, k 5 gives the pattern `x..x..x..x..x...` up to rotation.
- RATIO 3 against 2 gives three gates in two cycles of L.
- Each equation is periodic with period LEN when LEN is finite, COMPLEXITY 0 and RATIO 1.
- LORENZ and LOGISTIC stay finite for 10⁶ steps.

---

### 7.2 TETRA (TET): four phase-modulation voices

Four voices W, X, Y and Z, each a two-operator phase-modulation pair (eight oscillators in all), playable as pitched voices or as drums, with a cross-modulation matrix between voices and a stereo mix.

#### Controls (per voice *c* = W, X, Y, Z, four columns)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| PITCH_c | PITCH | K | −4 to +4 octaves / W 0, X −1, Y +1, Z −2 | Added to V/OCT |
| RATIO_c | RATIO | k snapped | 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7, 1.414, 2.76, 3.14, 5.19, 7.13 / W 2, X 1, Y 3.14, Z 1.414 | Modulator to carrier frequency ratio (last five inharmonic, for metals and bells) |
| INDEX_c | INDEX | K | 0 to 8 radians / 2 | PM index |
| FB_c | FEEDBACK | k | 0 to 1.5 / 0.2 | Modulator self-feedback (averaged two-sample, DX-style) |
| DECAY_c | DECAY | k | 5 ms to 5 s exponential / 300 ms | Amplitude envelope decay |
| PENV_c | PITCH ENV | k | 0 to 4 octaves / 0 (DRUM mode default 1.5) | Pitch drop at the attack, decaying at DECAY/4 |
| IENV_c | INDEX ENV | k | −1 to +1 / 0.5 | How much the index follows the envelope (negative: index rises as the note dies) |
| PAN_c | PAN | T | −1 to +1 / W −0.5, X +0.5, Y −0.2, Z +0.2 | |

#### Controls (global)

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| MODE | MODE | S | VOICE / DRUM | VOICE: attack 3 ms, sustain at 1 while gate high, DECAY as release. DRUM: trigger only, attack 0.5 ms, exponential decay, PITCH ENV active, a short noise click mixed at the attack (2 ms, −12 dB) |
| ROUTE | MATRIX ROUTE | K snapped | 1 to 8 / 1 | Routing of cross-modulation between voices (below) |
| MATRIX | MATRIX DEPTH | K | 0 to 1 / 0 | Depth of cross-PM between voices, up to index 4 |
| SPREAD | DETUNE | k | 0 to 50 cents / 0 | Each voice's carrier detuned by ±SPREAD in a fixed pattern |
| LEVEL | LEVEL | K | 0 to 1 / 0.8 | |

Matrix routes (source voice's carrier output phase-modulates destination voice's carrier):
1 NONE; 2 RING W→X→Y→Z→W; 3 PAIRS W↔X, Y↔Z; 4 TO W (X, Y, Z → W); 5 CASCADE W→X→Y→Z; 6 FROM W (W → X, Y, Z); 7 REVERSE RING W→Z→Y→X→W; 8 MESH (every voice to every other at depth/3).
Cross-modulation uses each source's output from the previous sample.

#### Inputs and outputs

| Id | Label | Notes |
|---|---|---|
| TRIG_c | TRIG | Gate/trigger per voice. Unpatched triggers are silent (no normalling). A poly cable in TRIG W spreads channels 1 to 4 over W, X, Y, Z |
| VOCT_c | V/OCT | Normalled down the row: X from W, Y from X, Z from Y. A poly cable in V/OCT W spreads like TRIG W |
| ACC_c | ACCENT | 0 to 10 V: scales level (0.4 to 1) and index (0.5× to 1.5×). Poly in ACC W spreads |
| INDEX_CV | INDEX | Poly, per voice, trimpot |
| MATRIX_CV | MATRIX | trimpot |
| OUT_c | OUT | O per voice, mono, post-envelope |
| OUT_L, OUT_R | LEFT, RIGHT | Panned mix |

Patch AXIOM's GATES → TETRA TRIG W, PITCHES → V/OCT W, ACCENTS → ACCENT W for the full model-Q-2-style pair (four cables carry everything).

#### Tests
- INDEX 0 gives a pure sine at the carrier pitch (THD < −80 dB).
- RATIO 1, INDEX 1 gives the first sideband at the expected level (Bessel J1(1) ≈ 0.44 relative).
- MATRIX 1 with MESH and FEEDBACK 1.5 stays bounded.
- DRUM mode: DECAY is defined as the time to −60 dB; measured decay within 5% of it.

---

### 7.3 CHEB (CHB): stereo Chebyshev waveshaper

A stereo waveshaper built from Chebyshev polynomials of the **second kind**, `U_n`, as the source says. 40 predefined polynomials, each a weighted sum of 10 Chebyshev terms; SCAN moves smoothly between neighbours, SPREAD offsets the right channel.

#### Model
- `U_0(x) = 1`, `U_1(x) = 2x`, `U_{n+1}(x) = 2x·U_n(x) − U_{n−1}(x)`. For `x = cos θ`, `U_n = sin((n+1)θ)/sin θ`, which for a sine input gives a family of odd and even harmonics up to *n+1* (not a single harmonic as with the first kind; that is the character of this shaper).
- Shaper *k* (0 to 39): `S_k(x) = Σ_{n=1..10} w_{k,n}·U_n(x)` (U_0 omitted to avoid DC), normalised so `max |S_k(x)|` over x ∈ [−1, 1] is 1 (computed on a 4097-point grid at generation).
- Weights `w_{k,n}` are generated by `scripts/gen_cheb_tables.py` into `src/dsp/ChebTables.hpp` (committed), in five families of eight:
  - 0 to 7 **SINGLE**: `w_n = 1` for `n = k+1`, others 0 (U_1 … U_8).
  - 8 to 15 **DECAY**: `w_n = n^(−p)`, p = 0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3.
  - 16 to 23 **ODD/EVEN**: 16 to 19 odd n only with `n^(−p)`, p = 0.5, 1, 1.5, 2; 20 to 23 even n only, same p.
  - 24 to 31 **FORMANT**: a Gaussian bump over n centred at `c = 2 + (k−24)` (2 to 9), width 1.2: `w_n = exp(−((n−c)/1.2)²)`.
  - 32 to 39 **RANDOM**: fixed pseudo-random weights uniform in [−1, 1] times `n^(−0.5)`, from a seeded generator (seed `1000 + k`) so they never change between builds.
- Scanning: position `s` ∈ [0, 39], `S(x) = (1−f)·S_floor(x) + f·S_ceil(x)` (weights interpolated, so one evaluation per sample using the Clenshaw recurrence).
- Input conditioning: `x = clamp(DRIVE·in/5 + BIAS, −1, 1)` using a smooth clip (`fastTanh` scaled to reach ±1 at ±1.2) so the polynomial is never evaluated outside [−1, 1].
- ×4 oversampling around the shaper (polynomial up to degree 10, plus the clip). DC blocker on the output.

#### Controls

| Id | Label | Type | Range / default | Notes |
|---|---|---|---|---|
| SCAN | SCAN | K | 0 to 39 / 9 | Display shows the shaper number and family name |
| SPREAD | SPREAD | K | −8 to +8 / 0 | Right channel scan = SCAN + SPREAD (wrapped into 0..39) |
| DRIVE | DRIVE | K | 0.1 to 4, exponential / 1 | |
| BIAS | BIAS | k | −0.5 to +0.5 / 0 | DC offset before shaping: brings in even content |
| MIX | MIX | k | 0 to 1 / 1 | |
| LEVEL | LEVEL | k | 0 to 2 / 1 | Output gain (shaper output ×5 V × LEVEL) |
| D | — | D | | Transfer curve of the current L and R shapers |

| Id | Label | Notes |
|---|---|---|
| IN_L, IN_R | IN L, IN R | R normalled to L |
| SCAN_CV, SPREAD_CV, DRIVE_CV, BIAS_CV | SCAN, SPREAD, DRIVE, BIAS | trimpots; SCAN at 10 V full range |
| OUT_L, OUT_R | OUT L, OUT R | |

#### Tests
- Shaper 0 (U_1 = 2x) with DRIVE giving a full-scale sine: output is a sine at the input frequency.
- Shaper 1 (U_2 = 4x² − 1): output contains DC-blocked 2nd harmonic only.
- SCAN sweep 0 → 39 over 1 s is click-free (no sample-to-sample jump above 0.5 V for a 100 Hz sine input).
- Generated tables match the script byte for byte (the test recomputes them).

---

## 8. Leaning on existing VCV modules

Do not build these; mention them in the manual where they help:

| Need | Existing module | Why |
|---|---|---|
| Gesture control from a camera or hands | any MIDI controller via VCV MIDI-Map or Stoermelder MIDI-CAT, then TRACE | The originals use camera hand tracking; in Rack, a controller plus TRACE recording is the equivalent |
| MIDI note looping (Programma 900 has a 3000-note MIDI looper) | VCV MIDI-CV with any recorder, or Stoermelder/Count Modula sequencers | Not worth rebuilding |
| Final mastering reverb or a large hall after TANGLE | Valley Plateau, Surge XT reverbs | TANGLE programs are characterful, not neutral |
| Plain mixing | MindMeld MixMaster, VCV Mixer | WEAVE's mixer is only four channels |
| Recording stems to perform later | VCV Recorder | Matches the "generate first, perform later" way of working |

---

## 9. Tests (`tests/`)

For every module, the tests in its section, plus:
- **Stability:** 10 s of white noise at ±10 V into every input with random knob settings at 44.1, 48, 96 and 192 kHz: all outputs finite and within ±12 V. Repeat with each module's main output fed back into its first input through one sample of delay.
- **Silence:** with nothing patched and default knobs, NONET, TANGLE and CHEB output exactly 0 V after their tails decay (flush denormals). HARMONIA (drone), WEAVE (modulators), AXIOM (internal clock) are generators by design; TETRA is silent until triggered; TRACE outputs 0 V while asleep.
- **Sample-rate independence:** pitches, rates, delay times and envelope times within 1% across the four rates.
- **CPU:** a host benchmark per module (heaviest setting) at 48 kHz printed by `make test`, checked against §0.7.

In Rack itself (if a Rack build is possible): load every module, run each manual recipe, change sample rate while running, save and reload (TRACE loops and HARMONIA morph banks survive), check CPU meters, place four TANGLEs side by side and confirm entanglement.

---

## 10. Manual (`docs/MANUAL.md` and `docs/Density-Manual.pdf`)

`scripts/make_manual_pdf.sh` builds the PDF from Markdown (pandoc with whatever engine CI can install); commit the PDF.

Structure:
1. Introduction: what Density is; a short paragraph on each of the three Destiny+ instruments that inspired it, stating plainly that these are independent designs, not emulations, and that Destiny+ is not involved.
2. Levels and conventions; the P1 to P6 / POT 1 to POT 6 display exception.
3. One section per module: what it is, a control table, and two or three patches as cable lists. TANGLE gets a full table of the 64 programs with pot meanings.
4. **Patch recipes** (cable list, knob settings, what to listen for):
   - *Harmonic drone:* HARMONIA alone in DRONE; TRACE lanes 1 to 3 recorded by hand into LOC A, LOC B, LOC C CV; lane 4 into TILT CV; HARMONIA L/R → NONET algorithm 3 (HADAMARD SPACE) mode 2.
   - *Morphing chord:* store two spectra in A and B, record a slow MORPH gesture on TRACE lane 5, ENVELOPE CYCLE with SPREAD 0.6.
   - *Stretched memory:* any source → NONET algorithm 7 (GRAIN CLOUD), push STRETCH past halfway, wait 12 s, then play POSITION and PITCH.
   - *Four entangled cores:* four TANGLEs side by side (banks A, B, C, D), ENTANGLE +0.6 on B, C, D; WEAVE modulators 1 to 6 into POT CVs; outputs into WEAVE's gates and mixer.
   - *Coupled cores:* the same with ENTANGLE −0.8: cores reshape each other without mixing.
   - *Equation rhythm:* AXIOM GATES/PITCHES/ACCENTS → TETRA TRIG W/V/OCT W/ACCENT W in DRUM mode; TETRA L/R → CHEB (SCAN modulated by WEAVE modulator 2) → TANGLE bank D.
   - *Polyrhythmic melody:* AXIOM ratios 1, 3/2, 4/3, 5/4 with RATIO and GOLDEN equations; TETRA in VOICE mode, MATRIX ROUTE 2 at 0.3; NONET algorithm 4 (SHIFTING SPACE).
   - *Self-playing system:* everything above together; record stems; perform later.
5. Credits and sources (§12).

---

## 11. Order of work

1. Copy this file to `docs/BUILD-INSTRUCTIONS.md` and the feasibility study to `docs/RESEARCH.md`. Repo skeleton, Makefile, CI workflow (macOS arm64), components, panel script, one empty module that loads.
2. Shared DSP (§3.2) and effect blocks (§3.3) with tests.
3. CHEB (small, validates oversampling and tables), then AXIOM and TETRA.
4. WEAVE.
5. NONET, then TANGLE with all 64 programs (shared blocks make most programs a few lines).
6. TRACE, then HARMONIA.
7. All panels drawn, rendered and inspected with the label check. Manual and PDF. Full test pass. CI green.
8. Hand-over note: what was built, the contents of `DECISIONS.md`, what could and could not be verified, install steps.

---

## 12. Accuracy ledger

**From sources (behaviour or structure reproduced):**
- Programma 900 2.0 (v2 manual): six voices with odd harmonics, even harmonics, partial count (0 to over 580), tilt, fundamental tuning and octave sliders; multiplication (self-modulation) per voice; harmonic lock making tilts follow voice 1; two morph banks; dice that spares tuning and buffers; global four-point envelope with two modes (A/D/S/R and rate/spread/fm-fade/amplitude) and one-shot / gate-cycle types; voice-assign modes stack, round robin and channel; location A/B/C with voices 2 and 5 on A, 1 and 3 on B, 4 and 6 on C; CV buffers with wake, hold-to-record, release-to-loop, double-press clear, a static voltage when awake, and offset/attenuate/speed on loops; nine effects with the parameter lists used in §5.3.2 (long regenerating delay, 16-pole phaser, Hadamard FDN, Hilbert/frequency-shifted FDN, phaser with seven nested allpasses, time-stretching through-zero flanger, granular, buffer scanner into FDN, self-modulating through-zero scanning); three modes per effect; record-past-halfway, loop-after-12-s, clear-at-zero behaviour.
- Double Q (quick start): slow-down 2×/4×/8×, random phase and random sub-loop on loops (used in TRACE).
- entanglement space (product page): four cores of 16 programs, the program types and pot assignments condensed in §6.1.2, buttons on some programs, 12 modulators, 4 voltage-controlled gates, 4-input pan mixer with stereo out.
- model-Q-2 (product page and app labels): 10 equations for polyrhythmic pulses, 8 PM oscillators as voices W/X/Y/Z, a matrix, Chebyshev 2nd-kind stereo waveshaper with 40 predefined 10-harmonic polynomials that are interpolated and scanned, six complex modulators, modes named drum assign, voice allocation, algorithm B, complexity.

**This spec's own design (not documented by the maker):**
- Every formula, range, default and constant; the additive table method; all effect topologies beyond the type names; TANGLE's 64 program chains and names; the entanglement mechanism; WEAVE's modulator shapes and gate behaviour; AXIOM's ten equations; TETRA's matrix routes; CHEB's 40 weight tables.
- Programma 900 hold-to-store tuning (long press of harmonic lock) is not implemented: its function is unclear in the manual. Note it in `DECISIONS.md`.

**Sources:**
- Destiny+ site: https://www.destiny-plus.com/ (Programma 900 2.0, entanglement space, model-Q-2, Double Q pages)
- Programma 900 v2 manual: https://www.destiny-plus.com/downloads/p900v2-manual.pdf
- Double Q quick start: https://www.destiny-plus.com/downloads/doubleqquickstartguide.pdf
- model-Q-2 control surface app: https://www.destiny-plus.com/apps/model-q-2/index.html
- Demo video index: https://www.matrixsynth.com/search/label/Destiny%20Plus
- Jon Dattorro, "Effect Design Part 1: Reverberator and Other Filters", JAES 1997 (plate topology)
- Olli Niemitalo, Hilbert transformer allpass coefficients
