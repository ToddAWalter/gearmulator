#include "PatchManager.h"

#include "VirusEditor.h"
#include "../VirusController.h"

#include "../../jucePluginLib/patchdb/datasource.h"
#include "../../jucePluginEditorLib/pluginEditor.h"

#include "../../virusLib/microcontroller.h"
#include "../../virusLib/device.h"
#include "../../virusLib/midiFileToRomData.h"

#include "../../synthLib/midiToSysex.h"
#include "../../synthLib/os.h"

#include "juce_cryptography/hashing/juce_MD5.h"

namespace Virus
{
	class Controller;
}

namespace genericVirusUI
{
	PatchManager::PatchManager(VirusEditor& _editor, juce::Component* _root, const juce::File& _dir) : jucePluginEditorLib::patchManager::PatchManager(_editor, _root, _dir), m_controller(_editor.getController())
	{
		addRomPatches();

		startLoaderThread();

		// rom patches are received via midi, make sure we add all remaining ones, too
		m_controller.onRomPatchReceived = [this](const virusLib::BankNumber _bank, const uint32_t _program)
		{
			if (_bank == virusLib::BankNumber::EditBuffer)
				return;

			const auto index = virusLib::toArrayIndex(_bank);

			const auto& banks = m_controller.getSinglePresets();

			if(index < banks.size())
			{
				const auto& bank = banks[index];

				if(_program == bank.size() - 1)
					addDataSource(createRomDataSource(index));
			}
		};
		addGroupTreeItemForTag(pluginLib::patchDB::TagType::CustomA, "Virus Model");
		addGroupTreeItemForTag(pluginLib::patchDB::TagType::CustomB, "Virus Features");
	}

	PatchManager::~PatchManager()
	{
		stopLoaderThread();
		m_controller.onRomPatchReceived = {};
	}

	bool PatchManager::loadRomData(pluginLib::patchDB::DataList& _results, const uint32_t _bank, const uint32_t _program)
	{
		const auto bankIndex = _bank;

		const auto& singles = m_controller.getSinglePresets();

		if (bankIndex >= singles.size())
			return false;

		const auto& bank = singles[bankIndex];

		if(_program != pluginLib::patchDB::g_invalidProgram)
		{
			if (_program >= bank.size())
				return false;
			const auto& s = bank[_program];
			if (s.data.empty())
				return false;
			_results.push_back(s.data);
		}
		else
		{
			_results.reserve(bank.size());
			for (const auto& patch : bank)
				_results.push_back(patch.data);
		}
		return true;
	}

	std::shared_ptr<pluginLib::patchDB::Patch> PatchManager::initializePatch(std::vector<uint8_t>&& _sysex)
	{
		if (_sysex.size() < 267)
			return nullptr;

		const auto& c = static_cast<const Virus::Controller&>(m_controller);

		pluginLib::MidiPacket::Data data;
		pluginLib::MidiPacket::AnyPartParamValues parameterValues;

		if (!c.parseSingle(data, parameterValues, _sysex))
			return nullptr;

		const auto idxVersion = c.getParameterIndexByName("Version");
		const auto idxCategory1 = c.getParameterIndexByName("Category1");
		const auto idxCategory2 = c.getParameterIndexByName("Category2");
		const auto idxUnison = c.getParameterIndexByName("Unison Mode");
//		const auto idxTranspose = c.getParameterIndexByName("Transpose");
		const auto idxArpMode = c.getParameterIndexByName("Arp Mode");
		const auto idxPhaserMix = c.getParameterIndexByName("Phaser Mix");
		const auto idxChorusMix = c.getParameterIndexByName("Chorus Mix");

		auto patch = std::make_shared<pluginLib::patchDB::Patch>();

		{
			const auto it = data.find(pluginLib::MidiDataType::Bank);
			if (it != data.end())
				patch->bank = it->second;
		}
		{
			const auto it = data.find(pluginLib::MidiDataType::Program);
			if (it != data.end())
				patch->program = it->second;
		}

		{
			constexpr auto frontOffset = 9;			// remove bank number, program number and other stuff that we don't need, first index is the patch version
			constexpr auto backOffset = 2;			// remove f7 and checksum
			const juce::MD5 md5(_sysex.data() + frontOffset, _sysex.size() - frontOffset - backOffset);

			static_assert(sizeof(juce::MD5) >= sizeof(pluginLib::patchDB::PatchHash));
			memcpy(patch->hash.data(), md5.getChecksumDataArray(), std::size(patch->hash));
		}

		patch->sysex = std::move(_sysex);

		patch->name = m_controller.getSinglePresetName(parameterValues);

		const auto version = virusLib::Microcontroller::getPresetVersion(*parameterValues[idxVersion]);
		const auto unison = *parameterValues[idxUnison];
//		const auto transpose = parameterValues.find(std::make_pair(pluginLib::MidiPacket::AnyPart, idxTranspose))->second;
		const auto arpMode = *parameterValues[idxArpMode];

		const auto category1 = *parameterValues[idxCategory1];
		const auto category2 = *parameterValues[idxCategory2];

		const auto* paramCategory1 = c.getParameter(idxCategory1, 0);
		const auto* paramCategory2 = c.getParameter(idxCategory2, 0);

		auto addCategory = [&patch, version](const uint8_t _value, const pluginLib::Parameter* _param)
		{
			if(!_value)
				return;
			const auto& values = _param->getDescription().valueList;
			if(_value >= values.texts.size())
				return;

			// Virus < TI had less categories
			if(version < virusLib::D && _value > 16)
				return;

			const auto t = _param->getDescription().valueList.valueToText(_value);
			patch->tags.add(pluginLib::patchDB::TagType::Category, t);
		};

		addCategory(category1, paramCategory1);
		addCategory(category2, paramCategory2);

		switch (version)
		{
		case virusLib::A:	patch->tags.add(pluginLib::patchDB::TagType::CustomA, "A");		break;
		case virusLib::B:	patch->tags.add(pluginLib::patchDB::TagType::CustomA, "B");		break;
		case virusLib::C:	patch->tags.add(pluginLib::patchDB::TagType::CustomA, "C");		break;
		case virusLib::D:	patch->tags.add(pluginLib::patchDB::TagType::CustomA, "TI");		break;
		case virusLib::D2:	patch->tags.add(pluginLib::patchDB::TagType::CustomA, "TI2");		break;
		}

		if(arpMode)
			patch->tags.add(pluginLib::patchDB::TagType::CustomB, "Arp");
		if(unison)
			patch->tags.add(pluginLib::patchDB::TagType::CustomB, "Unison");
		if(*parameterValues[idxPhaserMix] > 0)
			patch->tags.add(pluginLib::patchDB::TagType::CustomB, "Phaser");
		if(*parameterValues[idxChorusMix] > 0)
			patch->tags.add(pluginLib::patchDB::TagType::CustomB, "Chorus");
		return patch;
	}

