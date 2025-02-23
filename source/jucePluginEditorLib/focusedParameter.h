#pragma once

#include "focusedParameterTooltip.h"

#include "juce_events/juce_events.h"

namespace juce
{
	class MouseEvent;
}

namespace genericUI
{
	class Editor;
}

namespace pluginLib
{
	class Parameter;
	class Controller;
	class ParameterBinding;
}

namespace jucePluginEditorLib
{
	class FocusedParameter : juce::Timer
	{
	public:
		FocusedParameter(const pluginLib::Controller& _controller, const pluginLib::ParameterBinding& _parameterBinding, const genericUI::Editor& _editor);
		~FocusedParameter() override;

		void onMouseEnter(const juce::MouseEvent& _event);

		virtual void updateParameter(const std::string& _name, const std::string& _value);

	private:
		void timerCallback() override;
		void updateControlLabel(juce::Component* _component);

		const pluginLib::ParameterBinding& m_parameterBinding;
		const pluginLib::Controller& m_controller;

		juce::Label* m_focusedParameterName = nullptr;
		juce::Label* m_focusedParameterValue = nullptr;
		std::unique_ptr<FocusedParameterTooltip> m_tooltip;
		std::vector<pluginLib::Parameter*> m_boundParameters;
	};
}
