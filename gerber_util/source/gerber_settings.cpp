//////////////////////////////////////////////////////////////////////

#if defined(WIN32)
#include <Windows.h>
#include <Shlobj.h>

#endif

#include <format>
#include <filesystem>

#include "gerber_util.h"
#include "gerber_settings.h"

namespace gerber_util
{
    //////////////////////////////////////////////////////////////////////

#if defined(WIN32)

    namespace
    {
        //////////////////////////////////////////////////////////////////////

        std::string get_ini_filename()
        {
            static std::string ini_filename;

            if(ini_filename.empty()) {
                char path[MAX_PATH + 1];
                if(!SHGetSpecialFolderPathA(HWND_DESKTOP, path, CSIDL_LOCAL_APPDATA, FALSE)) {
                    MessageBox(nullptr, "Fatal error, can't get application data folder!?", "Gerber Explorer", MB_ICONEXCLAMATION);
                    ExitProcess(1);
                }
                ini_filename = std::format("{}\\gerber_explorer.ini", path);
            }
            return ini_filename;
        }

    }    // namespace

    //////////////////////////////////////////////////////////////////////

    void clear_settings()
    {
        std::filesystem::remove(get_ini_filename());
    }

    //////////////////////////////////////////////////////////////////////

    bool save_string(std::string const &name, std::string const &value)
    {
        return WritePrivateProfileStringA("GerberExplorer", name.c_str(), value.c_str(), get_ini_filename().c_str()) != 0;
    }

    //////////////////////////////////////////////////////////////////////

    bool load_string(std::string const &name, std::string &value)
    {
        char buffer[256];
        DWORD got =
            GetPrivateProfileStringA("GerberExplorer", name.c_str(), nullptr, buffer, (DWORD)gerber_util::array_length(buffer), get_ini_filename().c_str());
        if(got != 0) {
            value = std::string(buffer, got);
            return true;
        }
        return false;
    }

#elif defined(__linux__)
#elif defined(__APPLE__)
#else
#error "Implement save_string and load_string for this platform
#endif

    //////////////////////////////////////////////////////////////////////

    bool load_int(std::string const &name, int &value)
    {
        std::string d;
        if(load_string(name, d)) {
            value = (int)_strtoi64(d.c_str(), nullptr, 10);
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////

    bool load_uint(std::string const &name, uint32_t &value)
    {
        std::string d;
        if(load_string(name, d)) {
            value = (uint32_t)_strtoi64(d.c_str(), nullptr, 16);
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////

    bool load_double(std::string const &name, double &value)
    {
        std::string d;
        if(load_string(name, d)) {
            value = strtod(d.c_str(), nullptr);
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////

    bool load_bool(std::string const &name, bool &value)
    {
        std::string d;
        if(load_string(name, d)) {
            value = to_lowercase(d).compare("true") == 0;
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////

    bool save_int(std::string const &name, int value)
    {
        return save_string(name, std::format("{}", value));
    }

    //////////////////////////////////////////////////////////////////////

    bool save_uint(std::string const &name, uint32_t value)
    {
        return save_string(name, std::format("{:08x}", value));
    }

    //////////////////////////////////////////////////////////////////////

    bool save_double(std::string const &name, double value)
    {
        return save_string(name, std::format("{}", value));
    }

    //////////////////////////////////////////////////////////////////////

    bool save_bool(std::string const &name, bool value)
    {
        return save_string(name, value ? "true" : "false");
    }

}    // namespace gerber_util
