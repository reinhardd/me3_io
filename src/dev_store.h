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
} radiatorThermostat_config;

typedef struct wallThermostat_config
{
    double comfort, eco, max, min;
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
    // missing: the weekplan
} dev_config;

typedef struct room_conf
{
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
    double act;
    double set;
    std::chrono::system_clock::time_point acttime;
    std::map<rfaddr_t, uint16_t> flags;
    bool operator!=(const room_data &rhs) const
    {
        return ((act != rhs.act)
                || (set != rhs.set)
                || (acttime != rhs.acttime));
    }
    room_data()
        : act(0.0)
        , set(0.0)
    {}
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
