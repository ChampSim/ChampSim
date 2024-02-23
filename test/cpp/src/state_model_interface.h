#ifndef TEST_STATE_MODEL_INTERFACE_H
#define TEST_STATE_MODEL_INTERFACE_H

#include "channel.h"

namespace test
{
  struct state_model_update_interface
  {
    uint32_t cpu;
    long set;
    long way;
    uint64_t full_addr;
    uint64_t ip;
    uint64_t victim_addr;
    access_type type;
    uint8_t hit;
  };
}

#endif
