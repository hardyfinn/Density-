# Destiny+ (Destiny Plus) for VCV Rack: feasibility

Researched 2026-11-15. Question: is there enough public information on any Destiny+ product to make VCV Rack emulations?

## Who Destiny+ is

Destiny Plus (destiny-plus.com) is a small boutique maker whose site credits a "chief design engineer: kirkis" (Instagram @kirkis9000) and links xoramics.com. No full name or country is given. It is **not** Mike Hodnick (Kindohm); that guess was wrong. Production rotates monthly, ships with PSUs, and pauses each December. Instruments are digital (FPGA, ARM, DSP chips) with companion web and macOS apps. **No firmware, source code or schematics are published.** The site's /code page only has short SuperCollider and Python demo snippets.

Consequence for every product: **circuit-accurate emulation is impossible**, and there are no DSP equations to copy. Anything we make is a behavioural, "inspired-by" design built from the documented controls, signal flow and demo videos, using standard DSP techniques.

## Per-product verdicts

| Product | What it is | Public info | Verdict |
|---|---|---|---|
| **Programma 900 2.0** | "Harmonic musical computer": 6 additive voices, 32 gestural CV buffers, 9 DSP effect algorithms, MIDI looper | Full v2 manual: panel, page structure, 6 sliders per voice (odd, even, partial count 0 to 580+, tilt, tune, octave), harmonic lock, morph banks, buffer record/loop state machine, 4-point envelope in 2 modes, voice assign modes, parameter lists for all 9 algorithms (FDN reverbs, 16-pole phasers, through-zero flanger, granular, buffer scanner). Four or more demo videos | **Best candidate.** Behavioural emulation is very feasible: additive synthesis with odd/even/tilt controls and the listed effect topologies are standard DSP. Will not sound identical (no ranges or curves documented) |
| **entanglement space** | Quad-core DSP effects processor, 64 algorithms (4 cores x 16), 12 modulators, 4 VC gates, 4-in pan mixer | Every algorithm named with a one-line type (reverb, phaser, ringmod, pitch delay, etc.) and its pot assignments. Naming like "krt/kfl" and "pot0" suggests Spin FV-1-style chips. "Entanglement" between cores is not explained. Two demo videos | **Good second candidate**, as a 4-slot effects module with a curated set of algorithm types rather than all 64. The pot lists give real parameter structure; sound character is guesswork |
| **Double Q** | Eurorack 29HP camera hand-gesture controller (30 hand points, neural processor), 16 CV outs, 10 min+ loop buffer, MIDI, built-in "Double Luck" synth | Quick-start PDF (buttons, LEDs, 30-point hand map, loop modes, slow-down, random sub-loop). Many demo videos | **Core not emulatable**: the camera and hand tracking are the product. The 16-lane gesture recorder and looper (2x/4x/8x slow-down, random phase, random sub-loop) is doable on its own, but in VCV it would just be a strong multi-lane CV recorder. Double Luck synth is undocumented |
| **model-Q-2** | Small system: 10 "algorithmic equations", 8 phase-modulation oscillators, Chebyshev waveshaper (40 polynomials x 10 harmonics), 6 complex modulators, app/gesture control | Product page summary, app downloads, four demo videos. No equations | **Loose inspired-by only.** A PM-plus-Chebyshev voice is easy to build, but nothing ties it to the real instrument |
| **Czochralski cells** | 6/7-voice configurable complex drum instrument | One signal diagram image and two videos | **Loose inspired-by only** |
| **16 psyche** | Feedback/phase-modulation concept, "10 core configurations" | Mostly video and images | Not enough |
| **arc\*C/\*B** | Web app | Video, a quick-start on Google Drive | Not enough (and it is already software) |
| **easel card concepts** | Cards for the Buchla Music Easel (morphing phaser, triple entanglement space, dual function source) | I/O list for one card, little else | Not enough |
| **rosetta** | Unannounced ("2026") | None | Nothing to go on |

## Recommendation

Worth doing, in order:

1. **Programma 900-inspired additive voice system**: one large voice module (6 harmonic voices, odd/even/partials/tilt, harmonic lock, morph), a gestural CV buffer module, and an FX module with nine effect types following the manual's parameter lists. This is the only product with a real manual, and it matches the "generate material, then perform" approach of the earlier projects.
2. **entanglement-space-inspired quad FX**: four cores, each picking from a curated set of effect types (perhaps 16 to 24 rather than 64), plus modulators, gates and a pan mixer.

Optional small extra: a 16-lane gesture recorder/looper borrowing Double Q's loop features, driven by any CV source.

Not recommended: model-Q-2, Czochralski cells, 16 psyche, arc*C, easel cards, rosetta (too little documented; result would be our invention with their name on it).

## Ethics and naming

Destiny+ is a small, active maker selling these products now. Any modules should use our own names and not the product or algorithm names (e.g. not "Programma 900", "Silver tulips"), be described as "inspired by", and not copy panel graphics. Consider asking the maker (hellodestinyplus@gmail.com) before publishing anything publicly; for private use in Rack this matters less.

## Sources

- destiny-plus.com product pages: Programma 900 2.0, entanglement space, Double Q, model-Q-2, Czochralski cells, 16 psyche, arc*C/*B, easel card concepts, rosetta, contact, code
- Programma 900 v2 manual: https://www.destiny-plus.com/downloads/p900v2-manual.pdf
- Double Q quick start: https://www.destiny-plus.com/downloads/doubleqquickstartguide.pdf
- Schneidersladen Double Q listing: https://schneidersladen.de/en/destiny-plus-double-q
- MATRIXSYNTH Destiny Plus label (demo video index): https://www.matrixsynth.com/search/label/Destiny%20Plus
- Not yet read: model-Q-2 and Programma 900 app zips, arc*C quick start, older MATRIXSYNTH posts
