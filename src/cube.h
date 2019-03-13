#ifndef CUBE_H
#define CUBE_H
#pragma once

#include "cube_io.h"

namespace max_eq3 {

typedef struct cube_t
{
    rfaddr_t                        rfaddr{0};
    uint16_t                        fwbc{0};
    std::string                     serial;
    uint8_t                         duty_cycle{0};
    uint8_t                         freememslots{0};

    uint16_t                        flags;


    // working data
    boost::asio::ip::address        addr;
    boost::asio::ip::tcp::socket    sock;
    boost::asio::steady_timer       refreshtimer;
    boost::asio::streambuf          rxdata;

    cube_t(boost::asio::io_service &ios,
           std::string &&mcast_rsp,
           boost::asio::ip::udp::endpoint ep);

    cube_t(boost::asio::io_service &ios);
} cube_t;

}

#endif // CUBE_H