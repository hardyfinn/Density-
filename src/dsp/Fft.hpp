// Small radix-2 complex FFT and a real inverse helper. Plain C++, testable.
#pragma once
#include "Common.hpp"
#include <vector>
#include <complex>

namespace dens {

struct Fft {
	int n = 0, logn = 0;
	std::vector<std::complex<float>> tw;
	std::vector<int> rev;
	void init(int size) {
		n = size; logn = 0;
		while ((1 << logn) < n) logn++;
		tw.resize(n / 2);
		for (int i = 0; i < n / 2; i++) tw[i] = std::polar(1.f, -TWO_PI * i / n);
		rev.resize(n);
		for (int i = 0; i < n; i++) {
			int r = 0;
			for (int b = 0; b < logn; b++) if (i & (1 << b)) r |= 1 << (logn - 1 - b);
			rev[i] = r;
		}
	}
	// In place. inverse=true uses conjugate twiddles (no 1/n scaling).
	void transform(std::complex<float>* a, bool inverse) const {
		for (int i = 0; i < n; i++) if (i < rev[i]) std::swap(a[i], a[rev[i]]);
		for (int len = 2; len <= n; len <<= 1) {
			int step = n / len;
			for (int i = 0; i < n; i += len) {
				for (int j = 0; j < len / 2; j++) {
					std::complex<float> w = tw[j * step];
					if (inverse) w = std::conj(w);
					std::complex<float> u = a[i + j], v = a[i + j + len / 2] * w;
					a[i + j] = u + v;
					a[i + j + len / 2] = u - v;
				}
			}
		}
	}
};

// Build one period of sum_k amp[k]·sin(2πk·t + ph[k]) (k = 1..n/2-1) into out[0..n-1].
struct AdditiveTable {
	Fft fft;
	std::vector<std::complex<float>> work;
	void init(int n) { fft.init(n); work.assign(n, {0.f, 0.f}); }
	// amp, ph indexed by harmonic (index 0 ignored), count = highest harmonic + 1
	void build(const float* amp, const float* ph, int count, float* out) {
		int n = fft.n;
		std::fill(work.begin(), work.end(), std::complex<float>(0.f, 0.f));
		int top = std::min(count, n / 2);
		for (int k = 1; k < top; k++) {
			if (amp[k] == 0.f) continue;
			// x[t] = sum a·sin(wt+ph) = sum Re( a·e^{i(wt+ph-π/2)} ); place a/2·e^{i(ph-π/2)} at +k and conj at -k
			std::complex<float> c = std::polar(0.5f * amp[k], ph[k] - 0.5f * PI);
			work[k] = c;
			work[n - k] = std::conj(c);
		}
		fft.transform(work.data(), true);
		for (int i = 0; i < n; i++) out[i] = work[i].real();
	}
};

} // namespace dens
