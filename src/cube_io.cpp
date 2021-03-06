
#include <algorithm>
#include <chrono>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "cubio_io_p.h"
#include "cube_log_internal.h"
#include "utils.h"
#include "io_operator.h"

#define MULTICAST		"224.0.0.1"
#define MAX_UDP_PORT		23272
#define MAX_TCP_PORT		62910

namespace  {

std::string cdt = "eQ3Max*\0*********I";
std::string cdts("eQ3Max*\0", 8);
std::string cdte = "I";
std::string cdtu = "**********";

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

max_eq3::week_schedule get_schedule(const uint8_t *pD);

const max_eq3::room_conf * get_room_config(const std::string &room, const max_eq3::device_data_store::roomconfmap &cmap)
{
    const max_eq3::room_conf * roomconfig = nullptr;

    for (const auto &v: cmap)
    {
        if (room == v.second.name)
            roomconfig = &v.second;
    }
    return roomconfig;
}


}

namespace max_eq3 {

#if 0
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
    opmode      get_opmode() const
        { return static_cast<opmode>(flags &3 ); }
};
#endif

cube_event_target::~cube_event_target()
{}

void cube_io::set_logger(logging_target *target)
{
    set_log_target(target);
}

cube_io::cube_io(cube_event_target *iet, const std::string &serialno)
    : _p(new Private)
{
    _p->serial = serialno;
    _p->iet = iet;
    _p->io_thread = std::thread(std::bind(&cube_io::process_io, this));
}

cube_io::~cube_io()
{
    _p->io.post([this](){
        ba::async_write(_p->cube->sock, ba::buffer("q:\r\n"), [](const boost::system::error_code &e, std::size_t bytes_transferred){});
    });
    _p->io.stop();
    _p->io_thread.join();
}

void cube_io::change_temp(const std::string &room, double temp)
{
    LogI(__FUNCTION__ << " for " << room << " to " << temp);
    _p->io.post(boost::bind(&cube_io::do_send_temp, this, room, temp));
}

void cube_io::change_mode(const std::string &room, opmode mode)
{
    LogI(__FUNCTION__ << " for " << room << " to " << mode_as_string(mode));
    _p->io.post(boost::bind(&cube_io::do_send_mode, this, room, mode));
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

        std::string mcreq;
        if (_p->serial.size())
        {
            mcreq = cdts + _p->serial + cdte;
        }
        else
        {
            mcreq = cdts + cdtu + cdte;
        }
        LogV("size of serial req " << mcreq.size() << " [" << dump(mcreq) << "]\n")

        _p->socket.async_send_to(
            ba::buffer(mcreq, mcreq.size()),
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
                            // else
                            //    LogV("refresh started " << e << ": " << bytes_transferred)
                        }
        );
    }
}

