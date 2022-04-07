#pragma once

#include <mutex>

#include "../synthLib/midiTypes.h"
#include "../synthLib/resamplerInOut.h"

#include "../dsp56300/source/dsp56kEmu/ringbuffer.h"

#include "deviceTypes.h"

namespace synthLib
{
	class Device;

	class Plugin
	{
	public:
		Plugin(Device* _device);

		void addMidiEvent(const SMidiEvent& _ev);

		void setSamplerate(float _samplerate);
		void setBlockSize(uint32_t _blockSize);

		uint32_t getLatencyMidiToOutput() const;
		uint32_t getLatencyInputToOutput() const;

		void process(float** _inputs, float** _outputs, size_t _count, float _bpm, float _ppqPos, bool _isPlaying);
		void getMidiOut(std::vector<SMidiEvent>& _midiOut);

		bool isValid() const;

		bool getState(std::vector<uint8_t>& _state, StateType _type) const;
		bool setState(const std::vector<uint8_t>& _state);

		void insertMidiEvent(const SMidiEvent& _ev);

	private:
		void processMidiClock(float _bpm, float _ppqPos, bool _isPlaying, size_t _sampleCount);
		float* getDummyBuffer(size_t _minimumSize);
		void updateDeviceLatency();
		void processMidiInEvents();
		void processMidiInEvent(const SMidiEvent& _ev);

		dsp56k::RingBuffer<SMidiEvent, 1024, false> m_midiInRingBuffer;
		std::vector<SMidiEvent> m_midiIn;
		std::vector<SMidiEvent> m_midiOut;

		SMidiEvent m_pendingSysexInput;

		ResamplerInOut m_resampler;
		mutable std::mutex m_lock;
		mutable std::mutex m_lockAddMidiEvent;

		Device* const m_device;

		std::vector<float> m_dummyBuffer;

		float m_hostSamplerate = 0.0f;
		float m_hostSamplerateInv = 0.0f;

		uint32_t m_blockSize = 0;

		uint32_t m_deviceLatencyMidiToOutput = 0;
		uint32_t m_deviceLatencyInputToOutput = 0;

		// MIDI Clock
		bool m_isPlaying = false;
		bool m_needsStart = false;
		double m_clockTickPos = 0.0;
	};
}
