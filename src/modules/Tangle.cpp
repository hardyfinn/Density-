// TANGLE (TGL): one entangled effects core. DSP in dsp/TangleCore.hpp.
#include "Common.hpp"
#include "../dsp/TangleCore.hpp"

struct Tangle;
extern Model* modelTangle;

struct TangleMessage { float ent = 0.f; bool valid = false; };

struct Tangle : Module, TangleIds {
	static constexpr int PS = POT2_PARAM - POT1_PARAM;
	static_assert(PS == 2 && POT1_CV_PARAM == POT1_PARAM + 1, "pot layout");

	dens::TangleCore core;
	TangleMessage leftMsg[2];
	int shownProg = 0, knobProg = -1;
	bool loadDefaults = true;
	bool ring = false;
	bool latched = false, btnState = false;
	dsp::BooleanTrigger btnTrig;
	Trig gateTrig;
	float entOut = 0.f;        // read by the neighbour on the right (and by the leftmost in a ring)
	float entLevel = 0.f;      // display meter
	int entSource = 0;         // 0 none, 1 left neighbour, 2 jack, 3 ring
	float env = 0.f, envA = 0.f, envR = 0.f;
	float qs[6] = {};
	float smA = 0.f;
	dsp::ClockDivider lightDiv;

	struct PotQuantity : ParamQuantity {
		int index = 0;
		std::string getDisplayValueString() override {
			Tangle* t = static_cast<Tangle*>(module);
			if (!t) return ParamQuantity::getDisplayValueString();
			const dens::TangleProgram& P = dens::TANGLE_PROGRAMS[t->shownProg];
			if (P.kind[index] == dens::Kind::None) return "unused";
			return dens::formatKind(P.kind[index], getValue());
		}
	};
	struct BankQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return std::string(1, dens::TANGLE_BANK_NAMES[clamp((int)std::lround(getValue()), 0, 3)]); }
	};
	struct ProgQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Tangle* t = static_cast<Tangle*>(module);
			int p = clamp((int)std::lround(getValue()), 0, 15);
			if (!t) return string::f("%d", p);
			int bank = clamp((int)std::lround(t->params[BANK_PARAM].getValue()), 0, 3);
			return string::f("%c%d %s", dens::TANGLE_BANK_NAMES[bank], p, dens::TANGLE_PROGRAMS[bank * 16 + p].name);
		}
	};

	Tangle() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<BankQuantity>(BANK_PARAM, 0.f, 3.f, 0.f, "Bank")->snapEnabled = true;
		configParam<ProgQuantity>(PROG_PARAM, 0.f, 15.f, 0.f, "Program")->snapEnabled = true;
		configButton(BTN_PARAM, "Button");
		configParam(ENT_PARAM, -1.f, 1.f, 0.f, "Entangle (+ feed from the left core, − couple its activity to POT 1 and 2)");
		configParam(INLV_PARAM, 0.f, 2.f, 1.f, "Input gain", "%", 0.f, 100.f);
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix", "%", 0.f, 100.f);
		for (int i = 0; i < 6; i++) {
			auto* q = configParam<PotQuantity>(POT1_PARAM + PS * i, 0.f, 1.f, 0.5f, string::f("Pot %d", i + 1));
			q->index = i;
			configTrim(this, POT1_CV_PARAM + PS * i, string::f("Pot %d", i + 1));
			configInput(POT1_INPUT + i, string::f("Pot %d CV", i + 1));
		}
		configTrim(this, ENT_CV_PARAM, "Entangle");
		configInput(ENT_CV_INPUT, "Entangle CV (±5 V = ±1)");
		configInput(INL_INPUT, "Left");
		configInput(INR_INPUT, "Right (normalled to left)");
		configInput(PROG_INPUT, "Program CV (0 to 10 V adds 0 to 15)");
		configInput(BANK_INPUT, "Bank CV (0 to 10 V adds 0 to 3)");
		configInput(BTN_INPUT, "Button gate");
		configInput(ENTIN_INPUT, "Entangle in (replaces the left neighbour)");
		configOutput(OUTL_OUTPUT, "Left");
		configOutput(OUTR_OUTPUT, "Right");
		configOutput(ENT_OUTPUT, "Entangle out (mono wet)");
		configBypass(INL_INPUT, OUTL_OUTPUT);
		configBypass(INR_INPUT, OUTR_OUTPUT);
		leftExpander.producerMessage = &leftMsg[0];
		leftExpander.consumerMessage = &leftMsg[1];
		lightDiv.setDivision(128);
		onSampleRateChange();
		applyNames(0);
	}

	void onSampleRateChange() override {
		float fs = APP->engine->getSampleRate();
		int p = core.prog;
		core.init(fs);
		core.prog = p;
		envA = 1.f - std::exp(-1.f / (0.005f * fs));
		envR = 1.f - std::exp(-1.f / (0.12f * fs));
		smA = 1.f - std::exp(-1.f / (0.01f * fs));
	}

	void applyNames(int prog) {
		const dens::TangleProgram& P = dens::TANGLE_PROGRAMS[prog];
		for (int i = 0; i < 6; i++) {
			ParamQuantity* q = getParamQuantity(POT1_PARAM + PS * i);
			bool used = P.kind[i] != dens::Kind::None;
			q->name = used ? string::f("Pot %d: %s", i + 1, P.pot[i]) : string::f("Pot %d (unused)", i + 1);
			q->defaultValue = potDefault(P.kind[i]);
		}
		getParamQuantity(BTN_PARAM)->name = P.btn == dens::Btn::None ? "Button (unused)" : std::string("Button: ") + P.btnName;
	}
	static float potDefault(dens::Kind k) {
		if (k == dens::Kind::Level) return 0.7f;
		if (k == dens::Kind::Feedback) return 0.4f;
		return 0.5f;
	}

	Tangle* rightTangle() { return rightExpander.module && rightExpander.module->model == modelTangle ? static_cast<Tangle*>(rightExpander.module) : nullptr; }
	bool leftIsTangle() { return leftExpander.module && leftExpander.module->model == modelTangle; }

	void process(const ProcessArgs& args) override {
		// program
		int kb = clamp((int)std::lround(params[BANK_PARAM].getValue()), 0, 3);
		int kp = clamp((int)std::lround(params[PROG_PARAM].getValue()), 0, 15);
		int knob = kb * 16 + kp;
		if (knob != knobProg) {
			if (knobProg >= 0 && loadDefaults)
				for (int i = 0; i < 6; i++) params[POT1_PARAM + PS * i].setValue(potDefault(dens::TANGLE_PROGRAMS[knob].kind[i]));
			knobProg = knob;
		}
		int bank = kb, prog = kp;
		if (inputs[BANK_INPUT].isConnected()) bank = clamp(kb + (int)std::floor(clamp(inputs[BANK_INPUT].getVoltage(), 0.f, 10.f) * 0.3999f), 0, 3);
		if (inputs[PROG_INPUT].isConnected()) prog = clamp(kp + (int)std::floor(clamp(inputs[PROG_INPUT].getVoltage(), 0.f, 10.f) * 1.5999f), 0, 15);
		int p = bank * 16 + prog;
		if (p != shownProg) {
			shownProg = p;
			applyNames(p);
			latched = false;
		}
		core.select(p);
		const dens::TangleProgram& P = dens::TANGLE_PROGRAMS[p];

		// button
		bool held = params[BTN_PARAM].getValue() > 0.5f || inputs[BTN_INPUT].getVoltage() >= 1.f;
		bool rise = btnTrig.process(held);
		if (P.btn == dens::Btn::Latch && rise) latched = !latched;
		btnState = P.btn == dens::Btn::Latch ? latched : (P.btn == dens::Btn::Momentary ? held : false);

		// entanglement source
		float es = 0.f;
		entSource = 0;
		if (inputs[ENTIN_INPUT].isConnected()) { es = inputs[ENTIN_INPUT].getVoltage(); entSource = 2; }
		else if (leftIsTangle()) {
			TangleMessage* m = (TangleMessage*)leftExpander.consumerMessage;
			if (m->valid) { es = m->ent; entSource = 1; }
		} else if (ring) {
			Tangle* t = this;
			for (int k = 0; k < 16 && t->rightTangle(); k++) t = t->rightTangle();
			if (t != this) { es = dens::softClamp(t->entOut); entSource = 3; }
		}
		float ent = clamp(params[ENT_PARAM].getValue() + cvAmt(this, ENT_CV_INPUT, ENT_CV_PARAM) / 5.f, -1.f, 1.f);
		float ae = std::fabs(es) / 5.f;
		env += (ae - env) * (ae > env ? envA : envR);
		env = std::min(env, 2.f);

		// pots
		float q[6];
		for (int i = 0; i < 6; i++) {
			float v = params[POT1_PARAM + PS * i].getValue() + cvAmt(this, POT1_INPUT + i, POT1_CV_PARAM + PS * i) / 10.f;
			q[i] = v;
		}
		if (ent < 0.f && entSource) {
			float d = -ent * 0.5f * std::min(env, 1.f);
			q[0] += d; q[1] -= d;
		}
		for (int i = 0; i < 6; i++) { qs[i] += (clamp(q[i], 0.f, 1.f) - qs[i]) * smA; }

		// audio
		float inL = inputs[INL_INPUT].getVoltage();
		float inR = inputs[INR_INPUT].isConnected() ? inputs[INR_INPUT].getVoltage() : inL;
		float x = 0.5f * (inL + inR) * params[INLV_PARAM].getValue();
		if (ent > 0.f && entSource) x += ent * es;
		float wl, wr;
		core.process(qs, btnState, dens::softClamp(x), wl, wr);
		float a, b;
		dens::eqPower(params[MIX_PARAM].getValue(), a, b);
		outputs[OUTL_OUTPUT].setVoltage(dens::softClamp(inL * a + wl * b));
		outputs[OUTR_OUTPUT].setVoltage(dens::softClamp(inR * a + wr * b));
		entOut = dens::softClamp(0.5f * (wl + wr));
		outputs[ENT_OUTPUT].setVoltage(entOut);

		// send to the right neighbour
		if (Tangle* t = rightTangle()) {
			TangleMessage* m = (TangleMessage*)t->leftExpander.producerMessage;
			m->ent = entOut; m->valid = true;
			t->leftExpander.requestMessageFlip();
		}

		if (lightDiv.process()) {
			lights[BTN_LIGHT].setBrightness(btnState ? 1.f : (P.btn == dens::Btn::None ? 0.f : 0.08f));
			entLevel = std::min(1.f, env);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "latched", json_boolean(latched));
		json_object_set_new(r, "ring", json_boolean(ring));
		json_object_set_new(r, "loadDefaults", json_boolean(loadDefaults));
		return r;
	}
	void dataFromJson(json_t* r) override {
		ring = jsonBool(r, "ring", false);
		loadDefaults = jsonBool(r, "loadDefaults", true);
		knobProg = clamp((int)std::lround(params[BANK_PARAM].getValue()), 0, 3) * 16 + clamp((int)std::lround(params[PROG_PARAM].getValue()), 0, 15);
		shownProg = knobProg;
		applyNames(shownProg);
		latched = jsonBool(r, "latched", false);
	}
	void onReset() override { latched = false; ring = false; knobProg = -1; }
};

