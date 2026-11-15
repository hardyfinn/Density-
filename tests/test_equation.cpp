// EQUATION family: CHEB, AXIOM, TETRA.
#include "util.hpp"
#include "../src/dsp/Cheb.hpp"
#include "../src/dsp/Axiom.hpp"
#include "../src/dsp/Tetra.hpp"
#include <string>

using namespace dens;

static double lcgNext(uint32_t& s) { s = (uint32_t)(((uint64_t)s * 1103515245u + 12345u) & 0x7fffffffu); return s / 2147483648.0; }
static double Un(int n, double x) { double u0 = 1, u1 = 2 * x; if (n == 0) return u0; for (int i = 0; i < n - 1; i++) { double t = 2 * x * u1 - u0; u0 = u1; u1 = t; } return u1; }

TEST("cheb: tables match the generator") {
	for (int k = 0; k < 40; k++) {
		double w[10] = {};
		if (k < 8) w[k] = 1;
		else if (k < 16) { const double p[8] = {0.25, 0.5, 0.75, 1, 1.5, 2, 2.5, 3}; for (int n = 1; n <= 10; n++) w[n - 1] = std::pow(n, -p[k - 8]); }
		else if (k < 24) { const double p[4] = {0.5, 1, 1.5, 2}; bool odd = k < 20; for (int n = 1; n <= 10; n++) w[n - 1] = ((n % 2 == 1) == odd) ? std::pow(n, -p[(k - 16) % 4]) : 0; }
		else if (k < 32) { double c = 2 + (k - 24); for (int n = 1; n <= 10; n++) w[n - 1] = std::exp(-std::pow((n - c) / 1.2, 2)); }
		else { uint32_t s = 1000 + k; for (int n = 1; n <= 10; n++) w[n - 1] = (2 * lcgNext(s) - 1) / std::sqrt((double)n); }
		double m = 0;
		for (int i = 0; i <= 4096; i++) { double x = -1 + 2.0 * i / 4096, y = 0; for (int n = 1; n <= 10; n++) y += w[n - 1] * Un(n, x); m = std::max(m, std::fabs(y)); }
		for (int n = 0; n < 10; n++) CHECK_NEAR(CHEB_W[k][n], w[n] / m, 1e-6);
	}
}

TEST("cheb: Clenshaw equals direct sum") {
	for (int k = 0; k < 40; k += 3)
		for (float x = -1.f; x <= 1.f; x += 0.125f) {
			double d = 0; for (int n = 1; n <= 10; n++) d += CHEB_W[k][n - 1] * Un(n, x);
			CHECK_NEAR(chebU(CHEB_W[k], x), d, 1e-4);
		}
}

TEST("cheb: shaper 0 passes a sine, shaper 1 makes only the 2nd harmonic") {
	const float fs = 48000.f, f = 220.f;
	for (int k = 0; k < 2; k++) {
		Cheb c; c.setSampleRate(fs);
		Cheb::Params p; p.scan = (float)k; p.drive = 0.8f;
		std::vector<float> y;
		for (int i = 0; i < 48000; i++) { float x = 5.f * std::sin(2 * M_PI * f * i / fs), l, r; c.process(p, x, x, l, r); y.push_back(l); }
		double h1 = tone(y, f, fs, 4800), h2 = tone(y, 2 * f, fs, 4800), h3 = tone(y, 3 * f, fs, 4800);
		if (k == 0) { CHECK(h1 > 2.0); CHECK(db(h2, h1) < -50); CHECK(db(h3, h1) < -50); }
		else { CHECK(h2 > 1.0); CHECK(db(h1, h2) < -40); CHECK(db(h3, h2) < -40); }
	}
}

TEST("cheb: scan sweep is click-free") {
	const float fs = 48000.f;
	Cheb c; c.setSampleRate(fs);
	Cheb::Params p;
	float prev = 0.f, worst = 0.f;
	for (int i = 0; i < 48000; i++) {
		p.scan = 39.f * i / 48000.f;
		float x = 5.f * std::sin(2 * M_PI * 100.0 * i / fs), l, r;
		c.process(p, x, x, l, r);
		if (i > 100) worst = std::max(worst, std::fabs(l - prev));
		prev = l;
	}
	CHECK(worst < 0.5f);
}

static std::string pattern(int eq, int len, float a, float b, int steps, int ratioIdx = 5) {
	Axiom ax; ax.setSeed(5); ax.reset();
	ax.ch[0].eq = eq; ax.ch[0].len = len; ax.ch[0].a = a; ax.ch[0].b = b; ax.ratioIdx[0] = ratioIdx;
	ax.triggers = false;
	std::string s;
	// step the channel directly through the clock: 1 step = 1000 samples at bpm chosen for that
	float fs = 48000.f, dt = 1.f / fs;
	float bpm = 60.f / (4.f * 1000.f * dt);
	for (int i = 0; i < steps * 1000; i++) {
		ax.process(dt, false, false, bpm, true);
		if (i % 1000 == 250) s += ax.out[0] ? 'x' : '.';
	}
	return s;
}

TEST("axiom: euclid 5 of 16") {
	std::string p = pattern(0, 16, 5.f / 16.f, 0.f, 16);
	int n = 0; for (char c : p) n += c == 'x';
	CHECK(n == 5);
	std::string want = "x..x..x..x..x...";
	bool rot = false;
	for (int r = 0; r < 16; r++) if (want.substr(r) + want.substr(0, r) == p) rot = true;
	CHECK(rot);
	if (!rot) std::printf("  got %s\n", p.c_str());
}

