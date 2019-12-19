
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>

#include "cube_mqtt_client.h"   // stay before the boost includes

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "cube_io.h"
#include "cube_log.h"
#include "utils.h"

namespace bpo = boost::program_options;

class cube_logger : public max_eq3::logging_target
{
    class clo : public max_eq3::logtarget_object
    {
        std::ostream &_os;
        std::ostringstream _tmp;
        std::string _prefix;
    public:
        clo(std::ostream &os, const std::string &p) : _os(os), _prefix(p) {}

        virtual void sync() override
        {
            _os << _prefix << ": " << _tmp.str() << std::endl;
            _tmp.str("");
        }

        virtual std::ostream &get() override {
            return _tmp;
        }
    };


#if 0
    clo _cinf{std::cout, "INF"};
    clo _cverb{std::cout, "VERB"};
    clo _cerr{std::cerr, "ERR"};
#else
    std::ofstream _flog{"xout.log"};
    clo _cinf{_flog, "INF"};
    clo _cverb{_flog, "VERB"};
    clo _cerr{_flog, "ERR"};
#endif
public:
    cube_logger()
    {}
    virtual max_eq3::logtarget_object *info() override { return &_cinf; }
    virtual max_eq3::logtarget_object *verbose() override { return &_cverb; }
    virtual max_eq3::logtarget_object *error() override { return &_cerr; }
    virtual bool verbose_enabled() const { return true; }
    virtual bool info_enabled() const { return true; }
    virtual bool error_enabled() const { return true; }
};

std::ostream &operator << (std::ostream &s, const max_eq3::timestamped_temp &ts)
{
    std::chrono::system_clock::duration since = std::chrono::system_clock::now() - ts.second;

    std::ostringstream os;
    os << std::setw(4) << std::fixed << std::setprecision(1) << ts.first
       << "°C|+" << std::chrono::duration_cast<std::chrono::minutes>(since).count();

    s << os.str();
    return s;
}

std::ostream &operator << (std::ostream &s, const max_eq3::timestamped_valve_pos &vs)
{
    std::chrono::system_clock::duration since = std::chrono::system_clock::now() - vs.second;

    std::ostringstream os;
    os << vs.first << "%|+" << std::chrono::duration_cast<std::chrono::minutes>(since).count();

    s << os.str();
    return s;
}

class cube_io_callback
        : public max_eq3::cube_event_target
{
public:
    using roommap = std::map<std::string, max_eq3::room_sp>;
private:
    max_eq3::device_sp device;
    roommap rooms;

    cube_logger &_log;
    max_eq3::mqtt_client &_max_mqtt_client;
    mutable std::mutex   _mtx;

public:
    cube_io_callback(cube_logger &l, max_eq3::mqtt_client &mqtt_client)
        : _log(l)
        , _max_mqtt_client(mqtt_client)
    {}

    void complete()
    {
        _max_mqtt_client.complete();
    }
    virtual void device_info(max_eq3::device_sp dsp) override
    {
        device = dsp;
        _max_mqtt_client.expose_cube(dsp);
    }
    virtual void room_changed(max_eq3::room_sp rsp) override
    {
        {
            std::unique_lock<std::mutex> l(_mtx);
            rooms[rsp->name] = rsp;            
        }
        std::cout << "rchanged " << rsp->name
                  << " m:" << max_eq3::mode_as_string(rsp->mode)
                  << " s:" << rsp->set_temp
                  << " a:" << rsp->actual_temp
                  << " v:" << rsp->valve_pos
                  << std::endl;

        if (!rsp->changed.empty())
        {
            _max_mqtt_client.expose_room(rsp);
            std::ostringstream xs;
            xs << "room " << rsp->name << " changed[" << std::flush;
            for (auto cf: rsp->changed)
            {
                switch (cf)
                {
                case max_eq3::changeflags::act_temp:
                    xs << "act: " << rsp->actual_temp << ';';
                    break;
                case max_eq3::changeflags::set_temp:
                    xs << "set: " << rsp->set_temp << ';';
                    break;
                case max_eq3::changeflags::mode:
                    xs << "mode: " << int(rsp->mode) << ';';
                    break;
                case max_eq3::changeflags::config:
                    xs << "config;";
                    break;
                case max_eq3::changeflags::contained_devs:
                    xs << "devs;";
                    break;
                }
            }
            xs << "]";
            // std::cout << "changelog " << xs.str() << std::endl;

            _log.info()->get() << xs.str() << std::endl;
            // *(_log.info()) << xs.str() << std::endl;
        }
        else
            _log.info()->get() << "unspecified room change triggered\n";
    }

    roommap getroominfo() const
    {
        std::unique_lock<std::mutex> l(_mtx);
        return rooms;
    }

    virtual void connected()  override
    {
        _log.info()->get() << __PRETTY_FUNCTION__ << std::endl;
    }
    virtual void disconnected() override
    {
        _log.info()->get() << __PRETTY_FUNCTION__ << std::endl;
    }    

    bool has_room(const std::string &room) const
    {
        return rooms.find(room) != rooms.end();
    }

    void loginfo(std::ostream &os)
    {
        roommap tmp = getroominfo();
        os << "rooms:\n";
        for (const auto &v: tmp)
        {
            os << "  " << std::setw(25) << v.first
               << " m: " << std::setw(8) << max_eq3::mode_as_string(v.second->mode); // << int(v.second->mode)
            os << " set(" << v.second->set_temp
               << ") act(" << v.second->actual_temp;
            os << ") valve(" << v.second->valve_pos << ")"; // .first << "%: " << timeinfo(v.second->valve_pos.second);
            os << std::endl;
        }
        os << std::endl;
    }
};

