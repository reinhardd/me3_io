
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "cube_mqtt_client.h"
#include "io_operator.h"


namespace max_eq3 {

const char *pRootTopic = "max2mqtt/";

mqtt_client::mqtt_client(const std::string &host, const std::string &port)
    : _is_connected(false)
    , _ready(false)
{
    std::cout << "make client\n";
    _client = mqtt::make_sync_client(_ios, host, port);
    _client->set_client_id("wbmqtti");
    _client->set_clean_session(true);
    // _client->set_auto_pub_response(true, true);
    _client->set_close_handler(boost::bind(&mqtt_client::close_handler, this));
    _client->set_error_handler(boost::bind(&mqtt_client::error_handler, this, _1));
    _client->set_connack_handler(boost::bind(&mqtt_client::connack_handler, this, _1, _2));
    _client->set_puback_handler(boost::bind(&mqtt_client::puback_handler, this, _1));
    _client->set_suback_handler(boost::bind(&mqtt_client::suback_handler, this, _1, _2));
    _client->set_pubrec_handler(boost::bind(&mqtt_client::pubrec_handler, this, _1));
    _client->set_publish_handler(boost::bind(&mqtt_client::publish_handler, this, _1, _2, _3, _4));
    std::cout << "connect\n";
    _client->connect();
}

void mqtt_client::set_setter(set_method m)
{
    _setm = m;
}

void mqtt_client::stop()
{
    _ios.stop();
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

bool mqtt_client::connack_handler(bool sp, std::uint8_t connack_return_code)
{
    std::cout << "Connack handler called" << std::endl;
    std::cout << "Clean Session: " << std::boolalpha << sp << std::endl;
    std::cout << "Connack Return Code: "
              << mqtt::connect_return_code_to_str(connack_return_code) << std::endl;
    if (connack_return_code == mqtt::connect_return_code::accepted)
    {
        if (!_is_connected)
        {
            std::cout << "set connected\n";
            if (_device)
            {
                send_device();
            }
            for (auto x: _rooms)
            {
                send_room(x.second); // .second.roomsp);
            }
            _is_connected = true;
        }
    }
    return true;
}

void mqtt_client::close_handler()
{}

void mqtt_client::error_handler(boost::system::error_code const& ec)
{
    std::cout << "error: " << ec << std::endl;
}

bool mqtt_client::puback_handler(packet_id_t packet_id)
{
    // std::cout << "puback received. packet_id: " << packet_id << std::endl;
    return true;
}

bool mqtt_client::pubrec_handler(packet_id_t packet_id)
{
    // std::cout << "pubrec received. packet_id: " << packet_id << std::endl;
    return true;
}

void mqtt_client::pubcomp_handler(packet_id_t packet_id)
{
    std::cout << "pubcomp received. packet_id: " << packet_id << std::endl;
}

bool mqtt_client::suback_handler(packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> qoss)
{
    // std::cout << "suback received. packet_id: " << packet_id << std::endl;
    for (auto const& e : qoss) {
        if (e) {
            // std::cout << "subscribe success: " << mqtt::qos::to_str(*e) << std::endl;
        }
        else {
            std::cout << "subscribe failed for packet " << packet_id << std::endl;
        }
    }
#if 0
    if (packet_id == pid_sub1) {
        c->publish_at_most_once("mqtt_client_cpp/topic1", "test1");
    }
    else if (packet_id == pid_sub2) {
        c->publish_at_least_once("mqtt_client_cpp/topic2_1", "test2_1");
        c->publish_exactly_once("mqtt_client_cpp/topic2_2", "test2_2");
    }
#endif
    return true;
}

bool mqtt_client::publish_handler(std::uint8_t header,
                     boost::optional<packet_id_t> packet_id,
                     mqtt::buffer topic_name,
                     mqtt::buffer contents)
{
    std::cout << "publish received. "
              << "dup: " << std::boolalpha << mqtt::publish::is_dup(header)
              << " qos: " << mqtt::qos::to_str(mqtt::publish::get_qos(header))
              << " retain: " << mqtt::publish::is_retain(header) << std::endl;
    if (packet_id)
        std::cout << "packet_id: " << *packet_id << std::endl;
    std::cout << "topic_name: " << topic_name << std::endl;
    std::cout << "contents: " << contents << std::endl;


    std::string toremote = std::string(pRootTopic) + _device->name;
    if (topic_name.substr(0, toremote.size()) == toremote)
    {

        auto target = topic_name.substr(toremote.size()+1);
        std::cout << "valid topic " << target << std::endl;
        std::vector<std::string> out;
        boost::split(out, topic_name, boost::is_any_of("/"), boost::token_compress_on);
        for (const auto &n: out)
        {
            std::cout << "x: " << n << std::endl;

        }

        if ((out.size() > 2) && (out.back() == "set"))
        {
            std::string ct(contents.begin(), contents.end());
            _setm(out[out.size() - 2], ct);
        }
    }


    return true;
}

void mqtt_client::run()
{
    _ios.run();
}

void mqtt_client::send_room(roomdata &roomd) // room_sp room)
{
    std::string rname = roomd.roomsp->name;
    std::cout << "do emit room data for " << rname << std::endl;
    std::string base_topic_room = pRootTopic + _device->name + "/" + rname + "/";

    if ((_tmit_ctrl.find(base_topic_room) == _tmit_ctrl.end()) || (_tmit_ctrl[base_topic_room] & 1) == 0)
    {
        _tmit_ctrl[base_topic_room] = 0;
#if defined(HOMIE_CONVENTION)
        _client->publish(base_topic_room + "$name", rname, mqtt::qos::at_least_once);
        _client->publish(base_topic_room + "$type", "heating", mqtt::qos::at_least_once);
        _client->publish(topic_root + "$properties", "act-temp,set-temp,valve-pos", mqtt::qos::at_least_once);

        _client->publish(topic_root + "act-temp/$name", "current temp", mqtt::qos::at_least_once);
        _client->publish(topic_root + "act-temp/$datatype", "float", mqtt::qos::at_least_once);
        _client->publish(topic_root + "act-temp/$unit", "°C", mqtt::qos::at_least_once);
        _client->publish(topic_root + "act-temp/$settable", "false", mqtt::qos::at_least_once);
        _client->publish(topic_root + "act-temp/$retained", "true", mqtt::qos::at_least_once);

        _client->publish(topic_root + "set-temp/$name", "set temp", mqtt::qos::at_least_once);
        _client->publish(topic_root + "set-temp/$datatype", "float", mqtt::qos::at_least_once);
        _client->publish(topic_root + "set-temp/$unit", "°C", mqtt::qos::at_least_once);
        _client->publish(topic_root + "set-temp/$settable", "true", mqtt::qos::at_least_once);
        _client->publish(topic_root + "set-temp/$retained", "true", mqtt::qos::at_least_once);

        _client->publish(topic_root + "valve-pos/$name", "set temp", mqtt::qos::at_least_once);
        _client->publish(topic_root + "valve-pos/$datatype", "float", mqtt::qos::at_least_once);
        _client->publish(topic_root + "valve-pos/$unit", "°C", mqtt::qos::at_least_once);
        _client->publish(topic_root + "valve-pos/$settable", "true", mqtt::qos::at_least_once);
        _client->publish(topic_root + "valve-pos/$retained", "true", mqtt::qos::at_least_once);
#endif

        // std::cout << "subscribe to " << base_topic_room + "set" << std::endl;
        _client->subscribe(base_topic_room + "set/mode", mqtt::qos::at_least_once);
        _client->subscribe(base_topic_room + "set/temp", mqtt::qos::at_least_once);
        _client->subscribe(base_topic_room + "set/weekplan", mqtt::qos::at_least_once);
        _tmit_ctrl[base_topic_room] = _tmit_ctrl[base_topic_room] | 1;
    }

    _client->publish(base_topic_room + "act-temp", std::to_string(roomd.roomsp->actual_temp.first), mqtt::qos::at_least_once, true);
    _client->publish(base_topic_room + "set-temp", std::to_string(roomd.roomsp->set_temp.first), mqtt::qos::at_least_once, true);
    _client->publish(base_topic_room + "valve-pos", std::to_string(roomd.roomsp->valve_pos.first), mqtt::qos::at_least_once, true);
    std::string mode = "UNKNOWN";
    switch (roomd.roomsp->mode)
    {
        case opmode::AUTO: mode = "AUTO"; break;
        case opmode::MANUAL: mode = "MANUAL"; break;
        case opmode::BOOST: mode = "BOOST"; break;
        case opmode::VACATION: mode = "VACATION"; break;
    }
    _client->publish(base_topic_room + "mode", mode, mqtt::qos::at_least_once, true);
    _client->publish(base_topic_room + "weekplan", to_json(rname, roomd.roomsp->schedule), mqtt::qos::at_least_once, true);

    // std::cout << "weekschedule for " << roomd.roomsp->schedule
    //          << "\njson ###\n" << to_json(rname, roomd.roomsp->schedule)
    //          << std::endl;
}

void mqtt_client::send_device()
{
    std::cout << "do emit cube data" << std::endl;
#if defined(HOMIE_CONVENTION)
    std::string topic_root = pRootTopic + _device->name + "/";
    _client->publish(topic_root + "$homie", "4.0", mqtt::qos::at_least_once);
    _client->publish(topic_root + "$name", "max cube", mqtt::qos::at_least_once);
    _client->publish(topic_root + "$state", _ready ? "ready" : "init", mqtt::qos::at_least_once);
    _client->publish(topic_root + "$extensions", ""), mqtt::qos::at_least_once;
#endif
    update_nodes();
}

void mqtt_client::update_nodes()
{
    std::string nodes;
    bool first = true;
    for (auto x: _rooms)
    {
        if (!x.first.size())
            std::cerr << "wrong node name\n";
        else
        {
            if (first)
                first = false;
            else
                nodes += ',';
            nodes += x.first;
        }
    }
    std::ostringstream topic_root;

    topic_root << pRootTopic << _device->name << "/";
#if 0
    std::cout << __FUNCTION__ << " root tp " << topic_root.str()
              // << " to " << nodes
              << std::endl;
#endif
    _client->publish(topic_root.str() + "rooms", nodes, mqtt::qos::at_least_once);
}

void mqtt_client::expose_cube(device_sp dsp)
{
    std::cout << __FUNCTION__ << std::endl;
    _ios.post([this, dsp]()
    {
        if (_is_connected)
        {
            _device = dsp;
            send_device();
        }
    });
}


void mqtt_client::complete()
{
    std::cout << "device is complete" << std::endl;
    _ready = true;
    send_device();   // update_nodes();
    for (auto x: _rooms)
    {
        send_room(x.second);
    }
    std::string topic_root = pRootTopic +_device->name + "/";
    _client->publish(topic_root + "$version", "1.0", mqtt::qos::at_least_once);
    _client->publish(topic_root + "$state", "ready", mqtt::qos::at_least_once);
}

void mqtt_client::expose_room(room_sp rsp)
{
    std::cout << __FUNCTION__ << std::endl;
    _ios.post([this, rsp]()
    {
        // std::cout << "inside " << __FUNCTION__ << std::endl;
        auto room_it = _rooms.find(rsp->name);
        bool insertnode = (room_it == _rooms.end());
        if (insertnode)
        {
            _rooms.emplace(std::pair(rsp->name, roomdata(rsp)));
            room_it = _rooms.find(rsp->name);
        }
        bool updnode = insertnode || (room_it->second.roomsp.get() != rsp.get());

        if (_is_connected)
        {
            send_room(room_it->second);
            if (updnode)
            {
                std::cout << "update node for room " << room_it->first << std::endl;
                update_nodes();
            }
        }
    });
}

static const char *day_text[7] = {
    "saturday", "sunday", "monday", "tuesday", "wednesday", "thursday", "friday"
};

std::string mqtt_client::to_json(const std::string &roomname, const week_schedule &ws)
{
    using namespace boost::property_tree;

    ptree jsout;
    jsout.add("room", roomname);

    ptree dayout;
    for (unsigned day = static_cast<unsigned>(days::Saturday);
                  day <= static_cast<unsigned>(days::Friday); day++)
    {

        const day_schedule &ds(ws[day]);
        bool seen_end = false;
        for (const auto &oneslot: ds)
        {
            if (!seen_end)
            {
                ptree schedpt;
                schedpt.add("endtime", oneslot.minutes_since_midnight);
                schedpt.add("temp", oneslot.temp);
                dayout.add_child("", schedpt);
                seen_end = (oneslot.minutes_since_midnight == 1440);
            }
        }
        jsout.add_child(day_text[day], dayout);
    }

    std::ostringstream jsonout;
    json_parser::write_json(jsonout,jsout);
    return jsonout.str();
}


}
