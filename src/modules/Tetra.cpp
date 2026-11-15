// TETRA (TET): four two-operator phase-modulation voices with a cross-modulation matrix.
#include "Common.hpp"
#include "../dsp/Tetra.hpp"

static const char* const TETRA_RATIO_NAMES[16] = {"0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4", "5", "6", "7", "1.414", "2.76", "3.14", "5.19", "7.13"};

struct Tetra : Module, TetraIds {
	static constexpr int VS = PITCHX_PARAM - PITCHW_PARAM;
	static_assert(VS == 8 && TRIGX_INPUT - TRIGW_INPUT == 3, "voice layout");

	dens::Tetra tet;
	dens::Tetra::Params p;
	Trig trig[4];
	bool gate[4] = {}, rise[4] = {};

	Tetra() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		static const char* chn[4] = {"W", "X", "Y", "Z"};
		static const float pitchDef[4] = {0.f, -1.f, 1.f, -2.f};
		static const float ratioDef[4] = {3.f, 1.f, 13.f, 11.f};
		static const float panDef[4] = {-0.5f, 0.5f, -0.2f, 0.2f};
		for (int c = 0; c < 4; c++) {
			std::string n = std::string("Voice ") + chn[c] + " ";
			int b = PITCHW_PARAM + VS * c;
			configParam(b + 0, -4.f, 4.f, pitchDef[c], n + "pitch", " oct");
			configParam(b + 1, 0.f, 8.f, 2.f, n + "PM index", " rad");
			configNamed(this, b + 2, 0.f, 15.f, ratioDef[c], n + "modulator ratio", TETRA_RATIO_NAMES, 16);
			configParam(b + 3, 0.f, 1.5f, 0.2f, n + "modulator feedback");
			configExp(this, b + 4, 0.005f, 5.f, 0.3f, n + "decay", " s");
			configParam(b + 5, 0.f, 4.f, 0.f, n + "pitch envelope", " oct");
			configParam(b + 6, -1.f, 1.f, 0.5f, n + "index envelope");
			configParam(b + 7, -1.f, 1.f, panDef[c], n + "pan");
			int ib = TRIGW_INPUT + 3 * c;
			configInput(ib + 0, n + (c == 0 ? "trigger (poly spreads over W X Y Z)" : "trigger"));
			configInput(ib + 1, n + (c == 0 ? "V/OCT (poly spreads over W X Y Z)" : "V/OCT (normalled from the voice to the left)"));
			configInput(ib + 2, n + (c == 0 ? "accent (0 to 10 V, poly spreads)" : "accent (0 to 10 V)"));
			configOutput(OUTW_OUTPUT + c, n + "out");
		}
		configNamed(this, ROUTE_PARAM, 0.f, 7.f, 0.f, "Matrix route", dens::TETRA_ROUTE_NAMES, 8);
		configParam(MATRIX_PARAM, 0.f, 1.f, 0.f, "Matrix depth", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, 0.f, 50.f, 0.f, "Detune", " cents");
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", {"Voice", "Drum"});
		configTrim(this, INDEX_CV_PARAM, "Index");
		configTrim(this, MATRIX_CV_PARAM, "Matrix");
		configInput(INDEX_INPUT, "Index CV (poly, per voice; 10 V = full range)");
		configInput(MATRIX_INPUT, "Matrix depth CV");
		configOutput(OUTL_OUTPUT, "Left");
		configOutput(OUTR_OUTPUT, "Right");
		onSampleRateChange();
	}

	void onSampleRateChange() override { tet.setSampleRate(APP->engine->getSampleRate()); }

	// value for voice c from its own jack, or channel c of a poly cable in voice W's jack
	bool spread(int jack, int c, float& v) {
		Input& own = inputs[jack + 3 * c];
		if (own.isConnected()) { v = own.getVoltage(); return true; }
		Input& w = inputs[jack];
		if (c > 0 && w.getChannels() > c) { v = w.getVoltage(c); return true; }
		return false;
	}

	void process(const ProcessArgs& args) override {
		p.drum = params[MODE_PARAM].getValue() > 0.5f;
		p.route = (int)params[ROUTE_PARAM].getValue();
		p.matrix = clamp(params[MATRIX_PARAM].getValue() + cvAmt(this, MATRIX_INPUT, MATRIX_CV_PARAM) / 10.f, 0.f, 1.f);
		p.detuneCents = params[SPREAD_PARAM].getValue();
		p.level = params[LEVEL_PARAM].getValue();
		int idxCh = inputs[INDEX_INPUT].getChannels();
		float voct = 0.f;
		for (int c = 0; c < 4; c++) {
			int b = PITCHW_PARAM + VS * c;
			dens::Tetra::VoiceParams& v = p.v[c];
			v.pitchOct = params[b].getValue();
			float vo;
			if (spread(VOCTW_INPUT, c, vo)) voct = vo;      // otherwise keep the voice to the left (normalled)
			v.voct = voct;
			float idx = params[b + 1].getValue();
			if (idxCh > 0) idx += inputs[INDEX_INPUT].getVoltage(idxCh == 1 ? 0 : std::min(c, idxCh - 1)) * params[INDEX_CV_PARAM].getValue() * 0.8f;
			v.index = clamp(idx, 0.f, 8.f);
			v.ratio = (int)params[b + 2].getValue();
			v.feedback = params[b + 3].getValue();
			v.decay = expVal(this, b + 4, 0.005f, 5.f);
			v.penvOct = params[b + 5].getValue();
			v.ienv = params[b + 6].getValue();
			v.pan = params[b + 7].getValue();
			float acc;
			v.accent = spread(ACCW_INPUT, c, acc) ? clamp(acc, 0.f, 10.f) : -1.f;
			float tv;
			bool has = spread(TRIGW_INPUT, c, tv);
			rise[c] = has && trig[c].process(tv);
			gate[c] = has && trig[c].high;
		}
		float L, R;
		tet.process(p, gate, rise, L, R);
		outputs[OUTL_OUTPUT].setVoltage(L);
		outputs[OUTR_OUTPUT].setVoltage(R);
		for (int c = 0; c < 4; c++) outputs[OUTW_OUTPUT + c].setVoltage(tet.outs[c]);
	}
};

struct TetraWidget : ModuleWidget {
	TetraWidget(Tetra* module) {
		densScaffold(this, module, "Tetra");
		placeTetra<Tetra>(this, module);
	}
};

Model* modelTetra = createModel<Tetra, TetraWidget>("Tetra");
