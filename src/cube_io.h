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

enum struct changeflags : uint8_t {
    set_temp,
    act_temp,
    mode,
    config,
    contained_devs,
    valve_pos,
};

enum struct opmode {
    AUTO,
    MANUAL,
    VACATION,
    BOOST,
};

struct schedule_point
{
    double temp;
    unsigned minutes_since_midnight;
};

enum {
    Saturday = 0,
    Sunday,
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday
};

#define DAYS_A_WEEK 7
#define SCHED_POINTS 13

using day_schedule = std::array<schedule_point, SCHED_POINTS>;
using week_schedule = std::array<day_schedule, DAYS_A_WEEK>;

using changeflag_set = std::set<changeflags>;

using timestamped_valve_pos = std::pair<uint16_t, std::chrono::system_clock::time_point>;
using timestamped_temp = std::pair<double, std::chrono::system_clock::time_point>;

typedef struct room
{
    std::string         name;
    timestamped_temp    set_temp;
    timestamped_temp    actual_temp;
    opmode              mode{opmode::AUTO};
    timestamped_valve_pos
                        valve_pos;

    week_schedule       schedule;

    unsigned            version;                // increments with every creation
    changeflag_set      changed;
    room()
        : valve_pos(0,std::chrono::system_clock::now())
    {}
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
    cube_io(cube_event_target *iet, const std::string &serialno);
    ~cube_io();

    // room api
    void change_set_temp(const std::string &room, double temp);
    void change_mode(const std::string &room, opmode mode);
    void get_details(const std::string &room, room_details &rd);

    static void set_logger(logging_target *target);

private:

    // asio in process cmd handler
    void do_send_temp(std::string room, double temp);
    void do_send_mode(std::string room, opmode mode);

    void emit_S_temp_mode(rfaddr_t cubeto, rfaddr_t sendto, uint8_t roomid, uint8_t tmp_mode);

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
private:
    struct Private;
    std::unique_ptr<Private> _p;
};

}
#endif // MAX_IO_H
