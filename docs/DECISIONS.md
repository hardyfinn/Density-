# Decisions made during the build

Where the spec left a choice open, or where building it showed a better way, this is what was chosen and why.

## Panels

1. **Widths.** HARMONIA is 56 HP (spec 44), TANGLE 22 HP (spec 16) and AXIOM 28 HP (spec 22). At the spec's widths the required controls, jacks and labels could not meet the spacing rules of §4.3 (sliders 9 mm apart, knobs 14 mm, jacks 10 mm). The others keep their widths: TRACE 26, NONET 18, WEAVE 32, TETRA 30, CHEB 10.
2. **Grid labels.** TRACE's eight lanes and WEAVE's gate and mixer rows use one column header per column (and lane numbers 1 to 8) instead of a label under every control, which would not fit at any reasonable width. The panel script exempts exactly these grid items from its "every control has a label" check; every other control has its own printed label.
3. **Font.** Inter SemiBold (SIL OFL) instead of Jost, which was not available on the build machine. Text is converted to paths, so the font is not shipped.
4. **HARMONIA MULT buttons** sit as a seventh column at the right of each voice strip. The global controls are grouped as ENVELOPE (sliders and the four mode-2 knobs), PLAY (switches and buttons) and a bottom row of knobs and jacks.
5. **AXIOM LOGIC** is a four-position snapped knob (OFF, XOR, AND, OR), as the spec's table allows ("4 positions").
6. **AXIOM channel layout:** LENGTH, RATIO, ROTATE on the top row and A, EQUATION, B below, so that the A and B CV trimpots sit directly under A and B.

## Behaviour

7. **Loading defaults.** Turning NONET's ALGORITHM knob or TANGLE's BANK/PROGRAM knobs sets P1 to P6 / POT 1 to POT 6 to that algorithm's or program's defaults, so each one starts as designed. Changes made by CV never move the knobs. Both modules have a context-menu switch to turn this off, and NONET has "Set P1 to P6 to this algorithm's defaults". The knobs' double-click defaults also follow the current algorithm or program. A loaded patch keeps its saved knob positions.
8. **NONET long-delay buffer.** Allocated on module creation and, when the menu setting changes, on the UI thread; the audio thread only swaps pointers. The old buffer is freed on the UI thread later. Clearing it (RESTART, algorithm changes) zeroes 1024 samples per audio sample, oldest first, so clearing 70 MB never stalls the audio thread; the 10 ms fade-in after a change covers the first moments. The buffer length is fixed in samples (6 min at 48 kHz), so at 96 kHz it holds 3 minutes; the display shows the actual length.
9. **NONET CLOCK and LFO sync.** With a clock patched, P1 RATE of the LFO algorithms (2 SIXTEEN PHASER, 5 PHASER SPACE, 6 STRETCH FLANGER) selects a multiple of the clock rate from the same 1/16 to 64 list as LONG DELAY's CLOCK mode. Other algorithms ignore the clock.
10. **NONET ALG CV** adds 0 to 8 to the knob over 0 to 10 V (so with the knob at 1 the full range is reachable), clamped at 9.
11. **TANGLE CV ranges.** ENTANGLE CV: ±5 V = ±1. PROGRAM CV adds 0 to 15 and BANK CV 0 to 3 over 0 to 10 V. Pot CVs: 10 V = full range. POT values are smoothed over 10 ms.
12. **TANGLE ring** walks right through adjacent TANGLEs to find the rightmost and reads its last output sample through softClamp.
13. **TANGLE button** latch state resets to off on a program change.
14. **TRACE** saves a take that is still recording as a loop. SPEED has a dead zone of ±0.06 (in log2 units) around 1× as its detent. ASLEEP lanes output exactly 0 V (OFFSET does not apply).
15. **WEAVE gate normals** use the unipolar form of the modulator (0 to 10 V) whatever the BIPOLAR/UNIPOLAR switch says, so the gates always open and close fully. Context menu: "Gate CVs normalled to modulators 3, 6, 9, 12" (on); with it off and nothing in CV, LEVEL is a plain level.
16. **WEAVE LPG.** Vactrol fall time constant `50 ms × (1 + 2·(1 − level))` and output level = vactrol², giving a fall to −40 dB of about 0.3 s (spec test: at least 120 ms) while still closing completely within a second. The first attempt (150 ms × (1 + 3·(1 − level))) never fell below −40 dB within a second, which made the LPG useless for percussion.
17. **WEAVE seed** is random per new module and saved with the patch; "New random seed" in the context menu.
18. **AXIOM RATIO equation** is the union of two evenly spaced trains of P and Q pulses over the length (accent when both coincide), which is what "p against q" sounds like as a gate stream; the spec's increment formula only produces one train.
19. **AXIOM GOLDEN** maps ALPHA so that 50% is exactly 0.618 (golden ratio), linear either side between 0.01 and 0.99.
20. **AXIOM EQUATION CV:** a mono cable applies to all four channels; a poly cable applies channel *i* to channel *i* (the last channel repeats if fewer than four).
21. **AXIOM** defaults: channel W's A is 5/16 so EUCLID starts as 5 pulses in 16; RUN is on.
22. **TETRA PITCH ENV** default is 0 in both modes; switching MODE does not move the knob. The manual suggests 1.5 for kicks.
23. **TETRA V/OCT normalling:** each unpatched V/OCT takes the voltage of the voice to its left (or channel *i* of a poly cable in V/OCT W). Unpatched TRIG stays silent.
24. **HARMONIA** CVs: ODD/EVEN/PARTIALS 10 V = full range, TILT ±5 V = ±1, MORPH 10 V = full range, LOC ±5 V = ±1. Spectral parameters and envelope settings are read every 16 samples; V/OCT and gates every sample.
25. **HARMONIA hold-to-store tuning** (a long press of harmonic lock on the source) is not implemented: its function is unclear in the source manual.
26. **CHEB** reads its knobs every 8 samples. The display draws the left shaper in the family colour and the right (SCAN + SPREAD) in grey.

## Build and tests

27. **Tests** use a small built-in TEST/CHECK harness (`tests/check.hpp`) instead of doctest, so `make test` needs nothing downloaded. 36 tests cover the spec's §5 to §7 test lists that can run on the host (equations, shapers, effects stability and decay, stretch buffer, long delay timing, HARMONIA spectra, band limit and CPU, TRACE state machine, WEAVE rates, chaos bounds and LPG).
28. **Not tested on the host:** anything that needs Rack itself (TANGLE entanglement between modules, TRACE save/load through JSON, panel rendering in Rack). The TRACE save format is 16-bit at ±12 V, so its round-trip error is at most 0.37 mV by construction.
29. **CI** builds only the Apple silicon Mac plugin (`macos-latest`, Rack SDK 2.6.4) plus the host tests on Linux, as asked. A tag `v*` attaches the `.vcvplugin` to a GitHub release.