void cube_io::rxrh_done(cube_sp csp, const boost::system::error_code& e, std::size_t bytes_recvd)
{
    LogV("rxrh_done\n")
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

void cube_io::restart_wait_timer(cube_sp &csp)
{
    csp->refreshtimer.cancel();
    csp->refreshtimer.expires_after(std::chrono::seconds(_p->short_refresh ? 5 : 30));
    csp->refreshtimer.async_wait(
                boost::bind(&cube_io::timed_refresh,
                            this,
                            csp,
                            ba::placeholders::error)
                );
    _p->short_refresh = false;
}

void cube_io::start_rx_from_cube(cube_sp &csp)
{
    // update_config(csp);
    restart_wait_timer(csp);
    LogV("start_rx_from_cube\n")
    ba::async_read_until(csp->sock, csp->rxdata, "\r\n",
                        boost::bind(&cube_io::rxrh_done, this, csp,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred
                                    )
                         );

}

void cube_io::update_config(cube_sp csp)
{
    std::string txcmd;
    if (_p->rcvd_configs.size())
    {
        const auto t = _p->rcvd_configs.begin();
        switch (*t)
        {
            case cnf_tags::rd_timeserver:
                txcmd = "f:\r\n";
                break;
        }
    }

    LogV("query config " << txcmd)
    if (txcmd.size())
    {
        ba::async_write(csp->sock,
            ba::buffer(txcmd, txcmd.size()),
            [](const boost::system::error_code &e, std::size_t bytes_transferred)
            {
                if (e)
                    LogE("write failed on update config")
                else
                {
                    LogV("cnf query write done " << e << ": " << bytes_transferred)
                }
            }
        );

    }
}

void cube_io::process_connect(cube_sp cube, const bs::error_code &ec)
{
    if (ec)
    {
        LogE("connect failed " << ec)
        std::cout << "cube connect failed: " << ec << std::endl;
        std::unique_ptr<ba::steady_timer> timer = std::make_unique<ba::steady_timer>(_p->io,
                                                          std::chrono::steady_clock::now() + std::chrono::seconds(10));
        timer->async_wait([&timer, this](boost::system::error_code ec){
            std::cout << "reconnect timeout\n";
            this->process_io();
            timer.reset();
        });
    }
    else
    {
        LogV("connected")
        _p->cube = cube;
        start_rx_from_cube(_p->cube);
    }
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

                bool connect = !_p->cube;
                if (connect)
                {
                    ba::ip::tcp::endpoint ep(cube->addr, 62910);
                    boost::system::error_code ec;
                    LogV("connect to " << ep)

                    cube->sock.async_connect(ep, boost::bind(&cube_io::process_connect, this, cube, ba::placeholders::error));
                    /*
                    cube->sock.connect(ep, ec);
                    if (!ec)
                        _p->cube = cube;
                    else
                        LogV("connecting failed to " << ep)
                    */
                }
                // start_rx_from_cube(cube);
            }
        }
        else if (bytes_recvd == 19)
        {
            LogV("rcvd bcast himself");
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
        case 'S':
            {
               LogV("S-msg: " << dump(data))
               std::vector<std::string> inp;
               data.erase(0,2);
               boost::split(inp, data, boost::is_any_of(","), boost::token_compress_on);
               if (inp.size() != 3)
                   LogE("wrong S message recived ")
               else
               {
                   try {
                       unsigned dutycycle = boost::lexical_cast<unsigned>(inp[0]);
                       bool rspvalid = boost::lexical_cast<unsigned>(inp[1]);
                       unsigned freeslots = boost::lexical_cast<unsigned>(inp[2]);
                       LogV("dutycycle: " << dutycycle << "% cmd: " << (rspvalid ? "failed" : "ok") << " freeslots: " << freeslots)
                   } catch (boost::bad_lexical_cast &) {

                   }
               }
               _p->short_refresh = true;
            }
            break;
        case 'H':
            {
                LogV("H-msg: " << dump(data))
                data.erase(0, 2);
                std::vector<std::string> comma_separated;
                boost::split(comma_separated, data, boost::is_any_of(","), boost::token_compress_off);
                csp->serial = comma_separated[0]; // data.substr(0,10);
                uint32_t newrfaddr = rfaddr_from_string(comma_separated[1]);
                {
                    std::istringstream iss(comma_separated[5]);
                    // csp->duty_cycle = boost::lexical_cast<uint16_from_hex>(comma_separated[5]);
                    iss >> std::hex >> csp->duty_cycle;
                }

                if (newrfaddr != csp->rfaddr)
                    LogE("rfaddr mismatch " << std::hex << newrfaddr << " != "
                              << csp->rfaddr << std::dec
                              << " from " << dump(data))

                LogV(
                      "serial " << csp->serial
                      << " duty: " << csp->duty_cycle
                      << " date: " << comma_separated[7]
                      << " time: "  << comma_separated[8]
                     )
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
                         << " grp_rfaddr: " << std::hex << r.group_rfaddr << std::dec
                         << " n:" << r.name)

                    room_conf &rc = _p->devconfigs.roomconf[r.id];
                    rc.cube_rfaddr = csp->rfaddr;
                    rc.name = r.name;
                    rc.rfaddr = r.group_rfaddr;
                    rc.id = r.id;

                    room_data &rd = _p->devconfigs.rooms[r.id];
                }
                for (const m_device &d: devices)
                {
                    _p->device_defs[d.rfaddr] = d;
                    LogV("dev: " << std::hex << d.rfaddr << std::dec
                         << " n:" << d.name << " t:" << uint16_t(d.type) << " sn:" << d.serial
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
                    dc.name = d.name;
                    std::cout << "dev name " << dc.name << std::endl;
                }
            }
            break;
        case 'L':
            {
                LogI("process L-Msg");
                std::string decoded = decode64(data.substr(2, data.size() - 3));

                std::map<std::string, std::string> info;
                while (decoded.size())
                {
                    unsigned submsglen = decoded[0];

                    l_submsg_data adata;
                    if (l_response(decoded.substr(0, submsglen + 1), adata))
                    {
                        _p->device_data[adata.rfaddr] = adata;
                        std::ostringstream devinfo;
                        devinfo << std::setw(25) << _p->devconfigs.dev_name_from_rfaddr(adata.rfaddr)
                                << ":  " << l_submsg_as_string(adata);
                        info[_p->devconfigs.room_from_rfaddr(adata.rfaddr)] += std::string('\n' + devinfo.str()); // l_submsg_as_string(adata));
                        deploydata(adata);
                    }
                    else
                        LogE("l_resp failed")

                    decoded.erase(0, submsglen + 1);
                }
                LogI("ldevs size " << info.size())
                for (const auto &n: info)
                    LogI("devices: " << n.first << n.second)

                emit_changed_data();
            }
            break;
        case 'F':
            {
                LogI("process F-Msg " << data.substr(2));
                _p->rcvd_configs.erase(cnf_tags::rd_timeserver);
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

                    dev_config devconf;

                    const char *pData = decoded.c_str();

                    uint8_t len = fromPtr<uint8_t>(pData++);   // len
                    devconf.rfaddr = fromPtr<rfaddr_t>(pData, 3);

                    if (_p->devconfigs.devconf.find(devconf.rfaddr) != _p->devconfigs.devconf.end())
                    {
                        devconf = _p->devconfigs.devconf[devconf.rfaddr];
                    }

                    pData += 3;
                    devconf.devtype = devicetype(fromPtr<uint8_t>(pData++));
                    devconf.room_id = fromPtr<uint8_t>(pData++);
                    devconf.fwversion = fromPtr<uint8_t>(pData++);
                    uint8_t tres = fromPtr<uint8_t>(pData++);
                    devconf.serial = std::string(pData, pData + 10);
                    pData += 10;

                    LogV("C-msg len " << uint16_t(len)
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
                                pData += 6;
                                // points to week programs
                                rthc.schedule = get_schedule(reinterpret_cast<const uint8_t*>(pData));
                                LogV("radiatorThermostat "
                                          << " comfort " << rthc.comfort
                                          << " eco " << rthc.eco
                                          << " min " << rthc.min
                                          << " max " << rthc.max
                                          << " tofs " << rthc.tofs
                                          << " sched\n" << rthc.schedule)
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
                                pData += (0x1d - 0x15);
                                wthc.schedule = get_schedule(reinterpret_cast<const uint8_t*>(pData));
                                LogV("wallThermostat "
                                          << " comfort " << wthc.comfort
                                          << " eco " << wthc.eco
                                          << " min " << wthc.min
                                          << " max " << wthc.max
                                          << " sched\n" << wthc.schedule
                                     )
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
        default:
            LogV("unhandle Message " << data[0] << std::endl)
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

room_sp gen_rsp(const room_conf &rc, const room_data &rd, const changeflag_set &cfs, unsigned last_version)
{
    room_sp rsp = std::make_shared<room>();
    rsp->name = rc.name;
    rsp->changed = cfs;
    rsp->set_temp = rd.set;
    rsp->actual_temp = rd.act;
    // rsp->act_changed_time = rd.acttime;
    rsp->version = last_version+1;
    rsp->mode = rd.mode;
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
            {
                if (smd.act_temp != 0.0)
                    rd.change(rd.act, smd.act_temp, rcfs, changeflags::act_temp);
                rd.change(rd.set, smd.set_temp, rcfs, changeflags::set_temp);
                // LogI("mode " << std::hex << int(rd.mode) << " -- " << (smd.flags & 0x3) << " rfaddr " << smd.rfaddr << std::dec);
                rd.change(rd.mode, static_cast<opmode>(smd.flags & 0x3), rcfs, changeflags::mode);
                rd.change(rd.flags[smd.rfaddr], smd.flags, rcfs, changeflags::contained_devs);
                rd.change(smd.rfaddr, smd.valve_pos, rcfs);
            }
            break;
        case devicetype::WallThermostat:
            rd.change(rd.act, smd.act_temp, rcfs, changeflags::act_temp);
            rd.change(rd.set, smd.set_temp, rcfs, changeflags::set_temp);
            // LogI("mode wt " << std::hex << int(rd.mode) << " -- " << (smd.flags & 0x3) << " rfaddr " << smd.rfaddr <<  std::dec);
            rd.change(rd.mode, static_cast<opmode>(smd.flags & 0x3), rcfs, changeflags::mode);
            rd.change(rd.flags[smd.rfaddr], smd.flags, rcfs, changeflags::contained_devs);
            break;
        default: ;
        }

        if (rcfs.size())    // we have changes
            _p->changeset[rf.id].insert(rcfs.begin(), rcfs.end());
    }
    else
    {
        LogE("data incomplete")
    }
}

