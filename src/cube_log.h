#ifndef CUBE_LOG_H
#define CUBE_LOG_H

#include <iomanip>

namespace max_eq3 {

class logtarget_object
{
public:
    virtual ~logtarget_object();
    virtual std::ostream &get() = 0;
    virtual void sync() = 0;
};

class logging_target
{
public:
    virtual ~logging_target();
    virtual logtarget_object *info() = 0;
    virtual logtarget_object *verbose() = 0;
    virtual logtarget_object *error() = 0;
    virtual bool verbose_enabled() const = 0;
    virtual bool info_enabled() const = 0;
    virtual bool error_enabled() const = 0;
};

}

#endif // CUBE_LOG_H
