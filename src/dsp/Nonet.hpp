// NONET: nine stereo effect algorithms with three modes each (after Programma 900's DSP section).
#pragma once
#include "Common.hpp"
#include "Filters.hpp"
#include "DelayLine.hpp"
#include "Lfo.hpp"
#include "fx/FxParams.hpp"
#include "fx/Reverbs.hpp"
#include "fx/Modulation.hpp"
#include "fx/Buffers.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace dens {

struct NonetParamDef { const char* name; float def; };
struct NonetAlgDef { const char* name; const char* modes[3]; NonetParamDef p[6]; };

static const NonetAlgDef NONET_ALGS[9] = {
	{"LONG DELAY", {"FREE", "CLOCK", "REVERSE"}, {{"TIME L", 0.45f}, {"TIME R", 0.5f}, {"FEEDBACK", 0.5f}, {"AGE", 0.2f}, {"DRIFT", 0.1f}, {"CROSS", 0.f}}},
	{"SIXTEEN PHASER", {"SINE", "TRIANGLE", "FOLLOW"}, {{"RATE", 0.3f}, {"SPREAD", 0.3f}, {"RESONANCE", 0.763f}, {"SWEEP", 0.6f}, {"CENTRE", 0.539f}, {"STEREO", 0.5f}}},
	{"HADAMARD SPACE", {"ROOM", "HALL", "FREEZE"}, {{"SPACE", 0.5f}, {"TIME L", 0.5f}, {"TIME R", 0.55f}, {"MOTION", 0.3f}, {"PREDELAY", 0.1f}, {"DIFFUSE", 0.5f}}},
	{"SHIFTING SPACE", {"IN LOOP", "ON TAIL", "SPLIT"}, {{"DECAY", 0.6f}, {"BALANCE", 0.5f}, {"SHIFT", 0.55f}, {"SIZE", 0.5f}, {"SHIFT AMOUNT", 0.5f}, {"SHIFT DRIFT", 0.f}}},
	{"PHASER SPACE", {"PHASE>SPACE", "SPACE>PHASE", "PHASE IN SPACE"}, {{"RATE", 0.3f}, {"SPREAD", 0.4f}, {"RESONANCE", 0.711f}, {"RANGE", 0.5f}, {"SPACE MIX", 0.5f}, {"SPACE TIME", 0.6f}}},
	{"STRETCH FLANGER", {"SINE", "RANDOM", "FOLLOW"}, {{"FREQUENCY", 0.2f}, {"STRETCH", 0.f}, {"FEEDBACK", 0.658f}, {"DEPTH", 0.4f}, {"ZERO POINT", 0.3f}, {"STRETCH MIX", 1.f}}},
	{"GRAIN CLOUD", {"FORWARD", "REVERSE", "MIXED"}, {{"DENSITY", 0.4f}, {"STRETCH", 0.f}, {"POSITION", 0.2f}, {"SCATTER", 0.3f}, {"PITCH", 0.5f}, {"SIZE", 0.5f}}},
	{"SCAN SPACE", {"FORWARD", "PENDULUM", "JUMPS"}, {{"SCAN RATE", 0.5f}, {"STRETCH", 0.f}, {"ALLPASS TIME", 0.3f}, {"FEEDBACK", 0.5f}, {"DAMP", 0.6f}, {"SPACE MIX", 0.5f}}},
	{"TWIN SCAN", {"TWIN", "CROSS", "RUNAWAY"}, {{"BASE DELAY", 0.3f}, {"DEPTH", 0.4f}, {"FEEDBACK", 0.711f}, {"ALLPASS TIME", 0.4f}, {"ALLPASS MIX", 0.5f}, {"PRE-DELAY", 0.1f}}},
};

static const float CLOCK_RATIOS[15] = {1.f / 16, 1.f / 8, 1.f / 4, 1.f / 3, 1.f / 2, 2.f / 3, 1.f, 1.5f, 2.f, 3.f, 4.f, 8.f, 16.f, 32.f, 64.f};

// Long delay: 360 s at 48 kHz worth of samples per channel (fewer seconds at higher rates).
constexpr int LONG_SAMPLES_6MIN = 360 * 48000;
constexpr int LONG_SAMPLES_1MIN = 60 * 48000;

inline float bipolarSym(float v, float lim) { return (2.f * v - 1.f) * lim; }   // 0..1 -> −lim..+lim

