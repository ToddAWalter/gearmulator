#pragma once

#include <thread>
#include <vector>

#include "dsp56kEmu/types.h"

namespace dsp56k
{
	class Peripherals56362;
	class DSP;
}

namespace virusLib
{
class ROMFile
{
public:
	struct Chunk
	{
		uint8_t chunk_id = 0;
		uint8_t size1 = 0;
		uint8_t size2 = 0;
		std::vector<uint32_t> items;
	};

	struct BootRom
	{
		uint32_t size = 0;
		uint32_t offset = 0;
		std::vector<uint32_t> data;
	};

	enum class Model
	{
		Invalid = -1,
		ABC,
		Snow,
		TI
	};

	using TPreset = std::array<uint8_t, 512>;

	static void dumpToBin(const std::vector<dsp56k::TWord>& _data, const std::string& _filename);

	explicit ROMFile(const std::string& _path);

	bool getMulti(int _presetNumber, TPreset& _out) const;
	bool getSingle(int _bank, int _presetNumber, TPreset& _out) const;
	bool getPreset(uint32_t _offset, TPreset& _out) const;

	static std::string getSingleName(const TPreset& _preset);
	static std::string getMultiName(const TPreset& _preset);
	static std::string getPresetName(const TPreset& _preset, uint32_t _first, uint32_t _last);

	std::thread bootDSP(dsp56k::DSP& dsp, dsp56k::Peripherals56362& periph) const;

	bool isValid() const { return bootRom.size > 0; }

	Model getModel() const { return m_model; }

	uint32_t getSamplerate() const
	{
		return 12000000 / 256;
	}

	static uint32_t getMultiPresetSize()
	{
		return 256;
	}

	static uint32_t getSinglePresetSize()
	{
		return 256;
	}

	static uint8_t getSinglesPerBank()
	{
		return 128;
	}

	static constexpr uint32_t getRomSizeModelABC()
	{
		return 1024 * 512;
	}

	uint8_t getPresetsPerBank() const
	{
		return 128;
	}

	static std::string findROM();

	const std::vector<uint8_t>& getDemoData() const { return m_demoData; }

private:
	std::vector<Chunk> readChunks(std::istream& _file);

	BootRom bootRom;
	std::vector<uint32_t> commandStream;

	const std::string m_file;
	Model m_model = Model::Invalid;

	std::vector<TPreset> m_singles;
	std::vector<TPreset> m_multis;
	std::vector<uint8_t> m_demoData;
};

}