	pluginLib::patchDB::Data PatchManager::prepareSave(const pluginLib::patchDB::PatchPtr& _patch) const
	{
		pluginLib::MidiPacket::Data data;
		pluginLib::MidiPacket::AnyPartParamValues parameterValues;

		if (!m_controller.parseSingle(data, parameterValues, _patch->sysex))
			return _patch->sysex;

		// apply name
		if (!_patch->getName().empty())
			m_controller.setSinglePresetName(parameterValues, _patch->getName());

		// apply program
		auto bank = toMidiByte(virusLib::BankNumber::A);
		auto program = data[pluginLib::MidiDataType::Program];

		if (_patch->program != pluginLib::patchDB::g_invalidProgram)
		{
			const auto bankOffset = _patch->program / 128;
			program = static_cast<uint8_t>(_patch->program - bankOffset * 128);
			bank += static_cast<uint8_t>(bankOffset);
		}

		// apply categories
		const uint32_t indicesCategory[] = {
			m_controller.getParameterIndexByName("Category1"),
			m_controller.getParameterIndexByName("Category2")
		};

		const pluginLib::Parameter* paramsCategory[] = {
			m_controller.getParameter(indicesCategory[0], 0),
			m_controller.getParameter(indicesCategory[1], 0)
		};

		uint8_t val0 = 0;
		uint8_t val1 = 0;

		const auto& tags = _patch->getTags(pluginLib::patchDB::TagType::Category);

		size_t i = 0;
		for (const auto& tag : tags.getAdded())
		{
			const auto categoryValue = paramsCategory[i]->getDescription().valueList.textToValue(tag);
			if(categoryValue != 0)
			{
				auto& v = i ? val1 : val0;
				v = static_cast<uint8_t>(categoryValue);
				++i;
				if (i == 2)
					break;
			}
		}

		parameterValues[indicesCategory[0]] = val0;
		parameterValues[indicesCategory[1]] = val1;

		return m_controller.createSingleDump(bank, program, parameterValues);
	}

