// NONET, TANGLE and the shared effect blocks.
#include "util.hpp"
#include "../src/dsp/Nonet.hpp"
#include "../src/dsp/TangleCore.hpp"
#include <memory>
#include <chrono>

using namespace dens;

static float pinkNoise(Rng& r, Pink& p) { return 6.5f * p.process(r.bipolar()); }   // about 2.2 V rms

TEST("fx: tilt filter bypasses at centre, cuts at the ends") {
	TiltFilter t; t.set(0.f, 48000.f);
	CHECK_NEAR(t.process(1.f), 1.f, 1e-6);
	for (float tilt : {-1.f, 1.f}) {
		TiltFilter f; f.set(tilt, 48000.f);
		std::vector<float> y;
		double fq = tilt < 0 ? 5000.0 : 50.0;
		for (int i = 0; i < 48000; i++) y.push_back(f.process(std::sin(2 * M_PI * fq * i / 48000.0)));
		CHECK(tone(y, fq, 48000, 4800) < 0.1);
	}
}

TEST("fx: pitch shifter moves a sine by an octave") {
	PitchShift ps; ps.init(48000.f);
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) y.push_back(ps.process(std::sin(2 * M_PI * 300.0 * i / 48000.0), 12.f));
	CHECK(toneHann(y, 600, 48000, 4800) > 3 * toneHann(y, 300, 48000, 4800));
}

TEST("fx: frequency shifter moves a sine by 100 Hz") {
	FreqShift f;
	std::vector<float> y;
	for (int i = 0; i < 48000; i++) y.push_back(f.process(std::sin(2 * M_PI * 1000.0 * i / 48000.0), 100.f, 48000.f));
	CHECK(toneHann(y, 1100, 48000, 4800) > 0.8);
	CHECK(toneHann(y, 900, 48000, 4800) < 0.05);
}

static void runNonet(Nonet& n, int alg, int mode, const float* pots, float seconds, float noiseSeconds, std::vector<float>& outL, bool& finiteOk, float& peak) {
	n.select(alg); n.mode = mode;
	Nonet::Frame f;
	for (int i = 0; i < 6; i++) f.p[i] = pots ? pots[i] : NONET_ALGS[alg].p[i].def;
	f.filter = 0.f; f.mix = 1.f;
	Rng r(3); Pink p;
	finiteOk = true; peak = 0.f;
	int N = (int)(seconds * n.fs), noise = (int)(noiseSeconds * n.fs);
	outL.clear();
	for (int i = 0; i < N; i++) {
		float x = i < noise ? pinkNoise(r, p) : 0.f;
		float l, rr;
		n.process(f, x, x * 0.9f, l, rr);
		if (!std::isfinite(l) || !std::isfinite(rr)) finiteOk = false;
		peak = std::max(peak, std::max(std::fabs(l), std::fabs(rr)));
		outL.push_back(l);
	}
}

TEST("nonet: every algorithm and mode is stable and decays") {
	auto n = std::make_unique<Nonet>();
	n->init(32000.f);
	n->allocateLong(false);
	for (int alg = 0; alg < 9; alg++) {
		for (int mode = 0; mode < 3; mode++) {
			std::vector<float> y; bool ok; float peak;
			runNonet(*n, alg, mode, nullptr, 22.f, 2.f, y, ok, peak);
			CHECK(ok); CHECK(peak <= 12.f);
			bool exempt = (alg == 2 && mode == 2);   // FREEZE holds forever
			double tail = rms(y, y.size() - 32000);
			if (!exempt && tail > 5e-4) std::printf("  alg %d mode %d tail rms %g\n", alg + 1, mode + 1, tail);
			if (!exempt) CHECK(tail < 5e-4);   // below −80 dB re 5 V
		}
	}
}

TEST("nonet: long delay returns an impulse at 2.000 s") {
	auto n = std::make_unique<Nonet>();
	n->init(48000.f); n->allocateLong(false);
	n->select(0); n->mode = 0;
	Nonet::Frame f;
	float t = expNorm(2.f, 0.01f, 360.f);
	f.p[0] = f.p[1] = t; f.p[2] = 0.f; f.p[3] = 0.f; f.p[4] = 0.f; f.p[5] = 0.f; f.filter = 0.f; f.mix = 1.f;
	int hit = -1; float best = 0.f;
	for (int i = 0; i < 3 * 48000; i++) {
		float x = i == 4800 ? 5.f : 0.f, l, r;
		n->process(f, x, x, l, r);
		if (i > 4800 + 100 && std::fabs(l) > best) { best = std::fabs(l); hit = i; }
	}
	CHECK(hit >= 4800 + 96000 - 2 && hit <= 4800 + 96000 + 2);
}

