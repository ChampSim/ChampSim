#ifndef TEST_STATE_MODEL_INTERFACE_H
#define TEST_STATE_MODEL_INTERFACE_H

#include "channel.h"

namespace test
{
  struct state_model_request_interface
  {
    uint32_t cpu;
    long set;
    champsim::address full_addr;
    access_type type;
    uint8_t hit;
  };

  struct state_model_response_interface
  {
    uint32_t cpu;
    long set;
    champsim::address full_addr;
    access_type type;
  };
}

#endif
