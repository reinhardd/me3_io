
#include "cube_log.h"


namespace max_eq3 {
namespace  detail {

    max_eq3::logging_target *log_target{nullptr};
    bool verbose_enabled{false};
    bool info_enabled{false};
    bool error_enabled{true};

    bool verbose_log_enabled()
    {
        return detail::log_target && detail::verbose_enabled;
    }
    bool info_log_enabled()
    {
        return detail::log_target && detail::info_enabled;
    }
    bool error_log_enabled()
    {
        return detail::log_target && detail::error_enabled;
    }
}

logging_target::~logging_target()
{}

logtarget_object::~logtarget_object()
{}

void set_log_target(logging_target *target)
{
    detail::log_target = target;
    if (target)
    {
        detail::verbose_enabled = target->verbose_enabled();
        detail::info_enabled = target->info_enabled();
        detail::error_enabled = target->error_enabled();
    }
}



}
