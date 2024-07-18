#include "event_listener.h"

namespace champsim {
std::vector<EventListener*> event_listeners;
}

void call_event_listeners(event eventType, void* data) {
  for (auto & el : champsim::event_listeners) {
    el->process_event(eventType, data);
  }
}

void cleanup_event_listeners() {
  for (auto & el : champsim::event_listeners) {
    delete el;
  }
}
