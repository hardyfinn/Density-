// TANGLE: one effects core. Programs are short chains of the shared blocks (see TanglePrograms.hpp).
#pragma once
#include "Common.hpp"
#include "Filters.hpp"
#include "Lfo.hpp"
#include "fx/FxParams.hpp"
#include "fx/Reverbs.hpp"
#include "fx/Modulation.hpp"
#include "fx/Buffers.hpp"
#include "TanglePrograms.hpp"

namespace dens {

struct TangleCore {
	float fs = 48000.f;
	int prog = 0;                // 0..63 (bank*16 + program)
	int pending = -1;
	float fade = 1.f; int fadeDir = 0;
	// blocks
	Plate plate, plate2;
	Fdn8 fdn;
	Phaser phA, phB;
	Flanger fl;
	PitchShift ps1, ps2;
	FreqShift fq1, fq2;
	DelayLine dA, dB, pre;
	Chorus ch;
	RingMod rm;
	SineOsc am1, am2;
	Drive drv;
	Svf svf, svf2;
	OnePoleHP hp;
	NestedAllpass na[3];
	AllpassD decor[2];
	ReverseDelay rv1, rv2;
	HoldLooper hold;
	SliceRepeater slicer;
	Scanner scanner;
	EnvFollower ef;
	Lfo l1{101}, l2{103};
	SmoothRandom sr1{107}, sr2{109};
	LogisticLfo chaos;
	float fbA = 0.f, fbB = 0.f, last = 0.f;
	float scramble[16] = {};
	Rng rng{113};
	bool lastBtn = false;
	// plate's pitch-in-tank insert
	float tankPitch = 0.f;
	PitchShift tankPs[2];
	bool tankPsOn = false;

	void init(float sampleRate) {
		fs = sampleRate;
		plate.init(fs); plate2.init(fs);
		plate.tankInsert = [this](int s, float x) { return tankPsOn ? tankPs[s].process(x, tankPitch) : x; };
		for (auto& t : tankPs) t.init(fs, 60.f);
		fdn.init(fs, 3.2f);
		phA.init(fs, 12); phB.init(fs, 8);
		fl.init(fs, 25.f);
		ps1.init(fs); ps2.init(fs);
		dA.init((int)(1.25f * fs)); dB.init((int)(1.25f * fs)); pre.init((int)(0.3f * fs));
		ch.init(fs);
		for (auto& n : na) n.init(60.f, fs);
		decor[0].init(2.f, fs); decor[0].setMs(0.31f, fs); decor[0].g = 0.6f;
		decor[1].init(2.f, fs); decor[1].setMs(0.73f, fs); decor[1].g = 0.6f;
		rv1.init(fs, 1.2f); rv2.init(fs, 1.2f);
		hold.init(fs);
		slicer.init(fs);
		scanner.init(fs);
		ef.set(0.005f, 0.12f, fs);
		hp.set(20.f, fs);
		clear();
	}
	void clear() {
		plate.clear(); plate2.clear(); fdn.clear(); phA.reset(); phB.reset(); fl.clear(); ps1.clear(); ps2.clear();
		fq1.reset(); fq2.reset(); dA.clear(); dB.clear(); pre.clear(); ch.clear(); drv.reset(); svf.reset(); svf2.reset();
		for (auto& n : na) n.clear();
		decor[0].clear(); decor[1].clear(); rv1.clear(); rv2.clear(); hold.clear(); slicer.clear(); scanner.clear();
		for (auto& t : tankPs) t.clear();
		fbA = fbB = last = 0.f; tankPsOn = false;
		fdn.insertOn = false; fdn.freeze = false; fdn.loopGain = 1.f;
		plate.size = 1.f; plate.modDepthMs = 0.3f; plate.modRate = 0.8f; plate.predelayMs = 0.f;
	}
	void select(int p) {
		p = std::max(0, std::min(63, p));
		if (p == prog && pending < 0) return;
		pending = p; fadeDir = -1;
	}
	const TangleProgram& program() const { return TANGLE_PROGRAMS[prog]; }

	// ---- helpers
	void stereo(float y, float& l, float& r) { l = decor[0].process(y); r = decor[1].process(y); }
	float dly(DelayLine& d, float in, float ms) { d.push(softClamp(in, 12.f)); return d.read(std::max(1.f, ms * 0.001f * fs)); }
	void plateRun(Plate& pl, float in, float decay, float diffusion, float damp, float& l, float& r, float size = 1.f, float mod = 0.3f, float preMs = 0.f) {
		pl.decay = decay; pl.diffusion = diffusion / 0.75f; pl.dampHz = damp; pl.size = size; pl.modDepthMs = mod; pl.predelayMs = preMs;
		pl.process(in, in, l, r);
	}

