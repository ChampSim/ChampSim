#ifndef UTIL_RANDOM_H
#define UTIL_RANDOM_H

#include <boost/random.hpp>

namespace champsim
{
template <typename T>
using uniform_int_distribution = boost::random::uniform_int_distribution<T>;
} // namespace champsim

#endif // UTIL_RANDOM_H