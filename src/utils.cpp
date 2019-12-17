
#include <cstdint>

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>


#include "utils.h"

namespace max_eq3 {

std::string dump(const std::string &in)
{
    std::ostringstream xs;
    xs << std::setfill('0') << std::hex;
    bool first = true;
    for (uint8_t c: in)
    {
        if (first)
            first = false;
        else
            xs << ' ';

        xs << std::setw(2) << unsigned(c);
    }
    return xs.str();
}


rfaddr_t rfaddr_from_string(const char *pData)
{
    rfaddr_t rv = 0;
    // for (uint8_t c: data)
    for (unsigned u = 0; u < 3; ++u)
    {
        uint8_t c = *pData++;
        rv <<= 4;

        if (c > '9')
        {
            c -= ('a' - 10);
        }
        else
            c -= '0';
        rv += (c & 0x0f);
    }
    return rv;
}

rfaddr_t rfaddr_from_string(const std::string &data)
{
    rfaddr_t rv = 0;
    for (uint8_t c: data)
    {
        rv <<= 4;

        if (c > '9')
        {
            c -= ('a' - 10);
        }
        else
            c -= '0';
        rv += (c & 0x0f);
    }
    return rv;
}

std::string decode64(const std::string& s)
{
    namespace bai = boost::archive::iterators;

    std::stringstream os;

    typedef bai::transform_width<bai::binary_from_base64<const char *>, 8, 6> base64_dec;
    unsigned int size = s.size();
    // Remove the padding characters, cf. https://svn.boost.org/trac/boost/ticket/5629
    if (size && s[size - 1] == '=')
    {
        --size;
        if (size && s[size - 1] == '=') --size;
    }
    if (size == 0) return std::string();

    std::copy(base64_dec(s.data()), base64_dec(s.data() + size),
              std::ostream_iterator<char>(os));
    return os.str();
}

const std::string base64_padding[] = {"", "==","="};
std::string encode64(const std::string& s)
{
  namespace bai = boost::archive::iterators;

  std::stringstream os;

  // convert binary values to base64 characters
  typedef bai::base64_from_binary
  // retrieve 6 bit integers from a sequence of 8 bit bytes
  <bai::transform_width<const char *, 6, 8> > base64_enc; // compose all the above operations in to a new iterator

  std::copy(base64_enc(s.c_str()), base64_enc(s.c_str() + s.size()),
            std::ostream_iterator<char>(os));

  os << base64_padding[s.size() % 3];
  return os.str();
}

std::string mode_as_string(opmode m)
{
    switch (m)
    {
    case opmode::AUTO: return "AUTO";
    case opmode::BOOST: return "BOOST";
    case opmode::MANUAL: return "MANUAL";
    case opmode::VACATION: return "VACATION";
    }
    std::ostringstream xs;
    xs << "MODE " << static_cast<unsigned>(m) << " ?";
    return xs.str();
}

std::string flags_as_string(uint16_t flags)
{
    std::ostringstream xs;
    xs << std::hex << flags << std::dec
       << " mode:" << (flags & 3) << ':' << mode_as_string(static_cast<opmode>(flags & 3))
       << " dst:" << ((flags & 8) == 8)
       << " gw:" << ((flags & 0x10) == 0x10)
       << " panel:" << ((flags & 0x20) == 0x20)
       << " link:" << ((flags & 0x40) == 0x40)
       << " bat:" << ((flags & 0x80) == 0x80)
       << " init:" << ((flags & 0x200) == 0x200)
       << " answer:" << ((flags & 0x400) == 0x400)
       << " error:" << ((flags & 0x800) == 0x800)
       << " valid:" << ((flags & 0x1000) == 0x1000);
    return xs.str();
}

std::string l_submsg_as_string(const l_submsg_data &smgs)
{
    std::ostringstream xs;
    opmode m = smgs.get_opmode();
    xs << std::setfill('0') << std::hex << std::setw(6) << smgs.rfaddr
       << std::dec // << std::setfill(' ')
       << ':' << devicetype_as_string(smgs.submsg_src)
       << std::fixed << std::setprecision(1)
       << " s:" << std::setw(4) << smgs.set_temp
       << " a:" << std::setw(4) << smgs.act_temp
       << " v:" << smgs.valve_pos
       << " mode:" << mode_as_string(smgs.get_opmode())
       << " flags:" << flags_as_string(smgs.flags);
    return xs.str();
}
std::string devicetype_as_string(devicetype dt)
{
    switch (dt)
    {
        case devicetype::Cube: return "cube";
        case devicetype::WallThermostat: return "wt";
        case devicetype::RadiatorThermostat: return "rt";
        case devicetype::RadiatorThermostatPlus: return "rtp";
        case devicetype::ShutterContact: return "sc";
        case devicetype::EcoButton: return "ebtn";
    }
    std::ostringstream xs;
    xs << "dev " << static_cast<unsigned>(dt);
    return xs.str();
}

}

bool parse_room(std::string &input, std::string &roomname)
{
    std::string tmp = input;
    boost::trim(tmp);
    enum struct pstate_e {
        none,
        double_qmarks,
        single_qmarks,
    } pstate;

    unsigned cpos = 0;
    switch (tmp[0])
    {
    case '"':
        pstate = pstate_e::double_qmarks;
        cpos = 1;
        break;
    case '\'':
        pstate = pstate_e::single_qmarks;
        cpos = 1;
        break;
    default:
        pstate = pstate_e::none;
    }

    std::string rname;
    while (true)
    {
        if (cpos == tmp.size())
            return false;

        switch (tmp[cpos])
        {
        case ' ':
            if (pstate == pstate_e::none)
            {
                roomname = rname;
                input = tmp.substr(cpos+1);
                return true;
            }
            break;
        case '\'':
            if (pstate == pstate_e::single_qmarks)
            {
                roomname = rname;
                input = tmp.substr(cpos+1);
                return true;
            }
            break;
        case '"':
            if (pstate == pstate_e::double_qmarks)
            {
                roomname = rname;
                input = tmp.substr(cpos+1);
                return true;
            }
            break;
        }
        rname.push_back(tmp[cpos]);
        cpos++;
    }
}