struct Nonet {
	float fs = 48000.f;
	int alg = 2, mode = 0;              // 0-based
	int pendingAlg = -1;
	float fade = 1.f; int fadeDir = 0;  // wet fade for algorithm changes
	float clockPeriod = 0.f;            // seconds, 0 = no clock
	// shared
	TiltFilter tilt[2];
	EnvFollower env;
	Lfo lfo{17};
	SmoothRandom srnd{19};
	LogisticLfo chaos;
	// 1 long delay
	std::unique_ptr<DelayLine16> longL, longR;
	std::atomic<bool> longReady{false};
	OnePoleLP ageLp[2];
	Drive ageDrive[2];
	SmoothRandom wow[2] = {SmoothRandom(41), SmoothRandom(43)};
	float revPh = 0.f;
	float ldOut[2] = {0, 0};
	Smoother tSm[2];
	// 2 / 5 phasers
	Phaser ph[2];
	// 3 / 4 / 8 FDNs
	Fdn8 fdn;
	AllpassD diff[2][4];
	DelayLine pre[2];
	FreqShift shiftLine[8];
	FreqShift shiftPost[2];
	DelayLine early[2];
	// 5 nested allpasses
	NestedAllpass nest[2][3];
	AllpassD nestLast[2];
	float psFb[2] = {0, 0};
	// 6-8 stretch
	StretchBuffer stretch;
	Flanger flan[2];
	Grains grains;
	Scanner scan;
	AllpassD scanAp[4];
	// 9 twin scan
	DelayLine twinA, twinB;
	AllpassD twinAp[2];
	DelayLine twinPre;
	float yA = 0.f, yB = 0.f, twinFb = 0.f;
	Smoother pSm[6];
	float shiftHz = 0.f, shiftAmt = 0.f;

	void init(float sampleRate) {
		fs = sampleRate;
		for (int c = 0; c < 2; c++) {
			ph[c].init(fs, 16);
			for (int i = 0; i < 4; i++) { diff[c][i].init(12.f, fs); }
			pre[c].init((int)(0.26f * fs) + 8);
			early[c].init((int)(0.5f * fs) + 8);
			for (int i = 0; i < 3; i++) nest[c][i].init(40.f, fs);
			nestLast[c].init(50.f, fs);
			flan[c].init(fs, 20.f);
			ageDrive[c].reset();
			tilt[c].reset();
		}
		fdn.init(fs, 3.2f * 2.2f);
		fdn.insert = [this](int i, float x) {
			float hz = (mode == 2 && (i & 1)) ? -shiftHz : shiftHz;
			float y = shiftLine[i].process(x, hz, fs);
			return x + (y - x) * shiftAmt;
		};
		stretch.init(fs, 12.f);
		grains.init(fs);
		scan.init(fs);
		for (auto& a : scanAp) a.init(55.f, fs);
		twinA.init((int)(0.1f * fs)); twinB.init((int)(0.1f * fs));
		for (auto& a : twinAp) a.init(32.f, fs);
		twinPre.init((int)(0.26f * fs) + 8);
		env.set(0.005f, 0.2f, fs);
		clearAll();
	}
	// UI thread: allocate the long buffers (may take tens of MB).
	void allocateLong(bool sixMinutes) {
		longReady = false;
		int n = sixMinutes ? LONG_SAMPLES_6MIN : LONG_SAMPLES_1MIN;
		auto l = std::make_unique<DelayLine16>(); l->init(n);
		auto r = std::make_unique<DelayLine16>(); r->init(n);
		longL = std::move(l); longR = std::move(r);
		longClearLeft = 0;
		longReady = true;
	}
	int longClearLeft = 0, longClearPos = 0;
	void clearLongChunk() {
		int n = std::min(1024, longClearLeft);
		int L = longL->len;
		for (int k = 0; k < n; k++) {
			int i = longClearPos;
			if (i < 0) i += L;
			longL->buf[i] = 0; longR->buf[i] = 0;
			longClearPos = i - 1;
		}
		longClearLeft -= n;
	}
	void releaseLong() { longReady = false; longL.reset(); longR.reset(); }
	float longMaxSeconds() const { return longReady ? (longL->len - 8) / fs : 0.f; }