int main(int argc, char *argv[])
{
    bpo::options_description desc("Options");
    std::string cubeserial;
    std::string mqtthost = "localhost";
    std::string mqttport = "1883";
    desc.add_options()
            ("help,h",                            "show help")
            ("serial,s", bpo::value<std::string>(&cubeserial), "identifies cube by serial no")
            ("mqtthost,m", bpo::value<std::string>(&mqtthost), "mqtt server host")
            ("mqttport,p", bpo::value<std::string>(&mqttport), "mqtt server port")
        ;

    bpo::variables_map vm;
    bpo::store(bpo::parse_command_line(argc, argv, desc), vm);
    bpo::notify(vm);

    if (vm.count("help")) {
        std::cout << argv[0]
                << " controls exactly ONE MAX-EQ3 cube\n"
                << " and exposes the sampled data via mqtt\n\n"
                << desc << "\n";
        return 1;
    }

    std::cout << "using mqtt host at " << mqtthost << ":" << mqttport << std::endl;


    max_eq3::mqtt_client hmc(mqtthost, mqttport);
    std::thread t([&hmc](){
            std::cout << "hmc thread function\n";            
            hmc.run();
            std::cout << "hmc thread done\n";
        });

    cube_logger cl;
    max_eq3::cube_io::set_logger(&cl);
    cube_io_callback cic(cl, hmc);
    max_eq3::cube_io cub(&cic, cubeserial);
    hmc.set_setter([&cub](std::string_view room, std::string_view target, std::string_view data) {

        std::cout << "setter for room " << room << " target " << target << " data " << data << std::endl;
        if (target == "temp")
        {
            // std::string parms(cmd.begin() + 5, cmd.end());
            std::istringstream is(std::string(data.begin(), data.end()));
            double temp;
            is >> temp;
            std::cout << "set temp for " << room << " to " << temp << std::endl;
            cub.change_temp(std::string(room.begin(), room.end()), temp);
        }
        else if (target == "mode")
        {            
            boost::optional<max_eq3::opmode> m;
            if (data == "auto")
                m = max_eq3::opmode::AUTO;
            else if (data == "manual")
                m = max_eq3::opmode::MANUAL;
            else if (data == "boost")
                m = max_eq3::opmode::BOOST;

            if (m)
            {
                std::cout << "change mode for " << room << " to " << mode_as_string(*m) << std::endl;
                cub.change_mode(std::string(room.begin(), room.end()), *m);
            }
        }


    });

    while(true)
    {
        std::string cmdstring;

        std::getline(std::cin, cmdstring);  // std::cin >> cmdstring;

        boost::trim(cmdstring);

        if (cmdstring == "quit")
            break;
        if (cmdstring == "help")
        {
            std::cout << "commands:\n"
                      << "    temp <room> <temperature>      # example: temp livingroom 17.5\n"
                      << "    mode <room> <mode>             # tmode :== manual | auto | boost\n"
                      << "    status                         # show current status\n"
                      << "    quit                           # exit program\n";
        }
        else if (cmdstring == "status")
        {
            cic.loginfo(std::cout);
        }
        else if (cmdstring.substr(0,4) == "temp")
        {
            // std::string scmd = cmdstring.substr(5);
            cmdstring.erase(0,5);
            std::string roomname;
            if (parse_room(cmdstring, roomname))
            {
                boost::trim(cmdstring);

                try {
                    double temp = boost::lexical_cast<double>(cmdstring);
                    cub.change_temp(roomname, temp);
                } catch(boost::bad_lexical_cast &e) {
                    std::cerr << "error reading temperature: " << e.what() << std::endl;
                }
            }
        }
#if 0
        else if (cmdstring == "complete")
        {
            cic.complete();
        }
#endif
        else if (cmdstring.substr(0,4) == "mode")
        {
            cmdstring.erase(0,5);
            std::string roomname;
            if (parse_room(cmdstring, roomname))
            {
                boost::trim(cmdstring);
                max_eq3::opmode mode;
                if (cmdstring.substr(0,4) == "auto")
                    mode = max_eq3::opmode::AUTO;
                else if (cmdstring.substr(0,6) == "manual")
                    mode = max_eq3::opmode::MANUAL;
                else if (cmdstring.substr(0,5) == "boost")
                    mode = max_eq3::opmode::BOOST;
                else if (cmdstring.substr(0,8) == "vacation")
                {
                    std::cerr << "vacation mode not yet implemented\n";
                }
                // for VACATION we need more parameters
                else
                {
                    std::cerr << "mode set invalid parameter: " << cmdstring << std::endl;
                    continue;
                }
                cub.change_mode(roomname, mode);
            }
        }
        else if (cmdstring.size())
        {
            std::cout << "unknown command " << cmdstring << std::endl;
        }
    }
    std::cout << "left cmd loop\n";
    hmc.stop();
    std::cout << "stopped\n";

    t.join();
    return 0;
}
