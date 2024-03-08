#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

#include <vector>
#include <iostream>

enum class event {
  CYCLE_BEGIN,
  BRANCH
};

class EventListener {
public:
  void process_event(event eventType, char* data, int datalen);
  //void process_event(event eventType, char* data, int datalen) {
  //  std::cout << "Got an event!\n"; // << eventType << std::endl;
  //}
};

namespace champsim {
  extern std::vector<EventListener*> event_listeners;
}

extern void init_event_listeners();
extern void cleanup_event_listeners();

#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