	void clearAll() {
		// the long buffers (tens of MB) are cleared a chunk per sample, oldest first, so a restart never stalls the audio thread
		if (longReady) { longClearLeft = longL->len; longClearPos = longL->w - 1; }
		for (int c = 0; c < 2; c++) {
			ph[c].reset();
			for (auto& d : diff[c]) d.clear();
			pre[c].clear(); early[c].clear();
			for (auto& n : nest[c]) n.clear();
			nestLast[c].clear();
			flan[c].clear();
			ldOut[c] = 0.f; psFb[c] = 0.f;
			ageLp[c].reset();
		}
		fdn.clear(); fdn.freeze = false;
		for (auto& s : shiftLine) s.reset();
		for (auto& s : shiftPost) s.reset();
		stretch.clear(); grains.clear(); scan.clear();
		for (auto& a : scanAp) a.clear();
		twinA.clear(); twinB.clear(); twinPre.clear();
		for (auto& a : twinAp) a.clear();
		yA = yB = twinFb = 0.f;
	}
	void restart() { clearAll(); }
	void select(int a) {
		a = std::max(0, std::min(8, a));
		if (a == alg && pendingAlg < 0) return;
		pendingAlg = a; fadeDir = -1;
	}

	// Values with each algorithm's own ranges.
	static float value(int a, int i, float v) {
		switch (a) {
			case 0: switch (i) { case 0: case 1: return expRange(v, 0.01f, 360.f); case 2: return 1.1f * v; default: return v; }
			case 1: switch (i) { case 0: return mapKind(Kind::Rate, v); case 2: return bipolarSym(v, 0.95f); case 3: return 6.f * v; case 4: return expRange(v, 100.f, 4000.f); default: return v; }
			case 2: switch (i) { case 0: return 0.2f + 2.8f * v; case 1: case 2: return mapKind(Kind::RT60, v); case 3: return 16.f * v; case 4: return mapKind(Kind::Predelay, v); default: return v; }
			case 3: switch (i) { case 0: return mapKind(Kind::RT60, v); case 2: return mapKind(Kind::Shift, v); case 3: return 0.2f + 2.8f * v; case 5: return v < 0.01f ? 0.f : mapKind(Kind::Rate, v); default: return v; }
			case 4: switch (i) { case 0: return mapKind(Kind::Rate, v); case 2: return bipolarSym(v, 0.95f); case 3: return 6.f * v; case 5: return mapKind(Kind::Coef, v); default: return v; }
			case 5: switch (i) { case 0: return mapKind(Kind::Rate, v); case 2: return bipolarSym(v, 0.95f); case 3: return 8.f * v; case 4: return mapKind(Kind::Zero, v); default: return v; }
			case 6: switch (i) { case 0: return expRange(v, 1.f, 100.f); case 4: { float c = 2.f * v - 1.f, a2 = std::fabs(c); a2 = a2 < 0.05f ? 0.f : (a2 - 0.05f) / 0.95f; return (c < 0 ? -24.f : 24.f) * a2; } case 5: return expRange(v, 10.f, 500.f); default: return v; }
			case 7: switch (i) { case 0: return v < 0.5f ? 0.05f * std::pow(20.f, 2.f * v) : std::pow(4.f, 2.f * (v - 0.5f)); case 2: return 1.f + 49.f * v; case 3: return expRange(v, 0.5f, 20.f); case 4: return mapKind(Kind::LowPass, v); default: return v; }
			case 8: switch (i) { case 0: return expRange(v, 0.5f, 40.f); case 2: return bipolarSym(v, 0.95f); case 3: return 1.f + 29.f * v; case 5: return mapKind(Kind::Predelay, v); default: return v; }
		}
		return v;
	}
	static std::string format(int a, int i, float v) {
		char b[32];
		float m = value(a, i, v);
		auto pct = [&]() { std::snprintf(b, sizeof b, "%.0f%%", v * 100.f); };
		auto hz = [&]() { if (m >= 1000.f) std::snprintf(b, sizeof b, "%.1f kHz", m / 1000.f); else if (m >= 10.f) std::snprintf(b, sizeof b, "%.0f Hz", m); else std::snprintf(b, sizeof b, "%.2f Hz", m); };
		auto ms = [&]() { if (m >= 1000.f) std::snprintf(b, sizeof b, "%.2f s", m / 1000.f); else std::snprintf(b, sizeof b, "%.1f ms", m); };
		auto sec = [&]() { if (m >= 60.f) std::snprintf(b, sizeof b, "%dm%02ds", (int)m / 60, (int)m % 60); else std::snprintf(b, sizeof b, "%.2f s", m); };
		auto bip = [&]() { std::snprintf(b, sizeof b, "%+.2f", m); };
		auto oct = [&]() { std::snprintf(b, sizeof b, "%.1f oct", m); };
		pct();
		switch (a) {
			case 0: if (i < 2) sec(); else if (i == 2) std::snprintf(b, sizeof b, "%.0f%%", m * 100.f); break;
			case 1: if (i == 0 || i == 4) hz(); else if (i == 2) bip(); else if (i == 3) oct(); break;
			case 2: if (i == 0) std::snprintf(b, sizeof b, "x%.2f", m); else if (i == 1 || i == 2) std::snprintf(b, sizeof b, "%.1f s", m); else if (i == 3) std::snprintf(b, sizeof b, "%.1f smp", m); else if (i == 4) ms(); break;
			case 3: if (i == 0) std::snprintf(b, sizeof b, "%.1f s", m); else if (i == 2) std::snprintf(b, sizeof b, "%+.1f Hz", m); else if (i == 3) std::snprintf(b, sizeof b, "x%.2f", m); else if (i == 5) { if (m == 0.f) std::snprintf(b, sizeof b, "OFF"); else hz(); } break;
			case 4: if (i == 0) hz(); else if (i == 2) bip(); else if (i == 3) oct(); else if (i == 5) std::snprintf(b, sizeof b, "g %.2f", m); break;
			case 5: if (i == 0) hz(); else if (i == 2) bip(); else if (i == 3 || i == 4) ms(); break;
			case 6: if (i == 0) std::snprintf(b, sizeof b, "%.0f /s", m); else if (i == 4) std::snprintf(b, sizeof b, "%+.1f st", m); else if (i == 5) ms(); break;
			case 7: if (i == 0) std::snprintf(b, sizeof b, "x%.2f", m); else if (i == 2) ms(); else if (i == 3) std::snprintf(b, sizeof b, "%.1f s", m); else if (i == 4) hz(); break;
			case 8: if (i == 0 || i == 3 || i == 5) ms(); else if (i == 2) bip(); break;
		}
		return b;
	}

