
#include <iomanip>

#include "io_operator.h"

namespace maxeq3 { namespace io_operator {
std::ostream &operator<<(std::ostream &s, const max_eq3::day_schedule &ds)
{
    std::ostringstream xs;
    uint16_t current_hr = 0, current_min = 0;
    for (unsigned u = 0; u < SCHED_POINTS; ++u)
    {

        uint16_t hours = ds[u].minutes_since_midnight / 60;
        uint16_t minutes = ds[u].minutes_since_midnight % 60;
        xs << "[" << std::setfill('0') << std::setw(2) << current_hr << ':' << std::setw(2) << current_min << ' '
           << std::setfill(' ') << std::setw(5) << std::setprecision(3) << ds[u].temp << "°C "
           << std::setfill('0') << std::setw(2) << hours
           << ':' << std::setw(2) << minutes
           << "] ";

        if (ds[u].minutes_since_midnight == 1440)
            break;
        current_hr = hours;
        current_min = minutes;
    }
    s << xs.str();
    return s;
}

const char *days[DAYS_A_WEEK] = { "Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};

std::ostream &operator<<(std::ostream &s, const max_eq3::week_schedule &ws)
{
    std::ostringstream xs;
    for (unsigned u = 0; u < DAYS_A_WEEK; ++u)
    {
        xs << days[u] << ' ' << ws[u] << '\n';
    }
    s << xs.str();
    return s;
}
}}