	bool PatchManager::parseFileData(pluginLib::patchDB::DataList& _results, const pluginLib::patchDB::Data& _data)
	{
		{
			std::vector<synthLib::SMidiEvent> events;
			virusLib::Device::parseTIcontrolPreset(events, _data);

			for (const auto& e : events)
			{
				if (!e.sysex.empty())
					_results.push_back(e.sysex);
			}

			if (!_results.empty())
				return true;
		}

		if (virusLib::Device::parsePowercorePreset(_results, _data))
			return true;

		if(!synthLib::MidiToSysex::extractSysexFromData(_results, _data))
			return false;

		if(!_results.empty())
		{
			if(_data.size() > 500000)
			{
				virusLib::MidiFileToRomData romLoader;

				for (const auto& result : _results)
				{
					if(!romLoader.add(result))
						break;
				}
				if(romLoader.isComplete())
				{
					const auto& data = romLoader.getData();

					if(data.size() > 0x10000)
					{
						// presets are written to ROM address 0x50000, the second half of an OS update is therefore at 0x10000
						constexpr ptrdiff_t startAddr = 0x10000;
						ptrdiff_t addr = startAddr;
						uint32_t index = 0;

						while(addr + 0x100 <= static_cast<ptrdiff_t>(data.size()))
						{
							std::vector chunk(data.begin() + addr, data.begin() + addr + 0x100);

							// validate
//							const auto idxH = chunk[2];
							const auto idxL = chunk[3];

							if(/*idxH != (index >> 7) || */idxL != (index & 0x7f))
								break;

							bool validName = true;
							for(size_t i=240; i<240+10; ++i)
							{
								if(chunk[i] < 32 || chunk[i] > 128)
								{
									validName = false;
									break;
								}
							}

							if(!validName)
								continue;

							addr += 0x100;
							++index;
						}

						if(index > 0)
						{
							_results.clear();

							for(uint32_t i=0; i<index; ++i)
							{
								// pack into sysex
								std::vector<uint8_t>& sysex = _results.emplace_back(std::vector<uint8_t>
									{0xf0, 0x00, 0x20, 0x33, 0x01, virusLib::OMNI_DEVICE_ID, 0x10, static_cast<uint8_t>(0x01 + (i >> 7)), static_cast<uint8_t>(i & 0x7f)}
								);
								sysex.insert(sysex.end(), data.begin() + i * 0x100 + startAddr, data.begin() + i * 0x100 + 0x100 + startAddr);
								sysex.push_back(virusLib::Microcontroller::calcChecksum(sysex, 5));
								sysex.push_back(0xf7);
							}
						}
					}
				}
			}

		}

		return !_results.empty();
	}

	bool PatchManager::requestPatchForPart(pluginLib::patchDB::Data& _data, const uint32_t _part)
	{
		_data = m_controller.createSingleDump(static_cast<uint8_t>(_part), toMidiByte(virusLib::BankNumber::A), 0);
		return !_data.empty();
	}

	uint32_t PatchManager::getCurrentPart() const
	{
		return m_controller.getCurrentPart();
	}

	bool PatchManager::equals(const pluginLib::patchDB::PatchPtr& _a, const pluginLib::patchDB::PatchPtr& _b) const
	{
		pluginLib::MidiPacket::Data dataA, dataB;
		pluginLib::MidiPacket::AnyPartParamValues parameterValuesA, parameterValuesB;

		if (!m_controller.parseSingle(dataA, parameterValuesA, _a->sysex) || !m_controller.parseSingle(dataB, parameterValuesB, _b->sysex))
			return false;

		if(parameterValuesA.size() != parameterValuesB.size())
			return false;

		for(uint32_t i=0; i<parameterValuesA.size(); ++i)
		{
			const auto& itA = parameterValuesA[i];
			const auto& itB = parameterValuesB[i];

			if(!itA)
			{
				if(itB)
					return false;
				continue;
			}

			if(!itB)
				return false;

			auto vA = *itA;
			auto vB = *itB;

			if(vA != vB)
			{
				// parameters might be out of range because some dumps have values that are out of range indeed, clamp to valid range and compare again
				const auto* param = m_controller.getParameter(i);
				if(!param)
					return false;

				if(param->getDescription().isNonPartSensitive())
					continue;

				vA = static_cast<uint8_t>(param->getDescription().range.clipValue(vA));
				vB = static_cast<uint8_t>(param->getDescription().range.clipValue(vB));

				if(vA != vB)
					return false;
			}
		}
		return true;
	}

	bool PatchManager::activatePatch(const pluginLib::patchDB::PatchPtr& _patch)
	{
		return m_controller.activatePatch(_patch->sysex);
	}

	bool PatchManager::activatePatch(const pluginLib::patchDB::PatchPtr& _patch, uint32_t _part)
	{
		return m_controller.activatePatch(_patch->sysex, _part);
	}

	void PatchManager::addRomPatches()
	{
		const auto& singles = m_controller.getSinglePresets();

		for (uint32_t b = 0; b < singles.size(); ++b)
		{
			const auto& bank = singles[b];

			const auto& single = bank[bank.size()-1];

			if (single.data.empty())
				continue;

			addDataSource(createRomDataSource(b));
		}
	}

	pluginLib::patchDB::DataSource PatchManager::createRomDataSource(const uint32_t _bank) const
	{
		pluginLib::patchDB::DataSource ds;
		ds.type = pluginLib::patchDB::SourceType::Rom;
		ds.bank = _bank;
		ds.name = m_controller.getBankName(_bank);
		return ds;
	}

}
