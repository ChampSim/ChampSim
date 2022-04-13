#include "champsim.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <fstream>
#include <functional>
#include <iomanip>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

bool warmup_complete[NUM_CPUS] = {};

auto start_time = time(NULL);

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
  }
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    std::cout << cache->NAME;
    std::cout << " TOTAL     ACCESS: " << std::setw(10) << TOTAL_ACCESS << "  HIT: " << std::setw(10) << TOTAL_HIT << "  MISS: " << std::setw(10) << TOTAL_MISS
              << std::endl;

    std::cout << cache->NAME;
    std::cout << " LOAD      ACCESS: " << std::setw(10) << cache->roi_access[cpu][0] << "  HIT: " << std::setw(10) << cache->roi_hit[cpu][0]
              << "  MISS: " << std::setw(10) << cache->roi_miss[cpu][0] << std::endl;

    std::cout << cache->NAME;
    std::cout << " RFO       ACCESS: " << std::setw(10) << cache->roi_access[cpu][1] << "  HIT: " << std::setw(10) << cache->roi_hit[cpu][1]
              << "  MISS: " << std::setw(10) << cache->roi_miss[cpu][1] << std::endl;

    std::cout << cache->NAME;
    std::cout << " PREFETCH  ACCESS: " << std::setw(10) << cache->roi_access[cpu][2] << "  HIT: " << std::setw(10) << cache->roi_hit[cpu][2]
              << "  MISS: " << std::setw(10) << cache->roi_miss[cpu][2] << std::endl;

    std::cout << cache->NAME;
    std::cout << " WRITEBACK ACCESS: " << std::setw(10) << cache->roi_access[cpu][3] << "  HIT: " << std::setw(10) << cache->roi_hit[cpu][3]
              << "  MISS: " << std::setw(10) << cache->roi_miss[cpu][3] << std::endl;

    std::cout << cache->NAME;
    std::cout << " TRANSLATION ACCESS: " << std::setw(10) << cache->roi_access[cpu][4] << "  HIT: " << std::setw(10) << cache->roi_hit[cpu][4]
              << "  MISS: " << std::setw(10) << cache->roi_miss[cpu][4] << std::endl;

    std::cout << cache->NAME;
    std::cout << " PREFETCH  REQUESTED: " << std::setw(10) << cache->pf_requested << "  ISSUED: " << std::setw(10) << cache->pf_issued;
    std::cout << "  USEFUL: " << std::setw(10) << cache->pf_useful << "  USELESS: " << std::setw(10) << cache->pf_useless << std::endl;

    std::cout << cache->NAME;
    std::cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency)) / TOTAL_MISS << " cycles" << std::endl;
    // std::cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< std::endl;
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    std::cout << cache->NAME;
    std::cout << " TOTAL     ACCESS: " << std::setw(10) << TOTAL_ACCESS << "  HIT: " << std::setw(10) << TOTAL_HIT << "  MISS: " << std::setw(10) << TOTAL_MISS
              << std::endl;

    std::cout << cache->NAME;
    std::cout << " LOAD      ACCESS: " << std::setw(10) << cache->sim_access[cpu][0] << "  HIT: " << std::setw(10) << cache->sim_hit[cpu][0]
              << "  MISS: " << std::setw(10) << cache->sim_miss[cpu][0] << std::endl;

    std::cout << cache->NAME;
    std::cout << " RFO       ACCESS: " << std::setw(10) << cache->sim_access[cpu][1] << "  HIT: " << std::setw(10) << cache->sim_hit[cpu][1]
              << "  MISS: " << std::setw(10) << cache->sim_miss[cpu][1] << std::endl;

    std::cout << cache->NAME;
    std::cout << " PREFETCH  ACCESS: " << std::setw(10) << cache->sim_access[cpu][2] << "  HIT: " << std::setw(10) << cache->sim_hit[cpu][2]
              << "  MISS: " << std::setw(10) << cache->sim_miss[cpu][2] << std::endl;

    std::cout << cache->NAME;
    std::cout << " WRITEBACK ACCESS: " << std::setw(10) << cache->sim_access[cpu][3] << "  HIT: " << std::setw(10) << cache->sim_hit[cpu][3]
              << "  MISS: " << std::setw(10) << cache->sim_miss[cpu][3] << std::endl;
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    std::cout << std::endl << "CPU " << i << " Branch Prediction Accuracy: ";
    std::cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    std::cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr);
    std::cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
              << std::endl;

    // std::cout << "Branch types" << endl;
    // std::cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " << (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    // ooo_cpu[i]->begin_sim_instr) << "%" << std::endl; std::cout << "BRANCH_DIRECT_JUMP: " << ooo_cpu[i]->total_branch_types[1] << " " <<
    // (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << std::endl; std::cout << "BRANCH_INDIRECT: "
    // << ooo_cpu[i]->total_branch_types[2] << " " << (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%"
    // << std::endl; std::cout << "BRANCH_CONDITIONAL: " << ooo_cpu[i]->total_branch_types[3] << " " <<
    // (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << std::endl; std::cout << "BRANCH_DIRECT_CALL:
    // " << ooo_cpu[i]->total_branch_types[4] << " " << (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%"
    // << std::endl; std::cout << "BRANCH_INDIRECT_CALL: " << ooo_cpu[i]->total_branch_types[5] << " " <<
    // (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << std::endl; std::cout << "BRANCH_RETURN: " <<
    // ooo_cpu[i]->total_branch_types[6] << " " <<  (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" <<
    // std::endl; std::cout << "BRANCH_OTHER: " << ooo_cpu[i]->total_branch_types[7] << " " <<
    // (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << std::endl << std::endl;

    std::cout << "Branch type MPKI" << std::endl;
    std::cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << std::endl;
    std::cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << std::endl;
    std::cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << std::endl;
    std::cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << std::endl;
    std::cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr))
              << std::endl;
    std::cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << std::endl
              << std::endl;
  }
}

