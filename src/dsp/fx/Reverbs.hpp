// Reverb building blocks: Schroeder and nested allpasses, a Dattorro-style plate, an 8-line Hadamard FDN.
#pragma once
#include "../Common.hpp"
#include "../DelayLine.hpp"
#include "../Filters.hpp"
#include "../Hilbert.hpp"
#include <functional>

namespace dens {

// Schroeder allpass on a delay line: v = x - g·d; y = d + g·v; write v. Optional read modulation (samples).
struct AllpassD {
	DelayLine dl;
	float len = 1.f;   // samples
	float g = 0.5f;
	void init(float maxMs, float fs) { dl.init((int)(maxMs * 0.001f * fs) + 64); }
	void setMs(float ms, float fs) { len = clampf(ms * 0.001f * fs, 1.f, (float)dl.size() - 40.f); }
	float process(float x, float mod = 0.f) {
		float d = mod == 0.f ? dl.at((int)len - 1) : dl.read(clampf(len + mod, 2.f, (float)dl.size() - 8.f));
		float v = x - g * d;
		dl.push(v);
		return d + g * v;
	}
	float tap(int samples) const { return dl.at(samples); }
	void clear() { dl.clear(); }
};

// Two-level nested allpass (Gardner): an outer allpass whose delay path contains an inner allpass.
struct NestedAllpass {
	DelayLine outer;
	AllpassD inner;
	float lenO = 1.f, gO = 0.5f;
	void init(float maxMs, float fs) { outer.init((int)(maxMs * 0.001f * fs) + 64); inner.init(maxMs, fs); }
	void set(float outerMs, float innerMs, float g, float fs) {
		lenO = clampf(outerMs * 0.001f * fs, 2.f, (float)outer.size() - 8.f);
		inner.setMs(innerMs, fs);
		gO = g; inner.g = g * 0.8f;
	}
	float process(float x) {
		float d = inner.process(outer.at((int)lenO - 1));
		float v = x - gO * d;
		outer.push(v);
		return d + gO * v;
	}
	void clear() { outer.clear(); inner.clear(); }
};

// Dattorro plate (JAES 1997). Fully wet stereo output.
struct Plate {
	float fs = 48000.f, k = 1.f;   // k = fs / 29761
	DelayLine pre;
	OnePoleLP bw;
	AllpassD in[4];
	AllpassD tankAp1[2], tankAp2[2];
	DelayLine tankD1[2], tankD2[2];
	OnePoleLP damp[2];
	float fb[2] = {0, 0};
	float lfoPh[2] = {0.f, 0.25f};
	int ctr = 0;
	// params
	float size = 1.f, decay = 0.5f, dampHz = 6000.f, diffusion = 1.f, modDepthMs = 0.3f, modRate = 0.8f, predelayMs = 0.f;
	float shimmer = 0.f;  // unused by default
	std::function<float(int, float)> tankInsert;  // optional insert inside each tank branch (side, x) -> y
	static constexpr int L_AP1 = 672, L_D1A = 4453, L_AP2A = 1800, L_D2A = 3720;
	static constexpr int R_AP1 = 908, R_D1B = 4217, R_AP2B = 2656, R_D2B = 3163;
	void init(float sampleRate) {
		fs = sampleRate; k = fs / 29761.f;
		float maxScale = 2.2f;
		pre.init((int)(0.5f * fs) + 8);
		const float inMs[4] = {4.77f, 3.60f, 12.73f, 9.31f};
		for (int i = 0; i < 4; i++) { in[i].init(inMs[i] * 1.2f, fs); in[i].setMs(inMs[i], fs); }
		const int ap1[2] = {L_AP1, R_AP1}, d1[2] = {L_D1A, R_D1B}, ap2[2] = {L_AP2A, R_AP2B}, d2[2] = {L_D2A, R_D2B};
		for (int s = 0; s < 2; s++) {
			tankAp1[s].init((ap1[s] + 40) * maxScale / 29761.f * 1000.f, fs);
			tankAp2[s].init((ap2[s] + 40) * maxScale / 29761.f * 1000.f, fs);
			tankD1[s].init((int)((d1[s] + 40) * maxScale * k));
			tankD2[s].init((int)((d2[s] + 40) * maxScale * k));
		}
		bw.set(9000.f, fs);
		clear();
	}
	void clear() {
		pre.clear();
		for (auto& a : in) a.clear();
		for (int s = 0; s < 2; s++) { tankAp1[s].clear(); tankAp2[s].clear(); tankD1[s].clear(); tankD2[s].clear(); damp[s].reset(); fb[s] = 0; }
		bw.reset();
	}
	int L(int n) const { return std::max(2, (int)(n * k * size)); }
	void process(float xL, float xR, float& yL, float& yR) {
		float x = 0.5f * (xL + xR);
		if (++ctr >= 32) ctr = 0;
		pre.push(x);
		float p = predelayMs > 0.f ? pre.at(std::max(0, (int)(predelayMs * 0.001f * fs))) : x;
		p = bw.process(p);
		const float g[4] = {0.75f, 0.75f, 0.625f, 0.625f};
		for (int i = 0; i < 4; i++) { in[i].g = g[i] * clampf(diffusion, 0.f, 1.2f); p = in[i].process(p); }
		float dt = 1.f / fs;
		const int ap1[2] = {L_AP1, R_AP1}, d1[2] = {L_D1A, R_D1B}, ap2[2] = {L_AP2A, R_AP2B}, d2[2] = {L_D2A, R_D2B};
		float outTank[2];
		for (int s = 0; s < 2; s++) {
			lfoPh[s] += modRate * (s ? 1.13f : 1.f) * dt;
			if (lfoPh[s] >= 1.f) lfoPh[s] -= 1.f;
			float mod = modDepthMs * 0.001f * fs * std::sin(TWO_PI * lfoPh[s]);
			float t = p + fb[1 - s];
			tankAp1[s].g = -0.7f;
			tankAp1[s].len = (float)L(ap1[s]);
			t = tankAp1[s].process(t, mod);
			tankD1[s].push(t);
			t = tankD1[s].at(L(d1[s]) - 1);
			if (ctr == 0) damp[s].set(dampHz, fs);
			t = damp[s].process(t) * decay;
			if (tankInsert) t = tankInsert(s, t);
			tankAp2[s].g = 0.5f;
			tankAp2[s].len = (float)L(ap2[s]);
			t = tankAp2[s].process(t);
			tankD2[s].push(t);
			outTank[s] = softClamp(tankD2[s].at(L(d2[s]) - 1) * decay, 24.f);
		}
		fb[0] = outTank[0]; fb[1] = outTank[1];
		auto T = [&](int n) { return std::max(0, (int)(n * k * size)); };
		yL = 0.6f * (tankD1[1].at(T(266)) + tankD1[1].at(T(2974)) - tankAp2[1].tap(T(1913)) + tankD2[1].at(T(1996))
		           - tankD1[0].at(T(1990)) - tankAp2[0].tap(T(187)) - tankD2[0].at(T(1066)));
		yR = 0.6f * (tankD1[0].at(T(353)) + tankD1[0].at(T(3627)) - tankAp2[0].tap(T(1228)) + tankD2[0].at(T(2673))
		           - tankD1[1].at(T(2111)) - tankAp2[1].tap(T(335)) - tankD2[1].at(T(121)));
	}
	bool ok() const { return finite(fb[0]) && finite(fb[1]); }
};

// 8-line feedback delay network with a normalised Hadamard matrix.
struct Fdn8 {
	static constexpr int N = 8;
	float fs = 48000.f;
	DelayLine line[N];
	OnePoleLP damp[N];
	float state[N] = {};
	float lfo[N] = {};
	float gain[N] = {}, lenS[N] = {};
	int ctr = 0;
	float size = 1.f, rt60L = 2.f, rt60R = 2.f, dampHz = 8000.f, modSamples = 4.f;
	bool freeze = false;
	float loopGain = 1.f;            // extra multiplier on all line gains (programs may push above 1 into a limiter)
	std::function<float(int, float)> insert;   // optional per-line insert (line, x) -> y, set once
	bool insertOn = false;
	static constexpr float BASE_MS[N] = {29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 79.3f};
	void init(float sampleRate, float maxSize = 3.2f) {
		fs = sampleRate;
		for (int i = 0; i < N; i++) { line[i].init((int)(BASE_MS[i] * 0.001f * fs * maxSize) + 64); lfo[i] = i / (float)N; }
		clear();
	}
	void clear() { for (int i = 0; i < N; i++) { line[i].clear(); damp[i].reset(); state[i] = 0; } ctr = 0; }
	void invalidate() { ctr = 0; }
	static void hadamard(float* v) {
		for (int h = 1; h < N; h <<= 1)
			for (int i = 0; i < N; i += 2 * h)
				for (int j = i; j < i + h; j++) { float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
		const float s = 0.35355339f;
		for (int i = 0; i < N; i++) v[i] *= s;
	}
	void process(float xL, float xR, float& yL, float& yR) {
		float dt = 1.f / fs;
		float out[N];
		if (ctr-- <= 0) {
			ctr = 31;
			for (int i = 0; i < N; i++) {
				lenS[i] = BASE_MS[i] * clampf(size, 0.05f, 3.2f) * 0.001f * fs;
				float rt = (i & 1) ? rt60R : rt60L;
				gain[i] = freeze ? 1.f : std::pow(10.f, -3.f * (lenS[i] / fs) / std::max(rt, 0.05f));
				damp[i].set(dampHz, fs);
			}
		}
		for (int i = 0; i < N; i++) {
			lfo[i] += (0.1f + 0.8f * i / (N - 1)) * dt;
			if (lfo[i] >= 1.f) lfo[i] -= 1.f;
			float m = modSamples * std::sin(TWO_PI * lfo[i]);
			float d = clampf(lenS[i] + m, 2.f, (float)line[i].size() - 8.f);
			float r = line[i].read(d);
			if (!freeze) r = damp[i].process(r);
			r *= gain[i] * loopGain;
			if (insertOn && insert) r = insert(i, r);
			out[i] = r;
		}
		yL = (out[0] + out[2] + out[4] + out[6]) * 0.5f;
		yR = (out[1] + out[3] + out[5] + out[7]) * 0.5f;
		hadamard(out);
		float inL = freeze ? 0.f : xL, inR = freeze ? 0.f : xR;
		for (int i = 0; i < N; i++) {
			float in = (i & 1) ? inR : inL;
			in *= (i & 2) ? -0.5f : 0.5f;
			line[i].push(softClamp(out[i] + in, 20.f));
		}
	}
};

} // namespace dens
