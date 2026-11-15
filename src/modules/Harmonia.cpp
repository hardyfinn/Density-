// HARMONIA (HRM): six additive harmonic voices. DSP in dsp/Harmonia.hpp.
#include "Common.hpp"
#include "../dsp/Harmonia.hpp"

struct Harmonia : Module, HarmoniaIds {
	static constexpr int VS = ODD2_PARAM - ODD1_PARAM;   // params per voice strip
	static_assert(MULT1_PARAM - ODD1_PARAM == 6, "voice strip layout");
	static_assert(MULT2_LIGHT - MULT1_LIGHT == 1, "mult lights");

	dens::Harmonia hrm;
	dens::Harmonia::In in;
	float bankA[24] = {}, bankB[24] = {};
	bool hasA = false, hasB = false;
	bool snapTune = false;
	dsp::BooleanTrigger staBtn, stbBtn, diceBtn;
	Trig diceTrig;
	dsp::ClockDivider slow, lightDiv;
	float blink = 0.f;
	// for the display (written on the audio thread, read by the UI)
	dens::Spectrum shown[6];
	float shownF0[6] = {};
	bool shownActive[6] = {};

	Harmonia() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		static const float tuneDef[6] = {0.f, 7.f, 4.f, -5.f, 2.f, 0.f};
		static const float octDef[6] = {0.f, 0.f, 1.f, 0.f, 1.f, -1.f};
		for (int v = 0; v < 6; v++) {
			std::string n = "Voice " + std::to_string(v + 1) + " ";
			int b = ODD1_PARAM + VS * v;
			configParam(b + 0, 0.f, 1.f, 1.f, n + "odd harmonics", "%", 0.f, 100.f);
			configParam(b + 1, 0.f, 1.f, 0.5f, n + "even harmonics", "%", 0.f, 100.f);
			configParam<PartialsQuantity>(b + 2, 0.f, 1.f, 0.25f, n + "partials");
			configParam(b + 3, -1.f, 1.f, -0.2f, n + "tilt (dark to bright)");
			configParam(b + 4, -12.f, 12.f, tuneDef[v], n + "tune", " semitones");
			configParam(b + 5, -3.f, 3.f, octDef[v], n + "octave")->snapEnabled = true;
			configButton(b + 6, n + "self-modulation (MULT)");
		}
		configExp(this, ATK_PARAM, 0.001f, 20.f, 2.f, "Attack", " s");
		configExp(this, DEC_PARAM, 0.001f, 20.f, 3.f, "Decay", " s");
		configParam(SUS_PARAM, 0.f, 1.f, 0.8f, "Sustain", "%", 0.f, 100.f);
		configExp(this, REL_PARAM, 0.001f, 30.f, 6.f, "Release", " s");
		configExp(this, RATE_PARAM, 0.01f, 10.f, 0.1f, "Rate (cycle and breathing)", " Hz");
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.3f, "Spread (voices in turn)", "%", 0.f, 100.f);
		configParam(FMFADE_PARAM, -1.f, 1.f, 0.f, "FM fade (self-modulation follows the envelope)");
		configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
		configSwitch(ENVT_PARAM, 0.f, 2.f, 0.f, "Envelope", {"Drone", "One shot", "Cycle"});
		configSwitch(ASSIGN_PARAM, 0.f, 2.f, 1.f, "Voice assign", {"Stack", "Round robin", "Channel"});
		configButton(LOCK_PARAM, "Harmonic lock (all tilts follow voice 1)");
		configButton(MORPHON_PARAM, "Morph between banks A and B");
		configButton(STA_PARAM, "Store spectra in bank A");
		configButton(STB_PARAM, "Store spectra in bank B");
		configButton(DICE_PARAM, "Dice: randomise spectra and MULT");
		configTrim(this, ODD_CV_PARAM, "Odd");
		configTrim(this, EVEN_CV_PARAM, "Even");
		configTrim(this, PART_CV_PARAM, "Partials");
		configTrim(this, TILT_CV_PARAM, "Tilt");
		configTrim(this, MORPH_CV_PARAM, "Morph");
		configTrim(this, LOCA_CV_PARAM, "Location A");
		configTrim(this, LOCB_CV_PARAM, "Location B");
		configTrim(this, LOCC_CV_PARAM, "Location C");
		configParam(ROOT_PARAM, -2.f, 2.f, 0.f, "Root (1 V/oct, 0 = C3)", " V");
		configParam(MULTD_PARAM, 0.f, 1.f, 0.3f, "MULT depth", "%", 0.f, 100.f);
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph A to B", "%", 0.f, 100.f);
		configParam(LOCA_PARAM, -1.f, 1.f, -0.6f, "Location A pan (voices 2, 5)");
		configParam(LOCB_PARAM, -1.f, 1.f, 0.f, "Location B pan (voices 1, 3)");
		configParam(LOCC_PARAM, -1.f, 1.f, 0.6f, "Location C pan (voices 4, 6)");
		configInput(VOCT_INPUT, "V/OCT (poly, up to 6)");
		configInput(GATE_INPUT, "Gate (poly, up to 6)");
		configInput(DICE_INPUT, "Dice trigger");
		configInput(ODD_CV_INPUT, "Odd CV (poly, per voice)");
		configInput(EVEN_CV_INPUT, "Even CV (poly, per voice)");
		configInput(PART_CV_INPUT, "Partials CV (poly, per voice)");
		configInput(TILT_CV_INPUT, "Tilt CV (poly, per voice)");
		configInput(MORPH_CV_INPUT, "Morph CV");
		configInput(LOCA_CV_INPUT, "Location A CV");
		configInput(LOCB_CV_INPUT, "Location B CV");
		configInput(LOCC_CV_INPUT, "Location C CV");
		configOutput(OUTL_OUTPUT, "Left");
		configOutput(OUTR_OUTPUT, "Right");
		configOutput(VOICES_OUTPUT, "Voices (6 channels, post-envelope, pre-pan)");
		configOutput(ENVS_OUTPUT, "Envelopes (6 channels, 0 to 10 V)");
		slow.setDivision(16);
		lightDiv.setDivision(256);
		onSampleRateChange();
	}

	struct PartialsQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			return string::f("%d", (int)std::floor(dens::partialCount(getValue())));
		}
	};

	void onSampleRateChange() override { hrm.setSampleRate(APP->engine->getSampleRate()); }

	float sp(int v, int k) { return params[ODD1_PARAM + VS * v + k].getValue(); }

	void store(float* bank) {
		for (int v = 0; v < 6; v++)
			for (int k = 0; k < 4; k++) bank[v * 4 + k] = sp(v, k);
	}
	void dice() {
		for (int v = 0; v < 6; v++) {
			int b = ODD1_PARAM + VS * v;
			params[b + 0].setValue(random::uniform());
			params[b + 1].setValue(random::uniform());
			float u = random::uniform();
			params[b + 2].setValue(u * u);
			params[b + 3].setValue(2.f * random::uniform() - 1.f);
			params[b + 6].setValue(random::uniform() < 0.3f ? 1.f : 0.f);
		}
	}

	void updateSlow() {
		bool morphOn = params[MORPHON_PARAM].getValue() > 0.5f && hasA && hasB;
		float morph = clamp(params[MORPH_PARAM].getValue() + cvAmt(this, MORPH_CV_INPUT, MORPH_CV_PARAM) / 10.f, 0.f, 1.f);
		bool lock = params[LOCK_PARAM].getValue() > 0.5f;
		for (int v = 0; v < 6; v++) {
			float s[4];
			for (int k = 0; k < 4; k++) s[k] = morphOn ? dens::lerp(bankA[v * 4 + k], bankB[v * 4 + k], morph) : sp(v, k);
			s[0] += cvAmt(this, ODD_CV_INPUT, ODD_CV_PARAM, v) / 10.f;
			s[1] += cvAmt(this, EVEN_CV_INPUT, EVEN_CV_PARAM, v) / 10.f;
			s[2] += cvAmt(this, PART_CV_INPUT, PART_CV_PARAM, v) / 10.f;
			s[3] += cvAmt(this, TILT_CV_INPUT, TILT_CV_PARAM, v) / 5.f;
			dens::Spectrum& sp_ = in.spec[v];
			sp_.odd = clamp(s[0], 0.f, 1.f);
			sp_.even = clamp(s[1], 0.f, 1.f);
			sp_.part = clamp(s[2], 0.f, 1.f);
			sp_.tilt = clamp(s[3], -1.f, 1.f);
			float tune = sp(v, 4);
			in.tune[v] = snapTune ? std::round(tune) : tune;
			in.oct[v] = std::round(sp(v, 5));
			in.mult[v] = sp(v, 6) > 0.5f;
		}
		if (lock)
			for (int v = 1; v < 6; v++) in.spec[v].tilt = in.spec[0].tilt;
		in.root = params[ROOT_PARAM].getValue();
		in.multDepth = params[MULTD_PARAM].getValue();
		in.loc[0] = clamp(params[LOCA_PARAM].getValue() + cvAmt(this, LOCA_CV_INPUT, LOCA_CV_PARAM) / 5.f, -1.f, 1.f);
		in.loc[1] = clamp(params[LOCB_PARAM].getValue() + cvAmt(this, LOCB_CV_INPUT, LOCB_CV_PARAM) / 5.f, -1.f, 1.f);
		in.loc[2] = clamp(params[LOCC_PARAM].getValue() + cvAmt(this, LOCC_CV_INPUT, LOCC_CV_PARAM) / 5.f, -1.f, 1.f);
		in.atk = expVal(this, ATK_PARAM, 0.001f, 20.f);
		in.dec = expVal(this, DEC_PARAM, 0.001f, 20.f);
		in.sus = params[SUS_PARAM].getValue();
		in.rel = expVal(this, REL_PARAM, 0.001f, 30.f);
		in.rate = expVal(this, RATE_PARAM, 0.01f, 10.f);
		in.spread = params[SPREAD_PARAM].getValue();
		in.fmFade = params[FMFADE_PARAM].getValue();
		in.level = params[LEVEL_PARAM].getValue();
		in.envType = (dens::Harmonia::EnvType)(int)params[ENVT_PARAM].getValue();
		in.assign = (dens::Harmonia::Assign)(int)params[ASSIGN_PARAM].getValue();
	}

	void process(const ProcessArgs& args) override {
		if (staBtn.process(params[STA_PARAM].getValue() > 0.5f)) { store(bankA); hasA = true; }
		if (stbBtn.process(params[STB_PARAM].getValue() > 0.5f)) { store(bankB); hasB = true; }
		bool d = diceBtn.process(params[DICE_PARAM].getValue() > 0.5f);
		if (diceTrig.process(inputs[DICE_INPUT].getVoltage()) || d) dice();

		if (slow.process()) updateSlow();

		in.channels = inputs[VOCT_INPUT].getChannels();
		for (int c = 0; c < in.channels && c < 16; c++) in.voct[c] = inputs[VOCT_INPUT].getVoltage(c);
		in.gatePatched = inputs[GATE_INPUT].isConnected();
		in.gateChannels = inputs[GATE_INPUT].getChannels();
		for (int c = 0; c < in.gateChannels && c < 16; c++) {
			float g = inputs[GATE_INPUT].getVoltage(c);
			// Schmitt: high above 1 V, low below 0.1 V
			if (g >= 1.f) in.gates[c] = true;
			else if (g < 0.1f) in.gates[c] = false;
		}

		dens::Harmonia::Out o = hrm.process(in);
		outputs[OUTL_OUTPUT].setVoltage(o.L);
		outputs[OUTR_OUTPUT].setVoltage(o.R);
		outputs[VOICES_OUTPUT].setChannels(6);
		outputs[ENVS_OUTPUT].setChannels(6);
		for (int v = 0; v < 6; v++) {
			outputs[VOICES_OUTPUT].setVoltage(o.voices[v], v);
			outputs[ENVS_OUTPUT].setVoltage(o.envs[v], v);
		}

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			blink += dt * 3.f; blink -= std::floor(blink);
			for (int v = 0; v < 6; v++) {
				lights[MULT1_LIGHT + v].setBrightness(in.mult[v] ? 1.f : 0.f);
				shown[v] = in.spec[v];
				shownActive[v] = o.envs[v] > 0.05f;
				shownF0[v] = in.root - 1.f + in.oct[v] + in.tune[v] / 12.f;
			}
			lights[LOCK_LIGHT].setBrightness(params[LOCK_PARAM].getValue());
			bool mo = params[MORPHON_PARAM].getValue() > 0.5f;
			lights[MORPHON_LIGHT].setBrightness(mo ? ((hasA && hasB) ? 1.f : (blink < 0.5f ? 1.f : 0.f)) : 0.f);
			lights[STA_LIGHT].setBrightness(hasA ? 0.8f : 0.f);
			lights[STB_LIGHT].setBrightness(hasB ? 0.8f : 0.f);
			lights[DICE_LIGHT].setBrightness(params[DICE_PARAM].getValue());
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "bankA", jsonFloats(bankA, 24));
		json_object_set_new(r, "bankB", jsonFloats(bankB, 24));
		json_object_set_new(r, "hasA", json_boolean(hasA));
		json_object_set_new(r, "hasB", json_boolean(hasB));
		json_object_set_new(r, "snapTune", json_boolean(snapTune));
		json_object_set_new(r, "spreadPhases", json_boolean(hrm.spreadPhases));
		return r;
	}
	void dataFromJson(json_t* r) override {
		readFloats(r, "bankA", bankA, 24);
		readFloats(r, "bankB", bankB, 24);
		hasA = jsonBool(r, "hasA", false);
		hasB = jsonBool(r, "hasB", false);
		setSnap(jsonBool(r, "snapTune", false));
		hrm.spreadPhases = jsonBool(r, "spreadPhases", true);
	}
	void setSnap(bool s) {
		snapTune = s;
		for (int v = 0; v < 6; v++) getParamQuantity(ODD1_PARAM + VS * v + 4)->snapEnabled = s;
	}
	void onReset() override { hasA = hasB = false; setSnap(false); hrm.spreadPhases = true; }
};

