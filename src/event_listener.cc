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

void EventListener::process_event(event eventType, void* data) {
  if (eventType == event::BRANCH) {
    BRANCH_data* b_data = static_cast<BRANCH_data *>(data);
    fmt::print("[BRANCH] instr_id: {} ip: {} taken: {}\n", b_data->instr->instr_id, b_data->instr->ip, b_data->instr->branch_taken);
    //fmt::print("Got a branch\n");
  }
}

void call_event_listeners(event eventType, void* data) {
  for (auto & el : champsim::event_listeners) {
    el->process_event(eventType, data);
  }
}

void init_event_listeners() {
  champsim::event_listeners = std::vector<EventListener*>();
  champsim::event_listeners.push_back(new EventListener());
}

void cleanup_event_listeners() {
  // TODO: delete each EventListener that was added to event_listeners
}
