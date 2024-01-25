#include "address.h"

#include "champsim.h"
#include "util/units.h"
#include "util/to_underlying.h"

auto champsim::lowest_address_for_size(champsim::data::bytes sz) -> champsim::address
{
  return champsim::address{static_cast<champsim::address::underlying_type>(sz.count())};
}

auto champsim::lowest_address_for_width(champsim::data::bits width) -> champsim::address
{
  return champsim::lowest_address_for_size(champsim::data::bytes{1ll << champsim::to_underlying(width)});
}
