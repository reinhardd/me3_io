#ifndef MQTT_ROOM_H
#define MQTT_ROOM_H

#include <string>
#include <set>
#include <map>
#include "homie-cpp/client.h"

namespace max_eq3
{

struct mqtt_room : public homie::basic_node
{

};

struct mqtt_cube : public homie::basic_device
{

    std::map<std::string, std::string> attributes;
    mqtt_cube()
    {
        attributes["name"] = "Testdevice";
        attributes["state"] = "ready";
        attributes["localip"] = "mqtt_conn";
        attributes["mac"] = "AA:BB:CC:DD:EE:FF";
        attributes["fw/name"] = "max2mqtt";
        attributes["fw/version"] = "0.0.1";
        attributes["implementation"] = "homie-cpp";
        attributes["stats"] = "uptime";
        attributes["stats/uptime"] = "0";
        attributes["stats/interval"] = "60";
    }
    virtual ~mqtt_cube()
    {}

    virtual std::set<std::string> get_attributes() const override
    {
        std::set<std::string> res;
        for (auto& e : attributes) res.insert(e.first);
        return res;
    }
    virtual std::string get_attribute(const std::string& id) const
    {
        auto it = attributes.find(id);
        if (it != attributes.cend()) return it->second;
        return "";
    }
    virtual void set_attribute(const std::string& id, const std::string& value)
    {
        attributes[id] = value;
    }
};

}
#endif // MQTT_ROOM_H
