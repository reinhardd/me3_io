#ifndef DEV_STORE_H
#define DEV_STORE_H
#pragma once

#include <map>
#include <set>
#include <variant>
#include "cube_io.h"

namespace max_eq3 {

typedef struct cube_config
{
    bool portal_enabled;
    uint8_t pbupconf, pbdnconf;
    std::string portalurl;
} cube_config;

typedef struct radiatorThermostat_config
{
    double comfort, eco, max, min, tofs;
    week_schedule   schedule;
} radiatorThermostat_config;

typedef struct wallThermostat_config
{
    double comfort, eco, max, min;
    week_schedule   schedule;
} wallThermostat_config;

using dev_config_v = std::variant<cube_config, radiatorThermostat_config, wallThermostat_config>;

typedef struct dev_config
{
    rfaddr_t        rfaddr;
    devicetype      devtype;
    std::string     serial;
    uint16_t        room_id;
    uint16_t        fwversion;
    dev_config_v    specific;
} dev_config;

typedef struct room_conf
{
    rfaddr_t        cube_rfaddr;
    rfaddr_t        rfaddr;                     // groupaddr
    std::string     name;
    unsigned        id;
    rfaddr_t        wallthermostat;             // masterdevice wall (if more than on in a room)
    std::set<rfaddr_t>
                    thermostats;
    room_conf()
        : rfaddr(0), id(0), wallthermostat(0)
    {}
} room_conf;

typedef struct room_data
{
    timestamped_temp act;
    timestamped_temp set;
    opmode mode;
    // std::chrono::system_clock::time_point acttime;
    std::map<rfaddr_t, uint16_t> flags;
    using timestamped_valve_pos_by_rfaddr = std::map<rfaddr_t, timestamped_valve_pos>;
    timestamped_valve_pos_by_rfaddr valve_pos;
    bool operator!=(const room_data &rhs) const
    {
        return ((act != rhs.act)
                || (set != rhs.set));
                // || (acttime != rhs.acttime));
    }
    room_data()
    {
        act.second =
        set.second = std::chrono::system_clock::now();
    }

    template<typename V>
    bool change(V &cur, V nv, changeflag_set &cfs, changeflags flag)
    {
        bool changed = (nv != cur);
        if (changed)
        {
            cur = nv;
            cfs.insert(flag);
        }
        return changed;
    }
    template<typename V>
    bool change(std::pair<V, std::chrono::system_clock::time_point> &cur, V nv, changeflag_set &cfs, changeflags flag)
    {
        bool changed = (nv != cur.first);
        if (changed)
        {
            cur = std::make_pair(nv, std::chrono::system_clock::now());
            cfs.insert(flag);
            // cur.second = std::chrono::system_clock::now();
        }
        return changed;
    }

    bool change(rfaddr_t key, uint16_t vpos, changeflag_set &cfs)
    {

        if ((valve_pos.find(key) == valve_pos.end()) ||
                (valve_pos[key].first != vpos))
        {
            valve_pos[key] = std::make_pair(vpos, std::chrono::system_clock::now());
            cfs.insert(changeflags::valve_pos);
            return true;
        }
        return false;
    }
} room_data;

typedef struct device_data_store
{
    using devicemap = std::map<rfaddr_t, dev_config>;
    using roomconfmap = std::map<unsigned, room_conf>;
    using roomdatamap = std::map<unsigned, room_data>;

    devicemap devconf;

    roomconfmap roomconf;

    roomdatamap rooms;

}   device_data_store;

}

#endif // DEV_STORE_H
