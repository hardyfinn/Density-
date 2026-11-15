// AXIOM (AXM): ten equations, four pulse channels W X Y Z. DSP in dsp/Axiom.hpp.
#include "Common.hpp"
#include "../dsp/Axiom.hpp"
#include <atomic>

static const char* const LOGIC_NAMES[4] = {"OFF", "XOR", "AND", "OR"};

struct Axiom : Module, AxiomIds {
	static constexpr int CS = LENX_PARAM - LENW_PARAM;
	static_assert(CS == 8 && AW_PARAM - LENW_PARAM == 3 && EQW_PARAM - LENW_PARAM == 4 && AW_CV_PARAM - LENW_PARAM == 6, "channel layout");
	static_assert(AX_INPUT - AW_INPUT == 2 && GATEX_OUTPUT - GATEW_OUTPUT == 3, "io layout");

	dens::Axiom ax;
	uint64_t seed = 1;
	std::atomic<bool> seedPending{false};
	dsp::BooleanTrigger seedBtn;
	Trig clkTrig, rstTrig;
	dsp::ClockDivider lightDiv;
	float seedFlash = 0.f;

	struct LenQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int l = (int)std::lround(getValue());
			return l > 64 ? "∞" : string::f("%d", l);
		}
	};

	static std::string aText(int eq, float a, int L) {
		switch (eq) {
			case 0: return string::f("%d", (int)std::lround(a * L));
			case 1: return string::f("%d", 1 + (int)std::lround(a * 15.f));
			case 2: return string::f("%.3f", 3.4f + 0.6f * a);
			case 3: return string::f("%d", 1 + (int)std::lround(a * 30.f));
			case 4: return string::f("%d", 1 + (int)std::lround(a * 998.f));
			case 6: return string::f("%d", 1 + (int)std::lround(a * 8.f));
			case 7: return string::f("%d", (int)std::lround(a * 255.f));
			case 8: return string::f("%d", (int)std::lround(a * 5.f));
			default: return string::f("%.0f%%", a * 100.f);
		}
	}
	static std::string bText(int eq, float b, int L) {
		switch (eq) {
			case 0: return string::f("%d", 1 + (int)std::lround(b * 7.f));
			case 1: return string::f("%d", 1 + (int)std::lround(b * 15.f));
			case 4: return b < 0.5f ? "ODD" : "EVEN";
			case 6: return string::f("%d", (int)std::lround(b * L));
			case 7: return string::f("%d", (int)std::lround(b * 15.f));
			case 8: return b > 0.5f ? "ON" : "OFF";
			default: return string::f("%.0f%%", b * 100.f);
		}
	}

	struct ABQuantity : ParamQuantity {
		int ch = 0; bool isA = true;
		std::string getDisplayValueString() override {
			Axiom* m = static_cast<Axiom*>(module);
			if (!m) return ParamQuantity::getDisplayValueString();
			int eq = m->ax.ch[ch].eq, L = m->ax.ch[ch].L();
			return isA ? aText(eq, getValue(), L) : bText(eq, getValue(), L);
		}
	};

	Axiom() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TEMPO_PARAM, 20.f, 300.f, 110.f, "Tempo (16th-note steps)", " BPM");
		configButton(RUN_PARAM, "Run");
		params[RUN_PARAM].setValue(1.f);
		getParamQuantity(RUN_PARAM)->defaultValue = 1.f;
		configParam(COMPLEX_PARAM, 0.f, 1.f, 0.f, "Complexity", "%", 0.f, 100.f);
		configNamed(this, LOGIC_PARAM, 0.f, 3.f, 0.f, "Logic (each gate with the channel to its left)", LOGIC_NAMES, 4);
		configParam(RANGE_PARAM, 0.f, 5.f, 2.f, "Pitch range", " oct");
		configNamed(this, SCALE_PARAM, 0.f, 6.f, 4.f, "Scale", dens::AXIOM_SCALE_NAMES, 7);
		configButton(SEED_PARAM, "New seed");
		static const int eqDef[4] = {1, 2, 4, 6};
		static const char* chn[4] = {"W", "X", "Y", "Z"};
		for (int c = 0; c < 4; c++) {
			std::string n = std::string("Channel ") + chn[c] + " ";
			int b = LENW_PARAM + CS * c;
			configParam<LenQuantity>(b + 0, 1.f, 65.f, 16.f, n + "length (steps, ∞ at the end)")->snapEnabled = true;
			configNamed(this, b + 1, 0.f, 11.f, 5.f, n + "ratio", dens::AXIOM_RATIO_NAMES, 12);
			configParam(b + 2, 0.f, 63.f, 0.f, n + "rotate", " steps")->snapEnabled = true;
			auto* qa = configParam<ABQuantity>(b + 3, 0.f, 1.f, c == 0 ? 0.3125f : 0.5f, n + "A");
			qa->ch = c; qa->isA = true;
			configNamed(this, b + 4, 1.f, 10.f, (float)eqDef[c], n + "equation", dens::AXIOM_EQ_NAMES, 10, 1);
			auto* qb = configParam<ABQuantity>(b + 5, 0.f, 1.f, 0.5f, n + "B");
			qb->ch = c; qb->isA = false;
			configTrim(this, b + 6, n + "A");
			configTrim(this, b + 7, n + "B");
			configInput(AW_INPUT + 2 * c, n + "A CV");
			configInput(BW_INPUT + 2 * c, n + "B CV");
			configOutput(GATEW_OUTPUT + 3 * c, n + "gate");
			configOutput(ACCW_OUTPUT + 3 * c, n + "accent (0 to 10 V)");
			configOutput(PITCHW_OUTPUT + 3 * c, n + "pitch (1 V/oct)");
		}
		configInput(CLK_INPUT, "Clock (each rise is one 16th step)");
		configInput(RST_INPUT, "Reset");
		configInput(EQ_INPUT, "Equation CV (poly, per channel: 0 to 10 V adds 0 to 9)");
		configOutput(PGATE_OUTPUT, "Gates (W X Y Z)");
		configOutput(PPITCH_OUTPUT, "Pitches (W X Y Z)");
		configOutput(PACC_OUTPUT, "Accents (W X Y Z)");
		seed = random::u64() | 1;
		ax.setSeed(seed);
		ax.reset();
		lightDiv.setDivision(64);
	}

	void process(const ProcessArgs& args) override {
		if (seedBtn.process(params[SEED_PARAM].getValue() > 0.5f)) { seed = random::u64() | 1; seedPending = true; seedFlash = 1.f; }
		if (seedPending.exchange(false)) { ax.setSeed(seed); for (auto& c : ax.ch) c.resetState(); }
		ax.logic = (int)params[LOGIC_PARAM].getValue();
		ax.complexity = params[COMPLEX_PARAM].getValue();
		ax.range = params[RANGE_PARAM].getValue();
		ax.scale = (int)params[SCALE_PARAM].getValue();
		int eqCh = inputs[EQ_INPUT].getChannels();
		for (int c = 0; c < 4; c++) {
			int b = LENW_PARAM + CS * c;
			dens::AxiomChannel& k = ax.ch[c];
			int len = (int)std::lround(params[b].getValue());
			k.len = len > 64 ? 0 : len;
			ax.ratioIdx[c] = clamp((int)std::lround(params[b + 1].getValue()), 0, 11);
			k.rot = (int)std::lround(params[b + 2].getValue());
			k.a = clamp(params[b + 3].getValue() + cvAmt(this, AW_INPUT + 2 * c, b + 6) / 10.f, 0.f, 1.f);
			k.b = clamp(params[b + 5].getValue() + cvAmt(this, BW_INPUT + 2 * c, b + 7) / 10.f, 0.f, 1.f);
			int eq = (int)std::lround(params[b + 4].getValue()) - 1;
			if (eqCh > 0) {
				float v = inputs[EQ_INPUT].getVoltage(eqCh == 1 ? 0 : std::min(c, eqCh - 1));
				eq += (int)std::floor(clamp(v, 0.f, 10.f) * 0.9999f);
			}
			eq = clamp(eq, 0, 9);
			if (eq != k.eq) { k.eq = eq; k.resetState(); }
		}
		if (rstTrig.process(inputs[RST_INPUT].getVoltage())) ax.reset();
		bool ext = inputs[CLK_INPUT].isConnected();
		bool tick = ext && clkTrig.process(inputs[CLK_INPUT].getVoltage());
		ax.process(args.sampleTime, ext, tick, params[TEMPO_PARAM].getValue(), params[RUN_PARAM].getValue() > 0.5f);

		for (int o : {PGATE_OUTPUT, PPITCH_OUTPUT, PACC_OUTPUT}) outputs[o].setChannels(4);
		for (int c = 0; c < 4; c++) {
			float g = ax.out[c] ? 10.f : 0.f;
			outputs[GATEW_OUTPUT + 3 * c].setVoltage(g);
			outputs[ACCW_OUTPUT + 3 * c].setVoltage(ax.accent[c]);
			outputs[PITCHW_OUTPUT + 3 * c].setVoltage(ax.pitch[c]);
			outputs[PGATE_OUTPUT].setVoltage(g, c);
			outputs[PPITCH_OUTPUT].setVoltage(ax.pitch[c], c);
			outputs[PACC_OUTPUT].setVoltage(ax.accent[c], c);
		}
		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			for (int c = 0; c < 4; c++) lights[GATEW_LIGHT + c].setBrightnessSmooth(ax.out[c] ? 1.f : 0.f, dt, 30.f);
			lights[RUN_LIGHT].setBrightness(params[RUN_PARAM].getValue());
			seedFlash = std::max(0.f, seedFlash - dt * 4.f);
			lights[SEED_LIGHT].setBrightness(seedFlash);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "seed", json_string(std::to_string(seed).c_str()));
		json_object_set_new(r, "triggers", json_boolean(ax.triggers));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* s = json_object_get(r, "seed");
		if (s && json_is_string(s)) { seed = std::stoull(json_string_value(s)) | 1; ax.setSeed(seed); ax.reset(); }
		ax.triggers = jsonBool(r, "triggers", false);
	}
	void onReset() override { ax.reset(); }
};

