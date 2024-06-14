#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "event_listener.h"

namespace champsim {
std::vector<EventListener*> event_listeners;
}

/*class TempListener : public EventListener {
  std::vector<float> set_access_counts;
  bool in_warmup = true;
  float num_accesses = 0;

  void process_event(event eventType, void* data) {
    // handle warmup
    if (eventType == event::BEGIN_PHASE) {
      BEGIN_PHASE_data* b_data = static_cast<BEGIN_PHASE_data *>(data);
      in_warmup = b_data->is_warmup;
    }
    if (in_warmup) {
      return;
    }

    if (eventType == event::CACHE_TRY_HIT) {
      CACHE_TRY_HIT_data* c_data = static_cast<CACHE_TRY_HIT_data *>(data);
      //fmt::print("Set: {} Way: {}\n", c_data->set, c_data->way);
      if (c_data->NAME == "cpu0_L1D") {
        while (c_data->set+1 > set_access_counts.size()) {
          set_access_counts.push_back(0);
        }
        set_access_counts[c_data->set]++;
        num_accesses++;
      }
    } else if (eventType == event::END) {
      for (int i = 0; i < set_access_counts.size(); i++) {
        if (set_access_counts[i] > 0) {
          fmt::print("Set: {} Count: {} Percent: {:.2f}\n", i, set_access_counts[i], set_access_counts[i] / num_accesses);
        }
      }
    }
  }
};

class CPIStacksListener : public EventListener {
  // for detecting burstiness
  std::vector<long> five_streak_lengths;
  long current_five_streak_length = 0;

  long computing_cycles = 0;
  long stalled_cycles = 0;
  long stalled_pt_miss_cycles = 0;
  long flushed_cycles = 0;
  long drained_cycles = 0;
  long drained_streak_cycles = 0; 
 
  uint64_t last_instr_id = 99999;
  std::vector<int> computing_counts;
  std::map<std::string, int> stalled_cache_miss_counts;
  std::map<std::string, int> drained_cache_miss_counts;
  std::vector<std::pair<uint64_t, std::string> > cache_misses;

  bool in_warmup = false;
  bool has_previous_instruction = false;
  bool previous_instruction_mispredicted_branch = false;
  //std::string previous_instruction_cache_misses = "";

  void handle_five_streak(long val) {
    if (val <= 0) {
      return;
    }
    while (five_streak_lengths.size() < val) {
      five_streak_lengths.push_back(0);
    }
    five_streak_lengths[val - 1]++;
  }

  void process_event(event eventType, void* data) {
    if (eventType == event::BEGIN_PHASE) {
      BEGIN_PHASE_data* b_data = static_cast<BEGIN_PHASE_data *>(data);
      in_warmup = b_data->is_warmup;
    } else if (!in_warmup && eventType == event::CACHE_TRY_HIT) {
      CACHE_TRY_HIT_data* c_data = static_cast<CACHE_TRY_HIT_data *>(data);
      if (!c_data->hit && c_data->instr_id > last_instr_id) {
        cache_misses.push_back(std::make_pair(c_data->instr_id, c_data->NAME));
      }
    } else if (!in_warmup && eventType == event::RETIRE) {
      RETIRE_data* r_data = static_cast<RETIRE_data *>(data);
      if (r_data->instrs.empty()) {
        // handle burstiness
        handle_five_streak(current_five_streak_length);
        current_five_streak_length = 0;

        if (r_data->ROB->empty()) {
          if (previous_instruction_mispredicted_branch) {
            // if no instructions retired, ROB is empty, and previous instruction was a mispredicted branch, then it's flushed
            flushed_cycles++;
          } else {
            // if no instructions retired, ROB is empty, and previous instruction wasn't a mispredicted branch, then we assume it's drained from an icache miss
            // this assumes the only time the ROB is flushed is from a branch misprediction, which isn't true
            // this counts some startup cycles as drained cycles
            drained_cycles++;
            drained_streak_cycles++;

            // check if drained from cache miss
            /*if (previous_instruction_cache_misses != "") {
              if (drained_cache_miss_counts.count(previous_instruction_cache_misses)) {
                drained_cache_miss_counts[previous_instruction_cache_misses]++;
              } else {
                drained_cache_miss_counts[previous_instruction_cache_misses] = 1;
              }
            }
          }
        } else {
          // if no instructions retired but ROB isn't empty, then it's stalled
          // this counts some startup cycles as stalled cycles
          stalled_cycles++;

          std::string name = "";
          for (auto cm : cache_misses) {
            if (cm.first == r_data->ROB->front().instr_id) {
              if (name != "") {
                name += "-";
              }
              name += cm.second;
            }
          }
          if (name != "") {
            if (stalled_cache_miss_counts.count(name)) {
              stalled_cache_miss_counts[name]++;
            } else {
              stalled_cache_miss_counts[name] = 1;
            }
          }
        }
      } else {
        // measuring burstiness
        if (r_data->instrs.size() >= 4) {
          current_five_streak_length++;
        } else {
          if (current_five_streak_length > 0) {
            handle_five_streak(current_five_streak_length);
            current_five_streak_length = 0;
          }
        }

        // if any instructions retired this cycle, in computing state
        computing_cycles++;

        // remove cache misses from committed instructions and set previous_instruction_cache_miss
        std::string first_name = "";
        for (auto instr : r_data->instrs) {
          int idx = 0;
          std::vector<int> to_remove = std::vector<int>();
          for (auto cm : cache_misses) {
            if (cm.first == instr.instr_id) {
              to_remove.push_back(idx);
              if (instr.instr_id == r_data->instrs[0].instr_id) {
                if (first_name != "") {
                  first_name += "-";
                }
                first_name += cm.second;
              }
            }
            idx++;
          }
          for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            cache_misses.erase(cache_misses.begin() + *it);
          }
        }
        if (drained_streak_cycles > 0 && first_name != "") {
          if (drained_cache_miss_counts.count(first_name)) {
            drained_cache_miss_counts[first_name] += drained_streak_cycles;
          } else {
            drained_cache_miss_counts[first_name] = drained_streak_cycles;
          }
        }
        drained_streak_cycles = 0;

        while (r_data->instrs.size() > computing_counts.size()) {
          computing_counts.push_back(0);
        }
        computing_counts[r_data->instrs.size() - 1]++;

        // update previous instruction data
        has_previous_instruction = true;
        previous_instruction_mispredicted_branch = r_data->instrs[r_data->instrs.size()-1].branch_mispredicted;
        last_instr_id = r_data->instrs[r_data->instrs.size()-1].instr_id;

        // clear any retired instructions from instruction lists
        /*foreach (uint64_t i : instr_with_pt_misses) {
          if (std::find(r_data->instrs.begin(), r_data->instrs.end(), i) != r_data->instrs.end()) {
            instr_with_pt_misses.erase(i)
          }
        }
      }
    } else if (eventType == event::END) {
      fmt::print("CPI Stacks:\n");
      fmt::print("Computing cycles: {}\n", computing_cycles);
      for (unsigned long i = 0; i < computing_counts.size(); i++) {
        fmt::print("  Retired {}: {}", i + 1, computing_counts[i]);
      }
      fmt::print("\nStalled cycles: {}\n", stalled_cycles);
      // sort and print cache misses
      std::vector<std::pair<std::string, int> > to_sort;
      std::copy(stalled_cache_miss_counts.begin(), stalled_cache_miss_counts.end(), std::back_inserter(to_sort));
      std::sort(to_sort.begin(), to_sort.end(), [](auto &left, auto &right) {
        return left.second > right.second;
      });
      for (const auto& [name, count] : to_sort) {
        fmt::print("  {}: {}", name, count);
      }
      fmt::print("\nFlushed cycles: {}\n", flushed_cycles);
      fmt::print("Drained cycles: {}\n", drained_cycles);
      
      // sort and print cache misses
      to_sort = std::vector<std::pair<std::string, int> >();
      std::copy(drained_cache_miss_counts.begin(), drained_cache_miss_counts.end(), std::back_inserter(to_sort));
      std::sort(to_sort.begin(), to_sort.end(), [](auto &left, auto &right) {
        return left.second > right.second;
      });
      for (const auto& [name, count] : to_sort) {
        fmt::print("  {}: {}", name, count);
      }
      fmt::print("\n\nUnaccounted cache misses: {}\n", cache_misses.size());
      /*for (auto [first, second] : cache_misses) {
        fmt::print("{} {}\n", first, second);
      }

      /*for (int i = 0; i < five_streak_lengths.size(); i++) {
        if (five_streak_lengths[i] > 0) {
          fmt::print("{} : {} ({})\n", i+1, five_streak_lengths[i], (i+1) * five_streak_lengths[i]);
        }
      }
    }
  }
};*/

