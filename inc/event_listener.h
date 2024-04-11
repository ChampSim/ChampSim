#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

#include <vector>
#include <iostream>

#include "champsim.h"
#include "instruction.h"
#include "channel.h"
#include "access_type.h"

enum class event {
  CYCLE_BEGIN,
  BRANCH,
  RETIRE,
  VA_TO_PA,
  GET_PTE_PA,
  ADD_RQ,
  ADD_WQ,
  ADD_PQ
};

struct CYCLE_BEGIN_data {};

struct BRANCH_data {
  ooo_model_instr* instr;

  BRANCH_data() {
    instr = nullptr;
  }
};

struct RETIRE_data {
  long cycle;
  std::vector<ooo_model_instr> instrs;
  //ooo_model_instr* begin_instr;
  //ooo_model_instr* end_instr;

  RETIRE_data() {
    cycle = 0;
    //begin_instr = nullptr;
    //end_instr = nullptr;
  }
};

struct VA_TO_PA_data {
  champsim::page_number paddr;
  champsim::page_number vaddr;
  bool fault;
};

struct GET_PTE_PA_data {
  champsim::address paddr;
  champsim::page_number vaddr;
  uint64_t offset;
  std::size_t level;
  bool fault;
};

struct ADD_RQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

struct ADD_WQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

struct ADD_PQ_data {
  uint64_t instr_id;
  champsim::address address;
  champsim::address v_address;
  access_type type;
};

class EventListener {
public:
  void process_event(event eventType, void* data);
  //void process_event(event eventType, char* data, int datalen) {
  //  std::cout << "Got an event!\n"; // << eventType << std::endl;
  //}
};

namespace champsim {
  extern std::vector<EventListener*> event_listeners;
}

extern void call_event_listeners(event eventType, void* data);
extern void init_event_listeners();
extern void cleanup_event_listeners();

#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