	struct Frame { float p[6]; float filter, mix; };

	void process(const Frame& f, float inL, float inR, float& outL, float& outR) {
		float dt = 1.f / fs;
		// algorithm change: fade the wet out, swap and clear, fade back in
		if (fadeDir < 0) {
			fade -= dt / 0.01f;
			if (fade <= 0.f) { fade = 0.f; if (pendingAlg >= 0) { alg = pendingAlg; pendingAlg = -1; clearAll(); } fadeDir = 1; }
		} else if (fadeDir > 0) {
			fade += dt / 0.01f;
			if (fade >= 1.f) { fade = 1.f; fadeDir = 0; }
		}
		if (longClearLeft > 0 && longReady) clearLongChunk();
		float p[6];
		for (int i = 0; i < 6; i++) p[i] = pSm[i].process(f.p[i], dt);
		float wl = 0.f, wr = 0.f;
		bool filtered = false;
		switch (alg) {
			case 0: longDelay(p, f.filter, inL, inR, wl, wr); filtered = true; break;
			case 1: sixteenPhaser(p, inL, inR, wl, wr); break;
			case 2: hadamard(p, inL, inR, wl, wr); break;
			case 3: shifting(p, inL, inR, wl, wr); break;
			case 4: phaserSpace(p, inL, inR, wl, wr); break;
			case 5: stretchFlanger(p, inL, inR, wl, wr); break;
			case 6: grainCloud(p, inL, inR, wl, wr); break;
			case 7: scanSpace(p, inL, inR, wl, wr); break;
			case 8: twinScan(p, inL, inR, wl, wr); break;
		}
		if (!filtered) {
			tilt[0].set(f.filter, fs); tilt[1].set(f.filter, fs);
			wl = tilt[0].process(wl); wr = tilt[1].process(wr);
		}
		wl *= fade; wr *= fade;
		float a, b;
		eqPower(f.mix, a, b);
		outL = softClamp(inL * a + wl * b);
		outR = softClamp(inR * a + wr * b);
		if (!finite(outL) || !finite(outR)) { clearAll(); outL = outR = 0.f; }
	}