void cube_io::emit_changed_data()
{
    if (_p->iet)
    {
        if (!_p->deviceinfo)
        {
            if (_p->cube)
            {
                device_sp newdev = std::make_shared<device>();

                newdev->name = _p->cube->serial;
                newdev->addr = _p->cube->addr.to_string();
                LogI("newdev " << newdev->name << " a: " << newdev->addr)
                _p->deviceinfo = newdev;
                _p->iet->device_info(newdev);
            }
        }
        for (const auto val: _p->changeset)
        {
            unsigned roomid = val.first;

            device_data_store::roomdatamap::const_iterator
                    rdmcit = _p->devconfigs.rooms.find(roomid);

            device_data_store::roomconfmap::const_iterator
                    rcfcit = _p->devconfigs.roomconf.find(roomid);

            if ((rdmcit != _p->devconfigs.rooms.end())
                    && (rcfcit != _p->devconfigs.roomconf.end()))
            {

                rfaddr_t for_schedule = 0;

                // calc valve pos for room
                timestamped_valve_pos valvepossum;
                for (const auto x: rdmcit->second.valve_pos)
                {
                    timestamped_valve_pos cur = x.second;
                    valvepossum.first += cur.first;
                    if (valvepossum.second < cur.second)
                        valvepossum.second = cur.second;
                    if (for_schedule == 0)
                        for_schedule = x.first;
                }
                valvepossum.first /= rdmcit->second.valve_pos.size();

                unsigned vers = 0;
                if (_p->emit_rooms.find(roomid) != _p->emit_rooms.end())
                    vers = _p->emit_rooms[roomid]->version;

                room_sp newsp = gen_rsp(rcfcit->second, rdmcit->second, val.second, vers);

                newsp->valve_pos = valvepossum;

                // from room id -> schedule
                rfaddr_t devrfaddr = rcfcit->second.wallthermostat;

                if (!devrfaddr)
                    devrfaddr = *rcfcit->second.thermostats.begin();

                if (for_schedule)
                {
                    auto cit = _p->devconfigs.devconf.find(for_schedule);
                    if (cit != _p->devconfigs.devconf.end())
                    {
                        dev_config dc = _p->devconfigs.devconf.find(for_schedule)->second;
                        if (std::holds_alternative<radiatorThermostat_config>(dc.specific))
                        {
                            const radiatorThermostat_config *pConf = std::get_if<radiatorThermostat_config>(&dc.specific);
                            if (pConf)
                            {
                                newsp->schedule = pConf->schedule;
                            }
                            else
                                std::cout << "schedule missing for room: " << rcfcit->second.name << std::endl;
                        }
                    }
                }

                _p->emit_rooms[roomid] = newsp;

                _p->iet->room_changed(newsp);
            }

        }
        _p->changeset.clear();
    }
}