TEST("axiom: every equation repeats with its length") {
	for (int eq = 0; eq < 10; eq++) {
		std::string p = pattern(eq, 12, 0.6f, 0.4f, 48);
		CHECK(p.substr(0, 12) == p.substr(12, 12));
		CHECK(p.substr(0, 12) == p.substr(36, 12));
		if (p.substr(0, 12) != p.substr(12, 12)) std::printf("  eq %d: %s\n", eq + 1, p.c_str());
	}
}

TEST("axiom: ratio 3 against 2 over 12 steps") {
	std::string p = pattern(1, 12, 2.f / 15.f, 1.f / 15.f, 12);   // p = 3, q = 2
	int n = 0; for (char c : p) n += c == 'x';
	CHECK(n == 4);   // 0,4,8 and 0,6 -> 0,4,6,8
}

TEST("axiom: channel ratio 2 doubles the step rate") {
	std::string a = pattern(0, 16, 1.f, 0.f, 8, 5);   // all pulses, ratio 1
	Axiom ax; ax.setSeed(1); ax.reset(); ax.ch[0].eq = 0; ax.ch[0].a = 1.f; ax.ratioIdx[0] = 9; ax.triggers = true;
	int rises = 0; bool prev = false;
	float dt = 1.f / 48000.f, bpm = 60.f / (4.f * 1000.f * dt);
	for (int i = 0; i < 8000; i++) { ax.process(dt, false, false, bpm, true); if (ax.out[0] && !prev) rises++; prev = ax.out[0]; }
	CHECK(rises == 16);
}

TEST("axiom: chaotic equations stay finite") {
	AxiomChannel c; c.seed = 9; c.len = 0;
	for (int eq : {2, 9}) {
		c.eq = eq; c.reset();
		for (int i = 0; i < 1000000; i++) c.advance(0.9f);
		CHECK(std::isfinite(c.v));
	}
}

TEST("axiom: quantiser") {
	CHECK_NEAR(quantise(0.04f, 0), 0.0, 1e-6);
	CHECK_NEAR(quantise(1.f / 12.f, 4), 0.0, 1e-6);       // C# -> C in pentatonic
	CHECK_NEAR(quantise(1.6f / 12.f, 4), 2.f / 12.f, 1e-6);
	CHECK_NEAR(quantise(1.0f, 1), 1.0, 1e-6);
}

TEST("tetra: index 0 is a pure sine") {
	Tetra t; t.setSampleRate(48000.f);
	Tetra::Params p; p.v[0].index = 0.f; p.v[0].ienv = 0.f; p.v[0].decay = 5.f; p.v[0].pan = 0.f; p.level = 1.f;
	bool g[4] = {true, false, false, false}, r[4] = {true, false, false, false}, n[4] = {false, false, false, false};
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) { float L, R; t.process(p, g, i == 0 ? r : n, L, R); y.push_back(t.outs[0]); }
	double f = 261.6256;
	double h1 = toneHann(y, f, 48000, 4800), h2 = toneHann(y, 2 * f, 48000, 4800), h3 = toneHann(y, 3 * f, 48000, 4800);
	CHECK(h1 > 3.0);
	CHECK(db(h2, h1) < -80); CHECK(db(h3, h1) < -80);
}

TEST("tetra: first sideband follows Bessel J1(1)") {
	Tetra t; t.setSampleRate(48000.f);
	Tetra::Params p; p.v[0].ratio = 1; p.v[0].index = 1.f; p.v[0].ienv = 0.f; p.v[0].feedback = 0.f; p.v[0].decay = 5.f; p.v[0].pitchOct = 1.f; p.level = 1.f;
	bool g[4] = {true, false, false, false}, r[4] = {true, false, false, false}, n[4] = {false, false, false, false};
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) { float L, R; t.process(p, g, i == 0 ? r : n, L, R); y.push_back(t.outs[0]); }
	double f = 2 * 261.6256;
	// ratio 1, index 1: f gets J0(1) - J2(1) = 0.765 - 0.115 (the -f sideband folds back inverted),
	// 2f gets J1(1) + J3(1) = 0.440 + 0.020. Expected ratio about 0.71.
	double a1 = toneHann(y, f, 48000, 4800), a2 = toneHann(y, 2 * f, 48000, 4800);
	CHECK_NEAR(a2 / a1, 0.460 / 0.650, 0.04);
}

TEST("tetra: mesh matrix at full feedback stays bounded") {
	Tetra t; t.setSampleRate(48000.f);
	Tetra::Params p; p.route = 7; p.matrix = 1.f;
	for (auto& v : p.v) { v.feedback = 1.5f; v.index = 8.f; v.decay = 5.f; }
	bool g[4] = {true, true, true, true}, r[4] = {true, true, true, true}, n[4] = {false, false, false, false};
	float peak = 0;
	for (int i = 0; i < 96000; i++) { float L, R; t.process(p, g, i == 0 ? r : n, L, R); CHECK(std::isfinite(L)); peak = std::max(peak, std::fabs(L)); }
	CHECK(peak <= 12.f);
}

TEST("tetra: drum decay reaches -60 dB at DECAY") {
	Tetra t; t.setSampleRate(48000.f);
	Tetra::Params p; p.drum = true; p.v[0].decay = 0.5f; p.v[0].index = 0.f;
	bool g[4] = {false, false, false, false}, r[4] = {true, false, false, false}, n[4] = {false, false, false, false};
	for (int i = 0; i < 48000; i++) {
		float L, R; t.process(p, g, i == 0 ? r : n, L, R);
		if (i == 24000 + 24) CHECK_NEAR(t.voice[0].env, 0.001, 0.0003);
	}
}
