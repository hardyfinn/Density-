// Knobs, jacks, sliders, toggles, lights and the display base. SVGs come from scripts/make_panels.py.
#pragma once
#include "../plugin.hpp"

inline std::shared_ptr<window::Svg> densSvg(const std::string& name) {
	return window::Svg::load(asset::plugin(pluginInstance, "res/components/" + name + ".svg"));
}

struct DKnobBase : app::SvgKnob {
	DKnobBase() {
		minAngle = -0.75f * M_PI;
		maxAngle = 0.75f * M_PI;
		shadow->opacity = 0.2f;
	}
};
struct DKnob : DKnobBase { DKnob() { setSvg(densSvg("DKnob")); } };
struct DKnobSmall : DKnobBase { DKnobSmall() { setSvg(densSvg("DKnobSmall")); } };
struct DTrim : DKnobBase { DTrim() { setSvg(densSvg("DTrim")); shadow->opacity = 0.1f; } };

struct PortAudio : app::SvgPort { PortAudio() { setSvg(densSvg("PortAudio")); } };
struct PortCv : app::SvgPort { PortCv() { setSvg(densSvg("PortCv")); } };
struct PortGate : app::SvgPort { PortGate() { setSvg(densSvg("PortGate")); } };
struct PortPoly : app::SvgPort { PortPoly() { setSvg(densSvg("PortPoly")); } };

// Value 0 is the top position, which is the first printed label.
struct DToggle2 : app::SvgSwitch {
	DToggle2() {
		addFrame(densSvg("Toggle2_0"));
		addFrame(densSvg("Toggle2_1"));
		shadow->opacity = 0.f;
	}
};
struct DToggle3 : app::SvgSwitch {
	DToggle3() {
		addFrame(densSvg("Toggle3_0"));
		addFrame(densSvg("Toggle3_1"));
		addFrame(densSvg("Toggle3_2"));
		shadow->opacity = 0.f;
	}
};

// Vertical fader, 22 mm travel. Top is the maximum.
struct DSlider : app::SvgSlider {
	DSlider() {
		setBackgroundSvg(densSvg("SliderBg"));
		setHandleSvg(densSvg("SliderHandle"));
		// handle box top-left positions: max at the top of the travel, min at the bottom
		maxHandlePos = mm2px(Vec(0.f, 0.f));
		minHandlePos = mm2px(Vec(0.f, 22.f));
		background->box.pos = Vec(0, 0);
		box.size = background->box.size;
	}
};

// Warm white LED.
struct DLight : GrayModuleLightWidget {
	DLight() { addBaseColor(nvgRGB(0xff, 0xf2, 0xd8)); }
};

// Panel and screws. Placement of controls comes from generated/Layout.hpp.
template <class W>
inline void densScaffold(W* w, Module* m, const std::string& slug) {
	w->setModule(m);
	w->setPanel(createPanel(asset::plugin(pluginInstance, "res/" + slug + ".svg")));
	float width = w->box.size.x;
	w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	if (width > 6 * RACK_GRID_WIDTH) {
		w->addChild(createWidget<ScrewBlack>(Vec(width - 2 * RACK_GRID_WIDTH, 0)));
		w->addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}
}

// Display: ground drawn normally, contents in layer 1 so they glow when the room lights dim.
struct DDisplay : widget::Widget {
	NVGcolor accent = nvgRGB(0xc9, 0x8a, 0x1b);
	void setRectMm(const float* mm) {
		box.pos = mm2px(Vec(mm[0], mm[1]));
		box.size = mm2px(Vec(mm[2], mm[3]));
	}
	static NVGcolor ink(float a = 1.f) { return nvgRGBA(0xe6, 0xe2, 0xd3, (unsigned char)(255 * math::clamp(a, 0.f, 1.f))); }
	NVGcolor acc(float a = 1.f) const { NVGcolor c = accent; c.a = math::clamp(a, 0.f, 1.f); return c; }
	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
		nvgFillColor(args.vg, nvgRGB(0x15, 0x17, 0x1a));
		nvgFill(args.vg);
		Widget::draw(args);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			nvgSave(args.vg);
			nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
			drawContents(args);
			nvgRestore(args.vg);
		}
		Widget::drawLayer(args, layer);
	}
	virtual void drawContents(const DrawArgs& args) {}
	bool setFont(NVGcontext* vg, float size) {
		std::shared_ptr<window::Font> font = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!font || font->handle < 0) return false;
		nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, size);
		return true;
	}
	void text(NVGcontext* vg, float x, float y, const std::string& s, float size, int align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP, NVGcolor col = ink()) {
		if (!setFont(vg, size)) return;
		nvgFillColor(vg, col);
		nvgTextAlign(vg, align);
		nvgText(vg, x, y, s.c_str(), NULL);
	}
	void bar(NVGcontext* vg, float x, float y, float w, float h, float frac, NVGcolor col) {
		nvgBeginPath(vg); nvgRect(vg, x, y, w, h); nvgFillColor(vg, ink(0.15f)); nvgFill(vg);
		nvgBeginPath(vg); nvgRect(vg, x, y, w * math::clamp(frac, 0.f, 1.f), h); nvgFillColor(vg, col); nvgFill(vg);
	}
};

static const NVGcolor FAMILY_HARMONIC = nvgRGB(0xe0, 0xa0, 0x32);
static const NVGcolor FAMILY_ENTANGLE = nvgRGB(0x3c, 0xb0, 0xab);
static const NVGcolor FAMILY_EQUATION = nvgRGB(0x8f, 0x7d, 0xd6);
