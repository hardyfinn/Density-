// TRACE (TRC): eight gestural CV recorders. DSP in dsp/Trace.hpp.
#include "Common.hpp"
#include "../dsp/Trace.hpp"

struct Trace : Module, TraceIds {
	static constexpr int LS = SRC2_PARAM - SRC1_PARAM;
	static_assert(LS == 5, "lane layout");

	dens::Trace tr;
	bool saveRecordings = true;
	dsp::BooleanTrigger rphBtn, clrBtn;
	Trig rphTrig, clrTrig;
	dsp::ClockDivider lightDiv;
	float blink = 0.f;
	int lastWraps[8] = {};
	float pulse[8] = {};

	struct SpeedQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.2f×", std::pow(2.f, getValue())); }
	};

	Trace() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < 8; i++) {
			std::string n = "Lane " + std::to_string(i + 1) + " ";
			int b = SRC1_PARAM + LS * i;
			configParam(b + 0, -5.f, 5.f, 0.f, n + "source (recorded when IN is unpatched)", " V");
			configButton(b + 1, n + "REC: press to wake or sleep, hold to record, release to loop, double press to clear");
			configParam(b + 2, -5.f, 5.f, 0.f, n + "offset", " V");
			configParam(b + 3, -1.f, 1.f, 1.f, n + "atten", "%", 0.f, 100.f);
			configParam<SpeedQuantity>(b + 4, -2.f, 2.f, 0.f, n + "speed");
			configInput(CV1_INPUT + i, n + "IN (recorded instead of SOURCE)");
			configOutput(OUT1_OUTPUT + i, n + "out");
		}
		configSwitch(SLOW_PARAM, 0.f, 3.f, 0.f, "Slow-down", {"1×", "2×", "4×", "8×"});
		configButton(SUB_PARAM, "Sub loop (random 1/8 to 1/2 of each loop)");
		configButton(RPH_PARAM, "Random phase");
		configButton(CLR_PARAM, "Clear all lanes");
		configInput(RPH_INPUT, "Random phase trigger");
		configInput(CLR_INPUT, "Clear all trigger");
		configInput(RECG_INPUT, "REC gates (poly: channel i holds lane i's REC)");
		configOutput(ALL_OUTPUT, "All lanes (8 channels)");
		tr.init(random::u64());
		lightDiv.setDivision(64);
	}

	void process(const ProcessArgs& args) override {
		float dt = args.sampleTime;
		bool rphB = rphBtn.process(params[RPH_PARAM].getValue() > 0.5f);
		bool rph = rphTrig.process(inputs[RPH_INPUT].getVoltage()) || rphB;
		bool clrB = clrBtn.process(params[CLR_PARAM].getValue() > 0.5f);
		bool clr = clrTrig.process(inputs[CLR_INPUT].getVoltage()) || clrB;
		if (clr) for (auto& l : tr.lane) l.clear();
		bool sub = params[SUB_PARAM].getValue() > 0.5f;
		float slow = (float)(1 << (int)params[SLOW_PARAM].getValue());
		int gch = inputs[RECG_INPUT].getChannels();
		outputs[ALL_OUTPUT].setChannels(8);
		for (int i = 0; i < 8; i++) {
			dens::TraceLane& l = tr.lane[i];
			int b = SRC1_PARAM + LS * i;
			bool pressed = params[b + 1].getValue() > 0.5f || (i < gch && inputs[RECG_INPUT].getVoltage(i) >= 1.f);
			l.control(pressed, dt);
			if (rph) l.randomPhase();
			float src = inputs[CV1_INPUT + i].isConnected() ? inputs[CV1_INPUT + i].getVoltage() : params[b].getValue();
			float sv = params[b + 4].getValue();
			if (std::fabs(sv) < 0.06f) sv = 0.f;   // detent at 1×
			float raw = l.process(src, std::pow(2.f, sv) / slow, sub, args.sampleRate);
			float out = l.state == dens::TraceLane::ASLEEP ? 0.f : dens::softClamp(raw * params[b + 3].getValue() + params[b + 2].getValue());
			outputs[OUT1_OUTPUT + i].setVoltage(out);
			outputs[ALL_OUTPUT].setVoltage(out, i);
		}
		if (lightDiv.process()) {
			float ldt = dt * lightDiv.getDivision();
			blink += ldt * 4.f; blink -= std::floor(blink);
			for (int i = 0; i < 8; i++) {
				const dens::TraceLane& l = tr.lane[i];
				if (l.wraps != lastWraps[i]) { lastWraps[i] = l.wraps; pulse[i] = 1.f; }
				pulse[i] = std::max(0.f, pulse[i] - ldt * 4.f);
				float r = 0.f, g = 0.f, bl = 0.f;
				switch (l.state) {
					case dens::TraceLane::ASLEEP: break;
					case dens::TraceLane::AWAKE: r = g = bl = 0.25f; break;
					case dens::TraceLane::RECORDING: r = blink < 0.5f ? 1.f : 0.15f; break;
					case dens::TraceLane::LOOPING: g = 0.45f + 0.55f * pulse[i]; break;
				}
				lights[REC1_LIGHT + 3 * i].setBrightness(r);
				lights[REC1_LIGHT + 3 * i + 1].setBrightness(g);
				lights[REC1_LIGHT + 3 * i + 2].setBrightness(bl);
			}
			lights[SUB_LIGHT].setBrightness(sub ? 1.f : 0.f);
			lights[RPH_LIGHT].setBrightness(params[RPH_PARAM].getValue());
			lights[CLR_LIGHT].setBrightness(params[CLR_PARAM].getValue());
		}
	}

	void onReset() override { for (auto& l : tr.lane) l.clear(); }

	// Buffers saved as 16-bit integers (±12 V full scale), base64.
	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "saveRecordings", json_boolean(saveRecordings));
		if (!saveRecordings) return r;
		json_t* lanes = json_array();
		for (int i = 0; i < 8; i++) {
			const dens::TraceLane& l = tr.lane[i];
			json_t* o = json_object();
			int st = (int)l.state;
			if (st == dens::TraceLane::RECORDING) st = dens::TraceLane::LOOPING;   // a take in progress reopens as a loop
			json_object_set_new(o, "state", json_integer(st));
			int n = l.len;
			std::vector<uint8_t> bytes(2 * (size_t)n);
			for (int k = 0; k < n; k++) {
				int16_t q = (int16_t)clamp((int)std::lround(l.buf[k] / 12.f * 32767.f), -32767, 32767);
				bytes[2 * k] = (uint8_t)(q & 0xff);
				bytes[2 * k + 1] = (uint8_t)((q >> 8) & 0xff);
			}
			json_object_set_new(o, "len", json_integer(n));
			json_object_set_new(o, "data", json_string(string::toBase64(bytes.data(), bytes.size()).c_str()));
			json_array_append_new(lanes, o);
		}
		json_object_set_new(r, "lanes", lanes);
		return r;
	}
	void dataFromJson(json_t* r) override {
		saveRecordings = jsonBool(r, "saveRecordings", true);
		json_t* lanes = json_object_get(r, "lanes");
		for (int i = 0; i < 8; i++) tr.lane[i].clear();
		if (!lanes || !json_is_array(lanes)) return;
		for (int i = 0; i < 8 && i < (int)json_array_size(lanes); i++) {
			json_t* o = json_array_get(lanes, i);
			dens::TraceLane& l = tr.lane[i];
			int n = clamp(jsonInt(o, "len", 0), 0, dens::TraceLane::MAX);
			json_t* d = json_object_get(o, "data");
			std::vector<uint8_t> bytes;
			if (d && json_is_string(d)) bytes = string::fromBase64(json_string_value(d));
			n = std::min(n, (int)(bytes.size() / 2));
			for (int k = 0; k < n; k++) {
				int16_t q = (int16_t)(bytes[2 * k] | (bytes[2 * k + 1] << 8));
				l.buf[k] = q / 32767.f * 12.f;
			}
			l.len = n;
			int st = jsonInt(o, "state", 0);
			if (st == dens::TraceLane::LOOPING && n >= 2) l.beginLoop();
			else if (st == dens::TraceLane::AWAKE) l.state = dens::TraceLane::AWAKE;
			else l.state = dens::TraceLane::ASLEEP;
		}
	}
};

struct TraceWidget : ModuleWidget {
	TraceWidget(Trace* module) {
		densScaffold(this, module, "Trace");
		placeTrace<Trace>(this, module);
	}
	void appendContextMenu(Menu* menu) override {
		Trace* m = dynamic_cast<Trace*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("Save recordings with patch", "", &m->saveRecordings));
	}
};

Model* modelTrace = createModel<Trace, TraceWidget>("Trace");