template<typename T>
void append(std::ostream &os, T t, std::size_t sz)
{
    const char *pstart = reinterpret_cast<const char *>(&t);
    pstart = pstart + sizeof(T);
    int diff = sizeof(T) - sz;
    if (diff >= 0)
    {
        pstart = pstart - diff;
        for (unsigned u = 0; u < sz; ++u)
        {
            pstart--;
            os.write(pstart, 1);
        }
    }
    else
        LogE("conversion problem within " << __PRETTY_FUNCTION__ << std::endl);

}

const room_conf *roomconf_by_name(const device_data_store &dds, const std::string &n)
{
    for (const auto &v: dds.roomconf)
    {
        if (n == v.second.name)
            return &v.second;
    }
    return nullptr;
}

const room_data *roomdata_by_id(const device_data_store &dds, unsigned id)
{
    device_data_store::roomdatamap::const_iterator cit = dds.rooms.find(id);
    if (cit == dds.rooms.end())
        return nullptr;
    return &cit->second;
}


void cube_io::do_send_mode(std::string room, opmode mode)
{
    LogV(__FUNCTION__ << " for room " << room << " to " << int(mode) << std::endl);

    const room_conf * roomconfig = roomconf_by_name(_p->devconfigs, room);
    if (!roomconfig)
    {
        LogE("no room found for name " << room << std::endl)
        return;
    }
    const room_data * roomdata = roomdata_by_id(_p->devconfigs, roomconfig->id);
    if (!roomdata)
    {
        LogE("no room found for id " << roomconfig->id << std::endl)
        return;
    }

    uint8_t tmp = uint8_t(roomdata->set.first * 2);

    if ((tmp & 0xC0) != 0)
    {
        LogE("wrong mode : " << std::hex << tmp << std::endl)
        return;
    }

    switch (mode)
    {
    case opmode::AUTO:
        break;
    case opmode::MANUAL:
        tmp |= 0x40;
        break;
    case opmode::VACATION:
        tmp |= 0x80;
        break;
    case opmode::BOOST:
        tmp |= 0xC0;
        break;
    }

    emit_S_temp_mode(roomconfig->rfaddr, roomconfig->id, tmp);

}