	// q: pots 0..1, btn: button state (latched or held), x: mono input. Fully wet stereo out.
	void process(const float* q, bool btn, float x, float& outL, float& outR) {
		float dt = 1.f / fs;
		if (fadeDir < 0) {
			fade -= dt / 0.01f;
			if (fade <= 0.f) { fade = 0.f; if (pending >= 0) { prog = pending; pending = -1; clear(); } fadeDir = 1; }
		} else if (fadeDir > 0) {
			fade += dt / 0.01f;
			if (fade >= 1.f) { fade = 1.f; fadeDir = 0; }
		}
		const TangleProgram& P = TANGLE_PROGRAMS[prog];
		float v[6];
		for (int i = 0; i < 6; i++) v[i] = mapKind(P.kind[i], q[i]);
		float l = 0.f, r = 0.f;
		bool rise = btn && !lastBtn;
		lastBtn = btn;
		(void)rise;
		tankPsOn = false;
		switch (prog) {
			// ================= bank A
			case 0: {  // VERB PITCH DLY
				float s = ps1.process(x + (btn ? 0.5f * fbA : 0.f), v[4]);
				float d = dly(dA, s, v[5]);
				fbA = d;
				float pl, pr; plateRun(plate, d, v[0], v[1], v[2], pl, pr);
				l = pl * v[3] + d * 0.5f; r = pr * v[3] + d * 0.5f;
			} break;
			case 1: {  // SIGN DELAYS
				float a = dly(dA, x + ps1.process(fbA, v[1]) * v[2], v[0]);
				float b = dly(dB, x + fbB * v[3], v[4]);
				fbA = a; fbB = b;
				float s = std::fabs(a) * (b >= 0.f ? 1.f : -1.f);   // a with the sign of b
				float y = a + (s - a) * v[5] + 0.5f * b;
				stereo(y, l, r);
			} break;
			case 2: {  // ZERO FLANGE PCH
				float m = l1.process(v[0], dt, Lfo::SINE);
				float f = fl.process(x, m, mapKind(Kind::Zero, q[4]), 4.f * v[1], v[2]);
				float y = ps1.process(f, v[5]) * (0.3f + v[3]);
				stereo(y, l, r);
			} break;
			case 3: {  // PHASE DLY VERB
				phA.n = 12; phA.centre = 600.f; phA.depthOct = 5.f * v[1]; phA.spread = 0.3f; phA.resonance = v[2];
				float p = 0.5f * (x + phA.process(x, l1.process(v[0], dt, Lfo::SINE)));
				float d = dly(dA, p + fbA * v[5], v[4]); fbA = d;
				float pl, pr; plateRun(plate, d, 0.4f + 0.55f * q[1], 0.6f, 7000.f, pl, pr);
				l = p + (pl + d * 0.5f - p) * v[3]; r = p + (pr + d * 0.5f - p) * v[3];
			} break;
			case 4: {  // CHORUS SPACE
				float cl, cr; ch.process(x + 0.3f * fbA, 0.6f, 4.f * v[2], cl, cr);
				decor[0].g = v[4];
				float pl, pr; plateRun(plate, 0.5f * (cl + cr), v[0], 0.6f, v[1], pl, pr);
				fbA = 0.5f * (pl + pr);
				l = cl + (pl * (0.3f + v[3]) - cl) * v[5]; r = cr + (pr * (0.3f + v[3]) - cr) * v[5];
			} break;
			case 5: {  // TWO LFO VERB
				plate.modDepthMs = 1.f;
				plate.modRate = v[4];
				float pl, pr; plateRun(plate, x, v[1], 0.6f, v[2], pl, pr, 1.f, 1.f + 0.6f * l2.process(v[5], dt, Lfo::SINE), v[0]);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 6: {  // VERB CHORUS PCH
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr);
				float cl, cr; ch.process(0.5f * (pl + pr), btn ? 2.4f : 0.6f, 3.f, cl, cr);
				float p = ps1.process(x, v[5]);
				l = (pl + cl) * 0.5f * (0.3f + v[3]) + p * v[4]; r = (pr + cr) * 0.5f * (0.3f + v[3]) + p * v[4];
			} break;
			case 7: {  // TWIN REVERSE
				float a = rv1.process(x, v[0], v[1] * 0.9f), b = rv2.process(x, v[2], v[3] * 0.9f);
				l = (a * (1.f - v[4]) + b * v[4] * 0.5f) * (0.3f + v[5]); r = (b * v[4] + a * (1.f - v[4]) * 0.5f) * (0.3f + v[5]);
			} break;
			case 8: {  // DRIVE CHORUS
				float d = drv.process(x, v[0]);
				svf.set(v[1], v[2], fs); svf.process(d);
				ch.process(svf.lp, v[4], 5.f * v[5], l, r);
				l *= (0.3f + v[3]); r *= (0.3f + v[3]);
			} break;
			case 9: {  // TWO LFO DELAY
				float m2 = l2.process(v[4], dt, Lfo::SINE);
				float m1 = l1.process(v[0] * std::pow(2.f, 2.f * v[3] * m2), dt, Lfo::SINE);
				float ms = 120.f + 100.f * v[1] * m1 + 20.f * v[5] * m2;
				float d = dly(dA, x + fbA * v[2], ms); fbA = d;
				stereo(d, l, r);
			} break;
			case 10: {  // PHASE DLY 24
				if (btn && !scrambleHeld) { for (auto& s : scramble) s = rng.uniform(); }
				scrambleHeld = btn;
				phA.n = 12; phA.centre = 500.f; phA.depthOct = 4.f; phA.spread = btn ? 0.9f * scramble[0] + 0.3f : 0.3f; phA.resonance = 0.5f * v[1];
				phB.n = 12; phB.centre = 900.f; phB.depthOct = 4.f; phB.spread = btn ? 0.9f * scramble[1] + 0.3f : 0.5f; phB.resonance = -0.5f * v[1];
				float m = l1.process(v[0], dt, Lfo::SINE);
				float a = dly(dA, phA.process(x + fbA * v[2] * 0.6f, m), v[4]);
				float b = dly(dB, phB.process(x + fbB * v[2] * 0.6f, -m), v[5]);
				fbA = a; fbB = b;
				l = x + (a - x) * v[3]; r = x + (b - x) * v[3];
			} break;
			case 11: {  // GLITCH VERB
				float s = slicer.process(x, 1000.f / std::max(0.5f, v[4] * 10.f), v[5]);
				float pl, pr; plateRun(plate, s, v[1], 0.65f, v[2], pl, pr, 1.f, 0.3f, v[0]);
				l = s + (pl - s) * v[3]; r = s + (pr - s) * v[3];
			} break;
			case 12: {  // RING DELAY
				float rmx = rm.process(x + fbA * v[2], v[0], v[1], fs);
				float d = dly(dA, rmx, v[3]); fbA = d;
				stereo(0.5f * (rmx + d), l, r);
			} break;
			case 13: {  // FLANGE PCH VERB
				float f = fl.process(x, l1.process(v[0], dt, Lfo::SINE), 2.f, 4.f * v[3], v[1]);
				float m = x + (f - x) * v[2];
				float p = ps1.process(m, v[5]);
				float pl, pr; plateRun(plate, p, 0.7f, 0.4f + 0.4f * q[1], 8000.f, pl, pr);
				l = m + pl * v[4]; r = m + pr * v[4];
			} break;
			case 14: {  // FDN CHORUS SLAP
				fdn.size = 0.3f; fdn.rt60L = fdn.rt60R = 1.2f; fdn.dampHz = 7000.f; fdn.modSamples = 3.f; fdn.insertOn = false; fdn.freeze = false;
				float fl_, fr_; fdn.process(x, x, fl_, fr_);
				float cl, cr; ch.process(x, v[0], 4.f * v[1], cl, cr);
				float s = dly(dA, x, 80.f * (1.f + 0.01f * sr1.process(v[4], dt)));
				l = cl + fl_ * v[2] + s * v[3]; r = cr + fr_ * v[2] - s * v[3];
			} break;
			case 15: {  // NESTED VERB DLY
				float t = 0.4f + 0.55f * q[0] / 1.f;
				na[0].set(17.f, 7.1f, t * v[1] / 0.8f, fs); na[1].set(29.f, 11.3f, t * v[1] / 0.8f, fs);
				float a = na[0].process(x);
				svf.set(v[2], 0.5f, fs); svf.process(a);
				float d = dly(dA, svf.lp + fbA * v[5], v[4]); fbA = d;
				float b = na[1].process(d);
				stereo((a + b) * 0.7f * (0.3f + v[3]), l, r);
				l += 0.3f * b; r -= 0.3f * b;
			} break;
			// ================= bank B
			case 16: {  // SHIFT FLANGE AIR
				float f = fl.process(x, l1.process(0.2f, dt, Lfo::SINE), mapKind(Kind::Zero, q[4]), 3.f, v[1]);
				float air = btn ? 1.f + 0.3f * sr1.process(0.2f, dt) : 1.f;
				float a = fq1.process(f, v[0] * air, fs), b = fq2.process(f, v[2] * air, fs);
				float pl, pr; plateRun(plate, (a + b) * 0.5f + fbA * v[5] * 0.5f, 0.75f, 0.6f, 8000.f, pl, pr);
				fbA = 0.5f * (pl + pr);
				l = f * 0.5f + (a * 0.8f + pl * 0.6f) * v[3]; r = f * 0.5f + (b * 0.8f + pr * 0.6f) * v[3];
			} break;
			case 17: {  // WARP VERB
				plate.modRate = v[4];
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr, 1.f, 2.f * v[5] + 0.1f);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 18: {  // AM TWO DELAY
				float am = 0.5f + 0.5f * am1.process(v[0] * 4.f, fs);
				float xi = x * am;
				float a = dly(dA, xi + fbA * v[1], v[2]), b = dly(dB, xi + fbB * v[4], v[3]);
				fbA = a; fbB = b;
				l = (xi * 0.5f + a) * (0.3f + v[5]); r = (xi * 0.5f + b) * (0.3f + v[5]);
			} break;
			case 19: {  // SKY LOOPS
				float pl, pr; plateRun(plate, x + fbA * v[5] * 0.2f, v[1], 0.65f, v[2], pl, pr, 1.f, 0.3f + 1.5f * v[4], v[0]);
				float d = dly(dA, 0.5f * (pl + pr), 370.f);
				float ql, qr; plateRun(plate2, d, v[1] * 0.8f, 0.7f, v[2], ql, qr, 0.7f);
				fbA = softClamp(0.5f * (ql + qr), 8.f);
				l = (pl + ql) * (0.3f + v[3]); r = (pr + qr) * (0.3f + v[3]);
			} break;
			case 20: {  // SMALL ROOM DLY
				float pl, pr; plateRun(plate, x, v[0] * 0.8f, v[1], v[2], pl, pr, 0.3f);
				float d = dly(dA, 0.5f * (pl + pr) + fbA * v[5], v[4]); fbA = d;
				l = pl * (0.3f + v[3]) + d * 0.6f; r = pr * (0.3f + v[3]) + d * 0.6f;
			} break;
			case 21: {  // PHASE DELAY
				phB.n = 8; phB.centre = 700.f; phB.depthOct = 5.f * v[1]; phB.spread = 0.4f; phB.resonance = v[2];
				float p = 0.5f * (x + phB.process(x, l1.process(v[0], dt, Lfo::SINE)));
				float d = dly(dA, p + fbA * v[5], v[4]); fbA = d;
				float y = x + (p + d * 0.7f - x) * v[3];
				stereo(y, l, r);
			} break;
			case 22: {  // VERB DLY VERB
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr, 1.f, btn ? 0.9f : 0.3f);
				float d = dly(dA, 0.5f * (pl + pr) + fbA * v[5], v[4]); fbA = d;
				float ql, qr; plateRun(plate2, d, v[0] * 0.8f, v[1], v[2], ql, qr, 0.6f, btn ? 0.9f : 0.3f);
				l = (pl + ql) * (0.3f + v[3]); r = (pr + qr) * (0.3f + v[3]);
			} break;
			case 23: {  // DRIVE SVF VERB
				float gain = 1.f + (q[0] > 0.75f ? (q[0] - 0.75f) * 4.f * 20.f : 0.f);
				float d = drv.process(x, gain);
				svf.set(v[4] * (0.8f + 0.4f * q[1]), v[2], fs); svf.process(d);
				float pl, pr; plateRun(plate, svf.lp, std::min(v[0], 0.95f), v[1], 9000.f, pl, pr);
				l = (svf.lp * 0.6f + pl * v[3]) * (0.2f + v[5]); r = (svf.lp * 0.6f + pr * v[3]) * (0.2f + v[5]);
			} break;
			case 24: {  // PITCH TAIL LOOPS
				tankPsOn = true; tankPitch = v[5];
				float pl, pr; plateRun(plate, x, v[1], 0.65f, v[2], pl, pr, 1.f, 0.1f + 1.5f * v[4], v[0]);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 25: {  // AM DOUBLE DLY
				float a = dly(dA, x + fbA * v[1], v[0]), b = dly(dB, x + fbB * v[3], v[2]);
				fbA = a; fbB = ps1.process(b, v[5]);
				float ma = 0.5f + 0.5f * am1.process(v[4], fs), mb = 0.5f + 0.5f * am2.process(v[4] * 1.37f, fs);
				l = x * 0.3f + a * ma; r = x * 0.3f + fbB * mb;
			} break;
			case 26: {  // CROSS MOD ROOM
				pre.push(x);
				float in = v[0] > 0.5f ? pre.at((int)(v[0] * 0.001f * fs)) : x;
				fdn.size = 0.9f; fdn.rt60L = fdn.rt60R = v[1]; fdn.dampHz = v[2]; fdn.freeze = false; fdn.insertOn = false;
				fdn.modSamples = 4.f + 10.f * (0.5f + 0.5f * l1.process(v[4], dt, Lfo::SINE)) * (0.5f + 0.5f * l2.process(v[5], dt, Lfo::TRIANGLE));
				fdn.process(in, in, l, r);
				l *= (0.3f + v[3]); r *= (0.3f + v[3]);
			} break;
			case 27: {  // SMALL PITCH TAIL
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr, 0.4f);
				float p = ps1.process(0.5f * (pl + pr), v[5]);
				l = (pl + (p - pl) * v[4]) * (0.3f + v[3]); r = (pr + (p - pr) * v[4]) * (0.3f + v[3]);
			} break;
			case 28: {  // BANDPASS SPACE
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr);
				svf.set(v[5], v[4], fs); svf.process(pl); svf2.set(v[5] * 1.06f, v[4], fs); svf2.process(pr);
				l = svf.bpNorm() * (0.3f + v[3]) * 1.5f; r = svf2.bpNorm() * (0.3f + v[3]) * 1.5f;
			} break;
			case 29: {  // WET MOD LOOPS
				float t = 0.3f + 0.6f * q[0];
				float m = 1.f + 0.02f * l1.process(v[4] * (1.f + q[0]), dt, Lfo::SINE);
				na[0].set(23.f * m, 9.7f, t * v[1] / 0.8f, fs); na[1].set(37.f / m, 13.1f, t * v[1] / 0.8f, fs); na[2].set(51.f * m, 17.9f, t * v[1] / 0.8f, fs);
				float a = na[0].process(x + fbA * t * 0.8f); float b = na[1].process(a); float c = na[2].process(b);
				svf.set(v[2], 0.5f, fs); svf.process(c); fbA = softClamp(svf.lp, 10.f);
				float y = 0.07f * x + 0.93f * (a + c) * 0.6f;
				svf2.set(v[5] < 0.f ? 20000.f * std::pow(100.f, v[5]) : 20.f * std::pow(100.f, v[5]), 0.7f, fs); svf2.process(y);
				y = std::fabs(v[5]) < 0.03f ? y : (v[5] < 0.f ? svf2.lp : svf2.hp);
				stereo(y * (0.3f + v[3]), l, r);
				l += 0.3f * b; r -= 0.3f * b;
			} break;
			case 30: {  // ZERO WARP VERB
				float lo = l1.process(v[0], dt, Lfo::SINE), bk = l2.process(v[1], dt, Lfo::TRIANGLE);
				float f = fl.process(x, 0.5f * (lo + bk), mapKind(Kind::Zero, q[5]), 3.f, v[2]);
				plate.modRate = 0.5f + 2.f * q[4];
				float pl, pr; plateRun(plate, btn ? 0.f : f, btn ? 1.f : 0.85f, 0.65f, btn ? 18000.f : 7000.f, pl, pr, 1.f, 0.3f + 1.2f * q[4]);
				l = (f + pl * v[4]) * (0.3f + v[3]); r = (f + pr * v[4]) * (0.3f + v[3]);
			} break;
			case 31: {  // TWIN MID VERBS
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr, 1.f);
				plate2.modRate = 0.2f + 2.f * q[4];
				float ql, qr; plateRun(plate2, x, v[4], v[5], v[2], ql, qr, 0.6f, 0.4f + 0.8f * q[4]);
				l = (pl + ql) * 0.7f * (0.3f + v[3]); r = (pr + qr) * 0.7f * (0.3f + v[3]);
			} break;
			// ================= bank C
			case 32: {  // MIRROR SHIFTS
				float wob = btn ? 0.2f * l1.process(0.3f, dt, Lfo::SINE) : 0.f;
				float a = ps1.process(x, v[0] + wob), b = ps2.process(x, v[2] - wob);
				float da = dly(dA, a + fbB * v[1] * 0.7f, v[4]), db = dly(dB, b * (0.3f + v[3]) + fbA * v[1] * 0.7f, v[5]);
				fbA = da; fbB = db;
				float pl, pr; plateRun(plate, 0.5f * (da + db), 0.6f, 0.6f, 7000.f, pl, pr);
				l = da + pl * 0.5f; r = db + pr * 0.5f;
			} break;
			case 33: {  // RING FLANGE
				float rmx = rm.process(x, v[0], v[1], fs);
				float f = fl.process(rmx, l1.process(v[5], dt, Lfo::SINE), 2.f, 5.f * v[4], v[2]);
				stereo((rmx * 0.4f + f) * (0.3f + v[3]), l, r);
			} break;
			case 34: {  // CRYSTAL VERB
				tankPsOn = true; tankPitch = std::fabs(v[3]) < 0.01f ? 12.f : v[3];
				plate.modRate = v[4];
				float in = x + (btn ? ps2.process(x, 19.f) * 0.5f : 0.f);
				float pl, pr; plateRun(plate, in, v[0], v[1], v[2], pl, pr, 1.f, 0.1f + 1.2f * v[5]);
				l = pl; r = pr;
			} break;
			case 35: {  // DOMINO DELAYS
				float w = 1.f + 0.01f * v[5] * sr1.process(1.5f, dt);
				float a = dly(dA, x + fbA * v[1], v[0] * w);
				float b = dly(dB, a + fbB * v[3], v[2] / w);
				fbA = a; fbB = ps1.process(b, v[4]);
				l = x * 0.3f + a; r = x * 0.3f + fbB;
			} break;
			case 36: {  // PHASE VERB
				phB.n = 8; phB.centre = 700.f; phB.depthOct = 5.f * v[1]; phB.spread = 0.4f; phB.resonance = v[3];
				float p = 0.5f * (x + phB.process(x, l1.process(v[0], dt, Lfo::SINE)));
				float pl, pr; plateRun(plate, p, v[4], 0.65f, v[5], pl, pr);
				l = p + (pl - p) * v[2]; r = p + (pr - p) * v[2];
			} break;
			case 37: {  // NANO REVERSE
				float w = 5.f + 115.f * v[0];
				w *= 1.f + 0.3f * v[2] * l1.process(v[3], dt, Lfo::SINE);
				float a = rv1.process(x, w, v[1] * 0.9f), b = rv2.process(x, w * 1.13f, v[1] * 0.9f);
				l = a; r = b;
			} break;
			case 38: {  // PITCH FLANGE
				float f = fl.process(x, l1.process(v[0], dt, Lfo::SINE), 2.f, 4.f * v[3], v[1]);
				float p = ps1.process(f, v[4]);
				float y = x + (p - x) * v[2];
				stereo(y, l, r);
			} break;
			case 39: {  // LOWPASS MID VERB
				svf.set(v[2], 0.6f, fs); svf.process(x);
				plate.modRate = v[4];
				float pl, pr; plateRun(plate, svf.lp, v[0], v[1], 9000.f, pl, pr, 0.8f, 0.1f + 1.5f * v[5]);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 40: {  // SMALL CHORUS VERB
				float pl, pr; plateRun(plate, x, v[0], v[1], v[2], pl, pr, 0.4f);
				float d = dly(dA, 0.5f * (pl + pr) * v[4], 40.f + 30.f * v[5] * sr1.process(0.7f, dt));
				float cl, cr; ch.process(0.5f * (pl + pr) + d, 0.5f, 3.f, cl, cr);
				l = (pl + cl) * 0.6f * (0.3f + v[3]); r = (pr + cr) * 0.6f * (0.3f + v[3]);
			} break;
			case 41: {  // CATHEDRAL
				plate.modRate = v[4];
				float pl, pr; plateRun(plate, x, v[1], v[5], v[2], pl, pr, 2.f, 0.4f, v[0]);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 42: {  // PITCH PREDELAY
				pre.push(x);
				float in = v[0] > 0.5f ? pre.at((int)(v[0] * 0.001f * fs)) : x;
				float p = ps1.process(in, v[4] + 0.15f * l1.process(v[5], dt, Lfo::SINE));
				float pl, pr; plateRun(plate, p, v[1], 0.65f, v[2], pl, pr);
				l = pl * (0.3f + v[3]); r = pr * (0.3f + v[3]);
			} break;
			case 43: {  // PHASE CHORUS
				phB.n = 8; phB.centre = 700.f; phB.depthOct = 5.f * v[1]; phB.spread = 0.4f; phB.resonance = v[2];
				float p = 0.5f * (x + phB.process(x, l1.process(v[0], dt, Lfo::SINE)));
				float y = x + (p - x) * v[3];
				float cl, cr; ch.process(y, 0.5f, 5.f * v[4], cl, cr);
				l = (y + cl) * 0.6f * (0.3f + v[5]); r = (y + cr) * 0.6f * (0.3f + v[5]);
			} break;
			case 44: {  // DOUBLE PHASE DLY
				phA.n = 12; phA.centre = 500.f; phA.depthOct = 2.f + 3.f * q[1]; phA.spread = 0.3f; phA.resonance = v[2];
				phB.n = 8; phB.centre = 1100.f; phB.depthOct = 4.f; phB.spread = 0.4f; phB.resonance = -v[2];
				float a = phA.process(x + fbA * v[1] * 0.5f, l1.process(v[0], dt, Lfo::SINE));
				float d = dly(dA, a, 50.f + 300.f * q[0]); fbA = d;
				float b = phB.process(x + fbB * v[5] * 0.5f, l2.process(v[4], dt, Lfo::SINE)); fbB = b;
				l = x + (0.5f * (a + d) - x) * v[3]; r = x + (b - x) * v[3];
			} break;
			case 45: {  // MOD DELAY CHORUS
				float d = dly(dA, x + fbA * v[5], v[4] * (1.f + 0.05f * v[1] * l1.process(v[0], dt, Lfo::SINE))); fbA = d;
				float cl, cr; ch.process(d, v[0] * 0.5f + 0.2f, 4.f * v[1], cl, cr);
				l = (x + (cl - x) * v[3]) * (0.3f + v[2]); r = (x + (cr - x) * v[3]) * (0.3f + v[2]);
			} break;
			case 46: {  // FLANGE VERB MOD
				float f = fl.process(x, l1.process(v[0], dt, Lfo::SINE), mapKind(Kind::Zero, q[4]), 5.f * v[1], v[2]);
				float pl, pr; plateRun(plate, f, v[5], 0.6f, 8000.f, pl, pr);
				l = (f + pl) * 0.7f * (0.3f + v[3]); r = (f + pr) * 0.7f * (0.3f + v[3]);
			} break;
			case 47: {  // DANGER FDN: loop gain may exceed 1 into the soft limiter. Loud.
				fdn.size = 1.4f; fdn.rt60L = v[0]; fdn.rt60R = v[0] * 1.1f; fdn.dampHz = v[2]; fdn.freeze = false; fdn.insertOn = false;
				fdn.loopGain = 1.f + 0.06f * v[5] * (0.5f + 0.5f * l1.process(v[4], dt, Lfo::SINE));
				fdn.modSamples = 4.f + 12.f * v[1];
				float in = x;
				for (int i = 0; i < 2; i++) { na[i].set(7.3f + 4.1f * i, 3.1f + i, v[1], fs); in = na[i].process(in); }
				fdn.process(in, in, l, r);
				l = softClamp(l * (0.3f + v[3]), 10.f); r = softClamp(r * (0.3f + v[3]), 10.f);
			} break;
			// ================= bank D
			case 48: {  // NESTED CHORUS
				float t = 0.3f + 0.6f * q[0];
				float k = btn ? 3.f : 1.f;
				float m = 1.f + 0.01f * l1.process(0.4f * k, dt, Lfo::SINE);
				na[0].set(19.f * m, 7.7f, t * v[1] / 0.8f, fs); na[1].set(31.f / m, 12.3f, t * v[1] / 0.8f, fs);
				float a = na[0].process(x + fbA * t * 0.7f), b = na[1].process(a);
				svf.set(v[2], 0.5f, fs); svf.process(b); fbA = softClamp(svf.lp, 10.f);
				float cl, cr; ch.process(b, 0.5f * k, 5.f * v[4], cl, cr);
				l = a * 0.5f + cl * (0.3f + v[3]); r = b * 0.5f + cr * (0.3f + v[3]);
			} break;
			case 49: {  // ODD FLANGE PCH
				float f = fl.process(x + ps1.process(last, v[1]) * v[2] * 0.6f, l1.process(v[0], dt, Lfo::SINE), 2.f, 3.f, 0.f);
				last = softClamp(f, 10.f);
				stereo(f * (0.3f + v[3]), l, r);
			} break;
			case 50: {  // BANDPASS DELAY
				float e = ef.process(x);
				float f = clampf(200.f * std::pow(2.f, 5.f * v[0] * e / 2.5f) * (1.f + 0.3f * l1.process(v[3], dt, Lfo::SINE)), 40.f, 12000.f);
				svf.set(f, v[1], fs); svf.process(x);
				float bp = svf.bpNorm();
				float d = dly(dA, bp + fbA * 0.5f, v[2]); fbA = d;
				l = bp + d * 0.6f; r = bp * 0.6f + d;
			} break;
			case 51: {  // CHORUS VERB FLANGE
				float cl, cr; ch.process(x, v[0], 4.f * v[1], cl, cr);
				float pl, pr; plateRun(plate, 0.5f * (cl + cr), 0.7f, 0.6f, 8000.f, pl, pr, 1.f, 0.3f + 2.f * v[4]);
				float f = fl.process(0.5f * (pl + pr), l2.process(v[0] * 0.5f, dt, Lfo::SINE), 2.f, 3.f, v[5]);
				l = (cl + pl) * 0.5f * (0.3f + v[2]) + f * v[3]; r = (cr + pr) * 0.5f * (0.3f + v[2]) + f * v[3];
			} break;
			case 52: {  // RING PHASE DLY (one rate drives phaser and delay modulation)
				float m = l1.process(v[0], dt, Lfo::SINE);
				phB.n = 8; phB.centre = 700.f; phB.depthOct = 5.f * v[1]; phB.spread = 0.4f; phB.resonance = v[2] * (0.5f + 0.5f * v[4]);
				float p = 0.5f * (x + phB.process(x, m));
				float d = dly(dA, p + fbA * v[5], 200.f * (1.f + 0.05f * m)); fbA = d;
				float y = x + (p + 0.6f * d - x) * v[3];
				stereo(y, l, r);
			} break;
			case 53: {  // DOUBLE DLY CHORUS
				float a = dly(dA, x + fbA * v[1], v[0]), b = dly(dB, x + fbB * v[3], v[2]);
				fbA = a; fbB = b;
				float cl, cr; ch.process(0.5f * (a + b), v[4], 5.f * v[5], cl, cr);
				l = x * 0.4f + a * 0.5f + cl; r = x * 0.4f + b * 0.5f + cr;
			} break;
			case 54: {  // DRIVE VERB FLANGE
				float d = btn ? drv.process(x, 12.f) : x;
				float pl, pr; plateRun(plate, d, 0.7f, 0.6f, 8000.f, pl, pr);
				float lo = l1.process(v[0], dt, Lfo::SINE), bk = l2.process(v[1], dt, Lfo::TRIANGLE);
				float f = fl.process(0.5f * (pl + pr), 0.5f * (lo + bk) + 0.3f * v[5] * sr1.process(1.f, dt), mapKind(Kind::Zero, q[4]), 3.f, v[2]);
				l = (pl * 0.6f + f) * (0.3f + v[3]); r = (pr * 0.6f - f) * (0.3f + v[3]);
			} break;
			case 55: {  // PINK CLOUDS
				float t = 0.3f + 0.65f * q[0];
				float g = v[2] / 0.8f * 0.85f;
				na[0].set(13.f, 5.3f, g, fs); na[1].set(29.f, 11.7f, g, fs); na[2].set(47.f, 19.1f, g, fs);
				float a = na[0].process(x + fbA * t), b = na[1].process(a), c = na[2].process(b);
				fbA = softClamp(c * (0.5f + 0.5f * v[1]), 10.f);
				l = (a + c) * 0.6f * (0.3f + v[3]); r = (b - c) * 0.6f * (0.3f + v[3]);
			} break;
			case 56: {  // TIME SCANNER
				float lenMs = 50.f + 350.f * v[0];
				float rate = 2.f * v[1];
				if (std::fabs(rate) < 0.05f) rate = rate < 0.f ? -0.05f : 0.05f;
				float y = hold.process(x, btn, lenMs, -rate);
				float o = x + (y - x) * v[2];
				stereo(o, l, r);
			} break;
			case 57: {  // PHASE VERB B
				float pl, pr; plateRun(plate, x, v[1], 0.65f, v[2], pl, pr, 1.f, 0.3f, v[0]);
				phA.n = 8; phA.centre = 800.f; phA.depthOct = 5.f * v[5]; phA.spread = 0.4f; phA.resonance = 0.4f;
				phB.n = 8; phB.centre = 800.f; phB.depthOct = 5.f * v[5]; phB.spread = 0.4f; phB.resonance = 0.4f;
				float m = l1.process(v[4], dt, Lfo::SINE);
				l = 0.5f * (pl + phA.process(pl, m)) * (0.3f + v[3]); r = 0.5f * (pr + phB.process(pr, -m)) * (0.3f + v[3]);
			} break;
			case 58: {  // FLANGE DLY MOD
				float f = fl.process(x, l1.process(v[0], dt, Lfo::SINE), mapKind(Kind::Zero, q[4]), 5.f * v[1], v[2]);
				float d = dly(dA, f, v[5]);
				l = (f + d * 0.5f) * (0.3f + v[3]); r = (f * 0.5f + d) * (0.3f + v[3]);
			} break;
			case 59: {  // NESTED LOOPS
				float t = 0.3f + 0.6f * q[0];
				float m = 1.f + 0.02f * v[5] * l1.process(v[4], dt, Lfo::SINE);
				na[0].set(17.f * m, 6.1f, t * v[1] / 0.8f, fs); na[1].set(27.f / m, 9.9f, t * v[1] / 0.8f, fs); na[2].set(43.f * m, 15.3f, t * v[1] / 0.8f, fs);
				float a = na[0].process(x + fbA * t), b = na[1].process(a), c = na[2].process(b);
				svf.set(v[2], 0.5f, fs); svf.process(c); fbA = softClamp(svf.lp * 0.85f, 10.f);
				l = (a + c) * 0.6f * (0.3f + v[3]); r = (b + c) * 0.6f * (0.3f + v[3]);
			} break;
			case 60: {  // RING CHORUS VERB
				float rmx = rm.process(x, v[0], v[1], fs);
				float cl, cr; ch.process(rmx, v[2], 5.f * v[3], cl, cr);
				float pl, pr; plateRun(plate, 0.5f * (cl + cr), 0.5f, 0.6f, 8000.f, pl, pr, 0.4f);
				l = (cl + pl * 0.6f) * (0.3f + v[4]); r = (cr + pr * 0.6f) * (0.3f + v[4]);
			} break;
			case 61: {  // BUFFER FLANGE
				float s = scanner.process(x, 0.5f + v[0] * 0.2f, btn ? 2 : 0);
				float lfo = l1.process(v[4], dt, Lfo::SINE);
				if (btn) lfo = chaos.process(8.f, 3.95f, dt);
				float f = fl.process(s, lfo, mapKind(Kind::Zero, q[3]), 4.f * v[2], v[1]);
				stereo(f, l, r);
			} break;
			case 62: {  // TREMOLO VERB
				hp.set(v[2], fs);
				float h = hp.process(x);
				float pl, pr; plateRun(plate, h, v[0], v[1], 9000.f, pl, pr);
				float t1 = 0.5f + 0.5f * l1.process(v[4], dt, Lfo::SINE), t2 = 0.5f + 0.5f * l2.process(v[5], dt, Lfo::SQUARE);
				l = pl * t1 * (0.3f + v[3]); r = pr * (0.6f * t2 + 0.4f) * (0.3f + v[3]);
			} break;
			case 63: {  // RING PHASER
				float rmx = rm.process(x, v[0], v[1], fs);
				phA.n = 12; phA.centre = 700.f; phA.depthOct = 4.f; phA.spread = 0.4f; phA.resonance = v[4] * (0.3f + 0.7f * v[3]);
				float p = phA.process(rmx, l1.process(v[2], dt, Lfo::SINE));
				float y = rmx + (0.5f * (rmx + p) - rmx) * (0.3f + 0.7f * v[5]);
				stereo(y, l, r);
			} break;
		}
		outL = softClamp(l * fade);
		outR = softClamp(r * fade);
		if (!finite(outL) || !finite(outR) || !plate.ok()) { clear(); outL = outR = 0.f; }
	}
	bool scrambleHeld = false;
};

} // namespace dens
