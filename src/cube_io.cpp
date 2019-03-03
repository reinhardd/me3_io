
#include <chrono>

#include <boost/bind.hpp>

#include "cubio_io_p.h"
#include "cube_log_internal.h"
#include "utils.h"

#define MULTICAST		"224.0.0.1"
#define MAX_UDP_PORT		23272
#define MAX_TCP_PORT		62910

namespace  {

uint8_t cube_detect_request[19] = {
    0x65, 0x51, 0x33, 0x4d,
    0x61, 0x78, 0x2a, 0x00,
    0x2a, 0x2a, 0x2a, 0x2a,
    0x2a, 0x2a, 0x2a, 0x2a,
    0x2a, 0x2a, 0x49
};

bool l_response(std::string &&decoded, max_eq3::l_submsg_data &adata);

bool m_response(std::string &&rawdata,
                std::list<max_eq3::m_room> &roomlist,
                std::list<max_eq3::m_device> &devicelist);
}

namespace max_eq3 {

struct l_submsg_data
{
    max_eq3::devicetype
        submsg_src{max_eq3::devicetype::Undefined};

    uint32_t    rfaddr {0};
    uint16_t    flags {0};
    uint16_t    valve_pos {0};
    double      set_temp {0.0};
    double      act_temp {0.0};
    uint16_t    dateuntil;
    uint16_t    minutes_since_midnight;                      // since midnight
};

cube_event_target::~cube_event_target()
{}

void cube_io::set_logger(logging_target *target)
{
    set_log_target(target);
}

cube_io::cube_io(cube_event_target *iet)
    : _p(new Private)
{
    _p->iet = iet;
    _p->io_thread = std::thread(std::bind(&cube_io::process_io, this));
}

cube_io::~cube_io()
{

}

void cube_io::process_io()
{
    bs::error_code error;
    boost::asio::ip::address listen_address = ba::ip::address::from_string("0.0.0.0");
    boost::asio::ip::udp::endpoint listen_endpoint(listen_address, MAX_UDP_PORT);
    _p->socket.open(listen_endpoint.protocol());
    _p->socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    _p->socket.bind(listen_endpoint);
    if (!error)
    {
        ba::ip::udp::endpoint senderEndpoint(ba::ip::address_v4::from_string(MULTICAST), MAX_UDP_PORT);

        LogV("send now")
        _p->socket.async_send_to(
            ba::buffer(cube_detect_request, sizeof(cube_detect_request)),
            senderEndpoint,
            [](const bs::error_code &ec, size_t)
                { LogV("mcast send done") }
        );
        LogV("mcast send started ")

        _p->socket.async_receive_from(ba::buffer(_p->recvline, sizeof(_p->recvline)),
                                      _p->mcast_endpoint,
                                      boost::bind(&cube_io::handle_mcast_response, this,
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::bytes_transferred));

        _p->io.run();
        LogV("done")
    }
}

void cube_io::timed_refresh(cube_sp csp, const boost::system::error_code &ec)
{
    if (!ec)
    {
        ba::async_write(csp->sock,
                        ba::buffer("l:\r\n"),
                        [](const boost::system::error_code &e, std::size_t bytes_transferred)
                        {
                            if (e)
                                LogE("write failed on refresh start")
                            else
                                LogV("refress started " << e << ": " << bytes_transferred)
                        }
        );
    }
}

void cube_io::rxrh_done(cube_sp csp, const boost::system::error_code& e, std::size_t bytes_recvd)
{
    if (e)
    {
        LogE("error on receive for cube " << std::hex << csp->rfaddr)
    }
    else
    {
        std::istream is(&csp->rxdata);
        std::string result_line;
        std::getline(is, result_line);
        result_line.pop_back();
        csp->refreshtimer.cancel();

        // evaluation of data
        evaluate_data(csp, std::move(result_line));

    }
    start_rx_from_cube(csp);
}

void cube_io::start_rx_from_cube(cube_sp &csp)
{
    csp->refreshtimer.cancel();
    csp->refreshtimer.expires_after(std::chrono::seconds(30));
    csp->refreshtimer.async_wait(
                boost::bind(&cube_io::timed_refresh,
                            this,
                            csp,
                            ba::placeholders::error)
                );

    ba::async_read_until(csp->sock, csp->rxdata, "\r\n",
                        boost::bind(&cube_io::rxrh_done, this, csp,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred
                                    )
                         );

}

void cube_io::handle_mcast_response(const bs::error_code &error, size_t bytes_recvd)
{
    LogV(__FUNCTION__ << "(" << error << ',' << std::dec << bytes_recvd << ")")
    if (!error)
    {
        LogV(__FUNCTION__ << " received " << std::string(
                         reinterpret_cast<const char *>(_p->recvline), bytes_recvd))
        if (bytes_recvd == 26)
        {
            std::string data((const char *)_p->recvline, bytes_recvd);
            if (data.substr(0,8) == "eQ3MaxAp") // response from hub
            {
                cube_sp cube = std::make_shared<cube_t>(_p->io, std::move(data), _p->mcast_endpoint);
                LogV( "got cube: \n\t" << cube->addr
                      << "\n\tsn: " << cube->serial
                      << "\n\tfw: " << cube->fwbc
                      << "\n\taddr: " << std::hex << cube->rfaddr << std::dec)

                bool connect = (_p->cubes.find(cube->rfaddr) == _p->cubes.end());
                _p->cubes[cube->rfaddr] = cube;
                if (connect)
                {
                    ba::ip::tcp::endpoint ep(cube->addr, 62910);
                    cube->sock.connect(ep);
                }
                start_rx_from_cube(cube);

                // start_rx_from_cube(cc, true);
            }

        }
    }
    // restart broadcast receive
    _p->socket.async_receive_from(ba::buffer(_p->recvline, sizeof(_p->recvline)), _p->mcast_endpoint,
                                              boost::bind(&cube_io::handle_mcast_response, this,
                                              boost::asio::placeholders::error,
                                              boost::asio::placeholders::bytes_transferred));
}


void cube_io::evaluate_data(cube_sp csp, std::string &&data)
{
    if (data.size())
    {
        switch (data[0])
        {
        case 'H':
            {
                csp->serial = data.substr(2,10);
                LogV("serial " << csp->serial)
                uint32_t newrfaddr = rfaddr_from_string(data.substr(13,6));
                if (newrfaddr != csp->rfaddr)
                    LogE("rfaddr mismatch " << std::hex << newrfaddr << " != "
                              << csp->rfaddr << std::dec
                              << " from " << dump(data))

            }
            break;
        case 'M':
            {
                std::list<m_device> devices;
                std::list<m_room> rooms;
                m_response(std::move(data), rooms, devices);
                LogV("devs " << devices.size()
                          << " rooms " << rooms.size())
                for (const m_room &r: rooms)
                {
                    LogV("room id: " << uint16_t(r.id)
                         << " rfaddr: " << std::hex << r.group_rfaddr << std::dec
                         << " n:" << r.name)

                    room_conf &rc = _p->devconfigs.roomconf[r.id];
                    rc.name = r.name;
                    rc.rfaddr = r.group_rfaddr;
                    rc.id = r.id;

                    room_data &rd = _p->devconfigs.rooms[r.id];
                    rd.act = 0.0;
                    rd.set = 0.0;

                }
                for (const m_device &d: devices)
                {
                    _p->device_defs[d.rfaddr] = d;
                    LogV("dev: " << std::hex << d.rfaddr << std::dec
                         << " n:" << d.name << " t:" << uint8_t(d.type) << " sn:" << d.serial
                         << " rid:" << uint16_t(d.room_id))

                    if (_p->devconfigs.roomconf.find(d.room_id) == _p->devconfigs.roomconf.end())
                    {
                        room_conf &rc = _p->devconfigs.roomconf[d.room_id];
                        switch (d.type)
                        {
                            case devicetype::WallThermostat:
                                rc.wallthermostat = d.rfaddr;
                                break;
                            case devicetype::RadiatorThermostat:
                                rc.thermostats.insert(d.rfaddr);
                                break;
                        }
                    }
                    dev_config &dc = _p->devconfigs.devconf[d.rfaddr];
                    dc.serial = d.serial;
                    dc.devtype = d.type;
                    dc.room_id = d.room_id;
                }
            }
            break;
        case 'L':
            {
                std::string decoded = decode64(data.substr(2, data.size() - 3));

                while (decoded.size())
                {
                    unsigned submsglen = decoded[0];

                    l_submsg_data adata;
                    if (l_response(decoded.substr(0, submsglen + 1), adata))
                    {
                        deploydata(adata);
                    }
                    else
                        LogE("l_resp failed");

                    decoded.erase(0, submsglen + 1);

                }
            }
            break;
        case 'C':
            {
                std::string::size_type spos = data.find(',');
                if ((spos != std::string::npos)
                        || (data[1] != ':'))
                {
                    std::string addr = data.substr(2, spos - 2);
                    std::string decoded = decode64(data.substr(spos + 1));

                    // std::cout << "c-msg " << addr << ": " << dump(decoded) << std::endl;

                    dev_config devconf;

                    const char *pData = decoded.c_str();

                    uint8_t len = fromPtr<uint8_t>(pData++);   // len
                    devconf.rfaddr = fromPtr<rfaddr_t>(pData, 3);
                    pData += 3;
                    devconf.devtype = devicetype(fromPtr<uint8_t>(pData++));
                    devconf.room_id = fromPtr<uint8_t>(pData++);
                    devconf.fwversion = fromPtr<uint8_t>(pData++);
                    uint8_t tres = fromPtr<uint8_t>(pData++);
                    devconf.serial = std::string(pData, pData + 10);
                    pData += 10;

                    LogV("c-msg len " << uint16_t(len)
                              << " rfaddr " << std::hex << devconf.rfaddr << std::dec
                              << " dev " << uint16_t(devconf.devtype)
                              << " room " << uint16_t(devconf.room_id)
                              << " fwv " << uint16_t(devconf.fwversion)
                              << " serial " << devconf.serial)

                    switch (devconf.devtype)
                    {
                        case devicetype::Cube:
                            break;
                        case devicetype::RadiatorThermostat:
                        case devicetype::RadiatorThermostatPlus:
                            {
                                radiatorThermostat_config rthc;
                                rthc.comfort = fromPtr<uint8_t>(pData++) / 2.0;
                                rthc.eco = fromPtr<uint8_t>(pData++) / 2.0;
                                rthc.max = fromPtr<uint8_t>(pData++) / 2.0;;
                                rthc.min = fromPtr<uint8_t>(pData++) / 2.0;
                                rthc.tofs = fromPtr<uint8_t>(pData++) / 2.0 + 3.5;
                                LogV("radiatorThermostat "
                                          << " comfort " << rthc.comfort
                                          << " eco " << rthc.eco
                                          << " min " << rthc.min
                                          << " max " << rthc.max
                                          << " tofs " << rthc.tofs)
                                devconf.specific = rthc;
                            }
                            break;
                        case devicetype::WallThermostat:
                            {
                                wallThermostat_config wthc;
                                wthc.comfort = fromPtr<uint8_t>(pData++) / 2.0;
                                wthc.eco = fromPtr<uint8_t>(pData++) / 2.0;
                                wthc.max = fromPtr<uint8_t>(pData++) / 2.0;
                                wthc.min = fromPtr<uint8_t>(pData++) / 2.0;
                                LogV("wallThermostat "
                                          << " comfort " << wthc.comfort
                                          << " eco " << wthc.eco
                                          << " min " << wthc.min
                                          << " max " << wthc.max)
                                devconf.specific = wthc;
                            }
                            break;
                    }
                    _p->devconfigs.devconf[devconf.rfaddr] = devconf;
                }
                else
                    LogE("invalid C-Message")
            }
            break;
        }
    }
}

struct cube_io::rfaddr_related
{
    dev_config *p_dev_config;
    room_conf  *p_room_conf;
    room_data  *p_room_data;
    rfaddr_related()
        : p_dev_config(nullptr)
        , p_room_conf(nullptr)
        , p_room_data(nullptr)
    {}
};

cube_io::rfaddr_related cube_io::search(rfaddr_t addr)
{
    using devicemap = device_data_store::devicemap;
    using roomconfmap = device_data_store::roomconfmap;
    using roomdatamap = device_data_store::roomdatamap;

    devicemap::iterator
            it = _p->devconfigs.devconf.find(addr);

    if (it != _p->devconfigs.devconf.end())
    {
        rfaddr_related rrd;
        rrd.p_dev_config = &(it->second);

        dev_config &dc = _p->devconfigs.devconf[addr];
        roomconfmap::iterator rcm_it = _p->devconfigs.roomconf.find(dc.room_id);
        if (rcm_it != _p->devconfigs.roomconf.end())
            rrd.p_room_conf = &(rcm_it->second);
        roomdatamap::iterator rdm_it = _p->devconfigs.rooms.find(dc.room_id);
        if (rdm_it != _p->devconfigs.rooms.end())
            rrd.p_room_data = &(rdm_it->second);
        return rrd;
    }

    return rfaddr_related();
}

room_sp gen_rsp(const room_conf &rc, const room_data &rd, changeflag_set &&cfs, unsigned version)
{
    room_sp rsp = std::make_shared<room>();
    rsp->name = rc.name;
    rsp->changed = cfs;
    rsp->set_temp = rd.set;
    rsp->actual_temp = rd.act;
    rsp->act_changed_time = rd.acttime;
    rsp->mode = 18;
    rsp->version = version;
    return rsp;
}

void cube_io::deploydata(const l_submsg_data &smd)
{
    rfaddr_related rfa = search(smd.rfaddr);
    if (rfa.p_dev_config && rfa.p_room_conf)
    {
        if (!rfa.p_room_data)
        {
            rfa.p_room_data = &(_p->devconfigs.rooms[rfa.p_dev_config->room_id]);
        }
        room_data &rd = *rfa.p_room_data;
        room_conf &rf = *rfa.p_room_conf;

        changeflag_set rcfs;
        switch (smd.submsg_src)
        {
        case devicetype::RadiatorThermostatPlus:
        case devicetype::RadiatorThermostat:
            // if (!rf.wallthermostat)
            {
                if ((smd.act_temp != 0.0) &&
                        rd.change(rd.act, smd.act_temp, rcfs, changeflags::act_temp))
                    rd.change(rd.acttime, std::chrono::system_clock::now(), rcfs, changeflags::act_temp);
                rd.change(rd.set, smd.set_temp, rcfs, changeflags::set_temp);
                rd.change(rd.flags[smd.rfaddr], smd.flags, rcfs, changeflags::contained_devs);
            }
            break;
        case devicetype::WallThermostat:
            LogI("emit data")
            if (rd.change(rd.act, smd.act_temp, rcfs, changeflags::act_temp))
                rd.change(rd.acttime, std::chrono::system_clock::now(), rcfs, changeflags::act_temp);
            rfa.p_room_data->act = smd.act_temp;
            rd.change(rd.set, smd.set_temp, rcfs, changeflags::set_temp);
            rd.change(rd.flags[smd.rfaddr], smd.flags, rcfs, changeflags::contained_devs);
            break;
        default: ;
        }
        if (rcfs.size())
        {   // we have changes
            unsigned vers = 0;
            if (_p->emit_rooms.find(rf.name) != _p->emit_rooms.end())
                vers = _p->emit_rooms[rf.name]->version;

            room_sp newsp = gen_rsp(rf, rd, std::move(rcfs), vers);
            _p->emit_rooms[rf.name] = newsp;
            if (_p->iet)
                _p->iet->room_changed(newsp);
        }
    }
    else
    {
        LogE("data incomplete")
    }
}

} // ns max_eq3

