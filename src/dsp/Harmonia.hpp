// HARMONIA: six additive harmonic voices (after Programma 900's harmonic voices).
#pragma once
#include "Common.hpp"
#include "Fft.hpp"
#include "Envelope.hpp"
#include "Oversample.hpp"
#include "Noise.hpp"

namespace dens {

constexpr int HRM_TABLE = 2048;
constexpr int HRM_MAXH = 1023;

struct Spectrum { float odd = 1.f, even = 0.5f, part = 0.25f, tilt = -0.2f; };

inline float partialCount(float part) { return 1.f + 639.f * part * part; }

// Amplitude of harmonic n (1-based) before band-limiting.
inline float harmonicAmp(const Spectrum& s, int n) {
	float Nf = partialCount(s.part);
	int full = (int)std::floor(Nf);
	float pf = n <= full ? 1.f : (n == full + 1 ? Nf - full : 0.f);
	if (pf <= 0.f) return 0.f;
	float w = (n & 1) ? s.odd : s.even;
	float p = 1.25f - 1.75f * clampf(s.tilt, -1.f, 1.f);
	return w * std::pow((float)n, -p) * pf;
}

struct HarmoniaVoice {
	float table[2][HRM_TABLE];
	int cur = 0;
	float xf = 1.f;               // crossfade weight of table[cur]; < 1 while fading in
	bool fading = false;
	double phase = 0.0;
	float prevOut = 0.f;
	Spectrum built;               // spectrum of table[cur]
	int builtTop = 0;             // highest harmonic built
	bool builtSpread = true;
	bool hasTable = false;
	int cooldown = 0;
	Down2 down;
	// envelope
	Adsr adsr;
	float env = 0.f;
	float cyclePh = 0.f;
	bool gate = false, lastGate = false;
	float out = 0.f;              // last voice output (pre-pan), volts
	HarmoniaVoice() { for (auto& t : table) std::fill(t, t + HRM_TABLE, 0.f); }

	float readTable(int t, double ph) const {
		float x = (float)(ph * HRM_TABLE);
		int i = (int)x; float f = x - i;
		i &= HRM_TABLE - 1;
		int j = (i + 1) & (HRM_TABLE - 1);
		return table[t][i] + (table[t][j] - table[t][i]) * f;
	}
	float sample(double ph) const {
		float a = readTable(cur, ph);
		if (!fading) return a;
		float b = readTable(cur ^ 1, ph);
		return b + (a - b) * xf;
	}
};

struct Harmonia {
	enum EnvType { DRONE, ONE_SHOT, CYCLE };
	enum Assign { STACK, ROUND_ROBIN, CHANNEL };
	HarmoniaVoice voice[6];
	AdditiveTable builder;
	float amp[HRM_TABLE / 2], ph[HRM_TABLE / 2];
	float fs = 48000.f;
	int sinceRebuild = 0, rr = 0;
	int nextVoice = 0;            // round robin
	int voiceChan[6] = {-1, -1, -1, -1, -1, -1};
	bool chanGate[16] = {};
	float breathPh = 0.f;
	Smoother mixNorm;
	bool spreadPhases = true;

	Harmonia() { builder.init(HRM_TABLE); }
	void setSampleRate(float f) { fs = f; for (auto& v : voice) v.hasTable = false; }

	void build(HarmoniaVoice& v, const Spectrum& s, float f0) {
		int bl = (int)std::floor(0.45f * fs / std::max(f0, 1.f));
		int need = (int)std::ceil(partialCount(s.part));
		int top = std::max(1, std::min(std::min(need, bl), HRM_MAXH));
		for (int n = 0; n < HRM_TABLE / 2; n++) { amp[n] = 0.f; ph[n] = 0.f; }
		for (int n = 1; n <= top; n++) {
			amp[n] = harmonicAmp(s, n);
			ph[n] = spreadPhases ? PI * (float)n * (float)n / (float)top : 0.f;
		}
		int dst = v.hasTable ? (v.cur ^ 1) : v.cur;
		builder.build(amp, ph, top + 1, v.table[dst]);
		float peak = 0.f;
		for (int i = 0; i < HRM_TABLE; i++) peak = std::max(peak, std::fabs(v.table[dst][i]));
		float g = peak > 1e-9f ? 1.f / peak : 0.f;
		for (int i = 0; i < HRM_TABLE; i++) v.table[dst][i] *= g;
		if (v.hasTable) { v.cur = dst; v.xf = 0.f; v.fading = true; }
		v.hasTable = true;
		v.built = s; v.builtTop = top; v.builtSpread = spreadPhases;
		v.cooldown = 256;
	}
	static bool differs(const Spectrum& a, const Spectrum& b) {
		return std::fabs(a.odd - b.odd) > 0.002f || std::fabs(a.even - b.even) > 0.002f || std::fabs(a.part - b.part) > 0.002f || std::fabs(a.tilt - b.tilt) > 0.002f;
	}
	bool needsBuild(const HarmoniaVoice& v, const Spectrum& s, float f0) const {
		if (!v.hasTable) return true;
		if (v.builtSpread != spreadPhases) return true;
		if (differs(v.built, s)) return true;
		int need = (int)std::ceil(partialCount(s.part));
		// band-limit: rebuild if the built top would alias, or if many audible partials are missing
		if (v.builtTop * f0 > 0.48f * fs) return true;
		int bl = (int)std::floor(0.45f * fs / std::max(f0, 1.f));
		int want = std::min(std::min(need, bl), HRM_MAXH);
		if (want > v.builtTop * 1.1f + 1) return true;
		return false;
	}

