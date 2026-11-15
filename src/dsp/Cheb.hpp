// CHEB: stereo Chebyshev (second kind) waveshaper with 40 predefined 10-term shapers, scanned.
#pragma once
#include "Common.hpp"
#include "Filters.hpp"
#include "Oversample.hpp"
#include "ChebTables.hpp"

namespace dens {

// S(x) = sum_{n=1..10} w[n-1]·U_n(x), by Clenshaw: f = 2x·b1 - b2.
inline float chebU(const float* w, float x) {
	float b1 = 0.f, b2 = 0.f;
	for (int n = 10; n >= 1; n--) {
		float b0 = w[n - 1] + 2.f * x * b1 - b2;
		b2 = b1; b1 = b0;
	}
	// after the loop b1 = b_1, b2 = b_2
	return 2.f * x * b1 - b2;
}

// Clip that is linear to ±0.8, then a smooth knee reaching exactly ±1 at ±1.2 (slope 0 there).
inline float chebClip(float u) {
	float a = std::fabs(u);
	if (a <= 0.8f) return u;
	float t = std::min((a - 0.8f) / 0.4f, 1.f);
	float y = 0.8f + 0.2f * (2.f * t - t * t);
	return u < 0.f ? -y : y;
}

// Interpolated weights for a scan position 0..39.
inline void chebWeights(float s, float* w) {
	s = clampf(s, 0.f, 39.f);
	int a = (int)s; int b = std::min(a + 1, 39); float f = s - a;
	for (int i = 0; i < 10; i++) w[i] = CHEB_W[a][i] + (CHEB_W[b][i] - CHEB_W[a][i]) * f;
}

struct ChebChannel {
	Up4 up; Down4 down; DcBlock dc;
	float w[10] = {};
	void setSampleRate(float fs) { dc.setSampleRate(fs); }
	void reset() { up.reset(); down.reset(); dc.reset(); }
	float process(float in, float scan, float drive, float bias) {
		chebWeights(scan, w);
		float u = drive * in / 5.f + bias;
		float s4[4], y4[4];
		up.process(u, s4);
		for (int i = 0; i < 4; i++) y4[i] = chebU(w, chebClip(s4[i]));
		return dc.process(down.process(y4));
	}
};

struct Cheb {
	ChebChannel ch[2];
	void setSampleRate(float fs) { ch[0].setSampleRate(fs); ch[1].setSampleRate(fs); }
	void reset() { ch[0].reset(); ch[1].reset(); }
	static float wrapScan(float s) { while (s < 0.f) s += 40.f; while (s >= 40.f) s -= 40.f; return std::min(s, 39.f); }
	struct Params { float scan = 9.f, spread = 0.f, drive = 1.f, bias = 0.f, mix = 1.f, level = 1.f; };
	void process(const Params& p, float inL, float inR, float& outL, float& outR) {
		float sL = clampf(p.scan, 0.f, 39.f), sR = wrapScan(p.scan + p.spread);
		float wl = 5.f * ch[0].process(inL, sL, p.drive, p.bias);
		float wr = 5.f * ch[1].process(inR, sR, p.drive, p.bias);
		outL = softClamp((inL + (wl - inL) * p.mix) * p.level);
		outR = softClamp((inR + (wr - inR) * p.mix) * p.level);
		if (!finite(outL) || !finite(outR)) { reset(); outL = outR = 0.f; }
	}
};

} // namespace dens
