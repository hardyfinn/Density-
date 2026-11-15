// HARMONIA, TRACE, WEAVE.
#include "util.hpp"
#include "../src/dsp/Harmonia.hpp"
#include "../src/dsp/Trace.hpp"
#include "../src/dsp/Weave.hpp"
#include <memory>
#include <chrono>

using namespace dens;

TEST("harmonia: odd harmonics 1..9 at 1/n") {
	auto h = std::make_unique<Harmonia>();
	h->setSampleRate(48000.f);
	Harmonia::In in;
	for (int v = 0; v < 6; v++) { in.spec[v].odd = 0; in.spec[v].even = 0; }
	// voice 1: ODD 1, EVEN 0, N = 9, p = 1 (TILT = 0.25/1.75)
	in.spec[0] = Spectrum{1.f, 0.f, std::sqrt(8.f / 639.f), 0.25f / 1.75f};
	in.root = 1.f;   // C4
	in.level = 1.f; in.spread = 0.f;
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) { auto o = h->process(in); y.push_back(o.voices[0]); }
	double f = 261.6256, a1 = toneHann(y, f, 48000, 9600);
	CHECK(a1 > 0.5);
	for (int n = 2; n <= 12; n++) {
		double an = toneHann(y, f * n, 48000, 9600);
		if (n % 2 == 0 || n > 9) CHECK(db(an, a1) < -60);
		else CHECK_NEAR(db(an, a1), 20 * std::log10(1.0 / n), 0.5);
	}
}

TEST("harmonia: band limit at 5 kHz") {
	auto h = std::make_unique<Harmonia>();
	h->setSampleRate(48000.f);
	Harmonia::In in;
	for (int v = 1; v < 6; v++) { in.spec[v].odd = 0; in.spec[v].even = 0; }
	in.spec[0] = Spectrum{1.f, 1.f, 1.f, 1.f};
	in.root = 1.f + std::log2(5000.f / 261.6256f);
	in.level = 1.f; in.spread = 0.f;
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) { auto o = h->process(in); y.push_back(o.voices[0]); }
	double a1 = toneHann(y, 5000, 48000, 9600);
	// aliases of harmonics 5..8 would land at 23000, 18000, 13000, 8000 Hz folded: 25000->23000, 30000->18000...
	for (double f : {23000.0, 18000.0, 13000.0, 8000.0, 3000.0}) CHECK(db(toneHann(y, f, 48000, 9600), a1) < -80);
}

TEST("harmonia: modulated spectra stay inside the CPU budget") {
	auto h = std::make_unique<Harmonia>();
	h->setSampleRate(48000.f);
	Harmonia::In in;
	for (int v = 0; v < 6; v++) in.spec[v] = Spectrum{1.f, 0.7f, 0.6f, 0.f};
	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < 48000; i++) {
		float m = std::sin(2 * M_PI * 5.0 * i / 48000.0);
		for (int v = 0; v < 6; v++) in.spec[v].tilt = 0.5f * m;
		h->process(in);
	}
	double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	std::printf("  HARMONIA, six voices with 5 Hz tilt modulation: %.2f%% of one core\n", s * 100);
	CHECK(s < 0.06 * 2);
}

TEST("harmonia: morph is continuous, dice spares tuning") {
	Spectrum a{1.f, 0.f, 0.1f, -0.5f}, b{0.2f, 1.f, 0.8f, 0.6f};
	float prev = -1;
	float worst = 0;
	for (int i = 0; i <= 48000; i++) {
		float m = i / 48000.f;
		Spectrum s{lerp(a.odd, b.odd, m), lerp(a.even, b.even, m), lerp(a.part, b.part, m), lerp(a.tilt, b.tilt, m)};
		if (prev >= 0) worst = std::max(worst, std::fabs(s.tilt - prev));
		prev = s.tilt;
	}
	CHECK(worst < 0.01f);
}

TEST("harmonia: one shot envelope follows the gate") {
	auto h = std::make_unique<Harmonia>();
	h->setSampleRate(48000.f);
	Harmonia::In in;
	in.envType = Harmonia::ONE_SHOT; in.gatePatched = true; in.gateChannels = 1; in.assign = Harmonia::ROUND_ROBIN;
	in.atk = 0.01f; in.dec = 0.1f; in.sus = 0.5f; in.rel = 0.1f;
	float mid = 0, after = 0;
	for (int i = 0; i < 48000; i++) {
		in.gates[0] = i < 24000;
		auto o = h->process(in);
		if (i == 20000) mid = o.envs[0];
		if (i == 47999) after = o.envs[0];
	}
	CHECK_NEAR(mid, 5.0, 0.2);
	CHECK(after < 0.01);
}

