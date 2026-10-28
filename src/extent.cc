#include "extent.h"

#include "address.h"
#include "champsim.h"
#include "modules.h"

namespace
{
// Cached extents for the address slice helpers (page_number, block_offset,
// etc.). These are read on the hot path every time an address slice is
// constructed; caching them avoids a per-construction globals lookup.
//
// They start at the defaults (12-bit page, 6-bit block). The environment
// invokes ``refresh_address_extents()`` once it has published the system
// globals so any non-default block / page size takes effect before module
// construction starts. Tests that reset globals also reset these via the
// same hook.
champsim::dynamic_extent& page_number_ext()
{
  static champsim::dynamic_extent v{champsim::address::bits, champsim::data::bits{12}};
  return v;
}
champsim::dynamic_extent& page_offset_ext()
{
  static champsim::dynamic_extent v{champsim::data::bits{12}, champsim::data::bits{}};
  return v;
}
champsim::dynamic_extent& block_number_ext()
{
  static champsim::dynamic_extent v{champsim::address::bits, champsim::data::bits{6}};
  return v;
}
champsim::dynamic_extent& block_offset_ext()
{
  static champsim::dynamic_extent v{champsim::data::bits{6}, champsim::data::bits{}};
  return v;
}
} // namespace

void champsim::refresh_address_extents()
{
  auto& g = champsim::modules::ModuleBuilder::globals();
  auto log2_page = g.get_parameter<unsigned>("log2_page_size", true, 12u);
  auto log2_block = g.get_parameter<unsigned>("log2_block_size", true, 6u);
  page_number_ext() = dynamic_extent{address::bits, data::bits{log2_page}};
  page_offset_ext() = dynamic_extent{data::bits{log2_page}, data::bits{}};
  block_number_ext() = dynamic_extent{address::bits, data::bits{log2_block}};
  block_offset_ext() = dynamic_extent{data::bits{log2_block}, data::bits{}};
}

champsim::page_number_extent::page_number_extent() : dynamic_extent(page_number_ext()) {}
champsim::page_offset_extent::page_offset_extent() : dynamic_extent(page_offset_ext()) {}
champsim::block_number_extent::block_number_extent() : dynamic_extent(block_number_ext()) {}
champsim::block_offset_extent::block_offset_extent() : dynamic_extent(block_offset_ext()) {}

namespace
{
template <typename T>
auto size(const T& ext)
{
  return champsim::to_underlying(ext.upper) - champsim::to_underlying(ext.lower);
}
} // namespace

std::size_t champsim::size(dynamic_extent ext) { return ::size(ext); }

std::size_t champsim::size(page_offset_extent ext) { return ::size(ext); }

std::size_t champsim::size(page_number_extent ext) { return ::size(ext); }

std::size_t champsim::size(block_offset_extent ext) { return ::size(ext); }

std::size_t champsim::size(block_number_extent ext) { return ::size(ext); }
