// x2 oversampling with a 31-tap windowed-sinc half-band (used for hard clipping and rattle).
#pragma once
#include "Common.hpp"

namespace dens {

struct HalfBand {
	static constexpr int N = 31;
	float h[N];
	float z[N] = {};
	int p = 0;
	HalfBand() {
		int c = N / 2;
		for (int i = 0; i < N; i++) {
			int k = i - c;
			float s = k == 0 ? 0.5f : std::sin(PI * k * 0.5f) / (PI * k);
			float win = 0.42f - 0.5f * std::cos(TWO_PI * i / (N - 1)) + 0.08f * std::cos(2 * TWO_PI * i / (N - 1));
			h[i] = s * win;
		}
		float sum = 0; for (float v : h) sum += v;
		for (float& v : h) v /= sum;
	}
	float process(float x) {
		z[p] = x;
		float y = 0;
		int idx = p;
		for (int i = 0; i < N; i++) { y += h[i] * z[idx]; idx = idx == 0 ? N - 1 : idx - 1; }
		p = (p + 1) % N;
		return y;
	}
	void reset() { for (float& v : z) v = 0; }
};

// Usage: up.process(x, a, b) gives two samples at 2x; run f on both; down.process(fa, fb).
struct Up2 {
	HalfBand f;
	void process(float x, float& a, float& b) { a = 2.f * f.process(x); b = 2.f * f.process(0.f); }
	void reset() { f.reset(); }
};
struct Down2 {
	HalfBand f;
	float process(float a, float b) { f.process(a); return f.process(b); }
	void reset() { f.reset(); }
};

// x4 as two cascaded x2 stages.
struct Up4 {
	Up2 a, b, c;
	void process(float x, float* out4) {
		float p, q;
		a.process(x, p, q);
		b.process(p, out4[0], out4[1]);
		c.process(q, out4[2], out4[3]);
	}
	void reset() { a.reset(); b.reset(); c.reset(); }
};
struct Down4 {
	Down2 a, b, c;
	float process(const float* in4) {
		float p = a.process(in4[0], in4[1]);
		float q = b.process(in4[2], in4[3]);
		return c.process(p, q);
	}
	void reset() { a.reset(); b.reset(); c.reset(); }
};

} // namespace dens
