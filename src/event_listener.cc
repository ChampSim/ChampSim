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

void call_event_listeners(event eventType, void* data) {
  for (auto & el : champsim::event_listeners) {
    el->process_event(eventType, data);
  }
}

void cleanup_event_listeners() {
  // TODO: delete each EventListener that was added to event_listeners
  for (auto & el : champsim::event_listeners) {
    delete el;
  }
}