	struct In {
		Spectrum spec[6];
		float tune[6] = {}, oct[6] = {};
		bool mult[6] = {};
		float root = 0.f, multDepth = 0.3f;
		float loc[3] = {-0.6f, 0.f, 0.6f};
		float atk = 2.f, dec = 3.f, sus = 0.8f, rel = 6.f;
		float rate = 0.1f, spread = 0.3f, fmFade = 0.f, level = 0.8f;
		EnvType envType = DRONE;
		Assign assign = ROUND_ROBIN;
		int channels = 0;           // V/OCT channels
		float voct[16] = {};
		bool gatePatched = false;
		int gateChannels = 0;
		bool gates[16] = {};
	};
	struct Out { float L = 0.f, R = 0.f; float voices[6] = {}; float envs[6] = {}; };

	// voices 2 and 5 at A, 1 and 3 at B, 4 and 6 at C
	static int locOf(int v) { static const int m[6] = {1, 0, 1, 2, 0, 2}; return m[v]; }

	void assignVoices(const In& in, float* voct, bool* gate) {
		int nch = std::max(in.gateChannels, 1);
		for (int v = 0; v < 6; v++) { voct[v] = 0.f; gate[v] = false; }
		if (!in.gatePatched) {
			for (int v = 0; v < 6; v++) {
				int c = in.channels > 0 ? (in.assign == CHANNEL ? v : 0) : -1;
				voct[v] = (c >= 0 && c < in.channels) ? in.voct[c] : 0.f;
			}
			return;
		}
		auto vo = [&](int c) { return c < in.channels ? in.voct[c] : (in.channels > 0 ? in.voct[0] : 0.f); };
		if (in.assign == CHANNEL) {
			for (int v = 0; v < 6; v++) { voct[v] = v < nch ? vo(v) : 0.f; gate[v] = v < nch && in.gates[v]; }
		} else if (in.assign == STACK) {
			int c = -1;
			for (int k = 0; k < nch; k++) if (in.gates[k]) { c = k; break; }
			for (int v = 0; v < 6; v++) { voct[v] = c >= 0 ? vo(c) : vo(0); gate[v] = c >= 0; }
		} else {
			for (int k = 0; k < nch && k < 16; k++) {
				if (in.gates[k] && !chanGate[k]) { voiceChan[nextVoice] = k; nextVoice = (nextVoice + 1) % 6; }
				chanGate[k] = in.gates[k];
			}
			for (int v = 0; v < 6; v++) {
				int c = voiceChan[v];
				if (c >= 0 && c < nch) { voct[v] = vo(c); gate[v] = in.gates[c]; }
				else voct[v] = vo(0);
			}
		}
	}

