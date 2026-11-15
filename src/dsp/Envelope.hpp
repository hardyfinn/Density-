// ADSR with exponential segments (each stage reaches 99% in its time) and an AD cycle mode.
#pragma once
#include "Common.hpp"

namespace dens {

struct Adsr {
	enum Stage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
	Stage stage = IDLE;
	float v = 0.f;
	void gateOn() { stage = ATTACK; }
	void gateOff() { if (stage != IDLE) stage = RELEASE; }
	static float coef(float t, float dt) { return 1.f - std::exp(-dt * 4.6f / std::max(t, 1e-4f)); }
	// attack target overshoots to 1.2 so the attack finishes in about its time, like analogue envelopes
	float process(float a, float d, float s, float r, float dt) {
		switch (stage) {
			case ATTACK:
				v += (1.2f - v) * coef(a, dt) * 1.6f;
				if (v >= 1.f) { v = 1.f; stage = DECAY; }
				break;
			case DECAY:
				v += (s - v) * coef(d, dt);
				if (std::fabs(v - s) < 1e-4f) stage = SUSTAIN;
				break;
			case SUSTAIN: v = s; break;
			case RELEASE:
				v += (0.f - v) * coef(r, dt);
				if (v < 1e-5f) { v = 0.f; stage = IDLE; }
				break;
			default: v = 0.f;
		}
		return v;
	}
	void reset() { stage = IDLE; v = 0.f; }
};

// Attack-decay, retriggerable; decay to zero. Used for drums and cycling envelopes.
struct AdEnv {
	bool attacking = false;
	float v = 0.f;
	void trigger() { attacking = true; }
	float process(float a, float d, float dt) {
		if (attacking) {
			v += (1.2f - v) * Adsr::coef(a, dt) * 1.6f;
			if (v >= 1.f) { v = 1.f; attacking = false; }
		} else {
			v += (0.f - v) * Adsr::coef(d, dt);
			if (v < 1e-6f) v = 0.f;
		}
		return v;
	}
	void reset() { attacking = false; v = 0.f; }
};

} // namespace dens