void cube_io::emit_S_temp_mode(rfaddr_t sendto, uint8_t roomid, uint8_t tmp_mode)
{
    std::ostringstream xs;

    append(xs, uint8_t(0), 1);              // Unknown
    append(xs, uint8_t(4), 1);              // adress room
    append(xs, uint8_t(0x40), 1);           // set Temperatur
    append(xs, unsigned(0), 3);             // from rfaddr

    // if roomconfig->rfaddr is zero all room are affected
    // on room without rfaddr is set we should adress the radiator directly

    append(xs, uint32_t(sendto), 3);            // to rfaddr
    append(xs, uint8_t(roomid), 1);     // roomid
    append(xs, tmp_mode, 1);                         // temp

    std::string encoded = encode64(xs.str());

#if 0
    LogV("unencoded " << dump(xs.str()) << std::endl)
    LogV("encoded " << encoded << std::endl)
    LogV("redecoded " << dump(decode64(encoded)) << std::endl)
#endif
    std::string cmd2send = "s:" + encoded + "\r\n";
    LogV("should send " << dump(cmd2send) << std::endl)

    if (!_p->cube) // (_p->cubes.find(cubeto) == _p->cubes.end())
    {
        LogE("unable to find associated cube\n")
        return;
    }

    auto csp = _p->cube; // _p->cubes[cubeto];

    ba::async_write(csp->sock,
                    ba::buffer(cmd2send),
                    boost::bind(&cube_io::do_send_l_msg, this, _1, _2)
    );
}

