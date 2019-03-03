#ifndef CUBIO_IO_P_H
#define CUBIO_IO_P_H
#pragma once

#include <map>
#include <thread>
#include <memory>

#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

#include "cube_io.h"
#include "cube.h"
#include "dev_store.h"

namespace max_eq3 {

using cube_sp = std::shared_ptr<cube_t>;

using cube_map_t = std::map<rfaddr_t, cube_sp>;

struct m_room
{
    uint8_t id;             // 1 .. x

    std::string name;       // name of room

    /**
     * @brief group_rfaddr
     * this ist the rfaddr of the device first assigned to this room
     */
    rfaddr_t group_rfaddr;
};


struct m_device
{
    devicetype  type;
    rfaddr_t    rfaddr;
    std::string serial;
    std::string name;
    uint8_t     room_id;
};

typedef struct {

} device;


struct cube_io::Private
{
    std::thread                     comthread;
    boost::asio::io_service         io;
    boost::asio::ip::udp::socket    socket{io};

    uint8_t                         recvline[32];   // initial udp acceptance buffer for broadcast response
    boost::asio::ip::udp::endpoint  mcast_endpoint;

    std::thread                     io_thread;

    cube_event_target              *iet{nullptr};

    cube_map_t                      cubes;      // managed cubes

    // std::map<std::string, m_room>   room_defs;
    std::map<rfaddr_t, m_device>    device_defs;

    device_data_store               devconfigs;
    std::map<std::string, room_sp>  emit_rooms;

};
}
#endif // CUBIO_IO_P_H
