#ifndef TEST_REPL_INTERFACE_H
#define TEST_REPL_INTERFACE_H

#include "channel.h"

namespace test
{
  struct repl_update_interface
  {
    uint32_t cpu;
    long set;
    long way;
    champsim::address full_addr;
    champsim::address ip;
    champsim::address victim_addr;
    access_type type;
    bool hit;
  };

  struct repl_fill_interface
  {
    uint32_t cpu;
    long set;
    long way;
    champsim::address full_addr;
    champsim::address ip;
    champsim::address victim_addr;
    access_type type;
  };
}

#endif