struct HarmoniaDisplay : DDisplay {
	Harmonia* m = nullptr;
	static std::string noteName(float oct) {
		static const char* n[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		int semi = (int)std::lround(oct * 12.f) + 48;   // 0 V = C4
		int o = (int)std::floor(semi / 12.f);
		int s = semi - 12 * o;
		return std::string(n[s]) + std::to_string(o);
	}
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		float cw = W / 6.f;
		for (int v = 0; v < 6; v++) {
			dens::Spectrum s = m ? m->shown[v] : dens::Spectrum();
			bool act = m ? m->shownActive[v] : true;
			float x0 = v * cw + 4.f, w = cw - 8.f;
			float top = 14.f, bot = H - 14.f;
			// first 32 harmonics, normalised to the strongest
			float a[32], mx = 1e-6f;
			for (int n = 1; n <= 32; n++) { a[n - 1] = dens::harmonicAmp(s, n); mx = std::max(mx, a[n - 1]); }
			float bw = w / 32.f;
			for (int n = 0; n < 32; n++) {
				float h = (bot - top) * std::sqrt(a[n] / mx);
				nvgBeginPath(vg);
				nvgRect(vg, x0 + n * bw, bot - h, std::max(1.f, bw - 0.6f), h);
				nvgFillColor(vg, n == 0 ? ink(act ? 1.f : 0.5f) : acc(act ? 0.9f : 0.45f));
				nvgFill(vg);
			}
			text(vg, x0, 2.f, string::f("%d", v + 1), 10.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, acc());
			text(vg, x0 + w, 2.f, m ? noteName(m->shownF0[v] + 0.f) : "C3", 10.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
			text(vg, x0, H - 12.f, string::f("%d P", (int)std::floor(dens::partialCount(s.part))), 9.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(0.8f));
		}
	}
};

struct HarmoniaWidget : ModuleWidget {
	HarmoniaWidget(Harmonia* module) {
		densScaffold(this, module, "Harmonia");
		auto* d = new HarmoniaDisplay;
		d->m = module;
		d->accent = FAMILY_HARMONIC;
		d->setRectMm(HARMONIA_DISPLAY_MM);
		addChild(d);
		placeHarmonia<Harmonia>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Harmonia* m = dynamic_cast<Harmonia*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolMenuItem("Snap tune to semitones", "", [=]() { return m->snapTune; }, [=](bool b) { m->setSnap(b); }));
		menu->addChild(createIndexSubmenuItem("Phases", {"Aligned", "Spread (low crest factor)"},
			[=]() { return m->hrm.spreadPhases ? 1 : 0; }, [=](int i) { m->hrm.spreadPhases = i == 1; }));
		menu->addChild(createMenuItem("Clear morph banks", "", [=]() { m->hasA = m->hasB = false; }));
	}
};

Model* modelHarmonia = createModel<Harmonia, HarmoniaWidget>("Harmonia");
