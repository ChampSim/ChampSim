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

enum class event {
  CYCLE_BEGIN,
  BRANCH
};

struct CYCLE_BEGIN_data {};

struct BRANCH_data {
  ooo_model_instr* instr;

  BRANCH_data() {
    instr = nullptr;
  }
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
