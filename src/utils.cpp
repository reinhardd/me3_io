
#include <cstdint>

#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>


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


}
