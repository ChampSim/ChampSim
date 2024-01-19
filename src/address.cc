#include "address.h"

#include "champsim.h"
#include "util/units.h"

auto champsim::lowest_address_for_size(champsim::data::bytes sz) -> champsim::address
{
  return champsim::address{static_cast<champsim::address::underlying_type>(sz.count())};
}
