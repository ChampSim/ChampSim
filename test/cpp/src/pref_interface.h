#ifndef TEST_PREF_INTERFACE_H
#define TEST_PREF_INTERFACE_H

namespace test
{
  struct pref_cache_operate_interface
  {
    uint64_t addr;
    uint64_t ip;
    uint8_t cache_hit;
    bool useful_prefetch;
    uint8_t type;
    uint32_t metadata_in;
  };
}

#endif

