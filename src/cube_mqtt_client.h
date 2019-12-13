#ifndef MQTTCONNECT_H
#define MQTTCONNECT_H
#pragma once

#include <functional>

#include "mqtt_client_cpp.hpp"
#include "cube_types.h"

namespace max_eq3 {

/*
 * we have to expose
 *
 *          max2homie/<cubename>/$homie         -> 3.0
 *                              /$name          -> name
 *                              /$state         -> ready
 *                              /$extensions    -> ''
 *                              /$nodes         -> room1 room2
 *
 *                              /room1/$name                -> livingroom
 *                              /room1/$properties          -> act_temp set_temp weekschedule
 *
 *                              /room1/act_temp                 -> 23.5
 *                              /room1/act_temp/$name           -> Temperature
 *                              /room1/act_temp/$unit           -> °C
 *                              /room1/act_temp/$datatype       -> float
 *
 *                              /room1/set_temp                 -> 21.0
 *                              /room1/set_temp/$name           -> Temperature
 *                              /room1/set_temp/$unit           -> °C
 *                              /room1/set_temp/$datatype       -> float
 *                              /room1/set_temp/$settable       -> true
 *                              /room1/set_temp/$retained       -> false
 *
 *                              /room1/weekschedule
 *                              /room1/weekschedule/$datatype   -> string
 *
 * */
class mqtt_client
{
    using client_type_t = mqtt::sync_client<mqtt::tcp_endpoint<boost::asio::ip::tcp::socket, boost::asio::io_service::strand>>;
    using packet_id_t = client_type_t::packet_id_t;
public:

    using set_method = std::function<void (std::string_view room, std::string_view data)>;

    mqtt_client(const std::string &host, const std::string &port);
    void expose_cube(device_sp dsp);
    void expose_room(room_sp rsp);
    void complete();
    void run();    
    void stop();

    void set_setter(set_method m);

private:
private:    // types
    typedef struct roomdata
    {
        room_sp roomsp;
        std::uint16_t setpid;
        roomdata(room_sp room)
            : roomsp(room)
            , setpid(0)
        {}
    } roomdata;

private:

    void send_device();
    void update_nodes();
    void send_room(roomdata &roomd);
    std::string to_json(const std::string &roomname, const week_schedule &ws);

    bool connack_handler(bool sp, std::uint8_t connack_return_code);
    void close_handler();
    void error_handler(boost::system::error_code const& ec);
    // needed handlers
    bool puback_handler(packet_id_t packet_id);
    bool pubrec_handler(packet_id_t packet_id);
    void pubcomp_handler(packet_id_t packet_id);
    bool suback_handler(packet_id_t packet_id, std::vector<mqtt::optional<std::uint8_t>> qoss);
    bool publish_handler(std::uint8_t header,
                         boost::optional<packet_id_t> packet_id,
                         mqtt::buffer topic_name,
                         mqtt::buffer contents);

private:
    boost::asio::io_service _ios;
    std::shared_ptr<client_type_t> _client;

    std::map<std::string, roomdata> _rooms;

    device_sp _device;
    bool _is_connected;
    bool _ready;

    std::map<std::string, unsigned> _tmit_ctrl;

    set_method  _setm;
};

}


#endif // MQTTCONNECT_H
