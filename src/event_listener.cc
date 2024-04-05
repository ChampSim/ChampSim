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
  } else if (eventType == event::RETIRE) {
    RETIRE_data* r_data = static_cast<RETIRE_data *>(data);
    /*fmt::print("[RETIRE] cycle: {}", r_data->cycle);
    for (auto instr : r_data->instrs) {
      fmt::print(" instr_id: {}", instr.instr_id);
    }
    fmt::print("\n");*/
    for (auto instr: r_data->instrs) {
      fmt::print("[ROB] retire_rob instr_id: {} is retired cycle: {}\n", instr.instr_id, r_data->cycle);
    }
    /*std::for_each(retire_begin, retire_end, [cycle = current_time.time_since_epoch() / clock_period](const auto& x) {
      fmt::print("[ROB] retire_rob instr_id: {} is retired cycle: {}\n", x.instr_id, cycle);
    });*/
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
