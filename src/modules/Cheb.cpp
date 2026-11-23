// CHEB (CHB): stereo Chebyshev (second kind) waveshaper. DSP in dsp/Cheb.hpp.
#include "Common.hpp"
#include "../dsp/Cheb.hpp"

static const char* const CHEB_FAMILIES[5] = {"SINGLE", "DECAY", "ODD/EVEN", "FORMANT", "RANDOM"};

struct Cheb : Module, ChebIds {
	dens::Cheb chb;
	dens::Cheb::Params p;
	float shownL = 9.f, shownR = 9.f;
	dsp::ClockDivider slow;

	Cheb() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SCAN_PARAM, 0.f, 39.f, 9.f, "Scan (shaper 0 to 39)");
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Mix", "%", 0.f, 100.f);
		configParam(SPREAD_PARAM, -8.f, 8.f, 0.f, "Spread (right channel scan offset)");
		configExp(this, DRIVE_PARAM, 0.1f, 4.f, 1.f, "Drive", "×");
		configParam(BIAS_PARAM, -0.5f, 0.5f, 0.f, "Bias (brings in even content)");
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configTrim(this, SCAN_CV_PARAM, "Scan");
		configTrim(this, SPREAD_CV_PARAM, "Spread");
		configTrim(this, DRIVE_CV_PARAM, "Drive");
		configTrim(this, BIAS_CV_PARAM, "Bias");
		configInput(SCAN_INPUT, "Scan CV (10 V = full range)");
		configInput(SPREAD_INPUT, "Spread CV (±5 V = ±8)");
		configInput(DRIVE_INPUT, "Drive CV (10 V = full range)");
		configInput(BIAS_INPUT, "Bias CV (±5 V = ±0.5)");
		configInput(INL_INPUT, "Left");
		configInput(INR_INPUT, "Right (normalled to left)");
		configOutput(OUTL_OUTPUT, "Left");
		configOutput(OUTR_OUTPUT, "Right");
		configBypass(INL_INPUT, OUTL_OUTPUT);
		configBypass(INR_INPUT, OUTR_OUTPUT);
		slow.setDivision(8);
		onSampleRateChange();
	}

	void onSampleRateChange() override { chb.setSampleRate(APP->engine->getSampleRate()); chb.reset(); }

	void process(const ProcessArgs& args) override {
		if (slow.process()) {
			p.scan = clamp(params[SCAN_PARAM].getValue() + cvAmt(this, SCAN_INPUT, SCAN_CV_PARAM) * 3.9f, 0.f, 39.f);
			p.spread = clamp(params[SPREAD_PARAM].getValue() + cvAmt(this, SPREAD_INPUT, SPREAD_CV_PARAM) * 1.6f, -8.f, 8.f);
			p.drive = dens::expRange(clamp(params[DRIVE_PARAM].getValue() + cvAmt(this, DRIVE_INPUT, DRIVE_CV_PARAM) / 10.f, 0.f, 1.f), 0.1f, 4.f);
			p.bias = clamp(params[BIAS_PARAM].getValue() + cvAmt(this, BIAS_INPUT, BIAS_CV_PARAM) * 0.1f, -0.5f, 0.5f);
			p.mix = params[MIX_PARAM].getValue();
			p.level = params[LEVEL_PARAM].getValue();
			shownL = p.scan;
			shownR = dens::Cheb::wrapScan(p.scan + p.spread);
		}
		float inL = inputs[INL_INPUT].getVoltage();
		float inR = inputs[INR_INPUT].isConnected() ? inputs[INR_INPUT].getVoltage() : inL;
		float oL, oR;
		chb.process(p, inL, inR, oL, oR);
		outputs[OUTL_OUTPUT].setVoltage(oL);
		outputs[OUTR_OUTPUT].setVoltage(oR);
	}
};

struct ChebDisplay : DDisplay {
	Cheb* m = nullptr;
	void curve(NVGcontext* vg, float s, float x0, float y0, float w, float h, NVGcolor col) {
		float wts[10];
		dens::chebWeights(s, wts);
		nvgBeginPath(vg);
		for (int i = 0; i <= 64; i++) {
			float x = -1.f + 2.f * i / 64.f;
			float y = clamp(dens::chebU(wts, x), -1.f, 1.f);
			float px = x0 + (x + 1.f) * 0.5f * w, py = y0 + (1.f - (y + 1.f) * 0.5f) * h;
			if (i == 0) nvgMoveTo(vg, px, py); else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, col);
		nvgStrokeWidth(vg, 1.2f);
		nvgStroke(vg);
	}
	void drawContents(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		float W = box.size.x, H = box.size.y;
		float sL = m ? m->shownL : 9.f, sR = m ? m->shownR : 9.f;
		int k = (int)std::lround(sL);
		text(vg, 4.f, 3.f, string::f("%02d %s", k, CHEB_FAMILIES[clamp(k / 8, 0, 4)]), 9.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, acc());
		float top = 15.f, h = H - top - 4.f;
		nvgBeginPath(vg); nvgMoveTo(vg, 4.f, top + h * 0.5f); nvgLineTo(vg, W - 4.f, top + h * 0.5f);
		nvgStrokeColor(vg, ink(0.15f)); nvgStrokeWidth(vg, 0.6f); nvgStroke(vg);
		curve(vg, sR, 4.f, top, W - 8.f, h, ink(0.45f));
		curve(vg, sL, 4.f, top, W - 8.f, h, acc());
	}
};

struct ChebWidget : ModuleWidget {
	ChebWidget(Cheb* module) {
		densScaffold(this, module, "Cheb");
		auto* d = new ChebDisplay;
		d->m = module;
		d->accent = FAMILY_EQUATION;
		d->setRectMm(CHEB_DISPLAY_MM);
		addChild(d);
		placeCheb<Cheb>(this, module);
	}
};

Model* modelCheb = createModel<Cheb, ChebWidget>("Cheb");
