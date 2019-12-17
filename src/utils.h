#ifndef UTILS_H
#define UTILS_H

#include <string>
#include "cube_io.h"

namespace max_eq3 {
    std::string dump(const std::string &i);

    rfaddr_t rfaddr_from_string(const std::string &data);
    rfaddr_t rfaddr_from_string(const char *pData);
    std::string decode64(const std::string& s);
    std::string encode64(const std::string& s);

    std::string mode_as_string(opmode m);

    std::string flags_as_string(uint16_t f);

}

template <typename RT>
RT fromPtr(const uint8_t *p, std::size_t len = sizeof(RT))
{
    RT tmp = 0;
    int i = 0;
    std::size_t rlen = std::min(len, sizeof(RT));
    for (int i = 0; i < rlen; ++i)
    {
        tmp <<= 8;
        tmp += p[i];
    }
    return tmp;
}

template <typename RT>
RT fromPtr(const char *p, std::size_t len = sizeof(RT))
{
    return fromPtr<RT>((const uint8_t *)p, len);
}

/**
 * @brief parse_room
 * @param input
 * @param roomname
 * @return
 *
 * possibly not UTF-8 safe
 */
bool parse_room(std::string &input, std::string &roomname);

#endif // UTILS_H
