#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "event_listener.h"

//class EventListener {
//  void process_event(EventType eventType, char* data, int datalen) {
//    std::cout << "Got event: " << eventType << std::endl;
//  }
//}

//std::vector<EventListener> listeners;

namespace champsim {
std::vector<EventListener*> event_listeners;
}

void EventListener::process_event(event eventType, char* data, int datalen) {
  if (eventType == event::BRANCH) {
    fmt::print("Got a branch\n");
  }
}

void init_event_listeners() {
  champsim::event_listeners = std::vector<EventListener*>();
  champsim::event_listeners.push_back(new EventListener());
}

void cleanup_event_listeners() {
  // TODO: delete each EventListener that was added to event_listeners
}