struct TangleDisplay : DDisplay {
	Tangle* m = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		int p = m ? m->shownProg : 0;
		const dens::TangleProgram& P = dens::TANGLE_PROGRAMS[p];
		text(vg, 4.f, 3.f, string::f("%c%d", dens::TANGLE_BANK_NAMES[p / 16], p % 16), 12.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, acc());
		text(vg, 32.f, 3.f, P.name, 12.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		float rowH = 11.f;
		for (int i = 0; i < 6; i++) {
			float x = (i % 2) ? W * 0.5f + 2.f : 4.f;
			float y = 20.f + rowH * (i / 2);
			bool used = P.kind[i] != dens::Kind::None;
			float v = m ? m->params[Tangle::POT1_PARAM + Tangle::PS * i].getValue() : 0.5f;
			text(vg, x, y, string::f("%d %s", i + 1, used ? P.pot[i] : "—"), 8.5f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(used ? 0.75f : 0.35f));
			if (used) text(vg, x + W * 0.5f - 8.f, y, dens::formatKind(P.kind[i], v), 8.5f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		}
		float y = H - 12.f;
		std::string bt = P.btn == dens::Btn::None ? "BUTTON —" : string::f("BUTTON %s %s", P.btnName, (m && m->btnState) ? "ON" : "OFF");
		text(vg, 4.f, y, bt, 8.5f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(P.btn == dens::Btn::None ? 0.35f : 0.8f));
		static const char* src[4] = {"—", "LEFT", "JACK", "RING"};
		int s = m ? m->entSource : 0;
		text(vg, W - 34.f, y, string::f("ENT %s", src[s]), 8.5f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, ink(s ? 0.8f : 0.35f));
		bar(vg, W - 30.f, y + 2.f, 26.f, 5.f, m ? m->entLevel : 0.f, acc(0.9f));
	}
};

struct TangleWidget : ModuleWidget {
	TangleWidget(Tangle* module) {
		densScaffold(this, module, "Tangle");
		auto* d = new TangleDisplay;
		d->m = module;
		d->accent = FAMILY_ENTANGLE;
		d->setRectMm(TANGLE_DISPLAY_MM);
		addChild(d);
		placeTangle<Tangle>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Tangle* m = dynamic_cast<Tangle*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Entangle from rightmost core (on the leftmost core)", "", &m->ring));
		menu->addChild(createBoolPtrMenuItem("Changing program loads its default pots", "", &m->loadDefaults));
	}
};

Model* modelTangle = createModel<Tangle, TangleWidget>("Tangle");
