#include "focusedParameterTooltip.h"

#include "juce_gui_basics/juce_gui_basics.h"

namespace jucePluginEditorLib
{
	FocusedParameterTooltip::FocusedParameterTooltip(juce::Label* _label) : m_label(_label), m_defaultWidth(_label ? _label->getWidth() : 0)
	{
		setVisible(false);
	}

	void FocusedParameterTooltip::setVisible(bool _visible) const
	{
		if (isValid())
			m_label->setVisible(_visible);
	}

	void FocusedParameterTooltip::initialize(juce::Component* _component, const juce::String& _value) const
	{
		if (!isValid())
			return;

		if(dynamic_cast<juce::Slider*>(_component) && _component->isShowing())
		{
			int x = _component->getX();
			int y = _component->getY();

			// local to global
			auto parent = _component->getParentComponent();

			while(parent && parent != m_label->getParentComponent())
			{
				x += parent->getX();
				y += parent->getY();
				parent = parent->getParentComponent();
			}

			int labelWidth = m_defaultWidth;

			if(_value.length() > 3)
				labelWidth += (m_label->getHeight()>>1) * (_value.length() - 3);

			x += (_component->getWidth()>>1) - (labelWidth>>1);
			y += _component->getHeight() + (m_label->getHeight()>>1);

			/*
			// global to local of tooltip parent
			parent = m_label->getParentComponent();

			while(parent && parent != this)
			{
				x -= parent->getX();
				y -= parent->getY();
				parent = parent->getParentComponent();
			}
			*/
			if(m_label->getProperties().contains("offsetY"))
				y += static_cast<int>(m_label->getProperties()["offsetY"]);

			m_label->setTopLeftPosition(x,y);
			m_label->setSize(labelWidth, m_label->getHeight());
			m_label->setText(_value, juce::dontSendNotification);
			m_label->setVisible(true);
			m_label->toFront(false);
		}
		else if(m_label)
		{
			m_label->setVisible(false);
		}
	}
}
