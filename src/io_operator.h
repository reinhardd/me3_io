#ifndef IO_OPERATOR_H
#define IO_OPERATOR_H
#pragma once

#include <ostream>
#include "cube_types.h"

namespace maxeq3 { namespace io_operator {

// std::ostream &operator<<(std::ostream &s, const boost_def &bc);
// std::ostream &operator<<(std::ostream &s, const cube_config &wc);
std::ostream &operator<<(std::ostream &s, const max_eq3::day_schedule &ds);
std::ostream &operator<<(std::ostream &s, const max_eq3::week_schedule &ws);
}}

using namespace maxeq3::io_operator;

#endif // IO_OPERATOR_H
