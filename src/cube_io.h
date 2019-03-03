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

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

namespace boost {
    namespace system {
        class error_code;
    }
}

namespace max_eq3 {

//internal forwards

struct l_submsg_data;

class logging_target;

enum struct devicetype : uint8_t {
    Cube = 0,
    RadiatorThermostat = 1,
    RadiatorThermostatPlus = 2,
    WallThermostat = 3,
    ShutterContact = 4,
    EcoButton = 5,
    Undefined = 255
};

enum struct changeflags : uint8_t {
    set_temp,
    act_temp,
    mode,
    config,
    contained_devs
};

using changeflag_set = std::set<changeflags>;

typedef struct room
{
    std::string     name;
    double          set_temp{0.0};
    double          actual_temp{0.0};
    unsigned        mode{0};
    std::chrono::system_clock::time_point
                    act_changed_time;
    unsigned        version;                // increments with every creation
    changeflag_set  changed;
} room;

using room_sp = std::shared_ptr<room>;

using rfaddr_t = uint32_t;

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
    virtual void room_changed(room_sp rsp) = 0;
    virtual void connected()  = 0;
    virtual void disconnected() = 0;
};

class cube_t;
using cube_sp = std::shared_ptr<cube_t>;

class cube_io
{
public:
    cube_io(cube_event_target *iet);
    ~cube_io();

    // room api
    void change_set_temp(const std::string &room, double temp);
    void get_details(const std::string &room, room_details &rd);

    static void set_logger(logging_target *target);

private:
    //
    void process_io();
    void handle_mcast_response(const boost::system::error_code& error, size_t bytes_recvd);

    // cube related communication handlers
    void start_rx_from_cube(cube_sp &csp);
    void timed_refresh(cube_sp, const boost::system::error_code &);
    void rxrh_done(cube_sp, const boost::system::error_code& e, std::size_t bytes_recvd);

    void evaluate_data(cube_sp, std::string &&data);       

    struct rfaddr_related;
    rfaddr_related search(rfaddr_t addr);
    void deploydata(const l_submsg_data &smd);
private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
#endif // MAX_IO_H