namespace {
}

void cube_io::do_send_schedule(std::string room, days day, const day_schedule ds)
{
    LogV(__FUNCTION__ << " for room " << room << " to " << ds << std::endl)

    const room_conf * roomconfig = get_room_config(room, _p->devconfigs.roomconf);
    if (roomconfig == nullptr)
    {
        LogE("no room found for name " << room << std::endl)
        return;
    }
    if (_p->devconfigs.rooms.find(roomconfig->id) == _p->devconfigs.rooms.end())
    {
        LogE("no roomdata found for name " << room << std::endl)
        return;
    }
    room_data &roomdata = _p->devconfigs.rooms[roomconfig->id];

    rfaddr_t sendto = roomconfig->rfaddr;
    if (sendto == 0)
    {
        if (roomconfig->wallthermostat)
            sendto = roomconfig->wallthermostat;
        if (roomconfig->thermostats.size())
        {
            sendto = *roomconfig->thermostats.begin();
        }
    }

    std::ostringstream xs;

    append(xs, uint8_t(0), 1);              // Unknown
    append(xs, uint8_t(4), 1);              // adress room
    append(xs, uint8_t(0x10), 1);           // set Temperatur
    append(xs, unsigned(0), 3);             // from rfaddr

    append(xs, uint32_t(sendto), 3);            // to rfaddr
    append(xs, uint8_t(roomconfig->id), 1);     // roomid
    append(xs, uint8_t(day), 1);                // day

    for (const auto n: ds)
    {
        uint16_t time = std::round(std::min(n.temp * 2, 128.0));
        uint16_t date = n.minutes_since_midnight / 5;
        uint16_t combined = (time << 9) + date;
        append(xs, uint16_t(combined), 2);
    }

    // mark stored schedule data dirty

}

void cube_io::do_send_l_msg(const boost::system::error_code &e, std::size_t bytes_transferred)
{
    if (e)
        LogE("set temp failed " << e << std::endl)
    else
        LogV("set temp done " << bytes_transferred << std::endl)

    // force a reread
    LogV("send l")
    ba::async_write(_p->cube->sock, ba::buffer("l:\r\n"), [](const boost::system::error_code &e, std::size_t bytes_transferred){});
}

