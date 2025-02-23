#pragma once

#include "romfile.h"

namespace virusLib
{
	class ROMLoader
	{
	public:
		enum FileType
		{
			Invalid,
			BinaryRom,
			MidiRom,
			MidiPresets
		};

		struct FileData
		{
			std::string filename;
			FileType type;
			std::vector<uint8_t> data;

			bool isValid() const { return type != Invalid && !data.empty(); }
		};

		static std::vector<ROMFile> findROMs(DeviceModel _model = DeviceModel::ABC);
		static std::vector<ROMFile> findROMs(DeviceModel _modelA, DeviceModel _modelB);
		static std::vector<ROMFile> findROMs(const std::string& _path, DeviceModel _model = DeviceModel::ABC);
		static ROMFile findROM(DeviceModel _model = DeviceModel::ABC);
		static ROMFile findROM(const std::string& _filename, DeviceModel _model = DeviceModel::ABC);

	private:
		static std::vector<std::string> findFiles(const std::string& _extension, size_t _minSize, size_t _maxSize);
		static std::vector<std::string> findFiles(const std::string& _path, const std::string& _extension, size_t _minSize, size_t _maxSize);

		static FileData loadFile(const std::string& _name);

		static std::vector<ROMFile> initializeRoms(const std::vector<std::string>& _files, DeviceModel _model);
	};
}