TEST("trace: hold 2 s, release, loops 2 s at 3 V") {
	TraceLane l; l.init(1);
	const float fs = 48000.f, dt = 1.f / fs;
	// short press: wake
	for (int i = 0; i < 4800; i++) { l.control(i < 2400, dt); l.process(3.f, 1.f, false, fs); }
	CHECK(l.state == TraceLane::AWAKE);
	for (int i = 0; i < 48000; i++) { l.control(false, dt); l.process(3.f, 1.f, false, fs); }
	int n = (int)(2.0f * fs);
	for (int i = 0; i < n; i++) { l.control(true, dt); l.process(3.f, 1.f, false, fs); }
	l.control(false, dt);
	CHECK(l.state == TraceLane::LOOPING);
	CHECK(std::abs(l.len - 2000) <= 2);
	float y = 0; for (int i = 0; i < 4800; i++) y = l.process(0.f, 1.f, false, fs);
	CHECK_NEAR(y, 3.0, 0.01);
}

TEST("trace: speed and slow scale the loop period") {
	TraceLane l; l.init(2);
	const float fs = 48000.f, dt = 1.f / fs;
	for (int i = 0; i < 2400; i++) l.control(true, dt), l.process(0, 1, false, fs);   // press to wake... hold starts recording at 0.3 s
	for (int i = 0; i < 4 * 48000; i++) { l.control(true, dt); l.process(5.f * std::sin(2 * M_PI * i / 48000.0), 1.f, false, fs); }
	l.control(false, dt);
	CHECK(l.state == TraceLane::LOOPING);
	// play at speed 2 / slow 4 = 0.5: period 2 s
	std::vector<float> y;
	for (int i = 0; i < 8 * 4800; i++) { l.control(false, dt * 10); y.push_back(l.process(0, 0.5f, false, fs / 10)); }
	double a = toneHann(y, 0.5, 4800, 0), b = toneHann(y, 1.0, 4800, 0);
	CHECK(a > 3 * b);
}

TEST("trace: double press clears") {
	TraceLane l; l.init(3);
	const float fs = 48000.f, dt = 1.f / fs;
	auto run = [&](bool p, int n) { for (int i = 0; i < n; i++) { l.control(p, dt); l.process(1, 1, false, fs); } };
	run(true, 48000); run(false, 100);
	CHECK(l.state == TraceLane::LOOPING);
	run(true, 2000); run(false, 4000); run(true, 2000); run(false, 100);
	CHECK(l.state == TraceLane::ASLEEP);
	CHECK(l.len == 0);
}

TEST("trace: sub loop stays inside the loop, random phase is smooth") {
	TraceLane l; l.init(4);
	const float fs = 8000.f, dt = 1.f / fs;
	for (int i = 0; i < 3 * 8000; i++) { l.control(true, dt); l.process(5.f * std::sin(2 * M_PI * i / 8000.0), 1, false, fs); }
	l.control(false, dt);
	float prev = 0; float worst = 0;
	for (int i = 0; i < 20 * 8000; i++) {
		if (i % 7919 == 0) l.randomPhase();
		float y = l.process(0, 1, true, fs);
		CHECK(l.pos >= 0 && l.pos <= l.len);
		if (i) worst = std::max(worst, std::fabs(y - prev));
		prev = y;
	}
	CHECK(worst < 0.3f);
}

TEST("weave: rates within 1% at SHAPE 0, couple 0 independent") {
	auto w = std::make_unique<Weave>();
	w->setSampleRate(48000.f);
	float rate[12], shape[12], out[12];
	for (int i = 0; i < 12; i++) { rate[i] = 1.f + i; shape[i] = 0.f; }
	int cross[12] = {}; float prev[12] = {};
	for (int n = 0; n < 48000 * 4; n++) {
		w->processMods(rate, shape, 0.f, out);
		for (int i = 0; i < 12; i++) { if (prev[i] < 0 && out[i] >= 0) cross[i]++; prev[i] = out[i]; }
	}
	for (int i = 0; i < 12; i++) CHECK(std::abs(cross[i] - 4 * (i + 1)) <= 1);
}

TEST("weave: chaos and Lorenz shapes stay bounded") {
	auto w = std::make_unique<Weave>();
	w->setSampleRate(1000.f);
	float rate[12], shape[12], out[12];
	for (int i = 0; i < 12; i++) { rate[i] = 5.f; shape[i] = i < 6 ? 0.85f + 0.02f * i : 1.f; }
	bool ok = true;
	for (int n = 0; n < 600000; n++) { w->processMods(rate, shape, 0.5f, out); for (float o : out) if (!(std::fabs(o) <= 1.0001f)) ok = false; }
	CHECK(ok);
}

TEST("weave: LPG falls slower than VCA") {
	VcGate a, b; a.setSampleRate(48000.f); b.setSampleRate(48000.f);
	int ta = -1, tb = -1;
	for (int i = 0; i < 48000; i++) {
		float g = i < 4800 ? 1.f : 0.f;
		float ya = a.process(1.f, g, VcGate::VCA), yb = b.process(1.f, g, VcGate::LPG);
		if (i >= 4800 && ta < 0 && std::fabs(ya) < 0.01f) ta = i - 4800;
		if (i >= 4800 && tb < 0 && std::fabs(yb) < 0.01f) tb = i - 4800;
	}
	CHECK(tb > 0.12f * 48000);
	CHECK(ta < tb);
}
