// NONET (NON): nine stereo effect algorithms. DSP in dsp/Nonet.hpp.
#include "Common.hpp"
#include "../dsp/Nonet.hpp"
#include <atomic>

struct Nonet : Module, NonetIds {
	static constexpr int PS = P2_PARAM - P1_PARAM;
	static_assert(PS == 2 && P1_CV_PARAM == P1_PARAM + 1, "P layout");

	dens::Nonet non;
	int mode = 0;
	int shownAlg = 2;
	int knobAlg = -1;
	bool sixMinutes = true;
	bool loadDefaults = true;
	// long buffers are allocated on the UI thread and handed to the audio thread here
	std::unique_ptr<dens::DelayLine16> stagedL, stagedR;
	std::atomic<int> staged{0};
	dsp::BooleanTrigger modeBtn, rstBtn;
	Trig modeTrig, rstTrig, clkTrig;
	float sinceClk = 0.f;
	dsp::ClockDivider lightDiv;
	float rstFlash = 0.f;

	struct PQuantity : ParamQuantity {
		int index = 0;
		Nonet* nm() { return static_cast<Nonet*>(module); }
		std::string getDisplayValueString() override {
			return nm() ? dens::Nonet::format(nm()->shownAlg, index, getValue()) : ParamQuantity::getDisplayValueString();
		}
	};
	struct AlgQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int a = clamp((int)std::lround(getValue()), 1, 9);
			return string::f("%d %s", a, dens::NONET_ALGS[a - 1].name);
		}
	};

	Nonet() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<AlgQuantity>(ALG_PARAM, 1.f, 9.f, 3.f, "Algorithm")->snapEnabled = true;
		configButton(MODE_PARAM, "Mode (cycles 1, 2, 3)");
		configButton(RST_PARAM, "Restart (clear buffers)");
		configParam(FILT_PARAM, -1.f, 1.f, 0.f, "Filter (lowpass to highpass tilt on the wet signal)");
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Mix", "%", 0.f, 100.f);
		for (int i = 0; i < 6; i++) {
			auto* q = configParam<PQuantity>(P1_PARAM + PS * i, 0.f, 1.f, 0.5f, string::f("P%d", i + 1));
			q->index = i;
			configTrim(this, P1_CV_PARAM + PS * i, string::f("P%d", i + 1));
			configInput(P1_INPUT + i, string::f("P%d CV", i + 1));
		}
		configTrim(this, MIX_CV_PARAM, "Mix");
		configInput(INL_INPUT, "Left");
		configInput(INR_INPUT, "Right (normalled to left)");
		configInput(ALG_INPUT, "Algorithm CV (0 to 10 V adds 0 to 8)");
		configInput(MODE_INPUT, "Mode trigger");
		configInput(CLK_INPUT, "Clock (LONG DELAY mode 2, and LFO sync)");
		configInput(RST_INPUT, "Restart trigger");
		configInput(MIX_INPUT, "Mix CV");
		configOutput(OUTL_OUTPUT, "Left");
		configOutput(OUTR_OUTPUT, "Right");
		configBypass(INL_INPUT, OUTL_OUTPUT);
		configBypass(INR_INPUT, OUTR_OUTPUT);
		non.init(48000.f);
		non.allocateLong(sixMinutes);
		non.select(2);
		applyNames(2);
		lightDiv.setDivision(128);
	}

	void onSampleRateChange() override {
		// keep the long buffers (their length is in samples); rebuild everything else
		auto l = std::move(non.longL), r = std::move(non.longR);
		bool ready = non.longReady;
		non.longReady = false;
		int a = non.alg;
		non.init(APP->engine->getSampleRate());
		non.longL = std::move(l); non.longR = std::move(r);
		non.longReady = ready;
		non.alg = a; non.mode = mode;
		non.clearAll();
	}

	// UI thread
	void requestLong(bool six) {
		if (staged.load() != 0) return;          // previous hand-over not taken yet
		stagedL.reset(); stagedR.reset();         // these are the old buffers from the last swap
		auto l = std::make_unique<dens::DelayLine16>(); l->init(six ? dens::LONG_SAMPLES_6MIN : dens::LONG_SAMPLES_1MIN);
		auto r = std::make_unique<dens::DelayLine16>(); r->init(six ? dens::LONG_SAMPLES_6MIN : dens::LONG_SAMPLES_1MIN);
		stagedL = std::move(l); stagedR = std::move(r);
		sixMinutes = six;
		staged.store(1);
	}

	void applyNames(int a) {
		for (int i = 0; i < 6; i++) {
			ParamQuantity* q = getParamQuantity(P1_PARAM + PS * i);
			q->name = string::f("P%d: %s", i + 1, dens::NONET_ALGS[a].p[i].name);
			q->defaultValue = dens::NONET_ALGS[a].p[i].def;
		}
	}

	void process(const ProcessArgs& args) override {
		if (staged.load() == 1) {
			std::swap(non.longL, stagedL);
			std::swap(non.longR, stagedR);
			non.longClearLeft = 0;
			non.longReady = true;
			staged.store(0);
		}
		// algorithm: knob plus CV
		int ka = clamp((int)std::lround(params[ALG_PARAM].getValue()), 1, 9) - 1;
		if (ka != knobAlg) {
			if (knobAlg >= 0 && loadDefaults)
				for (int i = 0; i < 6; i++) params[P1_PARAM + PS * i].setValue(dens::NONET_ALGS[ka].p[i].def);
			knobAlg = ka;
		}
		int a = ka;
		if (inputs[ALG_INPUT].isConnected()) a = clamp(ka + (int)std::floor(clamp(inputs[ALG_INPUT].getVoltage(), 0.f, 10.f) * 0.8999f), 0, 8);
		if (a != shownAlg) { shownAlg = a; applyNames(a); }
		non.select(a);

		bool mb = modeBtn.process(params[MODE_PARAM].getValue() > 0.5f);
		bool mt = modeTrig.process(inputs[MODE_INPUT].getVoltage());
		if (mb || mt) mode = (mode + 1) % 3;
		non.mode = mode;
		bool rb = rstBtn.process(params[RST_PARAM].getValue() > 0.5f);
		bool rt = rstTrig.process(inputs[RST_INPUT].getVoltage());
		if (rb || rt) { non.restart(); rstFlash = 1.f; }

		// clock period
		sinceClk += args.sampleTime;
		if (inputs[CLK_INPUT].isConnected()) {
			if (clkTrig.process(inputs[CLK_INPUT].getVoltage())) {
				if (sinceClk < 10.f) non.clockPeriod = sinceClk;
				sinceClk = 0.f;
			}
			if (sinceClk > 10.f) non.clockPeriod = 0.f;
		} else non.clockPeriod = 0.f;

		dens::Nonet::Frame f;
		for (int i = 0; i < 6; i++) {
			float v = params[P1_PARAM + PS * i].getValue() + cvAmt(this, P1_INPUT + i, P1_CV_PARAM + PS * i) / 10.f;
			f.p[i] = clamp(v, 0.f, 1.f);
		}
		// LFO sync: with a clock patched, P1 RATE of the LFO algorithms picks a multiple of the clock rate
		if (non.clockPeriod > 0.f && (non.alg == 1 || non.alg == 4 || non.alg == 5)) {
			int idx = clamp((int)std::lround(f.p[0] * 14.f), 0, 14);
			float hz = dens::CLOCK_RATIOS[idx] / non.clockPeriod;
			f.p[0] = clamp(std::log(clamp(hz, 0.02f, 12.f) / 0.02f) / std::log(600.f), 0.f, 1.f);
		}
		f.filter = params[FILT_PARAM].getValue();
		f.mix = clamp(params[MIX_PARAM].getValue() + cvAmt(this, MIX_INPUT, MIX_CV_PARAM) / 10.f, 0.f, 1.f);
		float inL = inputs[INL_INPUT].getVoltage();
		float inR = inputs[INR_INPUT].isConnected() ? inputs[INR_INPUT].getVoltage() : inL;
		float oL, oR;
		non.process(f, inL, inR, oL, oR);
		outputs[OUTL_OUTPUT].setVoltage(oL);
		outputs[OUTR_OUTPUT].setVoltage(oR);

		if (lightDiv.process()) {
			float dt = args.sampleTime * lightDiv.getDivision();
			for (int i = 0; i < 3; i++) lights[MODE1_LIGHT + i].setBrightness(mode == i ? 1.f : 0.f);
			lights[MODE_LIGHT].setBrightness(params[MODE_PARAM].getValue());
			rstFlash = std::max(0.f, rstFlash - dt * 5.f);
			lights[RST_LIGHT].setBrightness(rstFlash);
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "mode", json_integer(mode));
		json_object_set_new(r, "sixMinutes", json_boolean(sixMinutes));
		json_object_set_new(r, "loadDefaults", json_boolean(loadDefaults));
		return r;
	}
	void dataFromJson(json_t* r) override {
		mode = clamp(jsonInt(r, "mode", 0), 0, 2);
		loadDefaults = jsonBool(r, "loadDefaults", true);
		bool six = jsonBool(r, "sixMinutes", true);
		if (six != sixMinutes) requestLong(six);
		// a loaded patch keeps its saved knob positions
		knobAlg = clamp((int)std::lround(params[ALG_PARAM].getValue()), 1, 9) - 1;
	}
	void onReset() override { mode = 0; knobAlg = -1; }
};

