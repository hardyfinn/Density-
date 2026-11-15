// Power-of-two ring buffer with Hermite fractional read. Allocate outside process().
#pragma once
#include "Common.hpp"
#include <vector>

namespace dens {

struct DelayLine {
	int size() const { return mask + 1; }
	float readLinear(float d) const {
		d = clampf(d, 0.f, (float)(mask - 2));
		int i = (int)d; float f = d - i;
		return at(i) + (at(i + 1) - at(i)) * f;
	}
	std::vector<float> buf;
	int mask = 0, w = 0;
	void init(int minLen) {
		int n = 1;
		while (n < minLen + 4) n <<= 1;
		buf.assign(n, 0.f);
		mask = n - 1;
		w = 0;
	}
	void push(float x) { buf[w] = x; w = (w + 1) & mask; }
	float at(int d) const { return buf[(w - 1 - d) & mask]; }
	// delay in samples (>= 1 means at least one sample old)
	float read(float d) const {
		d = clampf(d, 1.f, (float)(mask - 3));
		int i = (int)d;
		float f = d - i;
		float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
		float c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * f + c2) * f + c1) * f + x0;
	}
	void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
};

// Very long buffers stored as 16-bit (±12 V full scale), linear fractional read. For minutes of audio.
struct DelayLine16 {
	std::vector<int16_t> buf;
	int len = 0, w = 0;
	static constexpr float SCALE = 32767.f / 12.f, INV = 12.f / 32767.f;
	void init(int n) { len = std::max(n, 8); buf.assign(len, 0); w = 0; }
	bool ready() const { return len > 0 && (int)buf.size() == len; }
	void push(float x) {
		float v = clampf(x, -12.f, 12.f) * SCALE;
		buf[w] = (int16_t)std::lrint(v);
		if (++w >= len) w = 0;
	}
	float at(int d) const { int i = w - 1 - d; while (i < 0) i += len; return buf[i] * INV; }
	float read(float d) const {
		d = clampf(d, 1.f, (float)(len - 3));
		int i = (int)d; float f = d - i;
		return at(i) + (at(i + 1) - at(i)) * f;
	}
	void clear() { std::fill(buf.begin(), buf.end(), (int16_t)0); }
	void release() { std::vector<int16_t>().swap(buf); len = 0; w = 0; }
};

} // namespace dens
