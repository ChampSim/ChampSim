#include "cache_bus.h"

bool champsim::CacheBus::issue_read(request_type data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.is_translated = false;
  data_packet.cpu = cpu;
  data_packet.type = LOAD;

  return lower_level->add_rq(data_packet);
}

bool champsim::CacheBus::issue_write(request_type data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.is_translated = false;
  data_packet.cpu = cpu;
  data_packet.type = WRITE;
  data_packet.response_requested = false;

  return lower_level->add_wq(data_packet);
}
