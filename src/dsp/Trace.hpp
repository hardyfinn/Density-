// TRACE: eight gestural CV recorders. Hold to record, release to loop (after Programma 900's CV buffers).
#pragma once
#include "Common.hpp"
#include "Noise.hpp"
#include <vector>

namespace dens {

struct TraceLane {
	enum State { ASLEEP, AWAKE, RECORDING, LOOPING };
	static constexpr int RATE = 1000;              // storage rate (Hz)
	static constexpr int MAX = 300 * RATE;         // 5 minutes
	static constexpr int PRE = 300;                // 300 ms pre-roll at 1 kHz
	State state = ASLEEP;
	std::vector<float> buf;
	int len = 0;                                   // recorded length (samples at 1 kHz)
	float pre[PRE] = {};
	int preW = 0;
	// button timing
	bool down = false;
	float downTime = 0.f, sinceRelease = 10.f;
	bool holdStarted = false;
	// decimation
	float lp = 0.f, acc = 0.f;
	// playback
	double pos = 0.0;
	float subStart = 0.f, subLen = 0.f;
	float xfFrom = 0.f, xf = 1.f;   // jump crossfade
	float lastOut = 0.f;
	int wraps = 0;
	Rng rng{1};

	void init(uint64_t seed) { buf.assign(MAX, 0.f); rng.seedWith(seed); }
	void clear() { state = ASLEEP; len = 0; pos = 0.0; }

	void startRecording() {
		// begin with the pre-roll so the take starts at the button-down moment
		int n = std::min(PRE, (int)std::lround(downTime * RATE));
		for (int i = 0; i < n; i++) buf[i] = pre[(preW - n + i + PRE * 2) % PRE];
		len = n;
		state = RECORDING;
	}
	void beginLoop() { if (len < 2) { state = AWAKE; return; } state = LOOPING; pos = 0.0; pickSub(); }
	void pickSub() { subLen = len * (0.125f + 0.375f * rng.uniform()); subStart = rng.uniform() * (len - subLen); }
	float progress() const { return state == RECORDING ? (float)len / MAX : (len > 0 ? (float)(pos / len) : 0.f); }

	// pressed: button or REC GATE high. Returns nothing; read value() after.
	void control(bool pressed, float dt) {
		if (pressed && !down) {
			down = true; downTime = 0.f; holdStarted = false;
			if (sinceRelease < 0.35f) { clear(); sinceRelease = 10.f; down = true; holdStarted = true; return; }   // double press
		}
		if (down) {
			downTime += dt;
			if (!pressed) {
				down = false; sinceRelease = 0.f;
				if (holdStarted) {
					if (state == RECORDING) beginLoop();
				} else {
					// short press
					if (state == ASLEEP) state = AWAKE;
					else if (state == AWAKE) state = ASLEEP;
					else if (state == LOOPING) state = AWAKE;
				}
			} else if (!holdStarted && downTime >= 0.3f) {
				holdStarted = true;
				if (state != RECORDING) startRecording();
			}
		} else sinceRelease += dt;
	}

	// src: live source volts; speed: playback rate; sub: sub-loop on; returns raw (before ATTEN/OFFSET)
	float process(float src, float speed, bool sub, float fs) {
		float dt = 1.f / fs;
		lp += (src - lp) * (1.f - std::exp(-TWO_PI * 300.f * dt));
		acc += RATE * dt;
		bool tick = false;
		while (acc >= 1.f) { acc -= 1.f; tick = true; }
		if (tick) {
			pre[preW] = lp; preW = (preW + 1) % PRE;
			if (state == RECORDING) {
				buf[len++] = lp;
				if (len >= MAX) beginLoop();
			}
		}
		float out;
		if (state == LOOPING) {
			pos += speed * RATE * dt;
			double lo = sub ? subStart : 0.0, hi = sub ? subStart + subLen : len;
			if (hi - lo < 2.0) { lo = 0.0; hi = len; }
			if (pos < lo || pos >= hi) {
				pos = lo + std::fmod(std::max(0.0, pos - lo), hi - lo);
				wraps++;
				xfFrom = lastOut; xf = 0.f;
				if (sub) { pickSub(); pos = subStart; }
			}
			int i = (int)pos; float f = (float)(pos - i);
			int j = i + 1; if (j >= len) j = 0;
			out = buf[std::min(i, len - 1)] + (buf[j] - buf[std::min(i, len - 1)]) * f;
		} else if (state == ASLEEP) out = 0.f;
		else out = src;
		// jump crossfade
		if (xf < 1.f) { xf = std::min(1.f, xf + dt / 0.005f); out = xfFrom + (out - xfFrom) * xf; }
		lastOut = out;
		return out;
	}
	void randomPhase() {
		if (state != LOOPING) return;
		xfFrom = lastOut; xf = 0.f;
		pos = rng.uniform() * len;
	}
};

struct Trace {
	TraceLane lane[8];
	void init(uint64_t seed) { for (int i = 0; i < 8; i++) lane[i].init(seed * 8 + i + 1); }
};

} // namespace dens
