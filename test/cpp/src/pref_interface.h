#ifndef TEST_PREF_INTERFACE_H
#define TEST_PREF_INTERFACE_H

namespace test
{
  struct pref_cache_operate_interface
  {
    champsim::address addr;
    champsim::address ip;
    uint8_t cache_hit;
    bool useful_prefetch;
    access_type type;
    uint32_t metadata_in;
  };
}

#endif

