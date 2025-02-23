#include "os.h"

#include "../dsp56300/source/dsp56kEmu/logging.h"

#ifndef _WIN32
// filesystem is only available on macOS Catalina 10.15+
// filesystem causes linker errors in gcc-8 if linked statically
#define USE_DIRENT
#endif

#ifdef USE_DIRENT
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#else
#include <filesystem>
#endif

#ifdef _WIN32
#define NOMINMAX
#define NOSERVICE
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#ifdef _MSC_VER
#include <cfloat>
#elif defined(HAVE_SSE)
#include <immintrin.h>
#endif

namespace synthLib
{
    std::string getModulePath(bool _stripPluginComponentFolders/* = true*/)
    {
        std::string path;
#ifdef _WIN32
        char buffer[MAX_PATH];
        HMODULE hm = nullptr;

        if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(&getModulePath), &hm) == 0)
        {
            LOG("GetModuleHandle failed, error = " << GetLastError());
            return {};
        }
        if (GetModuleFileName(hm, buffer, sizeof(buffer)) == 0)
        {
            LOG("GetModuleFileName failed, error = " << GetLastError());
            return {};
        }

        path = buffer;
#else
        Dl_info info;
        if (!dladdr(reinterpret_cast<const void *>(&getModulePath), &info))
        {
            LOG("Failed to get module path");
            return std::string();
        }
        else
        {
            path = info.dli_fname;
        }
#endif

    	auto fixPathWithDelim = [&](const std::string& _key, const char _delim)
    	{
			const auto end = path.rfind(_key + _delim);

            // strip folders such as "/foo.vst/" but do NOT strip "/.vst/"
			if (end != std::string::npos && (path.find(_delim + _key) + 1) != end)
				path = path.substr(0, end);
		};

    	auto fixPath = [&](const std::string& _key)
    	{
			fixPathWithDelim(_key, '/');
			fixPathWithDelim(_key, '\\');
		};

        if(_stripPluginComponentFolders)
        {
			fixPath(".vst");
			fixPath(".vst3");
			fixPath(".clap");
			fixPath(".component");
			fixPath(".app");
        }

		const auto end = path.find_last_of("/\\");

        if (end != std::string::npos)
            path = path.substr(0, end + 1);

        return validatePath(path);
    }

    std::string getCurrentDirectory()
    {
#ifdef USE_DIRENT
        char temp[1024];
        getcwd(temp, sizeof(temp));
        return temp;
#else
		return std::filesystem::current_path().string();
#endif
    }

    bool createDirectory(const std::string& _dir)
    {
#ifdef USE_DIRENT
        return mkdir(_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
#else
        return std::filesystem::create_directories(_dir);
#endif
    }

    std::string validatePath(std::string _path)
    {
        if(_path.empty())
            return _path;
        if(_path.back() == '/' || _path.back() == '\\')
            return _path;
        _path += '/';
        return _path;
    }

    bool getDirectoryEntries(std::vector<std::string>& _files, const std::string& _folder)
    {
#ifdef USE_DIRENT
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(_folder.c_str())))
        {
            while ((ent = readdir(dir)))
            {
				std::string f = ent->d_name;

				if(f == "." || f == "..")
					continue;

                std::string file = _folder;

            	if(file.back() != '/' && file.back() != '\\')
                    file += '/';

            	file += f;

                _files.push_back(file);
            }
            closedir(dir);
        }
        else
        {
            return false;
        }
#else
    	try
        {
            const auto u8Path = std::filesystem::u8path(_folder);
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(u8Path))
            {
                const auto &file = entry.path();

                try
                {
	                _files.push_back(file.u8string());
                }
                catch(std::exception& e)
                {
	                LOG(e.what());
                }
            }
        }
        catch (std::exception& e)
        {
            LOG(e.what());
            return false;
        }