	// ---- 1 LONG DELAY
	void longDelay(const float* p, float filter, float inL, float inR, float& wl, float& wr) {
		if (!longReady) { wl = wr = 0.f; return; }
		float dt = 1.f / fs;
		float maxS = longMaxSeconds();
		float t[2] = {value(0, 0, p[0]), value(0, 1, p[1])};
		if (mode == 1 && clockPeriod > 0.f) {
			for (int c = 0; c < 2; c++) {
				// knob picks the nearest ratio on a log scale
				float best = CLOCK_RATIOS[0] * clockPeriod, bd = 1e9f;
				for (float r : CLOCK_RATIOS) { float d = std::fabs(std::log(r * clockPeriod / t[c])); if (d < bd) { bd = d; best = r * clockPeriod; } }
				t[c] = best;
			}
		}
		float fb = value(0, 2, p[2]), age = p[3], drift = p[4], cross = p[5];
		tilt[0].set(filter, fs); tilt[1].set(filter, fs);
		float lim = mode == 2 ? maxS * 0.49f : maxS;
		DelayLine16* line[2] = {longL.get(), longR.get()};
		float rd[2];
		revPh += 1.f;
		for (int c = 0; c < 2; c++) {
			float tt = tSm[c].process(clampf(t[c], 0.001f, lim), dt, 0.05f);
			float w = wow[c].process(0.3f + 1.7f * drift, dt);
			float d = tt * fs * (1.f + 0.003f * drift * w);
			if (mode == 2) {
				float W = std::max(64.f, d);
				float ph = std::fmod(revPh / W, 1.f), q = std::fmod(ph + 0.5f, 1.f);
				float wa = std::sin(PI * ph), wb = std::sin(PI * q);
				rd[c] = line[c]->read(1.f + 2.f * ph * W) * wa * wa + line[c]->read(1.f + 2.f * q * W) * wb * wb;
			} else {
				rd[c] = line[c]->read(std::max(1.f, d));
			}
		}
		if (revPh > 1e7f) revPh = 0.f;
		float in[2] = {inL, inR};
		for (int c = 0; c < 2; c++) {
			float back = rd[c] * (1.f - cross) + rd[1 - c] * cross;
			ageLp[c].set(18000.f * std::pow(1500.f / 18000.f, age), fs);
			float x = ageLp[c].process(back);
			x = tilt[c].process(x);
			if (age > 0.01f) x = ageDrive[c].processUnity(x, 1.f + 2.f * age);
			x = softClamp(x * fb, 11.f);
			line[c]->push(in[c] + x);
		}
		wl = rd[0]; wr = rd[1];
	}
	// ---- 2 SIXTEEN PHASER
	void sixteenPhaser(const float* p, float inL, float inR, float& wl, float& wr) {
		float dt = 1.f / fs;
		float rate = value(1, 0, p[0]);
		lfo.advance(rate, dt);
		float s0, s1;
		if (mode == 2) {
			float e = clampf(env.process(0.5f * (inL + inR)) / 5.f, 0.f, 1.f);
			s0 = s1 = 2.f * e - 1.f;
		} else {
			Lfo::Shape sh = mode == 0 ? Lfo::SINE : Lfo::TRIANGLE;
			s0 = lfo.value(sh);
			float keep = lfo.phase;
			lfo.phase = std::fmod(keep + 0.5f * p[5], 1.f);
			s1 = lfo.value(sh);
			lfo.phase = keep;
		}
		float in[2] = {inL, inR}, sw[2] = {s0, s1}, out[2];
		for (int c = 0; c < 2; c++) {
			ph[c].n = 16;
			ph[c].spread = p[1]; ph[c].resonance = value(1, 2, p[2]); ph[c].depthOct = value(1, 3, p[3]); ph[c].centre = value(1, 4, p[4]);
			out[c] = 0.5f * (in[c] + ph[c].process(in[c], sw[c]));
		}
		wl = out[0]; wr = out[1];
	}
	// ---- 3 HADAMARD SPACE
	void hadamard(const float* p, float inL, float inR, float& wl, float& wr) {
		float in[2] = {inL, inR};
		float pd = value(2, 4, p[4]);
		for (int c = 0; c < 2; c++) {
			pre[c].push(in[c]);
			float x = pd > 0.5f ? pre[c].at((int)(pd * 0.001f * fs)) : in[c];
			for (int i = 0; i < 4; i++) {
				static const float ms[4] = {3.1f, 4.7f, 7.3f, 11.f};
				diff[c][i].setMs(ms[i] * (c ? 1.07f : 1.f), fs);
				diff[c][i].g = 0.3f + 0.45f * p[5];
				x = diff[c][i].process(x);
			}
			in[c] = x;
		}
		float sz = value(2, 0, p[0]) * (mode == 1 ? 2.2f : 1.f);
		fdn.size = sz;
		fdn.rt60L = value(2, 1, p[1]); fdn.rt60R = value(2, 2, p[2]);
		fdn.modSamples = value(2, 3, p[3]) * (mode == 1 ? 1.5f : 1.f);
		fdn.dampHz = 9000.f;
		fdn.freeze = mode == 2;
		fdn.insertOn = false;
		fdn.loopGain = 1.f;
		fdn.process(in[0], in[1], wl, wr);
	}
	// ---- 4 SHIFTING SPACE
	void shifting(const float* p, float inL, float inR, float& wl, float& wr) {
		float dt = 1.f / fs;
		float shift = value(3, 2, p[2]);
		float drift = value(3, 5, p[5]);
		if (drift > 0.f) shift *= 1.f + 0.5f * std::sin(TWO_PI * (lfo.phase += drift * dt, lfo.phase -= std::floor(lfo.phase)));
		float amt = p[4];
		float size = value(3, 3, p[3]);
		// early reflections
		static const float tapMs[6] = {7.f, 19.f, 31.f, 47.f, 67.f, 89.f};
		float er[2] = {0, 0};
		float in[2] = {inL, inR};
		for (int c = 0; c < 2; c++) {
			early[c].push(in[c]);
			for (int t = 0; t < 6; t++) er[c] += early[c].at(std::min((int)(tapMs[t] * size * 0.001f * fs), early[c].size() - 2)) * (0.8f - 0.1f * t);
		}
		fdn.size = size;
		fdn.rt60L = fdn.rt60R = value(3, 0, p[0]);
		fdn.modSamples = 3.f;
		fdn.dampHz = 8000.f;
		fdn.freeze = false;
		fdn.loopGain = 1.f;
		float sh = shift;
		int md = mode;
		shiftHz = sh; shiftAmt = amt;
		fdn.insertOn = md == 0 || md == 2;
		float tl, tr;
		fdn.process(in[0], in[1], tl, tr);
		if (md == 1) {
			tl = tl + (shiftPost[0].process(tl, sh, fs) - tl) * amt;
			tr = tr + (shiftPost[1].process(tr, sh, fs) - tr) * amt;
		}
		float bal = p[1];
		wl = er[0] * 0.4f * (1.f - bal) + tl * bal;
		wr = er[1] * 0.4f * (1.f - bal) + tr * bal;
	}
	// ---- 5 PHASER SPACE
	float nested(int c, float x, float g) {
		static const float a[3][2] = {{5.3f, 7.9f}, {11.3f, 17.1f}, {23.7f, 31.9f}};
		for (int i = 0; i < 3; i++) { nest[c][i].set(a[i][0] * (c ? 1.05f : 1.f), a[i][1] * (c ? 1.05f : 1.f), g, fs); x = nest[c][i].process(x); }
		nestLast[c].setMs(41.3f * (c ? 1.05f : 1.f), fs);
		nestLast[c].g = g;
		return nestLast[c].process(x);
	}
	void phaserSpace(const float* p, float inL, float inR, float& wl, float& wr) {
		float dt = 1.f / fs;
		lfo.advance(value(4, 0, p[0]), dt);
		float s = lfo.value(Lfo::SINE);
		float g = value(4, 5, p[5]), smix = p[4];
		float in[2] = {inL, inR}, out[2];
		for (int c = 0; c < 2; c++) {
			ph[c].n = 16; ph[c].spread = p[1]; ph[c].resonance = value(4, 2, p[2]); ph[c].depthOct = value(4, 3, p[3]); ph[c].centre = 700.f;
			float sw = c ? -s : s;
			if (mode == 0) {
				float a = 0.5f * (in[c] + ph[c].process(in[c], sw));
				float v = nested(c, a, g);
				out[c] = a + (v - a) * smix;
			} else if (mode == 1) {
				float v = nested(c, in[c], g);
				float a = in[c] + (v - in[c]) * smix;
				out[c] = 0.5f * (a + ph[c].process(a, sw));
			} else {
				float x = in[c] + psFb[c] * 0.7f * p[5];
				float a = ph[c].process(x, sw);
				float v = nested(c, a, g);
				psFb[c] = softClamp(v, 10.f);
				out[c] = a + (v - a) * smix;
			}
		}
		wl = out[0]; wr = out[1];
	}
	// ---- 6 STRETCH FLANGER
	void stretchFlanger(const float* p, float inL, float inR, float& wl, float& wr) {
		float dt = 1.f / fs;
		float sl, sr;
		stretch.process(p[1], inL, inR, sl, sr);
		float mixS = p[5];
		float xl = inL + (sl - inL) * mixS, xr = inR + (sr - inR) * mixS;
		float rate = value(5, 0, p[0]);
		float m;
		if (mode == 0) m = lfo.process(rate, dt, Lfo::SINE);
		else if (mode == 1) m = srnd.process(rate, dt);
		else m = 2.f * clampf(env.process(0.5f * (inL + inR)) / 5.f, 0.f, 1.f) - 1.f;
		float depth = value(5, 3, p[3]), zero = value(5, 4, p[4]), fb = value(5, 2, p[2]);
		wl = flan[0].process(xl, m, zero, depth, fb);
		wr = flan[1].process(xr, -m, zero, depth, fb);
	}
	// ---- 7 GRAIN CLOUD
	void grainCloud(const float* p, float inL, float inR, float& wl, float& wr) {
		float sl, sr;
		stretch.process(p[1], inL, inR, sl, sr);
		grains.process(0.5f * (sl + sr), value(6, 0, p[0]), p[2], p[3], value(6, 4, p[4]), value(6, 5, p[5]), mode, wl, wr);
	}
	// ---- 8 SCAN SPACE
	void scanSpace(const float* p, float inL, float inR, float& wl, float& wr) {
		float sl, sr;
		stretch.process(p[1], inL, inR, sl, sr);
		float s = scan.process(0.5f * (sl + sr), value(7, 0, p[0]), mode);
		float x = s;
		float apMs = value(7, 2, p[2]);
		static const float mult[4] = {1.f, 1.37f, 0.71f, 1.93f};
		for (int i = 0; i < 4; i++) { scanAp[i].setMs(std::min(50.f, apMs * mult[i]), fs); scanAp[i].g = 0.6f; x = scanAp[i].process(x); }
		fdn.size = 1.f; fdn.rt60L = fdn.rt60R = value(7, 3, p[3]); fdn.dampHz = value(7, 4, p[4]); fdn.modSamples = 4.f; fdn.freeze = false; fdn.insertOn = false; fdn.loopGain = 1.f;
		float tl, tr;
		fdn.process(x, x, tl, tr);
		float m = p[5];
		wl = s + (tl - s) * m; wr = s + (tr - s) * m;
	}
	// ---- 9 TWIN SCAN
	void twinScan(const float* p, float inL, float inR, float& wl, float& wr) {
		float dt = 1.f / fs;
		float x = 0.5f * (inL + inR);
		twinPre.push(x);
		float pd = value(8, 5, p[5]);
		if (pd > 0.5f) x = twinPre.at((int)(pd * 0.001f * fs));
		float base = value(8, 0, p[0]) * 0.001f * fs, depth = p[1] * base, fb = value(8, 2, p[2]);
		float extra = 0.f;
		if (mode == 2) extra = chaos.process(0.1f + 0.9f * p[1], 3.9f, dt) * 0.5f * base;
		float inA = x + fastTanh(fb * twinFb), inB = inA;
		if (mode == 1) { inA = x + yB * 0.5f; inB = x + yA * 0.5f; }
		twinA.push(softClamp(inA, 12.f)); twinB.push(softClamp(inB, 12.f));
		float dA = clampf(base + depth * fastTanh(yB) + extra, 1.f, (float)twinA.size() - 8.f);
		float dB = clampf(base - depth * fastTanh(yA) - extra, 1.f, (float)twinB.size() - 8.f);
		yA = twinA.read(dA); yB = twinB.read(dB);
		float sum = yA - yB;
		float apMs = value(8, 3, p[3]), apMix = p[4];
		twinAp[0].setMs(apMs, fs); twinAp[1].setMs(apMs * 1.31f, fs);
		twinAp[0].g = twinAp[1].g = 0.6f;
		float a0 = twinAp[0].process(sum), a1 = twinAp[1].process(sum);
		twinFb = softClamp(sum, 10.f);
		wl = sum + (a0 - sum) * apMix;
		wr = -sum + (a1 + sum) * apMix;
		if (!finite(yA) || !finite(yB)) { yA = yB = twinFb = 0.f; twinA.clear(); twinB.clear(); }
	}
};

} // namespace dens
