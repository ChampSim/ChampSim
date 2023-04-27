#ifndef TEST_REPL_INTERFACE_H
#define TEST_REPL_INTERFACE_H

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
    uint32_t type;
    uint8_t hit;
  };
}

#endif