TEST("nonet: stretch buffer records at half, loops at 12 s, clears at zero") {
	StretchBuffer s; s.init(1000.f, 12.f);
	float l, r;
	s.process(0.4f, 1, 1, l, r); CHECK(s.state == StretchBuffer::LIVE);
	s.process(0.6f, 1, 1, l, r); CHECK(s.state == StretchBuffer::RECORDING);
	for (int i = 0; i < 12000; i++) s.process(0.6f, 1, 1, l, r);
	CHECK(s.state == StretchBuffer::LOOPING);
	s.process(0.01f, 1, 1, l, r); CHECK(s.state == StretchBuffer::LIVE);
}

TEST("nonet: algorithm change does not click") {
	auto n = std::make_unique<Nonet>();
	n->init(48000.f); n->allocateLong(false);
	Nonet::Frame f; for (int i = 0; i < 6; i++) f.p[i] = 0.5f; f.filter = 0.f; f.mix = 0.5f;
	float prev = 0.f, worst = 0.f;
	for (int i = 0; i < 48000; i++) {
		if (i == 24000) n->select(6);
		float x = 2.f * std::sin(2 * M_PI * 220.0 * i / 48000.0), l, r;
		n->process(f, x, x, l, r);
		if (i > 23000 && i < 26000) worst = std::max(worst, std::fabs(l - prev));
		prev = l;
	}
	CHECK(worst < 0.5f);
}

TEST("tangle: all 64 programs are stable and finite, and decay") {
	auto c = std::make_unique<TangleCore>();
	const int FS = 32000;
	c->init((float)FS);
	Rng r(5); Pink p;
	for (int prog = 0; prog < 64; prog++) {
		c->select(prog);
		float q[6]; for (int i = 0; i < 6; i++) q[i] = 0.5f;
		bool ok = true; float peak = 0.f; double tail = 0;
		std::vector<float> y;
		for (int i = 0; i < FS * 22; i++) {
			float x = i < FS * 2 ? pinkNoise(r, p) : 0.f, l, rr;
			bool btn = (i / (FS / 2)) % 2 == 1 && i < FS * 2;
			c->process(q, btn, x, l, rr);
			if (!std::isfinite(l) || !std::isfinite(rr)) ok = false;
			peak = std::max(peak, std::max(std::fabs(l), std::fabs(rr)));
			y.push_back(l);
		}
		tail = rms(y, y.size() - FS);
		CHECK(ok); CHECK(peak <= 12.f);
		bool exempt = prog == 47;   // DANGER FDN may run away by design
		if (!exempt && tail > 5e-4) std::printf("  program %c%d %s tail %g\n", "ABCD"[prog / 16], prog % 16, TANGLE_PROGRAMS[prog].name, tail);
		if (!exempt) CHECK(tail < 5e-4);
	}
}

TEST("tangle: random pots stay bounded") {
	auto c = std::make_unique<TangleCore>();
	c->init(44100.f);
	Rng r(9); Pink p;
	for (int prog = 0; prog < 64; prog++) {
		c->select(prog);
		float q[6]; for (int i = 0; i < 6; i++) q[i] = r.uniform();
		bool ok = true;
		for (int i = 0; i < 44100; i++) {
			float x = 10.f * r.bipolar(), l, rr;
			c->process(q, (i / 11025) & 1, x, l, rr);
			if (!std::isfinite(l) || std::fabs(l) > 12.f) ok = false;
		}
		CHECK(ok);
		if (!ok) std::printf("  program %d\n", prog);
	}
}

TEST("tangle and nonet: CPU per sample at 48 kHz") {
	auto c = std::make_unique<TangleCore>();
	c->init(48000.f);
	Rng r(1);
	double worstT = 0; int worstP = 0;
	for (int prog = 0; prog < 64; prog++) {
		c->select(prog);
		float q[6] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
		for (int i = 0; i < 2000; i++) { float l, rr; c->process(q, false, r.bipolar(), l, rr); }
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < 48000; i++) { float l, rr; c->process(q, false, r.bipolar(), l, rr); }
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		if (s > worstT) { worstT = s; worstP = prog; }
	}
	std::printf("  TANGLE heaviest program %s: %.2f%% of one core\n", TANGLE_PROGRAMS[worstP].name, worstT * 100);
	auto n = std::make_unique<Nonet>();
	n->init(48000.f); n->allocateLong(false);
	double worstN = 0; int worstA = 0;
	for (int alg = 0; alg < 9; alg++) {
		n->select(alg);
		Nonet::Frame f; for (int i = 0; i < 6; i++) f.p[i] = 0.5f; f.filter = 0.f; f.mix = 0.5f;
		for (int i = 0; i < 2000; i++) { float l, rr; n->process(f, r.bipolar(), r.bipolar(), l, rr); }
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < 48000; i++) { float l, rr; n->process(f, r.bipolar(), r.bipolar(), l, rr); }
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		if (s > worstN) { worstN = s; worstA = alg; }
	}
	std::printf("  NONET heaviest algorithm %s: %.2f%% of one core\n", NONET_ALGS[worstA].name, worstN * 100);
	CHECK(worstT < 0.03 * 2);   // budget 3% (2x slack for slow CI machines)
	CHECK(worstN < 0.04 * 2);
}
