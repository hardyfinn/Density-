#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelHarmonia);
	p->addModel(modelTrace);
	p->addModel(modelNonet);
	p->addModel(modelTangle);
	p->addModel(modelWeave);
	p->addModel(modelAxiom);
	p->addModel(modelTetra);
	p->addModel(modelCheb);
}
