// AXIOM: ten equations turned into four polyrhythmic pulse channels (W, X, Y, Z).
#pragma once
#include "Common.hpp"
#include "Noise.hpp"
#include "Lfo.hpp"
#include <string>

namespace dens {

static const char* const AXIOM_EQ_NAMES[10] = {"EUCLID", "RATIO", "LOGISTIC", "MODULO", "COLLATZ", "GOLDEN", "PRIMES", "AUTOMATON", "GRAY", "LORENZ"};
static const char* const AXIOM_A_NAMES[10] = {"PULSES", "P", "R", "MULTIPLIER", "START", "ALPHA", "STRIDE", "RULE", "BIT", "SPEED"};
static const char* const AXIOM_B_NAMES[10] = {"ACCENT EVERY", "Q", "THRESHOLD", "WINDOW", "ODD/EVEN", "OFFSET", "OFFSET", "CELL", "INVERT", "THRESHOLD"};
static const float AXIOM_RATIOS[12] = {0.25f, 1.f / 3.f, 0.5f, 2.f / 3.f, 0.75f, 1.f, 4.f / 3.f, 1.5f, 1.25f, 2.f, 3.f, 4.f};
static const char* const AXIOM_RATIO_NAMES[12] = {"1/4", "1/3", "1/2", "2/3", "3/4", "1", "4/3", "3/2", "5/4", "2", "3", "4"};
static const char* const AXIOM_SCALE_NAMES[7] = {"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PENTATONIC", "WHOLE TONE", "HARMONIC"};

inline bool isPrime(int n) {
	if (n < 2) return false;
	if (n < 4) return true;
	if (n % 2 == 0) return false;
	for (int d = 3; d * d <= n; d += 2) if (n % d == 0) return false;
	return true;
}
inline int primeIndex(int n) {   // 1-based index of prime n, or 0
	int c = 0;
	for (int k = 2; k <= n; k++) if (isPrime(k)) c++;
	return c;
}

// Quantise volts to a scale (1 V/oct).
inline float quantise(float v, int scale) {
	static const float sets[7][12] = {
		{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
		{0, 2, 4, 5, 7, 9, 11},
		{0, 2, 3, 5, 7, 8, 10},
		{0, 2, 3, 5, 7, 9, 10},
		{0, 2, 4, 7, 9},
		{0, 2, 4, 6, 8, 10},
		{0, 3.8631f, 7.0196f, 9.6883f},   // harmonics 1, 5/4, 3/2, 7/4 in semitones
	};
	static const int counts[7] = {12, 7, 7, 7, 5, 6, 4};
	scale = std::max(0, std::min(6, scale));
	float semis = v * 12.f;
	float oct = std::floor(semis / 12.f);
	float within = semis - oct * 12.f;
	float best = 0.f, bd = 1e9f;
	for (int i = 0; i <= counts[scale]; i++) {
		float s = i < counts[scale] ? sets[scale][i] : 12.f;
		float d = std::fabs(within - s);
		if (d < bd) { bd = d; best = s; }
	}
	return (oct * 12.f + best) / 12.f;
}

struct AxiomChannel {
	// settings
	int eq = 0;       // 0..9
	int len = 16;     // 1..64, 0 = infinite
	float a = 0.5f, b = 0.5f;
	int rot = 0;
	// state
	long step = -1;   // steps taken since reset
	int rotOffset = 0;
	double lx = 0.4;  // logistic
	long cn = 1, cmax = 1;  // collatz
	uint16_t row = 0;  // automaton
	Lorenz lor;
	double lastX = 0.0;
	uint64_t seed = 1;
	bool gate = false, rawGate = false;
	float v = 0.f;
	int pulsesSeen = 0;

	int L() const { return len > 0 ? len : 16; }
	static int euclid(long j, int k, int L) { return k > 0 && ((j * k) % L) < k; }

	void resetState() {
		Rng r(seed);
		lx = 0.1 + 0.8 * r.uniform();
		cn = 1 + (long)std::lround(a * 998.f); cmax = std::max<long>(cn, 2);
		row = (uint16_t)(r.next() & 0xffff); if (!row) row = 1;
		lor.reset(0.1 + 0.2 * r.uniform(), 0.0, 20.0);
		lastX = lor.x;
	}
	void reset() { step = -1; rotOffset = 0; resetState(); gate = rawGate = false; v = 0.f; }

	// Advance one step with effective A (possibly modulated). Returns raw gate.
	bool advance(float aEff) {
		step++;
		int Lc = L();
		long i = step;
		bool finite = len > 0;
		if (finite && (i % Lc) == 0) resetStateAt();
		int rr = rot + rotOffset;
		long j = finite ? ((i + rr) % Lc) : (i + rr);
		bool g = false;
		float val = 0.f;
		float A = clampf(aEff, 0.f, 1.f), B = clampf(b, 0.f, 1.f);
		switch (eq) {
			case 0: {  // EUCLID
				int k = (int)std::lround(A * Lc);
				g = euclid(j, k, Lc);
				int every = 1 + (int)std::lround(B * 7.f);
				long p = k > 0 ? (j * k) / Lc : 0;
				val = g ? ((p % every) == 0 ? 1.f : 0.5f) : 0.f;
			} break;
			case 1: {  // RATIO: p and q evenly spaced trains over the length, union
				int p = 1 + (int)std::lround(A * 15.f), q = 1 + (int)std::lround(B * 15.f);
				bool gp = euclid(j, std::min(p, Lc), Lc), gq = euclid(j, std::min(q, Lc), Lc);
				g = gp || gq;
				val = gp && gq ? 1.f : (gp ? 0.66f : (gq ? 0.33f : 0.f));
			} break;
			case 2: {  // LOGISTIC
				double r = 3.4 + 0.6 * A;
				lx = r * lx * (1.0 - lx);
				if (!(lx > 1e-6 && lx < 1.0 - 1e-6)) lx = 0.37;
				g = lx > B; val = (float)lx;
			} break;
			case 3: {  // MODULO
				int m = 1 + (int)std::lround(A * 30.f);
				long r = (j * m) % Lc;
				g = r < std::lround(B * Lc);
				val = (float)r / Lc;
			} break;
			case 4: {  // COLLATZ
				long start = 1 + (long)std::lround(A * 998.f);
				if (step == 0 || cn < 1) { cn = start; cmax = std::max<long>(start, 2); }
				bool odd = cn & 1;
				g = B < 0.5f ? odd : !odd;
				val = (float)(std::log2((double)cn) / std::log2((double)cmax));
				if (cn == 1) cn = start;
				else cn = odd ? 3 * cn + 1 : cn / 2;
				if (cn > 1000000000L) cn = start;
				cmax = std::max(cmax, cn);
			} break;
			case 5: {  // GOLDEN
				double al = A < 0.5f ? 0.01 + (A / 0.5) * (0.618034 - 0.01) : 0.618034 + ((A - 0.5) / 0.5) * (0.99 - 0.618034);
				g = (std::floor((j + 1) * al + B) - std::floor(j * al + B)) >= 1.0;
				double f = j * al; val = (float)(f - std::floor(f));
			} break;
			case 6: {  // PRIMES
				int s = 1 + (int)std::lround(A * 8.f), o = (int)std::lround(B * Lc);
				long n = j * s + o;
				g = isPrime((int)std::min<long>(n, 100000));
				val = g ? std::min(1.f, primeIndex((int)std::min<long>(n, 400)) / 64.f) : 0.f;
			} break;
			case 7: {  // AUTOMATON: elementary CA on a 16-cell ring
				int rule = (int)std::lround(A * 255.f);
				uint16_t nr = 0;
				for (int c = 0; c < 16; c++) {
					int l = (row >> ((c + 15) & 15)) & 1, m = (row >> c) & 1, r = (row >> ((c + 1) & 15)) & 1;
					int idx = (l << 2) | (m << 1) | r;
					if ((rule >> idx) & 1) nr |= (uint16_t)(1u << c);
				}
				row = nr;
				int cell = (int)std::lround(B * 15.f);
				g = (row >> cell) & 1;
				val = row / 65535.f;
			} break;
			case 8: {  // GRAY
				int bit = (int)std::lround(A * 5.f);
				long gg = j ^ (j >> 1);
				g = ((gg >> bit) & 1) != (B > 0.5f ? 1 : 0);
				val = (gg & 63) / 63.f;
			} break;
			case 9: {  // LORENZ
				double h = (0.002 + 0.02 * A) / 4.0;
				for (int s = 0; s < 4; s++) lor.step(h);
				double th = (B - 0.5) * 20.0;
				g = lastX < th && lor.x >= th;
				lastX = lor.x;
				val = clampf((float)((lor.z - 5.0) / 40.0), 0.f, 1.f);
			} break;
		}
		rawGate = g;
		v = clampf(val, 0.f, 1.f);
		return g;
	}
	void resetStateAt() {
		// equation restarts at each length so finite lengths repeat
		if (eq == 2 || eq == 7 || eq == 9) {
			resetState();
			// pre-roll for rotation on stateful equations is folded into j for the stateless ones only
		}
		if (eq == 4) { cn = 1 + (long)std::lround(a * 998.f); cmax = std::max<long>(cn, 2); }
	}
};

struct Axiom {
	AxiomChannel ch[4];
	int ratioIdx[4] = {5, 5, 5, 5};
	double beats = 0.0;      // global step count (fractional within)
	double phase[4] = {0, 0, 0, 0};
	long lastStep[4] = {-1, -1, -1, -1};
	float sinceTick = 0.f, period = 0.136f;   // external clock measurement
	bool haveTick = false;
	float gateTimer[4] = {0, 0, 0, 0};
	float accent[4] = {0, 0, 0, 0}, pitch[4] = {0, 0, 0, 0};
	bool out[4] = {false, false, false, false};
	int logic = 0;   // 0 off, 1 xor, 2 and, 3 or
	float complexity = 0.f, range = 2.f;
	int scale = 4;
	bool triggers = false;
	uint64_t seed = 1;

	void setSeed(uint64_t s) { seed = s; for (int c = 0; c < 4; c++) { ch[c].seed = s * 4 + c + 1; } }
	void reset() {
		beats = 0.0; sinceTick = 0.f;
		for (int c = 0; c < 4; c++) { ch[c].reset(); lastStep[c] = -1; phase[c] = 0; gateTimer[c] = 0; out[c] = false; }
	}
	int tier() const { return complexity > 0.66f ? 3 : (complexity > 0.33f ? 2 : 1); }

	// Internal clock: bpm (16th steps). External: tick on each rising edge.
	void process(float dt, bool external, bool tick, float bpm, bool run) {
		if (!run) { for (int c = 0; c < 4; c++) out[c] = false; return; }
		if (external) {
			sinceTick += dt;
			if (tick) {
				if (haveTick) period = clampf(sinceTick, 0.002f, 10.f);
				haveTick = true;
				beats = std::floor(beats) + 1.0;
				sinceTick = 0.f;
			}
			double frac = std::min(0.999, (double)(sinceTick / std::max(period, 1e-3f)));
			beats = std::floor(beats) + frac;
		} else {
			period = 60.f / (bpm * 4.f);
			beats += dt / period;
		}
		for (int c = 0; c < 4; c++) {
			float r = AXIOM_RATIOS[ratioIdx[c]];
			double ph = beats * r;
			long st = (long)std::floor(ph);
			if (st > lastStep[c] && beats >= 0.0) {
				lastStep[c] = st;
				stepChannel(c);
			}
			float stepLen = period / r;
			gateTimer[c] -= dt;
			float frac = (float)(ph - std::floor(ph));
			bool g = triggers ? gateTimer[c] > 0.f : (gateFired[c] && frac < 0.5f);
			out[c] = g;
			(void)stepLen;
		}
	}
	bool gateFired[4] = {false, false, false, false};
	void stepChannel(int c) {
		AxiomChannel& k = ch[c];
		float aEff = k.a;
		if (complexity > 0.33f) {
			const AxiomChannel& prev = ch[(c + 3) % 4];
			aEff = clampf(k.a + (complexity - 0.33f) * 0.6f * (prev.v - 0.5f) * 2.f, 0.f, 1.f);
		}
		bool g = k.advance(aEff);
		if (complexity > 0.66f && k.len > 0 && ((k.step + 1) % k.L()) == 0)
			k.rotOffset = (k.rotOffset + (int)std::lround(k.v * k.L())) % 64;
		bool left = ch[(c + 3) % 4].rawGate;
		switch (logic) {
			case 1: g = g != left; break;
			case 2: g = g && left; break;
			case 3: g = g || left; break;
			default: break;
		}
		gateFired[c] = g;
		if (g) {
			gateTimer[c] = 0.005f;
			accent[c] = 10.f * k.v;
			pitch[c] = quantise(k.v * range, scale);
		}
	}
};

} // namespace dens