void EventListener::process_event(event eventType, void* data) {
  // from ooo_cpu pipeline events
  if (eventType == event::PRE_CYCLE) {
    PRE_CYCLE_data* p_data = static_cast<PRE_CYCLE_data *>(data);
    fmt::print("[PRE_CYCLE] cycle: {} IFETCH_BUFFER cap.: {} DECODE_BUFFER cap.: {}, DISPATCH_BUFFER cap.: {} ROB cap.: {} LQ cap.: {}, SQ cap.: {}\n", p_data->cycle, p_data->IFETCH_BUFFER->size(), p_data->DECODE_BUFFER->size(), p_data->DISPATCH_BUFFER->size(), p_data->ROB->size(), p_data->LQ->size(), p_data->SQ->size());
  } else if (eventType == event::INITIALIZE) {
    INITIALIZE_data* i_data = static_cast<INITIALIZE_data *>(data);
    fmt::print("[INITIALIZE] instrs:");
    for (auto it = i_data->begin; it != i_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::CHECK_DIB) {
    CHECK_DIB_data* c_data = static_cast<CHECK_DIB_data *>(data);
    fmt::print("[CHECK_DIB] instrs:");
    for (auto it = c_data->begin; it != c_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_FETCH) {
    START_FETCH_data* s_data = static_cast<START_FETCH_data *>(data);
    fmt::print("[START_FETCH] successful: {} instrs:", s_data->success);
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_FETCH) {
    END_FETCH_data* e_data = static_cast<END_FETCH_data *>(data);
    fmt::print("[END_FETCH] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_DECODE) {
    START_DECODE_data* s_data = static_cast<START_DECODE_data *>(data);
    fmt::print("[START_DECODE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_DISPATCH) {
    START_DISPATCH_data* s_data = static_cast<START_DISPATCH_data *>(data);
    fmt::print("[START_DISPATCH] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_SCHEDULE) {
    START_SCHEDULE_data* s_data = static_cast<START_SCHEDULE_data *>(data);
    fmt::print("[START_SCHEDULE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_SCHEDULE) {
    END_SCHEDULE_data* e_data = static_cast<END_SCHEDULE_data *>(data);
    fmt::print("[END_SCHEDULE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::START_EXECUTE) {
    START_EXECUTE_data* e_data = static_cast<START_EXECUTE_data *>(data);
    fmt::print("[START_EXECUTE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::END_EXECUTE) {
    END_EXECUTE_data* e_data = static_cast<END_EXECUTE_data *>(data);
    fmt::print("[END_EXECUTE] instrs:");
    for (auto it = e_data->begin; it != e_data->end; ++it) {
      fmt::print(" {}", (*it)->instr_id);
    }
    fmt::print("\n");
  } else if (eventType == event::RETIRE) {
    RETIRE_data* s_data = static_cast<RETIRE_data *>(data);
    fmt::print("[RETIRE] instrs:");
    for (auto it = s_data->begin; it != s_data->end; ++it) {
      fmt::print(" {}", it->instr_id);
    }
    fmt::print("\n");
  }
  // from ooo_cpu.cc
  if (eventType == event::BRANCH) {
    BRANCH_data* b_data = static_cast<BRANCH_data *>(data);
    fmt::print("[BRANCH] do_predict_branch instr_id: {} ip: {} taken: {}\n", b_data->instr->instr_id, b_data->instr->ip, b_data->instr->branch_taken);
  } else if (eventType == event::LOAD_DEPENDENCY) {
    LOAD_DEPENDENCY_data* l_data = static_cast<LOAD_DEPENDENCY_data *>(data);
    fmt::print("[LOAD_DEPENDENCY] instr_id: {} waits on: {}\n", l_data->sink->instr_id, l_data->source->instr_id);
  } else if (eventType == event::SQ) {
    SQ_data* sq_data = static_cast<SQ_data *>(data);
     fmt::print("[SQ] {} instr_id: {} vaddr: {}\n", __func__, sq_data->instr->instr_id, sq_data->instr->virtual_address);
  } else if (eventType == event::CSTORE) {
    CSTORE_data* cs_data = static_cast<CSTORE_data *>(data);
     fmt::print("[CSTORE] {} instr_id: {} vaddr: {}\n", __func__, cs_data->instr->instr_id, cs_data->instr->virtual_address);
  } else if (eventType == event::ELOAD) {
    ELOAD_data* eload_data = static_cast<ELOAD_data *>(data);
    fmt::print("[EXELOAD] {} instr_id: {} vaddr: {}\n", __func__, eload_data->instr->instr_id, eload_data->instr->virtual_address);
  } else if (eventType == event::HANMEM) {
    HANMEM_data* hanmem_data = static_cast<HANMEM_data *>(data);
    fmt::print("[HANMEM] {} instr_id: {} fetch completed\n", __func__, hanmem_data->instr->instr_id);
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
  } else if (eventType == event::FINISH) {
    FINISH_data* finish_data = static_cast<FINISH_data *>(data);
     fmt::print("[LSQ] {} instr_id: {} full_address: {} remain_mem_ops: {}\n", __func__, finish_data->rob_entry->instr_id, finish_data->virtual_address,
               finish_data->rob_entry->num_mem_ops() - finish_data->rob_entry->completed_mem_ops);
  }
}

void call_event_listeners(event eventType, void* data) {
  for (auto & el : champsim::event_listeners) {
    el->process_event(eventType, data);
  }
}

/*void init_event_listeners() {
  champsim::event_listeners = std::vector<EventListener*>();
  //champsim::event_listeners.push_back(new EventListener());
  //champsim::event_listeners.push_back(new CPIStacksListener());
  //champsim::event_listeners.push_back(new TempListener());
}*/

void cleanup_event_listeners() {
  // TODO: delete each EventListener that was added to event_listeners
}