void cube_io::do_send_temp(std::string room, double temp)
{
    const room_conf * roomconfig = nullptr;

    for (const auto &v: _p->devconfigs.roomconf)
    {
        if (room == v.second.name)
            roomconfig = &v.second;
    }
    if (roomconfig == nullptr)
    {
        LogE("no room found for name " << room << std::endl)
        return;
    }

    if (_p->devconfigs.rooms.find(roomconfig->id) == _p->devconfigs.rooms.end())
    {
        LogE("no roomdata found for name " << room << std::endl)
        return;
    }
    room_data &roomdata = _p->devconfigs.rooms[roomconfig->id];

    LogV(__FUNCTION__ << " for room " << room << ':' << roomconfig->id << " to " << temp)

    uint8_t tmp = uint8_t(temp * 2);

    if ((tmp & 0xC0) != 0)
    {
        LogE("temp to high : " << temp << std::endl)
        return;
    }

    switch (roomdata.mode)
    {
    case opmode::AUTO:
        break;
    case opmode::MANUAL:
        tmp |= 0x40;
        break;
    case opmode::VACATION:
        tmp |= 0x80;
        break;
    case opmode::BOOST:
        tmp |= 0xC0;
        break;
    }

    rfaddr_t sendto = roomconfig->rfaddr;
    if (sendto == 0)
    {
        if (roomconfig->wallthermostat)
            sendto = roomconfig->wallthermostat;
        if (roomconfig->thermostats.size())
        {
            sendto = *roomconfig->thermostats.begin();
        }
    }

    std::ostringstream xs;

    append(xs, uint8_t(0), 1);              // Unknown
    append(xs, uint8_t(4), 1);              // adress room
    append(xs, uint8_t(0x40), 1);           // set Temperatur
    append(xs, unsigned(0), 3);             // from rfaddr

    // if roomconfig->rfaddr is zero all room are affected
    // on room without rfaddr is set we should adress the radiator directly

    append(xs, uint32_t(sendto), 3);            // to rfaddr
    append(xs, uint8_t(roomconfig->id), 1);     // roomid
    append(xs, tmp, 1);                         // temp

    std::string encoded = encode64(xs.str());

#if 0
    LogV("unencoded" << dump(xs.str()) << std::endl)
    LogV("encoded" << encoded << std::endl)
    LogV("redecoded" << dump(decode64(encoded)) << std::endl)
#endif
    std::string cmd2send = "s:" + encoded + "\r\n";
    LogV("should send " << dump(cmd2send) << std::endl)

    if (!_p->cube) // (_p->cubes.find(roomconfig->cube_rfaddr) == _p->cubes.end())
    {
        LogE("unable to find associated cube\n")
        return;
    }

    auto csp = _p->cube; // _p->cubes[roomconfig->cube_rfaddr];
    ba::async_write(csp->sock,
                    ba::buffer(cmd2send),
                    boost::bind(&cube_io::do_send_l_msg, this, _1, _2)     // force a reload
    );
}

} // ns max_eq3

namespace  {

using namespace max_eq3;

bool isvalid(uint16_t flags)
{
    return (flags & 0x1000) == 0x1000;
}

bool l_response(std::string &&decoded, max_eq3::l_submsg_data &adata)
{    
    unsigned len = *(reinterpret_cast<uint8_t *>(&decoded[0]));
    adata.rfaddr = fromPtr<uint32_t>(&decoded[1], 3);
    uint8_t unknown = fromPtr<uint8_t>(&decoded[4]);
    adata.flags = fromPtr<uint16_t>(&decoded[5]);
    // LogI("flags:" << flags_as_string(adata.flags));

    if (len > 6)
    {
        if (isvalid(adata.flags))
        {
            if (len >= 11)
            {
                adata.valve_pos = fromPtr<uint8_t>(&decoded[7+0]);
                adata.set_temp = (fromPtr<uint8_t>(&decoded[7+1]) & 0x7f) / 2.0;
                adata.minutes_since_midnight = fromPtr<uint8_t>(&decoded[7+4])*30;
                uint16_t act_temp = 0;
                if (len == 11) // thermostat or thermostat+
                {
                    adata.submsg_src = devicetype::RadiatorThermostat;
                    act_temp = (fromPtr<uint16_t>(&decoded[7+2]) & 0x1ff);
                }
                else if (len == 12) // wallthermostat
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
        else
        {
            LogE("Invalid Data reported for device " << std::hex << adata.rfaddr << std::dec)
        }
    }
    else {
        LogE("L-Msg: unprocessed data of length " << len)
    }
    return false;
}

week_schedule get_schedule(const uint8_t *pD)
{
    week_schedule ws;
    for (unsigned u = 0; u < DAYS_A_WEEK; ++u)
    {
        for (unsigned x = 0; x < SCHED_POINTS; ++x)
        {
            const uint8_t &lsb = pD[0];
            const uint8_t &msb = pD[1];

            ws[u][x].temp = (lsb >> 1) / 2.0;
            unsigned until = (lsb & 1 ? 0x100 : 0);
            until += msb;
            ws[u][x].minutes_since_midnight = until * 5;
            // LogV("sd " << u << ':' << x << " " << ws[u][x].temp << "°C " << ws[u][x].minutes_since_midnight)
            pD += 2;

        }
    }
    return ws;
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
