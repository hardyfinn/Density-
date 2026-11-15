// Buffer-based blocks: the 12-second stretch buffer, grains, a scanning read head, reverse delay,
// a hold looper and a slice repeater.
#pragma once
#include "../Common.hpp"
#include "../DelayLine.hpp"
#include "../Noise.hpp"
#include <vector>

namespace dens {

// "Record past halfway, loop after 12 s, clear at zero" (Programma 900 algorithms 6 to 8).
struct StretchBuffer {
	enum State { LIVE, RECORDING, LOOPING };
	State state = LIVE;
	std::vector<float> bl, br;
	int len = 0, w = 0;
	double r = 0.0;
	float fs = 48000.f, lastStretch = 0.f;
	float fade = 1.f;   // 0..1 crossfade from live to buffer
	void init(float sampleRate, float seconds = 12.f) {
		fs = sampleRate; len = (int)(seconds * fs);
		bl.assign(len, 0.f); br.assign(len, 0.f);
		clear();
	}
	void clear() { state = LIVE; w = 0; r = 0.0; fade = 0.f; }
	static float speedFor(float stretch) { return stretch < 0.5f ? 1.f : 1.f - (stretch - 0.5f) * 1.5f; }
	float progress() const { return state == RECORDING ? (float)w / len : (state == LOOPING ? 1.f : 0.f); }
	float position() const { return len ? (float)(r / len) : 0.f; }
	void process(float stretch, float inL, float inR, float& outL, float& outR) {
		if (stretch < 0.02f && state != LIVE) clear();
		if (state == LIVE && stretch >= 0.5f && lastStretch < 0.5f) { state = RECORDING; w = 0; r = 0.0; }
		lastStretch = stretch;
		if (state == LIVE) { fade = std::max(0.f, fade - 0.001f); outL = inL; outR = inR; return; }
		if (state == RECORDING) {
			bl[w] = inL; br[w] = inR;
			w++;
			if (w >= len) { state = LOOPING; w = len; }
		}
		double sp = speedFor(stretch);
		r += sp;
		int limit = state == RECORDING ? w : len;
		if (state == LOOPING) { if (r >= len) r -= len; }
		else if (r > limit - 1) r = std::max(0, limit - 1);
		int i = (int)r; float f = (float)(r - i);
		int j = i + 1; if (j >= limit) j = state == LOOPING ? 0 : i;
		float sl = bl[i] + (bl[j] - bl[i]) * f, sr = br[i] + (br[j] - br[i]) * f;
		fade = std::min(1.f, fade + 0.001f);
		outL = inL + (sl - inL) * fade;
		outR = inR + (sr - inR) * fade;
	}
};

// Up to 32 grains reading a 4 s mono ring buffer (one engine per channel, or mono into stereo pans).
struct Grains {
	static constexpr int MAXG = 32;
	struct Grain { bool on = false; double pos = 0; float inc = 1, len = 1, t = 0, attack = 0.5f, panL = 1, panR = 1; };
	Grain g[MAXG];
	DelayLine buf;
	float fs = 48000.f, timer = 0.f;
	Rng rng{11};
	void init(float sampleRate) { fs = sampleRate; buf.init((int)(4.2f * fs)); }
	void clear() { buf.clear(); for (auto& x : g) x.on = false; }
	// mode: 0 forward, 1 reverse, 2 mixed. position 0..1 of buffer behind write, scatter 0..1
	void process(float in, float density, float position, float scatter, float semis, float sizeMs, int mode, float& l, float& r) {
		buf.push(in);
		float dt = 1.f / fs;
		timer -= dt * density * (1.f + scatter * (rng.uniform() - 0.5f) * 0.8f);
		if (timer <= 0.f) {
			timer += 1.f;
			for (auto& x : g) if (!x.on) {
				x.on = true;
				x.len = sizeMs * 0.001f * fs;
				float maxBack = 4.f * fs - x.len * 4.f - 16.f;
				float back = clampf(position + scatter * 0.25f * rng.bipolar(), 0.f, 1.f) * std::max(0.f, maxBack) + x.len * 2.f + 8.f;
				bool rev = mode == 1 || (mode == 2 && rng.uniform() < 0.5f);
				x.inc = std::pow(2.f, semis / 12.f) * (rev ? -1.f : 1.f);
				// pos counts samples-behind-write at grain start; it moves with the write head
				x.pos = back;
				x.t = 0.f;
				x.attack = mode == 2 ? 0.1f + 0.8f * rng.uniform() : 0.5f;
				float pan = clampf(scatter * rng.bipolar(), -1.f, 1.f);
				x.panL = std::cos((pan + 1.f) * PI * 0.25f) * 1.414f;
				x.panR = std::sin((pan + 1.f) * PI * 0.25f) * 1.414f;
				break;
			}
		}
		l = r = 0.f;
		for (auto& x : g) {
			if (!x.on) continue;
			float ph = x.t / x.len;
			if (ph >= 1.f) { x.on = false; continue; }
			float w = ph < x.attack ? std::sin(0.5f * PI * ph / x.attack) : std::cos(0.5f * PI * (ph - x.attack) / (1.f - x.attack));
			w *= w;
			float d = clampf((float)x.pos, 1.f, (float)buf.size() - 8.f);
			float s = buf.readLinear(d) * w;
			l += s * x.panL; r += s * x.panR;
			x.pos += 1.0 - x.inc;   // write advances 1, read advances inc
			x.t += 1.f;
		}
		float norm = 1.f / std::sqrt(std::max(1.f, density * sizeMs * 0.001f));
		l *= norm; r *= norm;
	}
};

// A read head scanning a 12 s buffer: 0 forward, 1 pendulum, 2 random jumps. Two heads crossfade on jumps.
struct Scanner {
	DelayLine buf;
	float fs = 48000.f;
	double off[2] = {4800, 4800};
	float xf = 1.f;    // weight of head 0
	int active = 0;
	float pend = 0.f, jumpT = 0.f;
	Rng rng{23};
	void init(float sampleRate) { fs = sampleRate; buf.init((int)(12.2f * fs)); off[0] = off[1] = 0.1 * fs; }
	void clear() { buf.clear(); }
	float process(float in, float rate, int mode) {
		buf.push(in);
		float len = 12.f * fs;
		float minOff = 0.02f * fs;
		auto jump = [&](double to) { active ^= 1; off[active] = to; };
		if (mode == 0) {
			off[active] += 1.0 - rate;   // forward scan: offset changes by (1 - rate) per sample
			if (off[active] < minOff || off[active] > len - 16) jump(off[active] < minOff ? len * 0.9 : minOff * 2);
		} else if (mode == 1) {
			pend += rate * 0.05f / fs * 2.f;
			if (pend >= 2.f) pend -= 2.f;
			float tri = pend < 1.f ? pend : 2.f - pend;
			off[active] = minOff + tri * (len - minOff - 32);
		} else {
			off[active] += 0.0;
			jumpT -= rate * 2.f / fs;
			if (jumpT <= 0.f) { jumpT = 1.f; jump(minOff + rng.uniform() * (len - minOff - 32)); }
		}
		int other = active ^ 1;
		off[other] += 0.0;
		float target = active == 0 ? 1.f : 0.f;
		float step = 1.f / (0.005f * fs);
		xf += clampf(target - xf, -step, step);
		float a = buf.readLinear(clampf((float)off[0], 1.f, len));
		float b = buf.readLinear(clampf((float)off[1], 1.f, len));
		return a * xf + b * (1.f - xf);
	}
};

// Reverse delay: chunks of length W played backwards, two overlapped taps with sine-squared windows.
struct ReverseDelay {
	DelayLine dl;
	float fs = 48000.f, ph = 0.f, prev = 0.f;
	void init(float sampleRate, float maxSec = 2.f) { fs = sampleRate; dl.init((int)(2.f * maxSec * fs) + 16); }
	void clear() { dl.clear(); prev = 0.f; }
	float process(float x, float windowMs, float feedback) {
		dl.push(softClamp(x + feedback * prev, 12.f));
		float W = clampf(windowMs * 0.001f * fs, 64.f, (float)dl.size() / 2.f - 8.f);
		ph += 1.f / W; ph -= std::floor(ph);
		float q = ph + 0.5f; q -= std::floor(q);
		float wa = std::sin(PI * ph), wb = std::sin(PI * q);
		float y = dl.readLinear(1.f + 2.f * ph * W) * wa * wa + dl.readLinear(1.f + 2.f * q * W) * wb * wb;
		prev = y;
		return y;
	}
};

// Hold to capture the last L ms and loop it at a rate (negative = reverse).
struct HoldLooper {
	DelayLine dl;
	float fs = 48000.f;
	bool holding = false;
	double pos = 0;
	float loopLen = 4800, base = 0;
	void init(float sampleRate) { fs = sampleRate; dl.init((int)(1.f * fs) + 16); }
	void clear() { dl.clear(); holding = false; }
	float process(float x, bool hold, float lenMs, float rate) {
		if (!holding) dl.push(x);
		if (hold && !holding) { holding = true; loopLen = clampf(lenMs * 0.001f * fs, 64.f, (float)dl.size() - 32.f); pos = 0; }
		if (!hold) holding = false;
		if (!holding) return x;
		pos += rate;
		while (pos < 0) pos += loopLen;
		while (pos >= loopLen) pos -= loopLen;
		// read from the frozen buffer: offset = loopLen - pos samples behind the frozen write head
		float d = (float)(loopLen - pos);
		float y = dl.readLinear(clampf(d, 1.f, loopLen));
		float edge = std::min((float)pos, loopLen - (float)pos) / (0.004f * fs);
		return y * clampf(edge, 0.f, 1.f);
	}
};

// Repeats the previous slice instead of passing the live input, with probability p at each slice boundary.
struct SliceRepeater {
	DelayLine dl;
	float fs = 48000.f, t = 0.f, sliceLen = 2400.f;
	bool repeating = false;
	Rng rng{31};
	void init(float sampleRate) { fs = sampleRate; dl.init((int)(0.3f * fs)); }
	void clear() { dl.clear(); repeating = false; }
	float process(float x, float sliceMs, float prob) {
		dl.push(x);
		t += 1.f;
		if (t >= sliceLen) {
			t = 0.f;
			sliceLen = clampf(sliceMs * 0.001f * fs, 32.f, (float)dl.size() / 2.f - 8.f);
			repeating = rng.uniform() < prob;
		}
		if (!repeating) return x;
		float w = std::min(1.f, std::min(t, sliceLen - t) / (0.002f * fs));
		return x * (1.f - w) + dl.readLinear(sliceLen) * w;
	}
};

} // namespace dens