	Out process(const In& in) {
		Out o;
		float dt = 1.f / fs;
		float voct[6]; bool gate[6];
		assignVoices(in, voct, gate);
		bool assigned[6];
		for (int v = 0; v < 6; v++) assigned[v] = !in.gatePatched || in.assign != CHANNEL || v < std::max(in.gateChannels, 1);
		breathPh += in.rate * dt; breathPh -= std::floor(breathPh);
		// table rebuild scheduling: at most one voice per 64 samples
		float f0s[6];
		for (int v = 0; v < 6; v++) f0s[v] = 261.6256f * std::pow(2.f, clampf(in.root - 1.f + voct[v] + in.oct[v] + in.tune[v] / 12.f, -7.f, 7.f));
		if (++sinceRebuild >= 64) {
			for (int k = 0; k < 6; k++) {
				int v = (rr + k) % 6;
				HarmoniaVoice& hv = voice[v];
				if (hv.cooldown <= 0 && !hv.fading && needsBuild(hv, in.spec[v], f0s[v])) {
					build(hv, in.spec[v], f0s[v]);
					rr = (v + 1) % 6;
					sinceRebuild = 0;
					break;
				}
			}
		}
		float active = 0.f;
		for (int v = 0; v < 6; v++) {
			HarmoniaVoice& hv = voice[v];
			if (hv.cooldown > 0) hv.cooldown--;
			if (hv.fading) { hv.xf += 1.f / 256.f; if (hv.xf >= 1.f) { hv.xf = 1.f; hv.fading = false; } }
			// envelope
			bool g = gate[v];
			bool rise = g && !hv.lastGate, fall = !g && hv.lastGate;
			hv.lastGate = g;
			float e = 0.f;
			EnvType et = in.envType;
			if (et == DRONE && in.gatePatched && assigned[v]) et = CYCLE;
			if (et == DRONE) {
				e = 1.f - 0.5f * in.spread * (1.f - std::cos(TWO_PI * (breathPh + v / 6.f))) * 0.5f;
			} else if (et == ONE_SHOT) {
				if (rise) hv.adsr.gateOn();
				if (fall) hv.adsr.gateOff();
				e = hv.adsr.process(in.atk, in.dec, in.sus, in.rel, dt);
			} else {
				bool running = !in.gatePatched || !assigned[v] ? true : g;
				if (rise) hv.cyclePh = 0.f;
				if (running) {
					hv.cyclePh += in.rate * dt;
					if (hv.cyclePh >= 1.f) hv.cyclePh -= 1.f;
					float ph = hv.cyclePh + (in.gatePatched ? 0.f : in.spread * v / 6.f);
					ph -= std::floor(ph);
					float af = in.atk / std::max(in.atk + in.dec, 1e-4f);
					float c;
					if (ph < af) { float u = ph / std::max(af, 1e-4f); c = 1.f - (1.f - u) * (1.f - u); }
					else { float u = (ph - af) / std::max(1.f - af, 1e-4f); c = (1.f - u) * (1.f - u); }
					hv.env += (c - hv.env) * (1.f - std::exp(-dt / 0.002f));
					e = hv.env;
				} else {
					hv.env -= hv.env * (1.f - std::exp(-dt * 4.6f / std::max(in.rel, 1e-3f)));
					e = hv.env;
				}
			}
			hv.env = e;
			o.envs[v] = 10.f * e;
			if (!hv.hasTable || e <= 1e-5f) { hv.out = 0.f; o.voices[v] = 0.f; continue; }
			active += 1.f;
			float f0 = std::min(f0s[v], 0.45f * fs);
			float y;
			if (in.mult[v]) {
				float idx = in.fmFade >= 0.f ? in.multDepth * (1.f + in.fmFade * (e - 1.f)) : in.multDepth * std::max(0.f, 1.f - (-in.fmFade) * e);
				float s2[2];
				for (int k = 0; k < 2; k++) {
					hv.phase += f0 * dt * 0.5f;
					hv.phase -= std::floor(hv.phase);
					double p = hv.phase + idx * 0.25f * hv.prevOut;
					p -= std::floor(p);
					s2[k] = hv.sample(p);
					hv.prevOut = s2[k];
				}
				y = hv.down.process(s2[0], s2[1]);
			} else {
				hv.phase += f0 * dt;
				hv.phase -= std::floor(hv.phase);
				y = hv.sample(hv.phase);
				hv.prevOut = y;
			}
			float vo = 5.f * y * e * in.level;
			hv.out = vo;
			o.voices[v] = vo;
			float pan = clampf(in.loc[locOf(v)], -1.f, 1.f);
			o.L += vo * std::cos((pan + 1.f) * PI * 0.25f) * 1.4142f;
			o.R += vo * std::sin((pan + 1.f) * PI * 0.25f) * 1.4142f;
		}
		float norm = mixNorm.process(1.f / std::sqrt(std::max(1.f, active)), dt, 0.05f);
		o.L = softClamp(o.L * norm * 0.7071f);
		o.R = softClamp(o.R * norm * 0.7071f);
		if (!finite(o.L) || !finite(o.R)) { for (auto& v : voice) { v.phase = 0; v.prevOut = 0; v.env = 0; v.adsr.reset(); } o.L = o.R = 0.f; }
		return o;
	}
};

} // namespace dens