struct NonetDisplay : DDisplay {
	Nonet* m = nullptr;
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		int a = m ? m->shownAlg : 2;
		int mode = m ? m->mode : 0;
		const dens::NonetAlgDef& d = dens::NONET_ALGS[a];
		text(vg, 4.f, 3.f, string::f("%d %s", a + 1, d.name), 11.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, acc());
		text(vg, W - 4.f, 3.f, string::f("%d %s", mode + 1, d.modes[mode]), 10.f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		float rowH = (H - 30.f) / 3.f;
		for (int i = 0; i < 6; i++) {
			float x = (i % 2) ? W * 0.5f + 2.f : 4.f;
			float y = 18.f + rowH * (i / 2);
			float v = m ? m->params[Nonet::P1_PARAM + Nonet::PS * i].getValue() : d.p[i].def;
			text(vg, x, y, string::f("P%d %s", i + 1, d.p[i].name), 8.5f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(0.75f));
			text(vg, x + W * 0.5f - 8.f, y, dens::Nonet::format(a, i, v), 8.5f, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		}
		float by = H - 9.f;
		if (!m) return;
		if (a == 0) {
			float s = m->non.longMaxSeconds();
			std::string t = s > 0.f ? string::f("BUFFER %d:%02d", (int)s / 60, (int)s % 60) : "BUFFER LOADING";
			text(vg, 4.f, by - 2.f, t, 8.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, ink(0.6f));
		} else if (a >= 5 && a <= 7) {
			const auto& sb = m->non.stretch;
			bool rec = sb.state == dens::StretchBuffer::RECORDING, loop = sb.state == dens::StretchBuffer::LOOPING;
			std::string t = rec ? "REC" : (loop ? "LOOP" : "LIVE");
			text(vg, 4.f, by - 2.f, t, 8.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, rec ? nvgRGB(0xe0, 0x50, 0x40) : ink(0.7f));
			bar(vg, 30.f, by, W - 34.f, 4.f, loop ? sb.position() : sb.progress(), acc(0.9f));
		}
	}
};

struct NonetWidget : ModuleWidget {
	NonetWidget(Nonet* module) {
		densScaffold(this, module, "Nonet");
		auto* d = new NonetDisplay;
		d->m = module;
		d->accent = FAMILY_HARMONIC;
		d->setRectMm(NONET_DISPLAY_MM);
		addChild(d);
		placeNonet<Nonet>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Nonet* m = dynamic_cast<Nonet*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Long delay buffer", {"6 min (about 70 MB)", "1 min"},
			[=]() { return m->sixMinutes ? 0 : 1; }, [=](int i) { m->requestLong(i == 0); }));
		menu->addChild(createBoolPtrMenuItem("Turning ALGORITHM loads its default P1 to P6", "", &m->loadDefaults));
		menu->addChild(createMenuItem("Set P1 to P6 to this algorithm's defaults", "", [=]() {
			for (int i = 0; i < 6; i++) m->params[Nonet::P1_PARAM + Nonet::PS * i].setValue(dens::NONET_ALGS[m->shownAlg].p[i].def);
		}));
	}
};

Model* modelNonet = createModel<Nonet, NonetWidget>("Nonet");