void print_dram_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  std::cout << std::endl;
  std::cout << "DRAM Statistics" << std::endl;
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    std::cout << " CHANNEL " << i << std::endl;

    auto& channel = DRAM.channels[i];
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (DRAM_CHANNELS > 1) {
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
  }
}

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
  }

  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_fill = 0;

  cache->total_miss_latency = 0;

  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

  std::cout << std::endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    std::cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++) {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
  std::cout << std::endl;

  // reset DRAM stats
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
}

int champsim_main(uint64_t warmup_instructions, uint64_t simulation_instructions, bool show_heartbeat, bool knob_cloudsuite,
                  bool repeat_trace, std::vector<std::string> trace_names)
{
  std::cout << std::endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl << std::endl;

  // consequences of knobs
  std::cout << "Warmup Instructions: " << warmup_instructions << std::endl;
  std::cout << "Simulation Instructions: " << simulation_instructions << std::endl;
  std::cout << "Number of CPUs: " << NUM_CPUS << std::endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  std::vector<tracereader*> traces;
  for (auto name : trace_names) {
    std::cout << "CPU " << traces.size() << " runs " << name << std::endl;

    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite, repeat_trace));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      return 1;
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    return 1;
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu) {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  bool warmup_finished = false;
  std::bitset<NUM_CPUS> simulation_complete;
  while (!simulation_complete.all()) {
    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables) {
      try {
        op->_operate();
      } catch (champsim::deadlock& dl) {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables) {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i) {
      // read from trace
      while ((ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0) && (ooo_cpu[i]->trace_drained == 0)) {
        ooo_cpu[i]->init_instruction(traces[i]->get());
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
        float cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

        std::cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        std::cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      warmup_complete[i] = (ooo_cpu[i]->num_retired > warmup_instructions);
      if (std::all_of(std::begin(warmup_complete), std::end(warmup_complete), [](auto x) { return x; }) && !warmup_finished) {
        finish_warmup();
        warmup_finished = true;
      }

      // simulation complete
      if ((warmup_finished && !simulation_complete[i] && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions))) || (ooo_cpu[i]->trace_drained == 1)) {
        simulation_complete.set(i);

        ooo_cpu[i]->trace_drained = 2;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

        std::cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        std::cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
          record_roi_stats(i, *it);
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  std::cout << std::endl << "ChampSim completed all CPUs" << std::endl;
  if (NUM_CPUS > 1) {
    std::cout << std::endl << "Total Simulation Statistics (not including warmup)" << std::endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      std::cout << std::endl
                << "CPU " << i << " cumulative IPC: "
                << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      std::cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
                << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << std::endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  std::cout << std::endl << "Region of Interest Statistics" << std::endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    std::cout << std::endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    std::cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << std::endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
  print_dram_stats();
  print_branch_stats();
#endif

  return 0;
}
