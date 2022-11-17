#ifndef TEST_REPL_INTERFACE_H
#define TEST_REPL_INTERFACE_H

namespace test
{
  struct repl_update_interface
  {
    uint32_t cpu;
    uint32_t set;
    uint32_t way;
    uint64_t full_addr;
    uint64_t ip;
    uint64_t victim_addr;
    uint32_t type;
    uint8_t hit;
  };
}

#endif
