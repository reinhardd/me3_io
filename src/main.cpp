
#include <string>
#include <iostream>
#include <sstream>
#include <thread>


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
    clo _cinf{std::cout, "INF"};
    clo _cverb{std::cout, "VERB"};
    clo _cerr{std::cerr, "ERR"};
public:
    cube_logger() {}
    virtual max_eq3::logtarget_object *info() override { return &_cinf; }
    virtual max_eq3::logtarget_object *verbose() override { return &_cverb; }
    virtual max_eq3::logtarget_object *error() override { return &_cerr; }
    virtual bool verbose_enabled() const { return true; }
    virtual bool info_enabled() const { return true; }
    virtual bool error_enabled() const { return true; }
};

class cube_io_callback : public max_eq3::cube_event_target
{
    std::map<std::string, max_eq3::room_sp> rooms;
public:
    virtual void room_changed(max_eq3::room_sp rsp) override
    {
        // std::cout << __PRETTY_FUNCTION__ << std::endl;

        bool is_new = (rooms.find(rsp->name) == rooms.end());

        if ( is_new || (rsp->changed.size()))
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
                    xs << "mode: " << rsp->mode << ';';
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
            std::cout << xs.str() << std::endl;
        }
        else
            std::cout << "unspecified room change triggered\n";
    }
    virtual void connected()  override
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }
    virtual void disconnected() override
    {
        std::cout << __PRETTY_FUNCTION__ << std::endl;
    }

};

int main(int argc, char *argv[])
{
    cube_logger cl;
    max_eq3::cube_io::set_logger(&cl);
    cube_io_callback cic;
    max_eq3::cube_io cub(&cic);

    while(true)
        std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}
