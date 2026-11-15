// TANGLE program table: 4 banks × 16 programs, our own names, pot kinds and names, button use.
#pragma once
#include "fx/FxParams.hpp"

namespace dens {

enum class Btn { None, Latch, Momentary };

struct TangleProgram {
	const char* name;
	Kind kind[6];
	const char* pot[6];
	Btn btn;
	const char* btnName;
};

#define K(x) Kind::x
#define NO Kind::None
static const TangleProgram TANGLE_PROGRAMS[64] = {
	// ---------------- bank A: mixed reverbs with pitch and delay
	{"VERB PITCH DLY", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Pitch), K(DelayShort)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "REVERB LEVEL", "PITCH", "DELAY"}, Btn::Latch, "SHIMMER LOOP"},
	{"SIGN DELAYS", {K(DelayShort), K(Pitch), K(Feedback), K(Feedback), K(DelayShort), K(Mix)}, {"DELAY A", "PITCH", "FEEDBACK A", "FEEDBACK B", "DELAY B", "SIGN BLEND"}, Btn::None, ""},
	{"ZERO FLANGE PCH", {K(Rate), K(Depth), K(Feedback), K(Level), K(Zero), K(Pitch)}, {"RATE", "DEPTH", "FEEDBACK", "LEVEL", "ZERO POINT", "PITCH"}, Btn::None, ""},
	{"PHASE DLY VERB", {K(Rate), K(Depth), K(Coef), K(Mix), K(DelayShort), K(Feedback)}, {"RATE", "SWEEP+DECAY", "RESONANCE", "MIX", "DELAY", "FEEDBACK"}, Btn::None, ""},
	{"CHORUS SPACE", {K(RevTime), K(LowPass), K(Depth), K(Level), K(Coef), K(Mix)}, {"REVERB TIME", "DAMPING", "CHORUS DEPTH", "WET LEVEL", "INPUT ALLPASS", "MIX"}, Btn::None, ""},
	{"TWO LFO VERB", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Rate), K(Rate)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "LFO A RATE", "LFO B RATE"}, Btn::None, ""},
	{"VERB CHORUS PCH", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Level), K(Pitch)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "REVERB LEVEL", "PITCH LEVEL", "PITCH"}, Btn::Latch, "FAST CHORUS"},
	{"TWIN REVERSE", {K(DelayShort), K(Feedback), K(DelayShort), K(Feedback), K(Mix), K(Level)}, {"WINDOW A", "FEEDBACK A", "WINDOW B", "FEEDBACK B", "A/B BLEND", "LEVEL"}, Btn::None, ""},
	{"DRIVE CHORUS", {K(Gain), K(LowPass), K(Q), K(Level), K(Rate), K(Depth)}, {"DRIVE", "LOWPASS", "RESONANCE", "LEVEL", "CHORUS RATE", "CHORUS DEPTH"}, Btn::None, ""},
	{"TWO LFO DELAY", {K(Rate), K(Depth), K(Feedback), K(Mix), K(Rate), K(Depth)}, {"LFO 1 RATE", "LFO 1 DEPTH", "FEEDBACK", "FM AMOUNT", "LFO 2 RATE", "LFO 2 DEPTH"}, Btn::None, ""},
	{"PHASE DLY 24", {K(Rate), K(Coef), K(Feedback), K(Mix), K(DelayShort), K(DelayShort)}, {"RATE", "RESONANCE", "FEEDBACK", "MIX", "DELAY 1", "DELAY 2"}, Btn::Momentary, "SCRAMBLE"},
	{"GLITCH VERB", {K(Predelay), K(RevTime), K(LowPass), K(Mix), K(Rate), K(Feedback)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "MIX", "SLICE RATE", "REPEAT CHANCE"}, Btn::None, ""},
	{"RING DELAY", {K(RingFreq), K(Depth), K(Feedback), K(DelayShort), NO, NO}, {"RING FREQ", "RING DEPTH", "FEEDBACK", "DELAY", "-", "-"}, Btn::None, ""},
	{"FLANGE PCH VERB", {K(Rate), K(Feedback), K(Mix), K(Depth), K(Level), K(Pitch)}, {"RATE", "FEEDBACK", "MIX", "DEPTH", "REVERB LEVEL", "PITCH"}, Btn::None, ""},
	{"FDN CHORUS SLAP", {K(Rate), K(Depth), K(Level), K(Level), K(Rate), NO}, {"CHORUS RATE", "CHORUS DEPTH", "FDN LEVEL", "SLAP LEVEL", "SLAP WOW", "-"}, Btn::None, ""},
	{"NESTED VERB DLY", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(DelayShort), K(Feedback)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "DELAY", "FEEDBACK"}, Btn::None, ""},
	// ---------------- bank B: spaces and warped reverbs
	{"SHIFT FLANGE AIR", {K(Shift), K(Feedback), K(Shift), K(Level), K(Zero), K(Feedback)}, {"SHIFT 1", "FLANGE FEEDBACK", "SHIFT 2", "SHIFT LEVEL", "ZERO POINT", "TAIL FEEDBACK"}, Btn::Latch, "AIR"},
	{"WARP VERB", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Rate), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "WARP RATE", "WARP DEPTH"}, Btn::None, ""},
	{"AM TWO DELAY", {K(Rate), K(Feedback), K(DelayShort), K(DelayShort), K(Feedback), K(Level)}, {"AM RATE", "FEEDBACK A", "DELAY A", "DELAY B", "FEEDBACK B", "LEVEL"}, Btn::None, ""},
	{"SKY LOOPS", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Coef), K(Feedback)}, {"PREDELAY", "REVERB TIME", "DAMPING", "LEVEL", "TAIL MOD", "FEEDBACK"}, Btn::None, ""},
	{"SMALL ROOM DLY", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(DelayShort), K(Feedback)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "DELAY", "FEEDBACK"}, Btn::None, ""},
	{"PHASE DELAY", {K(Rate), K(Depth), K(Coef), K(Mix), K(DelayShort), K(Feedback)}, {"RATE", "SWEEP", "RESONANCE", "MIX", "DELAY", "FEEDBACK"}, Btn::None, ""},
	{"VERB DLY VERB", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(DelayShort), K(Feedback)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "DELAY", "FEEDBACK"}, Btn::Latch, "DEEP MOD"},
	{"DRIVE SVF VERB", {K(RevTime), K(Diffusion), K(Q), K(Level), K(Hz), K(Level)}, {"REVERB TIME", "DIFFUSION", "RESONANCE", "REVERB LEVEL", "FILTER FREQ", "OUTPUT"}, Btn::None, ""},
	{"PITCH TAIL LOOPS", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Depth), K(Pitch)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "TANK MOD", "TAIL PITCH"}, Btn::None, ""},
	{"AM DOUBLE DLY", {K(DelayShort), K(Feedback), K(DelayShort), K(Feedback), K(Rate), K(Pitch)}, {"DELAY A", "FEEDBACK A", "DELAY B", "FEEDBACK B", "AM RATE", "PITCH B"}, Btn::None, ""},
	{"CROSS MOD ROOM", {K(Predelay), K(RT60), K(LowPass), K(Level), K(Rate), K(Rate)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "MOD RATE A", "MOD RATE B"}, Btn::None, ""},
	{"SMALL PITCH TAIL", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Mix), K(Pitch)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "PITCH MIX", "PITCH"}, Btn::None, ""},
	{"BANDPASS SPACE", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Q), K(Hz)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "BAND Q", "BAND FREQ"}, Btn::None, ""},
	{"WET MOD LOOPS", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Rate), K(Bipolar)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "MOD RATE", "TILT"}, Btn::None, ""},
	{"ZERO WARP VERB", {K(Rate), K(Rate), K(Feedback), K(Level), K(Level), K(Zero)}, {"RATE OUT", "RATE BACK", "FEEDBACK", "LEVEL", "REVERB LEVEL", "ZERO POINT"}, Btn::Latch, "FREEZE"},
	{"TWIN MID VERBS", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(RevTime), K(Diffusion)}, {"TIME A", "DIFFUSION A", "LOWPASS A", "LEVEL", "TIME B", "DIFFUSION B"}, Btn::None, ""},
	// ---------------- bank C: pitch, phase and modulated reverbs
	{"MIRROR SHIFTS", {K(Pitch), K(Feedback), K(Pitch), K(Level), K(DelayShort), K(DelayShort)}, {"PITCH 1", "FEEDBACK B", "PITCH 2", "SHIFT 2 LEVEL", "DELAY A", "DELAY B"}, Btn::Latch, "WOBBLE"},
	{"RING FLANGE", {K(RingFreq), K(Depth), K(Feedback), K(Level), K(Depth), K(Rate)}, {"RING FREQ", "RING DEPTH", "FEEDBACK", "LEVEL", "FLANGE DEPTH", "FLANGE RATE"}, Btn::None, ""},
	{"CRYSTAL VERB", {K(RevTime), K(Diffusion), K(LowPass), K(Pitch), K(Rate), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "SHIMMER PITCH", "MOD RATE", "MOD DEPTH"}, Btn::Latch, "SECOND VOICE"},
	{"DOMINO DELAYS", {K(DelayShort), K(Feedback), K(DelayShort), K(Feedback), K(Pitch), K(Depth)}, {"DELAY A", "FEEDBACK A", "DELAY B", "FEEDBACK B", "PITCH", "WARP"}, Btn::None, ""},
	{"PHASE VERB", {K(Rate), K(Depth), K(Mix), K(Coef), K(RevTime), K(LowPass)}, {"RATE", "SWEEP", "MIX", "RESONANCE", "REVERB TIME", "LOWPASS"}, Btn::None, ""},
	{"NANO REVERSE", {K(Depth), K(Feedback), K(Depth), K(Rate), NO, NO}, {"WINDOW", "FEEDBACK", "MOD DEPTH", "MOD RATE", "-", "-"}, Btn::None, ""},
	{"PITCH FLANGE", {K(Rate), K(Feedback), K(Mix), K(Depth), K(Pitch), NO}, {"RATE", "FEEDBACK", "MIX", "DEPTH", "PITCH", "-"}, Btn::None, ""},
	{"LOWPASS MID VERB", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Rate), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "MOD RATE", "MOD DEPTH"}, Btn::None, ""},
	{"SMALL CHORUS VERB", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Diffusion), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "DIFFUSION 2", "DELAY MOD"}, Btn::None, ""},
	{"CATHEDRAL", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Rate), K(Diffusion)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "MOD RATE", "DIFFUSION"}, Btn::None, ""},
	{"PITCH PREDELAY", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Pitch), K(Rate)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "PITCH", "WOBBLE RATE"}, Btn::None, ""},
	{"PHASE CHORUS", {K(Rate), K(Depth), K(Coef), K(Mix), K(Depth), K(Level)}, {"RATE", "SWEEP", "RESONANCE", "MIX", "CHORUS DEPTH", "LEVEL"}, Btn::None, ""},
	{"DOUBLE PHASE DLY", {K(Rate), K(Feedback), K(Coef), K(Mix), K(Rate), K(Feedback)}, {"RATE A", "FEEDBACK A", "RESONANCE", "MIX", "RATE B", "FEEDBACK B"}, Btn::None, ""},
	{"MOD DELAY CHORUS", {K(Rate), K(Depth), K(Level), K(Mix), K(DelayShort), K(Feedback)}, {"RATE", "DEPTH", "LEVEL", "MIX", "DELAY", "FEEDBACK"}, Btn::None, ""},
	{"FLANGE VERB MOD", {K(Rate), K(Depth), K(Feedback), K(Level), K(Zero), K(RevTime)}, {"RATE", "DEPTH", "FEEDBACK", "LEVEL", "ZERO POINT", "REVERB TIME"}, Btn::None, ""},
	{"DANGER FDN", {K(RT60), K(Coef), K(LowPass), K(Level), K(Rate), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "MOD RATE", "MOD DEPTH"}, Btn::None, ""},
	// ---------------- bank D: filters, distortion, scanners and oddities
	{"NESTED CHORUS", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Depth), NO}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "CHORUS LEVEL", "CHORUS DEPTH", "-"}, Btn::Latch, "FAST"},
	{"ODD FLANGE PCH", {K(Rate), K(Pitch), K(Feedback), K(Level), NO, NO}, {"RATE", "PITCH", "FEEDBACK", "LEVEL", "-", "-"}, Btn::None, ""},
	{"BANDPASS DELAY", {K(Depth), K(Q), K(DelayShort), K(Rate), NO, NO}, {"SENSITIVITY", "Q", "DELAY", "MOD RATE", "-", "-"}, Btn::None, ""},
	{"CHORUS VERB FLANGE", {K(Rate), K(Depth), K(Level), K(Level), K(Depth), K(Feedback)}, {"RATE", "DEPTH", "LEVEL", "FLANGE LEVEL", "MOD (KEEP LOW)", "FEEDBACK"}, Btn::None, ""},
	{"RING PHASE DLY", {K(Rate), K(Depth), K(Coef), K(Mix), K(Feedback), K(Feedback)}, {"RATE", "SWEEP", "RESONANCE", "MIX", "PHASER FEEDBACK", "DELAY FEEDBACK"}, Btn::None, ""},
	{"DOUBLE DLY CHORUS", {K(DelayShort), K(Feedback), K(DelayShort), K(Feedback), K(Rate), K(Depth)}, {"DELAY A", "FEEDBACK A", "DELAY B", "FEEDBACK B", "CHORUS RATE", "CHORUS DEPTH"}, Btn::None, ""},
	{"DRIVE VERB FLANGE", {K(Rate), K(Rate), K(Feedback), K(Level), K(Zero), K(Coef)}, {"RATE OUT", "RATE BACK", "FEEDBACK", "LEVEL", "ZERO POINT", "MOD B"}, Btn::Latch, "DRIVE"},
	{"PINK CLOUDS", {K(RevTime), K(Coef), K(Diffusion), K(Level), NO, NO}, {"REVERB TIME", "DECAY", "DIFFUSION", "LEVEL", "-", "-"}, Btn::None, ""},
	{"TIME SCANNER", {K(Depth), K(Bipolar), K(Mix), NO, NO, NO}, {"LOOP LENGTH", "SCAN SPEED", "MIX", "-", "-", "-"}, Btn::Momentary, "HOLD TO LOOP"},
	{"PHASE VERB B", {K(Predelay), K(RevTime), K(LowPass), K(Level), K(Rate), K(Depth)}, {"PREDELAY", "REVERB TIME", "LOWPASS", "LEVEL", "PHASE RATE", "PHASE RANGE"}, Btn::None, ""},
	{"FLANGE DLY MOD", {K(Rate), K(Depth), K(Feedback), K(Level), K(Zero), K(DelayShort)}, {"RATE", "DEPTH", "FEEDBACK", "LEVEL", "ZERO POINT", "DELAY"}, Btn::None, ""},
	{"NESTED LOOPS", {K(RevTime), K(Diffusion), K(LowPass), K(Level), K(Rate), K(Depth)}, {"REVERB TIME", "DIFFUSION", "LOWPASS", "LEVEL", "MOD RATE", "MOD DEPTH"}, Btn::None, ""},
	{"RING CHORUS VERB", {K(RingFreq), K(Depth), K(Rate), K(Depth), K(Level), NO}, {"RING FREQ", "RING DEPTH", "CHORUS RATE", "CHORUS DEPTH", "LEVEL", "-"}, Btn::None, ""},
	{"BUFFER FLANGE", {K(Rate), K(Feedback), K(Depth), K(Zero), K(Rate), NO}, {"POINTER RATE", "FEEDBACK", "DEPTH (RESTRAINT)", "ZERO POINT", "RATE OUT", "-"}, Btn::Momentary, "CHAOS"},
	{"TREMOLO VERB", {K(RevTime), K(Diffusion), K(HighPass), K(Level), K(Rate), K(Rate)}, {"REVERB TIME", "DIFFUSION", "HIGHPASS", "LEVEL", "TREM RATE 1", "TREM RATE 2"}, Btn::None, ""},
	{"RING PHASER", {K(RingFreq), K(Depth), K(Rate), K(Feedback), K(Coef), K(Mix)}, {"RING FREQ", "RING DEPTH", "PHASER RATE", "PHASER FEEDBACK", "RESONANCE", "PHASER MIX"}, Btn::None, ""},
};
#undef K
#undef NO

static const char TANGLE_BANK_NAMES[4] = {'A', 'B', 'C', 'D'};

} // namespace dens
