// LFO with several shapes, plus smooth random and chaos helpers. Outputs are -1..1.
#pragma once
#include "Common.hpp"
#include "Noise.hpp"

namespace dens {

struct Lfo {
	enum Shape { SINE, TRIANGLE, RAMP_UP, RAMP_DOWN, SQUARE, STEP_RANDOM, SMOOTH_RANDOM };
	float phase = 0.f;
	float a = 0.f, b = 0.f;   // random endpoints
	Rng rng;
	explicit Lfo(uint64_t seed = 3) : rng(seed) { a = rng.bipolar(); b = rng.bipolar(); }
	// returns true at a wrap
	bool advance(float hz, float dt) {
		phase += hz * dt;
		if (phase >= 1.f || phase < 0.f) {
			phase -= std::floor(phase);
			a = b; b = rng.bipolar();
			return true;
		}
		return false;
	}
	float value(Shape s) const {
		float p = phase;
		switch (s) {
			case SINE: return std::sin(TWO_PI * p);
			case TRIANGLE: return p < 0.25f ? 4.f * p : (p < 0.75f ? 2.f - 4.f * p : 4.f * p - 4.f);
			case RAMP_UP: return 2.f * p - 1.f;
			case RAMP_DOWN: return 1.f - 2.f * p;
			case SQUARE: return p < 0.5f ? 1.f : -1.f;
			case STEP_RANDOM: return a;
			case SMOOTH_RANDOM: { float t = 0.5f - 0.5f * std::cos(PI * p); return a + (b - a) * t; }
		}
		return 0.f;
	}
	float process(float hz, float dt, Shape s) { advance(hz, dt); return value(s); }
	void setPhase(float p) { phase = p - std::floor(p); }
};

// Smooth random wander at a given rate (cosine-interpolated random points).
struct SmoothRandom {
	Lfo l;
	explicit SmoothRandom(uint64_t seed = 5) : l(seed) {}
	float process(float hz, float dt) { return l.process(hz, dt, Lfo::SMOOTH_RANDOM); }
};

// Logistic map stepped at a rate, cosine-smoothed between steps; output -1..1.
struct LogisticLfo {
	float x = 0.37f, prev = 0.37f, phase = 0.f;
	float process(float hz, float r, float dt) {
		phase += hz * dt;
		if (phase >= 1.f) {
			phase -= std::floor(phase);
			prev = x;
			x = r * x * (1.f - x);
			if (!(x > 1e-4f && x < 1.f - 1e-4f)) x = 0.37f;
		}
		float t = 0.5f - 0.5f * std::cos(PI * phase);
		return 2.f * (prev + (x - prev) * t) - 1.f;
	}
};

// Lorenz attractor (sigma 10, rho 28, beta 8/3), RK4. speed scales time.
struct Lorenz {
	double x = 0.1, y = 0.0, z = 20.0;
	void reset(double x0 = 0.1, double y0 = 0.0, double z0 = 20.0) { x = x0; y = y0; z = z0; }
	void step(double h) {
		auto f = [](double X, double Y, double Z, double& dx, double& dy, double& dz) {
			dx = 10.0 * (Y - X); dy = X * (28.0 - Z) - Y; dz = X * Y - (8.0 / 3.0) * Z;
		};
		double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
		f(x, y, z, k1x, k1y, k1z);
		f(x + 0.5 * h * k1x, y + 0.5 * h * k1y, z + 0.5 * h * k1z, k2x, k2y, k2z);
		f(x + 0.5 * h * k2x, y + 0.5 * h * k2y, z + 0.5 * h * k2z, k3x, k3y, k3z);
		f(x + h * k3x, y + h * k3y, z + h * k3z, k4x, k4y, k4z);
		x += h / 6.0 * (k1x + 2 * k2x + 2 * k3x + k4x);
		y += h / 6.0 * (k1y + 2 * k2y + 2 * k3y + k4y);
		z += h / 6.0 * (k1z + 2 * k2z + 2 * k3z + k4z);
		if (!(std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) || std::fabs(x) > 1e3) reset();
	}
};

} // namespace dens
