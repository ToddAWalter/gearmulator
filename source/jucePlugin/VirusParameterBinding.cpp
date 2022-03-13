#include "VirusParameterBinding.h"
#include "VirusParameter.h"
#include "PluginProcessor.h"

class Parameter;

VirusParameterBinding::~VirusParameterBinding()
{
	clearBindings();
}
void VirusParameterBinding::clearBindings() {
	for (const auto b : m_bindings)
	{
		b->onValueChanged = nullptr;
	}
	m_bindings.clear();
}
void VirusParameterBinding::setPart(uint8_t _part)
{
	clearBindings();
	m_processor.getController().setCurrentPart(_part);
}
void VirusParameterBinding::bind(juce::Slider &_slider, Virus::ParameterType _param)
{
	bind(_slider, _param, m_processor.getController().getCurrentPart());
}
void VirusParameterBinding::bind(juce::Slider &_slider, Virus::ParameterType _param, uint8_t part)
{
	const auto v = m_processor.getController().getParameter(_param, part);
	if (!v)
	{
		assert(false && "Failed to find parameter");
		return;
	}
	_slider.addMouseListener(new VirusParameterBindingMouseListener(v, _slider), false);
	const auto range = v->getNormalisableRange();
	_slider.setRange(range.start, range.end, range.interval);
	_slider.setDoubleClickReturnValue(true, v->convertFrom0to1(v->getDefaultValue()));
	_slider.getValueObject().referTo(v->getValueObject());
	_slider.getProperties().set("type", "slider");
	_slider.getProperties().set("name", v->getDescription().name);
	if (v->isBipolar()) {
		_slider.getProperties().set("bipolar", true);
	}
}
void VirusParameterBinding::bind(juce::ComboBox& _combo, Virus::ParameterType _param) {
	bind(_combo, _param, m_processor.getController().getCurrentPart());
}
void VirusParameterBinding::bind(juce::ComboBox& _combo, Virus::ParameterType _param, uint8_t _part)
{
	const auto v = m_processor.getController().getParameter(_param, _part);
	if (!v)
	{
		assert(false && "Failed to find parameter");
		return;
	}
	_combo.setTextWhenNothingSelected("--");
	_combo.setScrollWheelEnabled(true);
	int idx = 1;
	for (auto vs : v->getAllValueStrings()) {
		if(vs.isNotEmpty())
			_combo.addItem(vs, idx);
		idx++;
	}
	//_combo.addItemList(v->getAllValueStrings(), 1);
	_combo.setSelectedId((int)v->getValueObject().getValueSource().getValue() + 1, juce::dontSendNotification);
	_combo.onChange = [this, &_combo, v]() {
		v->beginChangeGesture();
		v->setValueNotifyingHost(v->convertTo0to1(static_cast<float>(_combo.getSelectedId() - 1)));
		v->endChangeGesture();
		v->getValueObject().getValueSource().setValue((int)_combo.getSelectedId() - 1);
	};
	v->onValueChanged = [this, &_combo, v]() { _combo.setSelectedId((int)v->getValueObject().getValueSource().getValue() + 1, juce::dontSendNotification); };
	m_bindings.add(v);
}

void VirusParameterBinding::bind(juce::DrawableButton &_btn, Virus::ParameterType _param)
{
	const auto v = m_processor.getController().getParameter(_param, m_processor.getController().getCurrentPart());
	if (!v)
	{
		assert(false && "Failed to find parameter");
		return;
	}
	_btn.getToggleStateValue().referTo(v->getValueObject());
}
