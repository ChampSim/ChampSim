#include "extent.h"

std::size_t champsim::size(dynamic_extent ext) { return to_underlying(ext.upper) - to_underlying(ext.lower); }

std::size_t champsim::size(sized_extent ext) { return to_underlying(ext.upper) - to_underlying(ext.lower); }
