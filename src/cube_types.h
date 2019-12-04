#ifndef CUBE_TYPES_H
#define CUBE_TYPES_H
#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <chrono>

namespace max_eq3 {

using rfaddr_t = uint32_t;

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

using changeflag_set = std::set<changeflags>;
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

typedef struct device
{
    std::string name;   // name
    std::string addr;   // ip
} device;

using device_sp = std::shared_ptr<device>;


}

#endif // CUBE_TYPES_H
