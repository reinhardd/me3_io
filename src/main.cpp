
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "cube_io.h"
#include "cube_log.h"

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

class cube_io_callback : public max_eq3::cube_event_target
{
public:
    using roommap = std::map<std::string, max_eq3::room_sp>;
private:
    roommap rooms;

    cube_logger &_log;
    mutable std::mutex   _mtx;
public:
    cube_io_callback(cube_logger &l)
        : _log(l)
    {}

    virtual void room_changed(max_eq3::room_sp rsp) override
    {
        {
            std::unique_lock<std::mutex> l(_mtx);
            rooms[rsp->name] = rsp;
        }

        if (!rsp->changed.empty())
        {
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

    /*
    std::string timeinfo(std::chrono::system_clock::time_point tp)
    {
        std::time_t set_time = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream tmp;
        std::tm tm = *std::localtime(&set_time);
        tmp << std::put_time(&tm, "%R_%d.%m");
        return tmp.str();
    }*/

    void loginfo(std::ostream &os)
    {
        roommap tmp = getroominfo();
        os << "rooms:\n";
        for (const auto &v: tmp)
        {
            os << "\t" << v.first
               << " m: "; // << int(v.second->mode)
            switch (v.second->mode)
            {
            case max_eq3::opmode::AUTO:       os << "auto    "; break;
            case max_eq3::opmode::MANUAL:     os << "manual  "; break;
            case max_eq3::opmode::VACATION:   os << "vacation"; break;
            case max_eq3::opmode::BOOST:      os << "boost   "; break;
            default:
                os << "unknown_" << int(v.second->mode) << "_";
            }
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
    cube_logger cl;
    max_eq3::cube_io::set_logger(&cl);
    cube_io_callback cic(cl);
    max_eq3::cube_io cub(&cic);

    while(true)
    {
        std::string cmdstring;

        std::getline(std::cin, cmdstring);  // std::cin >> cmdstring;

        boost::trim(cmdstring);

        if (cmdstring == "quit")
            break;
        else if (cmdstring == "status")
        {
            cic.loginfo(std::cout);
        }
        else if (cmdstring.substr(0,4) == "temp")
        {
            std::string scmd = cmdstring.substr(5);
            std::vector<std::string> spv;
            boost::split(spv, scmd, boost::is_any_of(" "), boost::token_compress_on);
            if (spv.size() != 2)
            {
                std::cerr << "invalid syntax <" << cmdstring << "> usage: temp kitchen 19.5 \n";
            }
            else
            {
                if (!cic.has_room(spv[0]))
                {
                    std::cerr << "no room named " << spv[0] << std::endl;
                }
                else
                {
                    double temp = boost::lexical_cast<double>(spv[1]);
                    if (temp >= 31.6)
                    {
                        std::cerr << "temp higher than 31.5°C: " << temp << std::endl;
                    }
                    else
                        cub.change_set_temp(spv[0], temp);
                }
            }
        }
        else if (cmdstring.substr(0,4) == "mode")
        {
            std::string scmd = cmdstring.substr(5);
            std::vector<std::string> spv;
            boost::split(spv, scmd, boost::is_any_of(" "), boost::token_compress_on);
            if (spv.size() != 2)
            {
                std::cerr << "invalid syntax <" << cmdstring << "> usage: temp kitchen 19.5 \n";
            }
            else
            {
                if (!cic.has_room(spv[0]))
                {
                    std::cerr << "no room named " << spv[0] << std::endl;
                }
                else
                {
                    max_eq3::opmode mode;
                    if (spv[1] == "auto")
                        mode = max_eq3::opmode::AUTO;
                    else if (spv[1] == "manual")
                        mode = max_eq3::opmode::MANUAL;
                    else if (spv[1] == "boost")
                        mode = max_eq3::opmode::BOOST;
                    // for VACATION we need more parameters
                    else
                    {
                        std::cerr << "invalid parameter " << spv[1] << std::endl;
                    }
                    cub.change_mode(spv[0], mode);
                }
            }

        }
        else {
            std::cout << "unknown command " << cmdstring << std::endl;
        }
    }
    return 0;
}
