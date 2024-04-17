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
  else if (eventType == event::DIB) {
    DIB_data* dib_data = static_cast<DIB_data *>(data);
    fmt::print("[DIB] {} instr_id: {} ip: {} hit: {} cycle: {}\n", __func__, dib_data->instr->instr_id, dib_data->instr->ip, dib_data->instr->fetch_completed,
               dib_data->cycle);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::FETCH) {
    FETCH_data* f_data = static_cast<FETCH_data *>(data);
    fmt::print("[IFETCH] {} instr_id: {} ip: {} dependents: {} event_cycle: {}\n", __func__, f_data->begin->instr_id, f_data->begin->ip,
               std::size(f_data->instr_depend_on_me), f_data->cycle);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::DECODE) {
    DECODE_data* d_data = static_cast<DECODE_data *>(data);
    fmt::print("[DECODE] do_decode instr_id: {} cycle: {}\n", d_data->instr->instr_id, d_data->cycle);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::EXE) {
    EXE_data* e_data = static_cast<EXE_data *>(data);
     fmt::print("[EXE] {} instr_id: {} ready_time: {}\n", __func__, e_data->instr->instr_id, e_data->cycle);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::MEM) {
    MEM_data* m_data = static_cast<MEM_data *>(data);
     fmt::print("[MEM] {} instr_id: {} loads: {} stores: {} cycle: {}\n", __func__, m_data->instr->instr_id, std::size(m_data->instr->source_memory),
               std::size(m_data->instr->destination_memory), m_data->cycle);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::SQ) {
    SQ_data* sq_data = static_cast<SQ_data *>(data);
     fmt::print("[SQ] {} instr_id: {} vaddr: {}\n", __func__, sq_data->instr->instr_id, sq_data->instr->virtual_address);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::CSTORE) {
    CSTORE_data* cs_data = static_cast<CSTORE_data *>(data);
     fmt::print("[CSTORE] {} instr_id: {} vaddr: {}\n", __func__, cs_data->instr->instr_id, cs_data->instr->virtual_address);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::ELOAD) {
    ELOAD_data* eload_data = static_cast<ELOAD_data *>(data);
     fmt::print("[EXELOAD] {} instr_id: {} vaddr: {}\n", __func__, eload_data->instr->instr_id, eload_data->instr->virtual_address);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::HANMEM) {
    HANMEM_data* hanmem_data = static_cast<HANMEM_data *>(data);
     fmt::print("[HANMEM] {} instr_id: {} fetch completed\n", __func__, hanmem_data->instr->instr_id);
    //fmt::print("Got a branch\n");
  }
  else if (eventType == event::RETIRE) {
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
  } else if (eventType == event::VA_TO_PA) {
    VA_TO_PA_data* v_data = static_cast<VA_TO_PA_data *>(data);
    fmt::print("[VMEM] va_to_pa paddr: {} vaddr: {} fault: {}\n", v_data->paddr, v_data->vaddr, v_data->fault);
  } else if (eventType == event::GET_PTE_PA) {
    GET_PTE_PA_data* g_data = static_cast<GET_PTE_PA_data *>(data);
    fmt::print("[VMEM] get_pta_pa paddr: {} vaddr: {} pt_page_offset: {} translation_level: {} fault: {}\n", g_data->paddr, g_data->vaddr, g_data->offset, g_data->level, g_data->fault);
  } else if (eventType == event::ADD_RQ) {
    ADD_RQ_data* a_data = static_cast<ADD_RQ_data *>(data);
    fmt::print("[channel_rq] add_rq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  } else if (eventType == event::ADD_WQ) {
    ADD_WQ_data* a_data = static_cast<ADD_WQ_data *>(data);
    fmt::print("[channel_wq] add_wq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  } else if (eventType == event::ADD_PQ) {
    ADD_PQ_data* a_data = static_cast<ADD_PQ_data *>(data);
    fmt::print("[channel_pq] add_pq instr_id: {} address: {} v_address: {} type: {}\n", a_data->instr_id, a_data->address, a_data->v_address,
               access_type_names.at(champsim::to_underlying(a_data->type)));
  }
  else if (eventType == event::FINISH) {
    FINISH_data* finish_data = static_cast<FINISH_data *>(data);
     fmt::print("[LSQ] {} instr_id: {} full_address: {} remain_mem_ops: {}\n", __func__, finish_data->rob_entry->instr_id, finish_data->virtual_address,
               finish_data->rob_entry->num_mem_ops() - finish_data->rob_entry->completed_mem_ops);
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
