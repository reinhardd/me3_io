#ifndef CUBE_LOG_INTERNAL_H
#define CUBE_LOG_INTERNAL_H

#include <memory>

#include "cube_io.h"
#include "cube_log.h"

namespace max_eq3 {

    void set_log_target(logging_target *target);


    namespace detail {
        extern logging_target *log_target;
        extern bool verbose_enabled;
        extern bool info_enabled;
        extern bool error_enabled;

        bool verbose_log_enabled();
        bool info_log_enabled();
        bool error_log_enabled();
    }
}

#define LogI(m) \
    for (int i = 0; i < max_eq3::detail::info_log_enabled(); ++i)                           \
    {                                                                                       \
        logtarget_object *plt = max_eq3::detail::log_target->info();                        \
        if (plt)                                                                            \
        {                                                                                   \
            plt->get() << m;                                                                \
            plt->sync();                                                                    \
        }                                                                                   \
    }

#define LogV(m) \
    for (int i = 0; i < max_eq3::detail::verbose_log_enabled(); ++i)                        \
    {                                                                                       \
        logtarget_object *plt = max_eq3::detail::log_target->verbose();                     \
        if (plt)                                                                            \
        {                                                                                   \
            plt->get() << m;                                                                \
            plt->sync();                                                                    \
        }                                                                                   \
    }

#define LogE(m) \
    for (int i = 0; i < max_eq3::detail::error_log_enabled(); ++i)                          \
    {                                                                                       \
        logtarget_object *plt = max_eq3::detail::log_target->error();                       \
        if (plt)                                                                            \
        {                                                                                   \
            plt->get() << m;                                                                \
            plt->sync();                                                                    \
        }                                                                                   \
    }

#endif // CUBE_LOG_INTERNAL_H
