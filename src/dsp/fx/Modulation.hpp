// Modulation and colour blocks: phaser, through-zero flanger, chorus, ring modulator, pitch and
// frequency shifters, drive, band-pass and an envelope follower.
#pragma once
#include "../Common.hpp"
#include "../DelayLine.hpp"
#include "../Filters.hpp"
#include "../Hilbert.hpp"
#include "../Oversample.hpp"

namespace dens {

struct EnvFollower {
	float y = 0.f, ca = 0.f, cr = 0.f;
	void set(float attack, float release, float fs) { ca = 1.f - std::exp(-1.f / (attack * fs)); cr = 1.f - std::exp(-1.f / (release * fs)); }
	float process(float x) { float a = std::fabs(x); y += (a - y) * (a > y ? ca : cr); return y; }
	void reset() { y = 0.f; }
};

// N first-order allpass stages swept by an external control (0..1 position) around a centre.
struct Phaser {
	static constexpr int MAXN = 16;
	Allpass1 st[MAXN];
	int n = 16;
	float fb = 0.f, fs = 48000.f;
	int ctr = 0;
	// sweep: -1..1 control; centre Hz; depthOct octaves total; spread 0..1 per-stage offset; resonance -0.95..0.95
	float centre = 600.f, depthOct = 3.f, spread = 0.3f, resonance = 0.5f;
	void init(float sampleRate, int stages) { fs = sampleRate; n = std::min(stages, MAXN); reset(); }
	void reset() { for (auto& s : st) s.reset(); fb = 0.f; ctr = 0; }
	float process(float x, float sweep) {
		if (ctr-- <= 0) {
			ctr = 7;
			float f0 = centre * std::pow(2.f, 0.5f * depthOct * clampf(sweep, -1.f, 1.f));
			for (int k = 0; k < n; k++) {
				float off = 1.f + spread * ((k + 0.5f) / n - 0.5f) * 1.6f;
				st[k].set(clampf(f0 * off, 20.f, 0.45f * fs), fs);
			}
		}
		float v = x + fastTanh(fb * clampf(resonance, -0.95f, 0.95f));
		for (int k = 0; k < n; k++) v = st[k].process(v);
		fb = softClamp(v, 12.f);
		return v;   // wet (all-passed); the caller mixes with dry for notches
	}
};

// Through-zero flanger: line A fixed at zero point, line B at zero + depth·lfo. Out = A - B (or A + B).
struct Flanger {
	DelayLine dl;
	float fs = 48000.f, prev = 0.f;
	void init(float sampleRate, float maxMs = 30.f) { fs = sampleRate; dl.init((int)(maxMs * 0.001f * fs) + 16); prev = 0.f; }
	void clear() { dl.clear(); prev = 0.f; }
	// lfo -1..1, zeroMs, depthMs, feedback -0.95..0.95 (above 1 saturates)
	float process(float x, float lfo, float zeroMs, float depthMs, float feedback, bool throughZero = true) {
		dl.push(softClamp(x + fastTanh(feedback * prev), 12.f));
		float za = zeroMs * 0.001f * fs;
		float zb = (zeroMs + depthMs * lfo) * 0.001f * fs;
		float a = dl.read(std::max(1.f, za));
		float b = dl.read(std::max(1.f, zb));
		float y = throughZero ? 0.5f * (a - b) * 2.f : 0.5f * (a + b);
		prev = y;
		return y;
	}
};

// Three-tap stereo chorus around 7, 11, 15 ms.
struct Chorus {
	DelayLine dl;
	float fs = 48000.f, ph = 0.f;
	void init(float sampleRate) { fs = sampleRate; dl.init((int)(0.03f * fs) + 16); }
	void clear() { dl.clear(); }
	void process(float x, float rate, float depthMs, float& l, float& r) {
		dl.push(x);
		ph += rate / fs; if (ph >= 1.f) ph -= 1.f;
		float t[3];
		const float base[3] = {7.f, 11.f, 15.f};
		for (int i = 0; i < 3; i++) {
			float m = std::sin(TWO_PI * (ph + i / 3.f));
			t[i] = dl.read((base[i] + depthMs * m) * 0.001f * fs);
		}
		l = (t[0] + t[2]) * 0.5f;
		r = (t[1] + t[2]) * 0.5f;
	}
};

struct SineOsc {
	float ph = 0.f;
	float process(float hz, float fs) { ph += hz / fs; ph -= std::floor(ph); return std::sin(TWO_PI * ph); }
};

struct RingMod {
	SineOsc osc;
	float process(float x, float hz, float depth, float fs) { float c = osc.process(hz, fs); return x * (1.f - depth) + x * c * depth; }
};

// Pitch shifter: two taps on a 100 ms delay, 180 degrees apart, sine-squared windows.
struct PitchShift {
	DelayLine dl;
	float fs = 48000.f, p = 0.f, win = 4800.f;
	void init(float sampleRate, float windowMs = 100.f) { fs = sampleRate; win = windowMs * 0.001f * fs; dl.init((int)win + 16); p = 0.f; }
	void clear() { dl.clear(); }
	float process(float x, float semis) {
		dl.push(x);
		float r = std::pow(2.f, clampf(semis, -24.f, 24.f) / 12.f);
		p += (1.f - r) / win;
		p -= std::floor(p);
		float q = p + 0.5f; q -= std::floor(q);
		float wa = std::sin(PI * p), wb = std::sin(PI * q);
		return dl.read(1.f + p * (win - 2.f)) * wa * wa + dl.read(1.f + q * (win - 2.f)) * wb * wb;
	}
};

// Single-sideband frequency shifter (with this Hilbert pair, q leads i): up = I·cos + Q·sin.
struct FreqShift {
	Hilbert h;
	float ph = 0.f;
	void reset() { h.reset(); ph = 0.f; }
	float process(float x, float hz, float fs, float* other = nullptr) {
		float i, q;
		h.process(x, i, q);
		ph += hz / fs; ph -= std::floor(ph);
		float c = std::cos(TWO_PI * ph), s = std::sin(TWO_PI * ph);
		if (other) *other = i * c - q * s;   // opposite direction
		return i * c + q * s;
	}
};

// x2-oversampled tanh drive, normalised so small signals keep unity gain at gain 1.
struct Drive {
	Up2 up; Down2 down;
	float process(float x, float gain) {
		float a, b;
		up.process(x * 0.2f, a, b);
		float n = fastTanh(gain);
		a = fastTanh(gain * a) / n; b = fastTanh(gain * b) / n;
		return down.process(a, b) * 5.f;
	}
	// Same curve, but small signals pass at unity gain (for use inside feedback loops).
	float processUnity(float x, float gain) {
		float n = fastTanh(gain);
		return process(x, gain) * n / std::max(gain, 1e-3f);
	}
	void reset() { up.reset(); down.reset(); }
};

} // namespace dens
