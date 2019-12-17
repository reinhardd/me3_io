#ifndef MAX_IO_H
#define MAX_IO_H
#pragma once

#include <iomanip>
#include <thread>
#include <deque>
#include <chrono>
#include <memory>
#include <variant>
#include <set>
#include <array>

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include "cube.h"

namespace boost {
    namespace system {
        class error_code;
    }
}

namespace max_eq3 {

//internal forwards

struct l_submsg_data;

class logging_target;

// definitions

enum struct devicetype : uint8_t {
    Cube = 0,
    RadiatorThermostat = 1,
    RadiatorThermostatPlus = 2,
    WallThermostat = 3,
    ShutterContact = 4,
    EcoButton = 5,
    Undefined = 255
};

typedef struct radiator_thermostat
{
    rfaddr_t    addr;
    bool        plus_variant{false};
    double      set_temp{0.0};          // degree celsius
    double      act_temp{0.0};          // degree celsius
    unsigned    valve_pos_percent{0};   // 0..100
} radiator_thermostat;

typedef struct wall_thermostat
{
    rfaddr_t    addr;
    double      set_temp{0.0};          // degree celsius
    double      act_temp{0.0};          // degree celsius
} wall_thermostat;

typedef struct wall_thermostat_config
{
    rfaddr_t addr;
    double comfort, eco, min, max, tofs;
} wall_thermostat_config;

using device_v = std::variant<wall_thermostat, radiator_thermostat>;

struct room_details
{
    rfaddr_t addr;


};

class cube_event_target
{
public:
    virtual ~cube_event_target();
    virtual void device_info(device_sp dsp) = 0;
    virtual void room_changed(room_sp rsp) = 0;
    virtual void connected()  = 0;
    virtual void disconnected() = 0;
};


class cube_io
{
public:
    cube_io(cube_event_target *iet, const std::string &serialno);
    ~cube_io();

    // room api
    void change_set_temp(const std::string &room, double temp);
    void change_mode(const std::string &room, opmode mode);
    void change_schedule(std::string room, const day_schedule &ds);
    void get_details(const std::string &room, room_details &rd);


    static void set_logger(logging_target *target);

private:

    // const room_conf * get_room_config(const std::string &room);

    // asio in process cmd handler
    void do_send_temp(std::string room, double temp);
    void do_send_mode(std::string room, opmode mode);
    void send_temp_done(const boost::system::error_code &e, std::size_t bytes_transferred);
    void do_send_schedule(std::string room, days day, const day_schedule ds);
    void process_connect(cube_sp, const boost::system::error_code &err);

    void emit_S_temp_mode(rfaddr_t sendto, uint8_t roomid, uint8_t tmp_mode);

    // asio internal processing
    void process_io();
    void handle_mcast_response(const boost::system::error_code& error, size_t bytes_recvd);

    // cube related communication handlers
    void start_rx_from_cube(cube_sp &csp);
    void restart_wait_timer(cube_sp &csp);
    void timed_refresh(cube_sp, const boost::system::error_code &);
    void rxrh_done(cube_sp, const boost::system::error_code& e, std::size_t bytes_recvd);

    void evaluate_data(cube_sp, std::string &&data);

    struct rfaddr_related;
    rfaddr_related search(rfaddr_t addr);
    void deploydata(const l_submsg_data &smd);
    void emit_changed_data();
    void update_config(cube_sp csp);
private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
#endif // MAX_IO_H