#endif
        return !_files.empty();
    }

    std::string lowercase(const std::string &_src)
    {
        std::string str(_src);
        for (char& i : str)
	        i = static_cast<char>(tolower(i));
        return str;
    }

    std::string getExtension(const std::string &_name)
    {
        const auto pos = _name.find_last_of('.');
        if (pos != std::string::npos)
            return _name.substr(pos);
        return {};
    }

    std::string getFilenameWithoutPath(const std::string& _name)
    {
        const auto pos = _name.find_last_of("/\\");
        if (pos != std::string::npos)
            return _name.substr(pos + 1);
        return _name;
    }

    std::string getPath(const std::string& _filename)
    {
        const auto pos = _filename.find_last_of("/\\");
        if (pos != std::string::npos)
            return _filename.substr(0, pos);
        return _filename;
    }

    size_t getFileSize(const std::string& _file)
    {
        FILE* hFile = openFile(_file, "rb");
        if (!hFile)
            return 0;

        fseek(hFile, 0, SEEK_END);
        const auto size = static_cast<size_t>(ftell(hFile));
        fclose(hFile);
        return size;
    }

    bool isDirectory(const std::string& _path)
    {
#ifdef USE_DIRENT
		struct stat statbuf;
		stat(_path.c_str(), &statbuf);
		if (S_ISDIR(statbuf.st_mode))
            return true;
        return false;
#else
        return std::filesystem::is_directory(_path);
#endif
    }

    std::string findFile(const std::string& _extension, const size_t _minSize, const size_t _maxSize, const bool _stripPluginComponentFolders)
    {
        std::string path = getModulePath(_stripPluginComponentFolders);

        if(path.empty())
            path = getCurrentDirectory();

        return findFile(path, _extension, _minSize, _maxSize);
    }

    std::string findFile(const std::string& _extension, const size_t _minSize, const size_t _maxSize)
    {
        auto res = findFile(_extension, _minSize, _maxSize, true);
		if (!res.empty())
			return res;
		return findFile(_extension, _minSize, _maxSize, false);
    }

    std::string findFile(const std::string& _rootPath, const std::string& _extension, const size_t _minSize, const size_t _maxSize)
    {
        std::vector<std::string> files;
        if(!findFiles(files, _rootPath, _extension, _minSize, _maxSize))
            return {};
        return files.front();
    }

    bool findFiles(std::vector<std::string>& _files, const std::string& _rootPath, const std::string& _extension, const size_t _minSize, const size_t _maxSize)
    {
        std::vector<std::string> files;

        getDirectoryEntries(files, _rootPath);

        for (const auto& file : files)
        {
            if(!hasExtension(file, _extension))
                continue;

            if (!_minSize && !_maxSize)
            {
                _files.push_back(file);
                continue;
            }

            const auto size = getFileSize(file);

            if (_minSize && size < _minSize)
	            continue;
            if (_maxSize && size > _maxSize)
	            continue;

            _files.push_back(file);
        }
        return !_files.empty();
    }

	std::string findROM(const size_t _minSize, const size_t _maxSize)
    {
        std::string path = getModulePath();

        if(path.empty())
            path = getCurrentDirectory();

		auto f = findFile(path, ".bin", _minSize, _maxSize);
        if(!f.empty())
            return f;

    	path = getModulePath(false);

		return findFile(path, ".bin", _minSize, _maxSize);
    }

    std::string findROM(const size_t _expectedSize)
    {
	    return findROM(_expectedSize, _expectedSize);
    }

    bool hasExtension(const std::string& _filename, const std::string& _extension)
    {
        if (_extension.empty())
            return true;

        return lowercase(getExtension(_filename)) == lowercase(_extension);
    }

    void setFlushDenormalsToZero()
    {
#if defined(_MSC_VER)
        _controlfp(_DN_FLUSH, _MCW_DN);
#elif defined(HAVE_SSE)
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
    }

    bool writeFile(const std::string& _filename, const std::vector<uint8_t>& _data)
    {
        return writeFile(_filename, _data.data(), _data.size());
    }

    bool writeFile(const std::string& _filename, const uint8_t* _data, size_t _size)
    {
        auto* hFile = openFile(_filename, "wb");
        if(!hFile)
            return false;
        const auto written = fwrite(&_data[0], 1, _size, hFile);
        fclose(hFile);
        return written == _size;
    }

    bool readFile(std::vector<uint8_t>& _data, const std::string& _filename)
    {
        auto* hFile = openFile(_filename, "rb");
        if(!hFile)
            return false;

    	fseek(hFile, 0, SEEK_END);
        const auto size = ftell(hFile);
        fseek(hFile, 0, SEEK_SET);

    	if(!size)
        {
	        fclose(hFile);
            _data.clear();
            return true;
        }

    	if(_data.size() != static_cast<size_t>(size))
            _data.resize(size);

    	const auto read = fread(_data.data(), 1, _data.size(), hFile);
        fclose(hFile);
        return read == _data.size();
    }

    FILE* openFile(const std::string& _name, const char* _mode)
    {
#ifdef _WIN32
        // convert filename
		std::wstring nameW;
		nameW.resize(_name.size());
		const int newSize = MultiByteToWideChar(CP_UTF8, 0, _name.c_str(), static_cast<int>(_name.size()), const_cast<wchar_t *>(nameW.c_str()), static_cast<int>(_name.size()));
		nameW.resize(newSize);

        // convert mode
        wchar_t mode[32]{0};
		MultiByteToWideChar(CP_UTF8, 0, _mode, static_cast<int>(strlen(_mode)), mode, (int)std::size(mode));
		return _wfopen(nameW.c_str(), mode);
#else
		return fopen(_name.c_str(), _mode);
#endif
    }
} // namespace synthLib