struct AxiomDisplay : DDisplay {
	Axiom* m = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		static const char* chn = "WXYZ";
		float rowH = (H - 14.f) / 4.f;
		for (int c = 0; c < 4; c++) {
			float y = 3.f + rowH * c;
			int eq = m ? m->ax.ch[c].eq : c;
			bool on = m && m->ax.out[c];
			text(vg, 4.f, y, std::string(1, chn[c]), 10.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, on ? acc() : ink(0.6f));
			text(vg, 14.f, y, dens::AXIOM_EQ_NAMES[eq], 10.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			if (m) {
				const dens::AxiomChannel& k = m->ax.ch[c];
				std::string ab = string::f("%s %s  %s %s", dens::AXIOM_A_NAMES[eq], Axiom::aText(eq, k.a, k.L()).c_str(), dens::AXIOM_B_NAMES[eq], Axiom::bText(eq, k.b, k.L()).c_str());
				text(vg, W - 4.f, y + 1.f, ab, 8.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, ink(0.7f));
			}
		}
		if (m) {
			bool ext = m->inputs[Axiom::CLK_INPUT].isConnected();
			std::string t = ext ? "EXT CLOCK" : string::f("%.0f BPM", m->params[Axiom::TEMPO_PARAM].getValue());
			text(vg, 4.f, H - 11.f, t, 8.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(0.6f));
			text(vg, W * 0.5f, H - 11.f, dens::AXIOM_SCALE_NAMES[m->ax.scale], 8.f, NVG_ALIGN_CENTER | NVG_ALIGN_TOP, ink(0.6f));
			text(vg, W - 4.f, H - 11.f, string::f("TIER %d", m->ax.tier()), 8.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, acc());
		}
	}
};

struct AxiomWidget : ModuleWidget {
	AxiomWidget(Axiom* module) {
		densScaffold(this, module, "Axiom");
		auto* d = new AxiomDisplay;
		d->m = module;
		d->accent = FAMILY_EQUATION;
		d->setRectMm(AXIOM_DISPLAY_MM);
		addChild(d);
		placeAxiom<Axiom>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Axiom* m = dynamic_cast<Axiom*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Gate outputs", {"Gates (half a step)", "Triggers (5 ms)"},
			[=]() { return m->ax.triggers ? 1 : 0; }, [=](int i) { m->ax.triggers = i == 1; }));
	}
};

Model* modelAxiom = createModel<Axiom, AxiomWidget>("Axiom");
