// Helpers shared by the Rack wrappers.
#pragma once
#include "../plugin.hpp"
#include "../generated/Layout.hpp"
#include "../dsp/Common.hpp"

// Exponential knob: stored value 0..1, shown as lo·(hi/lo)^v in `unit`.
inline ParamQuantity* configExp(Module* m, int id, float lo, float hi, float def, const std::string& name, const std::string& unit, float mult = 1.f) {
	return m->configParam(id, 0.f, 1.f, dens::expNorm(def, lo, hi), name, unit, hi / lo, lo * mult);
}
inline float expVal(Module* m, int id, float lo, float hi) { return dens::expRange(m->params[id].getValue(), lo, hi); }

// CV through its trimpot: volts × trim (−1..1).
inline float cvAmt(Module* m, int input, int trim, int ch = 0) {
	if (!m->inputs[input].isConnected()) return 0.f;
	return m->inputs[input].getPolyVoltage(ch) * m->params[trim].getValue();
}

inline ParamQuantity* configTrim(Module* m, int id, const std::string& name, float def = 1.f) {
	return m->configParam(id, -1.f, 1.f, def, name + " CV amount", "%", 0.f, 100.f);
}

// Schmitt trigger with the plugin's thresholds: rise through +1 V, re-arm below +0.1 V.
struct Trig {
	bool high = false;
	bool process(float v) {
		if (high) { if (v < 0.1f) high = false; return false; }
		if (v >= 1.f) { high = true; return true; }
		return false;
	}
};

// json helpers
inline float jsonFloat(json_t* root, const char* key, float def) {
	json_t* j = json_object_get(root, key);
	return j ? (float)json_number_value(j) : def;
}
inline int jsonInt(json_t* root, const char* key, int def) {
	json_t* j = json_object_get(root, key);
	return j ? (int)json_integer_value(j) : def;
}
inline bool jsonBool(json_t* root, const char* key, bool def) {
	json_t* j = json_object_get(root, key);
	return j ? json_is_true(j) : def;
}
inline json_t* jsonFloats(const float* v, int n) {
	json_t* a = json_array();
	for (int i = 0; i < n; i++) json_array_append_new(a, json_real(v[i]));
	return a;
}
inline void readFloats(json_t* root, const char* key, float* v, int n) {
	json_t* a = json_object_get(root, key);
	if (!a || !json_is_array(a)) return;
	for (int i = 0; i < n && i < (int)json_array_size(a); i++) v[i] = (float)json_number_value(json_array_get(a, i));
}

// Snapped knob whose tooltip shows a name from a table.
struct NamedQuantity : ParamQuantity {
	const char* const* names = nullptr;
	int count = 0, offset = 0;
	std::string getDisplayValueString() override {
		int i = (int)std::lround(getValue()) - offset;
		if (names && i >= 0 && i < count) return names[i];
		return ParamQuantity::getDisplayValueString();
	}
};
template <class Q = NamedQuantity>
inline Q* configNamed(Module* m, int id, float lo, float hi, float def, const std::string& name, const char* const* names, int count, int offset = 0) {
	Q* q = m->configParam<Q>(id, lo, hi, def, name);
	q->snapEnabled = true;
	q->names = names; q->count = count; q->offset = offset;
	return q;
}
