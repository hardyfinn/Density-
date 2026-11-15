// WEAVE (WVE): twelve complex modulators, four voltage-controlled gates, a four-input pan mixer.
#include "Common.hpp"
#include "../dsp/Weave.hpp"
#include <atomic>

struct Weave : Module, WeaveIds {
	static_assert(RATE2_PARAM - RATE1_PARAM == 2 && GLEV2_PARAM - GLEV1_PARAM == 2 && MLEV2_PARAM - MLEV1_PARAM == 2, "layout");
	static_assert(GIN2_INPUT - GIN1_INPUT == 2 && MIN2_INPUT - MIN1_INPUT == 2, "layout");

	dens::Weave wv;
	uint64_t seed = 1;
	bool gateNormals = true;
	std::atomic<bool> reseedPending{false};
	Trig resetTrig;
	float mods[12] = {};

	struct ShapeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			int a; float f;
			dens::ComplexMod::weights(getValue(), a, f);
			return f < 0.5f ? dens::WEAVE_SHAPE_NAMES[a] : dens::WEAVE_SHAPE_NAMES[a + 1];
		}
	};

	Weave() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < 12; i++) {
			std::string n = "Modulator " + std::to_string(i + 1) + " ";
			configExp(this, RATE1_PARAM + 2 * i, 0.005f, 50.f, 0.05f * std::pow(1.5f, (float)i), n + "rate", " Hz");
			configParam<ShapeQuantity>(SHAPE1_PARAM + 2 * i, 0.f, 1.f, 0.2f, n + "shape (sine, triangle, ramp, stepped, smooth, chaos, Lorenz)");
			configOutput(MOD1_OUTPUT + i, "Modulator " + std::to_string(i + 1));
		}
		configParam(COUPLE_PARAM, 0.f, 1.f, 0.2f, "Couple (each modulator FMs the next)", "%", 0.f, 100.f);
		configExp(this, SPEED_PARAM, 0.01f, 100.f, 1.f, "Speed (all rates)", "×");
		configSwitch(UNI_PARAM, 0.f, 1.f, 0.f, "Output range", {"Bipolar ±5 V", "Unipolar 0 to 10 V"});
		configInput(SPEED_INPUT, "Speed CV (1 V/oct on all rates)");
		configInput(RESET_INPUT, "Reset (phases to zero, random states reseeded)");
		configOutput(ALL_OUTPUT, "All modulators (12 channels)");
		static const char* resp[3] = {"VCA", "LPG", "Both"};
		for (int j = 0; j < 4; j++) {
			std::string n = "Gate " + std::to_string(j + 1) + " ";
			configParam(GLEV1_PARAM + 2 * j, 0.f, 1.f, 1.f, n + "level (CV amount)", "%", 0.f, 100.f);
			configSwitch(GRESP1_PARAM + 2 * j, 0.f, 2.f, 1.f, n + "response", {resp[0], resp[1], resp[2]});
			configInput(GIN1_INPUT + 2 * j, n + "in");
			configInput(GCV1_INPUT + 2 * j, n + string::f("CV (0 to 10 V, normalled to modulator %d)", 3 * (j + 1)));
			configOutput(GOUT1_OUTPUT + j, n + "out");
			static const float pans[4] = {-0.6f, -0.2f, 0.2f, 0.6f};
			std::string mn = "Mixer " + std::to_string(j + 1) + " ";
			configParam(MLEV1_PARAM + 2 * j, 0.f, 1.f, 0.7f, mn + "level", "%", 0.f, 100.f);
			configParam(MPAN1_PARAM + 2 * j, -1.f, 1.f, pans[j], mn + "pan");
			configInput(MIN1_INPUT + 2 * j, mn + "in (normalled from gate out)");
			configInput(MPCV1_INPUT + 2 * j, mn + "pan CV (±5 V = ±1)");
		}
		configOutput(MOUTL_OUTPUT, "Mix left");
		configOutput(MOUTR_OUTPUT, "Mix right");
		seed = random::u64() | 1;
		wv.reseed(seed);
		onSampleRateChange();
	}

	void onSampleRateChange() override { wv.setSampleRate(APP->engine->getSampleRate()); }

	void process(const ProcessArgs& args) override {
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage()) || reseedPending.exchange(false)) wv.reseed(seed);
		float speed = expVal(this, SPEED_PARAM, 0.01f, 100.f) * std::pow(2.f, clamp(inputs[SPEED_INPUT].getVoltage(), -10.f, 10.f));
		float rate[12], shape[12];
		for (int i = 0; i < 12; i++) {
			rate[i] = expVal(this, RATE1_PARAM + 2 * i, 0.005f, 50.f) * speed;
			shape[i] = params[SHAPE1_PARAM + 2 * i].getValue();
		}
		wv.processMods(rate, shape, params[COUPLE_PARAM].getValue(), mods);
		bool uni = params[UNI_PARAM].getValue() > 0.5f;
		outputs[ALL_OUTPUT].setChannels(12);
		for (int i = 0; i < 12; i++) {
			float v = uni ? 5.f * (mods[i] + 1.f) : 5.f * mods[i];
			outputs[MOD1_OUTPUT + i].setVoltage(v);
			outputs[ALL_OUTPUT].setVoltage(v, i);
		}
		float L = 0.f, R = 0.f;
		for (int j = 0; j < 4; j++) {
			float lev = params[GLEV1_PARAM + 2 * j].getValue();
			float g;
			if (inputs[GCV1_INPUT + 2 * j].isConnected()) g = inputs[GCV1_INPUT + 2 * j].getVoltage() / 10.f * lev;
			else if (gateNormals) g = 0.5f * (mods[3 * j + 2] + 1.f) * lev;
			else g = lev;
			auto resp = (dens::VcGate::Response)(int)params[GRESP1_PARAM + 2 * j].getValue();
			float y = wv.gate[j].process(inputs[GIN1_INPUT + 2 * j].getVoltage(), g, resp);
			outputs[GOUT1_OUTPUT + j].setVoltage(y);
			float m = inputs[MIN1_INPUT + 2 * j].isConnected() ? inputs[MIN1_INPUT + 2 * j].getVoltage() : y;
			m *= params[MLEV1_PARAM + 2 * j].getValue();
			float pan = clamp(params[MPAN1_PARAM + 2 * j].getValue() + inputs[MPCV1_INPUT + 2 * j].getVoltage() / 5.f, -1.f, 1.f);
			float th = (pan + 1.f) * 0.25f * M_PI;
			L += m * std::cos(th) * 1.4142f;
			R += m * std::sin(th) * 1.4142f;
		}
		outputs[MOUTL_OUTPUT].setVoltage(dens::softClamp(L));
		outputs[MOUTR_OUTPUT].setVoltage(dens::softClamp(R));
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "seed", json_string(std::to_string(seed).c_str()));
		json_object_set_new(r, "gateNormals", json_boolean(gateNormals));
		return r;
	}
	void dataFromJson(json_t* r) override {
		json_t* s = json_object_get(r, "seed");
		if (s && json_is_string(s)) { seed = std::stoull(json_string_value(s)) | 1; wv.reseed(seed); }
		gateNormals = jsonBool(r, "gateNormals", true);
	}
};

struct WeaveWidget : ModuleWidget {
	WeaveWidget(Weave* module) {
		densScaffold(this, module, "Weave");
		placeWeave<Weave>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Weave* m = dynamic_cast<Weave*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Gate CVs normalled to modulators 3, 6, 9, 12", "", &m->gateNormals));
		menu->addChild(createMenuItem("New random seed", "", [=]() { m->seed = random::u64() | 1; m->reseedPending = true; }));
	}
};

Model* modelWeave = createModel<Weave, WeaveWidget>("Weave");
