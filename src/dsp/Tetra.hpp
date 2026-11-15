// TETRA: four 2-operator phase-modulation voices (W X Y Z), voice or drum mode, cross-modulation matrix.
#pragma once
#include "Common.hpp"
#include "Noise.hpp"

namespace dens {

static const float TETRA_RATIOS[16] = {0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 1.414f, 2.76f, 3.14f, 5.19f, 7.13f};
static const char* const TETRA_ROUTE_NAMES[8] = {"NONE", "RING", "PAIRS", "TO W", "CASCADE", "FROM W", "REVERSE RING", "MESH"};

// route matrix: M[dst][src]
inline void tetraRoute(int r, float M[4][4]) {
	for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) M[i][j] = 0.f;
	switch (r) {
		case 1: M[1][0] = M[2][1] = M[3][2] = M[0][3] = 1.f; break;                  // W→X→Y→Z→W
		case 2: M[1][0] = M[0][1] = M[3][2] = M[2][3] = 1.f; break;                  // W↔X, Y↔Z
		case 3: M[0][1] = M[0][2] = M[0][3] = 1.f; break;                            // X,Y,Z → W
		case 4: M[1][0] = M[2][1] = M[3][2] = 1.f; break;                            // cascade
		case 5: M[1][0] = M[2][0] = M[3][0] = 1.f; break;                            // W → all
		case 6: M[3][0] = M[2][3] = M[1][2] = M[0][1] = 1.f; break;                  // W→Z→Y→X→W
		case 7: for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if (i != j) M[i][j] = 1.f / 3.f; break;
		default: break;
	}
}

struct TetraVoice {
	double pc = 0.0, pm = 0.0;     // phases (cycles)
	float m1 = 0.f, m2 = 0.f;      // modulator history for feedback
	float env = 0.f, penv = 0.f, click = 0.f;
	bool attacking = false, gateHigh = false;
	float out = 0.f;               // last carrier (−1..1), for the matrix
	float level = 1.f, idxScale = 1.f;
	void reset() { pc = pm = 0.0; m1 = m2 = 0.f; env = penv = click = 0.f; attacking = false; out = 0.f; }
};

struct Tetra {
	struct VoiceParams { float pitchOct = 0.f, voct = 0.f; int ratio = 1; float index = 2.f, feedback = 0.2f, decay = 0.3f, penvOct = 0.f, ienv = 0.5f, accent = -1.f, pan = 0.f; };
	struct Params { VoiceParams v[4]; bool drum = false; int route = 0; float matrix = 0.f, detuneCents = 0.f, level = 0.8f; };
	TetraVoice voice[4];
	Rng rng{77};
	float fs = 48000.f;
	float outs[4] = {0, 0, 0, 0};
	void setSampleRate(float f) { fs = f; }
	void reset() { for (auto& v : voice) v.reset(); }

	// gate: current gate level per voice (true while high); rise: trigger event this sample
	void process(const Params& p, const bool* gate, const bool* rise, float& L, float& R) {
		float dt = 1.f / fs;
		float M[4][4];
		tetraRoute(p.route, M);
		static const float detuneSign[4] = {1.f, -1.f, 0.5f, -0.5f};
		float prevOut[4];
		for (int i = 0; i < 4; i++) prevOut[i] = voice[i].out;
		L = R = 0.f;
		for (int i = 0; i < 4; i++) {
			TetraVoice& v = voice[i];
			const VoiceParams& vp = p.v[i];
			if (rise[i]) {
				v.attacking = true;
				v.penv = 1.f;
				v.click = 1.f;
				// ACCENT unpatched (accent < 0): plain level and index
				float acc = clampf(vp.accent / 10.f, 0.f, 1.f);
				v.level = vp.accent < 0.f ? 1.f : 0.4f + 0.6f * acc;
				v.idxScale = vp.accent < 0.f ? 1.f : 0.5f + acc;
				if (p.drum) { v.pc = 0.0; v.pm = 0.0; }
			}
			v.gateHigh = gate[i];
			// envelope: DECAY = time to −60 dB
			float dec = std::max(vp.decay, 0.005f);
			float kd = 1.f - std::exp(-6.9078f * dt / dec);
			float att = p.drum ? 0.0005f : 0.003f;
			if (v.attacking) {
				v.env += (1.f - v.env) * (1.f - std::exp(-dt / (att / 4.f)));
				if (v.env > 0.999f) { v.env = 1.f; v.attacking = false; }
			} else if (p.drum || !v.gateHigh) {
				v.env -= v.env * kd;
			}
			if (v.env < 1e-6f) v.env = 0.f;
			float pd = 1.f - std::exp(-6.9078f * dt / (dec * 0.25f));
			v.penv -= v.penv * pd;
			v.click = std::max(0.f, v.click - dt / 0.002f);
			if (v.env == 0.f && !v.attacking) { v.out = 0.f; outs[i] = 0.f; continue; }
			float cents = p.detuneCents * detuneSign[i];
			float pitch = vp.voct + vp.pitchOct + vp.penvOct * v.penv + cents / 1200.f;
			float fc = 261.6256f * std::pow(2.f, clampf(pitch, -8.f, 7.f));
			fc = std::min(fc, 0.45f * fs);
			float fm = fc * TETRA_RATIOS[std::max(0, std::min(15, vp.ratio))];
			v.pm += fm * dt; v.pm -= std::floor(v.pm);
			float mod = std::sin(TWO_PI * (float)v.pm + vp.feedback * 0.5f * (v.m1 + v.m2));
			v.m2 = v.m1; v.m1 = mod;
			float ie = vp.ienv >= 0.f ? (1.f - vp.ienv + vp.ienv * v.env) : (1.f - (-vp.ienv) * v.env);
			float I = vp.index * ie * v.idxScale;
			float cross = 0.f;
			for (int j = 0; j < 4; j++) cross += M[i][j] * prevOut[j];
			cross *= p.matrix * 4.f;
			v.pc += fc * dt; v.pc -= std::floor(v.pc);
			float car = std::sin(TWO_PI * (float)v.pc + I * mod + cross);
			v.out = car * v.env;
			float s = car * v.env * v.level;
			if (p.drum && v.click > 0.f) s += 0.25f * rng.bipolar() * v.click;
			s *= 5.f * p.level;
			outs[i] = s;
			float pan = clampf(vp.pan, -1.f, 1.f);
			L += s * std::cos((pan + 1.f) * PI * 0.25f);
			R += s * std::sin((pan + 1.f) * PI * 0.25f);
		}
		L = softClamp(L); R = softClamp(R);
		if (!finite(L) || !finite(R)) { reset(); L = R = 0.f; }
	}
};

} // namespace dens
