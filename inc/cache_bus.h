#ifndef CACHE_BUS_H
#define CACHE_BUS_H

#include "channel.h"

namespace champsim
{
class CacheBus
{
public:
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;

//private:
  channel_type* lower_level;
  uint32_t cpu;

public:
  CacheBus(uint32_t cpu_idx, champsim::channel* ll) : lower_level(ll), cpu(cpu_idx) {}
  bool issue_read(request_type packet);
  bool issue_write(request_type packet);
};
}

#endif
