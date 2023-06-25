#pragma once

#include "../synthLib/device.h"

namespace pluginLib
{
	class DummyDevice : public synthLib::Device
	{
	public:
		float getSamplerate() const override { return 44100.0f; }
		bool isValid() const override { return false; }
		bool getState(std::vector<uint8_t>& _state, synthLib::StateType _type) override { return false; }
		bool setState(const std::vector<uint8_t>& _state, synthLib::StateType _type) override { return false; }
		uint32_t getChannelCountIn() override { return 2; }
		uint32_t getChannelCountOut() override { return 2; }

	protected:
		void readMidiOut(std::vector<synthLib::SMidiEvent>& _midiOut) override {}
		void processAudio(const synthLib::TAudioInputs& _inputs, const synthLib::TAudioOutputs& _outputs, size_t _samples) override;
		bool sendMidi(const synthLib::SMidiEvent& _ev, std::vector<synthLib::SMidiEvent>& _response) override { return false; }
	};
}
