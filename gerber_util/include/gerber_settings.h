#pragma once

#include <string>

namespace gerber_util
{
    bool load_string(std::string const &name, std::string &value);
    bool load_int(std::string const &name, int &value);
    bool load_double(std::string const &name, double &value);
    bool load_bool(std::string const &name, bool &value);

    bool save_string(std::string const &name, std::string const &value);
    bool save_int(std::string const &name, int value);
    bool save_double(std::string const &name, double value);
    bool save_bool(std::string const &name, bool value);

}    // namespace gerber_util::settings
