#include <gerber_util.h>

namespace
{
    static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter{};

}    // namespace

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////

    std::wstring utf16_from_utf8(std::string const &s)
    {
        return converter.from_bytes(s);
    }
}    // namespace gerber_lib
