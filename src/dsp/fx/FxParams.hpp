// Parameter kinds shared by NONET and TANGLE: knob 0..1 -> value, plus display name and unit.
#pragma once
#include "../Common.hpp"
#include <cstdio>
#include <string>

namespace dens {

enum class Kind {
	None, RevTime, RT60, Diffusion, LowPass, HighPass, Level, Mix, Rate, Depth, Feedback, DelayShort, DelayLong,
	Predelay, Pitch, Shift, RingFreq, Zero, Gain, Q, Coef, Bipolar, Hz, Size, Custom
};

// Generic mappings (§3.3 of the build instructions).
inline float mapKind(Kind k, float v) {
	v = clampf(v, 0.f, 1.f);
	switch (k) {
		case Kind::RevTime: return 0.98f * std::sqrt(v);                // plate decay 0..0.98
		case Kind::RT60: return expRange(v, 0.2f, 30.f);                 // seconds
		case Kind::Diffusion: return 0.3f + 0.5f * v;
		case Kind::LowPass: return expRange(v, 400.f, 18000.f);
		case Kind::HighPass: return expRange(v, 20.f, 2000.f);
		case Kind::Level: return v * v;
		case Kind::Mix: return v;
		case Kind::Rate: return expRange(v, 0.02f, 12.f);
		case Kind::Depth: return v;
		case Kind::Feedback: return 1.05f * v;
		case Kind::DelayShort: return expRange(v, 1.f, 1000.f);          // ms
		case Kind::DelayLong: return expRange(v, 10.f, 6000.f);          // ms
		case Kind::Predelay: return 250.f * v;                           // ms
		case Kind::Pitch: { float c = 2.f * v - 1.f; float a = std::fabs(c); a = a < 0.05f ? 0.f : (a - 0.05f) / 0.95f; return (c < 0 ? -12.f : 12.f) * a; }
		case Kind::Shift: { float c = 2.f * v - 1.f; return 500.f * c * c * c; }
		case Kind::RingFreq: return expRange(v, 0.1f, 5000.f);
		case Kind::Zero: return 0.5f + 9.5f * v;                         // ms
		case Kind::Gain: return expRange(v, 1.f, 40.f);
		case Kind::Q: return expRange(v, 0.5f, 20.f);
		case Kind::Coef: return 0.95f * v;
		case Kind::Bipolar: return 2.f * v - 1.f;
		case Kind::Hz: return expRange(v, 80.f, 8000.f);
		case Kind::Size: return 0.25f + 1.75f * v;
		default: return v;
	}
}

inline std::string formatKind(Kind k, float v) {
	char b[32];
	float m = mapKind(k, v);
	switch (k) {
		case Kind::RT60: std::snprintf(b, sizeof b, "%.1f s", m); break;
		case Kind::LowPass: case Kind::HighPass: case Kind::RingFreq: case Kind::Hz:
			if (m >= 1000.f) std::snprintf(b, sizeof b, "%.1f kHz", m / 1000.f); else std::snprintf(b, sizeof b, "%.0f Hz", m); break;
		case Kind::Rate: std::snprintf(b, sizeof b, "%.2f Hz", m); break;
		case Kind::DelayShort: case Kind::DelayLong: case Kind::Predelay: case Kind::Zero:
			if (m >= 1000.f) std::snprintf(b, sizeof b, "%.2f s", m / 1000.f); else std::snprintf(b, sizeof b, "%.1f ms", m); break;
		case Kind::Pitch: std::snprintf(b, sizeof b, "%+.1f st", m); break;
		case Kind::Shift: std::snprintf(b, sizeof b, "%+.1f Hz", m); break;
		case Kind::Gain: std::snprintf(b, sizeof b, "x%.1f", m); break;
		case Kind::Q: std::snprintf(b, sizeof b, "Q %.1f", m); break;
		case Kind::Bipolar: std::snprintf(b, sizeof b, "%+.0f%%", m * 100.f); break;
		case Kind::None: std::snprintf(b, sizeof b, "-"); break;
		default: std::snprintf(b, sizeof b, "%.0f%%", v * 100.f);
	}
	return b;
}

} // namespace dens
