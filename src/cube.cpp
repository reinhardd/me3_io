
#include "cube.h"

namespace max_eq3 {

namespace ba = boost::asio;
namespace bs = boost::system;

cube_t::cube_t(boost::asio::io_service &ios)
    : sock(ios)
    , refreshtimer(ios)
{}

cube_t::cube_t(boost::asio::io_service &ios, std::string &&mcast_rsp, boost::asio::ip::udp::endpoint ep)
    : sock(ios)
    , refreshtimer(ios)
{

    addr = ep.address();
    serial = mcast_rsp.substr(8, 10);
    const char *rdata = mcast_rsp.c_str();
    rfaddr = ((rdata[21] & 0xFF) << 16) + ((rdata[22] & 0xFF) << 8) + (rdata[23] & 0xFF);
    fwbc = ((rdata[24] & 0xFF) << 8) + (rdata[25] & 0xFF);
}

}
