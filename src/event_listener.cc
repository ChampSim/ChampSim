#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "event_listener.h"

namespace champsim {
std::vector<EventListener*> event_listeners;
}

class CPIStacksListener : public EventListener {
  long computing_cycles = 0;
  long stalled_cycles = 0;
  long flushed_cycles = 0;
  long drained_cycles = 0;
  
  std::vector<int> computing_counts;

  bool in_warmup = false;
  bool has_previous_instruction = false;
  bool previous_instruction_mispredicted_branch = false;

  void process_event(event eventType, void* data) {
    if (eventType == event::BEGIN_PHASE) {
      BEGIN_PHASE_data* b_data = static_cast<BEGIN_PHASE_data *>(data);
      in_warmup = b_data->is_warmup;
    }
    if (!in_warmup && eventType == event::RETIRE) {
      RETIRE_data* r_data = static_cast<RETIRE_data *>(data);
      if (r_data->instrs.empty()) {
        if (r_data->ROB->empty()) {
          if (previous_instruction_mispredicted_branch) {
            // if no instructions retired, ROB is empty, and previous instruction was a mispredicted branch, then it's flushed
            flushed_cycles++;
          } else {
            // if no instructions retired, ROB is empty, and previous instruction wasn't a mispredicted branch, then we assume it's drained from an icache miss
            // this assumes the only time the ROB is flushed is from a branch misprediction, which isn't true
            // this counts some startup cycles as drained cycles
            drained_cycles++;
          }
        } else {
          // if no instructions retired but ROB isn't empty, then it's stalled
          // this counts some startup cycles as stalled cycles
          stalled_cycles++;
        }
      } else {
        // if any instructions retired this cycle, in computing state
        computing_cycles++;

        while (r_data->instrs.size() > computing_counts.size()) {
          computing_counts.push_back(0);
        }
        computing_counts[r_data->instrs.size() - 1]++;

        // update previous instruction data
        has_previous_instruction = true;
        previous_instruction_mispredicted_branch = r_data->instrs[r_data->instrs.size()-1].branch_mispredicted;
      }
    } else if (eventType == event::END) {
      fmt::print("CPI Stacks:\n");
      fmt::print("Computing cycles: {}\n", computing_cycles);
      for (unsigned long i = 0; i < computing_counts.size(); i++) {
        fmt::print("  Retired {}: {}", i + 1, computing_counts[i]);
      }
      fmt::print("\nStalled cycles: {}\n", stalled_cycles);
      fmt::print("Flushed cycles: {}\n", flushed_cycles);
      fmt::print("Drained cycles: {}\n", drained_cycles);
    }
  }
};

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
  /*else if (eventType == event::FETCH) {
    FETCH_data* f_data = static_cast<FETCH_data *>(data);
    fmt::print("[IFETCH] {} instr_id: {} ip: {:#x} dependents: {} event_cycle: {}\n", __func__, begin->instr_id, begin->ip,
               std::size(fetch_packet.instr_depend_on_me), cycle);
    //fmt::print("Got a branch\n");
  }*/
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
  else if (eventType == event::RETIRE) {
    RETIRE_data* r_data = static_cast<RETIRE_data *>(data);
    for (auto instr: r_data->instrs) {
      fmt::print("[ROB] retire_rob instr_id: {} is retired cycle: {}\n", instr.instr_id, r_data->cycle);
    }
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
  } else if (eventType == event::PTW_HANDLE_READ) {
    PTW_HANDLE_READ_data* p_data = static_cast<PTW_HANDLE_READ_data *>(data);
    fmt::print("[{}] handle_read address: {} v_address: {} pt_page_offset: {} translation_level: {} cycle: {}\n", p_data->NAME, p_data->address, p_data->v_address, p_data->pt_page_offset, p_data->translation_level, p_data->cycle);
  } else if (eventType == event::PTW_HANDLE_FILL) {
    PTW_HANDLE_FILL_data* p_data = static_cast<PTW_HANDLE_FILL_data *>(data);
    fmt::print("[{}] handle_fill address: {} v_address: {} data: {} pt_page_offset: {} translation_level: {} cycle: {}\n", p_data->NAME, p_data->address, p_data->v_address, p_data->data, p_data->pt_page_offset, p_data->translation_level, p_data->cycle);
  } else if (eventType == event::PTW_OPERATE) {
    PTW_OPERATE_data* p_data = static_cast<PTW_OPERATE_data *>(data);
    fmt::print("[{}] operate MSHR contents: {} cycle: {}\n", p_data->NAME, p_data->mshr_addresses, p_data->cycle);
  } else if (eventType == event::PTW_FINISH_PACKET) {
    PTW_FINISH_PACKET_data* p_data = static_cast<PTW_FINISH_PACKET_data *>(data);
    fmt::print("[{}] finish_packet address: {} v_address: {} data: {} translation_level: {} cycle: {} penalty: {}\n", p_data->NAME, p_data->address, p_data->v_address, p_data->data, p_data->translation_level, p_data->cycle, p_data->penalty);
  } else if (eventType == event::PTW_FINISH_PACKET_LAST_STEP) {
    PTW_FINISH_PACKET_LAST_STEP_data* p_data = static_cast<PTW_FINISH_PACKET_LAST_STEP_data *>(data);
    fmt::print("[{}] complete_packet address: {} v_address: {} data: {} translation_level: {} cycle: {} penalty: {}\n", p_data->NAME, p_data->address, p_data->v_address, p_data->data, p_data->translation_level, p_data->cycle, p_data->penalty);
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
  champsim::event_listeners.push_back(new CPIStacksListener());
}

void cleanup_event_listeners() {
  // TODO: delete each EventListener that was added to event_listeners
}
