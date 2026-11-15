// WEAVE: twelve coupled complex modulators, four voltage-controlled gates, a four-input pan mixer.
#pragma once
#include "Common.hpp"
#include "Filters.hpp"
#include "Lfo.hpp"

namespace dens {

// SHAPE anchors: sine, triangle, ramp, stepped random, smooth random, logistic chaos, Lorenz.
static const float WEAVE_ANCHORS[7] = {0.f, 0.2f, 0.4f, 0.55f, 0.7f, 0.85f, 1.f};
static const char* const WEAVE_SHAPE_NAMES[7] = {"SINE", "TRIANGLE", "RAMP", "STEPPED", "SMOOTH", "CHAOS", "LORENZ"};

struct ComplexMod {
	Lfo lfo;
	LogisticLfo logi;
	Lorenz lor;
	float out = 0.f;   // −1..1
	explicit ComplexMod(uint64_t seed = 1) : lfo(seed) { logi.x = 0.2f + 0.6f * lfo.rng.uniform(); }
	void reseed(uint64_t seed) { lfo = Lfo(seed); logi = LogisticLfo(); logi.x = 0.2f + 0.6f * lfo.rng.uniform(); logi.prev = logi.x; lor.reset(0.1 + 0.1 * lfo.rng.uniform(), 0.0, 20.0); }
	static void weights(float shape, int& a, float& f) {
		shape = clampf(shape, 0.f, 1.f);
		a = 0;
		while (a < 5 && shape > WEAVE_ANCHORS[a + 1]) a++;
		f = (shape - WEAVE_ANCHORS[a]) / (WEAVE_ANCHORS[a + 1] - WEAVE_ANCHORS[a]);
	}
	float shapeValue(int s, float hz, float dt, bool active) {
		switch (s) {
			case 0: return lfo.value(Lfo::SINE);
			case 1: return lfo.value(Lfo::TRIANGLE);
			case 2: return lfo.value(Lfo::RAMP_UP);
			case 3: return lfo.value(Lfo::STEP_RANDOM);
			case 4: return lfo.value(Lfo::SMOOTH_RANDOM);
			case 5: return 0.f;
			case 6: return 0.f;
		}
		return 0.f;
	}
	float process(float hz, float shape, float dt) {
		int a; float f;
		weights(shape, a, f);
		lfo.advance(hz, dt);
		float va = 0.f, vb = 0.f;
		auto val = [&](int s) -> float {
			if (s == 5) {
				float r = 3.57f + 0.43f * clampf((shape - 0.85f) / 0.15f, 0.f, 1.f);
				return logi.process(hz, r, dt);
			}
			if (s == 6) {
				lor.step(std::min(0.02, (double)hz * dt * 1.3));
				return clampf((float)(lor.x / 20.0), -1.f, 1.f);
			}
			return shapeValue(s, hz, dt, true);
		};
		va = val(a);
		vb = f > 0.f ? val(a + 1) : 0.f;
		out = va + (vb - va) * f;
		return out;
	}
};

struct VcGate {
	enum Response { VCA, LPG, BOTH };
	Svf lp;
	float vac = 0.f;   // vactrol state 0..1
	float fs = 48000.f;
	int ctr = 0;
	void setSampleRate(float f) { fs = f; }
	float process(float in, float gain, Response r) {
		gain = clampf(gain, 0.f, 1.f);
		float dt = 1.f / fs;
		// vactrol: fast rise, slow nonlinear fall that slows as the level drops
		float tau = gain > vac ? 0.005f : 0.05f * (1.f + 2.f * (1.f - vac));
		vac += (gain - vac) * (1.f - std::exp(-dt / tau));
		if (r == VCA) return in * gain * gain;
		if (ctr-- <= 0) { ctr = 7; lp.set(80.f * std::pow(18000.f / 80.f, vac), 0.6f, fs); }
		lp.process(in);
		return r == LPG ? lp.lp * vac * vac : lp.lp * gain * gain;
	}
};

struct Weave {
	ComplexMod mod[12];
	VcGate gate[4];
	float fs = 48000.f;
	uint64_t seed = 1;
	Weave() { for (int i = 0; i < 12; i++) mod[i].reseed(i + 1); }
	void setSampleRate(float f) { fs = f; for (auto& g : gate) g.setSampleRate(f); }
	void reseed(uint64_t s) { seed = s; for (int i = 0; i < 12; i++) mod[i].reseed(s * 13 + i + 1); }
	// rates in Hz (already × speed), shapes 0..1, couple 0..1. outs −1..1
	void processMods(const float* rate, const float* shape, float couple, float* outs) {
		float dt = 1.f / fs;
		for (int i = 0; i < 12; i++) {
			float prev = mod[(i + 11) % 12].out;
			float hz = rate[i] * std::pow(2.f, 2.f * couple * prev);
			outs[i] = mod[i].process(clampf(hz, 0.f, 0.2f * fs), shape[i], dt);
			if (!finite(outs[i])) { mod[i].reseed(seed + i); outs[i] = 0.f; }
		}
	}
};

} // namespace dens