namespace  {

using namespace max_eq3;

bool l_response(std::string &&decoded, max_eq3::l_submsg_data &adata)
{
    // std::cout << "l_rsp " << dump(decoded) << std::endl;
    unsigned len = *(reinterpret_cast<uint8_t *>(&decoded[0]));
    adata.rfaddr = fromPtr<uint32_t>(&decoded[1], 3);
    uint8_t unknown = fromPtr<uint8_t>(&decoded[4]);
    adata.flags = fromPtr<uint16_t>(&decoded[5]);
    if (len > 6)
    {
        if (len >= 11)
        {
            adata.valve_pos = fromPtr<uint8_t>(&decoded[7+0]);
            adata.set_temp = (fromPtr<uint8_t>(&decoded[7+1]) & 0x7f) / 2.0;
            adata.minutes_since_midnight = fromPtr<uint8_t>(&decoded[7+4])*30;
            uint16_t act_temp = 0;
            if (len == 11) // thermostat of wallthermostat
            {
                adata.submsg_src = devicetype::RadiatorThermostat;
                act_temp = (fromPtr<uint16_t>(&decoded[7+2]) & 0x1ff);
            }
            else if (len == 12)
            {
                adata.submsg_src = devicetype::WallThermostat;
                act_temp = (uint16_t(decoded[7+1] & 0x80) << 1) + fromPtr<uint8_t>(&decoded[7+5]);
                adata.dateuntil = fromPtr<uint16_t>(&decoded[7+2]);
            }
            else
            {
                LogE("L-Msg: unprocessed data of length " << len)
            }
            adata.act_temp = act_temp / 10.0;
            return true;
        }
    }
    else {
        LogE("L-Msg: unprocessed data of length " << len)
    }
    return false;
}

bool m_response(std::string &&rawdata,
                std::list<max_eq3::m_room> &roomlist,
                std::list<max_eq3::m_device> &devicelist)
{
    rawdata.erase(0,2);
    std::vector<std::string> sarray;
    boost::split(sarray, rawdata, boost::is_any_of(","), boost::token_compress_on);
    std::string decoded = decode64(sarray[2]);
    // room data
    const char *pData = &decoded[2];
    unsigned roomcnt = uint8_t(*pData++); // decoded[0];
    for (unsigned u = 0; u < roomcnt; ++u)
    {
        max_eq3::m_room rd;
        rd.id = fromPtr<uint8_t>(pData++); // &decoded[0]);
        rd.name = std::string(&pData[1], size_t(pData[0]));
        pData += (rd.name.size() + 1);
        rd.group_rfaddr = fromPtr<uint32_t>(&pData[0], 3);
        pData += 3;
        roomlist.push_back(rd);
    }
    // device data
    unsigned devcnt = *pData++;
    for (unsigned u = 0; u < devcnt; ++u)
    {
        max_eq3::m_device dd;
        dd.type = devicetype(*pData++);
        dd.rfaddr = fromPtr<uint32_t>(&pData[0], 3);
        pData += 3;
        dd.serial = std::string(&pData[0], 10);
        pData += 10;
        dd.name = std::string(&pData[1], pData[0]);
        pData += (1 + dd.name.size());
        dd.room_id = fromPtr<uint8_t>(pData++);
        devicelist.push_back(dd);
    }
    return true;
}


} // ns -
