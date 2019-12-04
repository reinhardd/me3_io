#ifndef PARSERS_H
#define PARSERS_H

#include <map>
#include <string>
#include <variant>


enum struct Modes
{
    MODE_auto,
    MODE_manual,
    MODE_vacation,
    MODE_off
};

using cmd_set_temp = std::pair<std::string, float>;
using cmd_set_mode = std::pair<Modes, time_t>;


using parsed_cmd = std::variant<cmd_set_mode, cmd_set_temp>;

parsed_cmd scan_input(std::string &&cmd);

#endif // PARSERS_H
