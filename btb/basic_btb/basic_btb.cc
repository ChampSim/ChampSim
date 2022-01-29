
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include "ooo_cpu.h"

#define BASIC_BTB_SETS 1024
#define BASIC_BTB_WAYS 8
#define BASIC_BTB_INDIRECT_SIZE 4096
#define BASIC_BTB_RAS_SIZE 64
#define BASIC_BTB_CALL_INSTR_SIZE_TRACKERS 1024

struct BASIC_BTB_ENTRY {
  uint64_t ip_tag;
  uint64_t target;
  uint8_t always_taken;
  uint64_t lru;
};

BASIC_BTB_ENTRY basic_btb[NUM_CPUS][BASIC_BTB_SETS][BASIC_BTB_WAYS];
uint64_t basic_btb_lru_counter[NUM_CPUS];

uint64_t basic_btb_indirect[NUM_CPUS][BASIC_BTB_INDIRECT_SIZE];
uint64_t basic_btb_conditional_history[NUM_CPUS];

uint64_t basic_btb_ras[NUM_CPUS][BASIC_BTB_RAS_SIZE];
int basic_btb_ras_index[NUM_CPUS];
/*
 * The following two variables are used to automatically identify the
 * size of call instructions, in bytes, which tells us the appropriate
 * target for a call's corresponding return.
 * They exist because ChampSim does not model a specific ISA, and
 * different ISAs could use different sizes for call instructions,
 * and even within the same ISA, calls can have different sizes.
 */
uint64_t basic_btb_call_instr_sizes[NUM_CPUS][BASIC_BTB_CALL_INSTR_SIZE_TRACKERS];

uint64_t basic_btb_abs_addr_dist(uint64_t addr1, uint64_t addr2)
{
  if (addr1 > addr2) {
    return addr1 - addr2;
  }

  return addr2 - addr1;
}

uint64_t basic_btb_set_index(uint64_t ip) { return ((ip >> 2) & (BASIC_BTB_SETS - 1)); }

BASIC_BTB_ENTRY* basic_btb_find_entry(uint8_t cpu, uint64_t ip)
{
  uint64_t set = basic_btb_set_index(ip);
  for (uint32_t i = 0; i < BASIC_BTB_WAYS; i++) {
    if (basic_btb[cpu][set][i].ip_tag == ip) {
      return &(basic_btb[cpu][set][i]);
    }
  }

  return NULL;
}

BASIC_BTB_ENTRY* basic_btb_get_lru_entry(uint8_t cpu, uint64_t set)
{
  uint32_t lru_way = 0;
  uint64_t lru_value = basic_btb[cpu][set][lru_way].lru;
  for (uint32_t i = 0; i < BASIC_BTB_WAYS; i++) {
    if (basic_btb[cpu][set][i].lru < lru_value) {
      lru_way = i;
      lru_value = basic_btb[cpu][set][lru_way].lru;
    }
  }

  return &(basic_btb[cpu][set][lru_way]);
}

void basic_btb_update_lru(uint8_t cpu, BASIC_BTB_ENTRY* btb_entry)
{
  btb_entry->lru = basic_btb_lru_counter[cpu];
  basic_btb_lru_counter[cpu]++;
}

uint64_t basic_btb_indirect_hash(uint8_t cpu, uint64_t ip)
{
  uint64_t hash = (ip >> 2) ^ (basic_btb_conditional_history[cpu]);
  return (hash & (BASIC_BTB_INDIRECT_SIZE - 1));
}

void push_basic_btb_ras(uint8_t cpu, uint64_t ip)
{
  basic_btb_ras_index[cpu]++;
  if (basic_btb_ras_index[cpu] == BASIC_BTB_RAS_SIZE) {
    basic_btb_ras_index[cpu] = 0;
  }

  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = ip;
}

uint64_t peek_basic_btb_ras(uint8_t cpu) { return basic_btb_ras[cpu][basic_btb_ras_index[cpu]]; }

uint64_t pop_basic_btb_ras(uint8_t cpu)
{
  uint64_t target = basic_btb_ras[cpu][basic_btb_ras_index[cpu]];
  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = 0;

  basic_btb_ras_index[cpu]--;
  if (basic_btb_ras_index[cpu] == -1) {
    basic_btb_ras_index[cpu] += BASIC_BTB_RAS_SIZE;
  }

  return target;
}

uint64_t basic_btb_call_size_tracker_hash(uint64_t ip) { return (ip & (BASIC_BTB_CALL_INSTR_SIZE_TRACKERS - 1)); }

uint64_t basic_btb_get_call_size(uint8_t cpu, uint64_t ip)
{
  uint64_t size = basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(ip)];

  return size;
}

void O3_CPU::initialize_btb()
{
  std::cout << "Basic BTB sets: " << BASIC_BTB_SETS << " ways: " << BASIC_BTB_WAYS << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
            << " RAS size: " << BASIC_BTB_RAS_SIZE << std::endl;

  for (uint32_t i = 0; i < BASIC_BTB_SETS; i++) {
    for (uint32_t j = 0; j < BASIC_BTB_WAYS; j++) {
      basic_btb[cpu][i][j].ip_tag = 0;
      basic_btb[cpu][i][j].target = 0;
      basic_btb[cpu][i][j].always_taken = 0;
      basic_btb[cpu][i][j].lru = 0;
    }
  }
  basic_btb_lru_counter[cpu] = 0;

  for (uint32_t i = 0; i < BASIC_BTB_INDIRECT_SIZE; i++) {
    basic_btb_indirect[cpu][i] = 0;
  }
  basic_btb_conditional_history[cpu] = 0;

  for (uint32_t i = 0; i < BASIC_BTB_RAS_SIZE; i++) {
    basic_btb_ras[cpu][i] = 0;
  }
  basic_btb_ras_index[cpu] = 0;
  for (uint32_t i = 0; i < BASIC_BTB_CALL_INSTR_SIZE_TRACKERS; i++) {
    basic_btb_call_instr_sizes[cpu][i] = 4;
  }
}

std::pair<uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type)
{
  uint8_t always_taken = false;
  if (branch_type != BRANCH_CONDITIONAL) {
    always_taken = true;
  }

  if ((branch_type == BRANCH_DIRECT_CALL) || (branch_type == BRANCH_INDIRECT_CALL)) {
    // add something to the RAS
    push_basic_btb_ras(cpu, ip);
  }

  if (branch_type == BRANCH_RETURN) {
    // peek at the top of the RAS
    uint64_t target = peek_basic_btb_ras(cpu);
    // and adjust for the size of the call instr
    target += basic_btb_get_call_size(cpu, target);

    return std::make_pair(target, always_taken);
  } else if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    return std::make_pair(basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)], always_taken);
  } else {
    // use BTB for all other branches + direct calls
    auto btb_entry = basic_btb_find_entry(cpu, ip);

    if (btb_entry == NULL) {
      // no prediction for this IP
      always_taken = true;
      return std::make_pair(0, always_taken);
    }

    always_taken = btb_entry->always_taken;
    basic_btb_update_lru(cpu, btb_entry);

    return std::make_pair(btb_entry->target, always_taken);
  }

  return std::make_pair(0, always_taken);
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)] = branch_target;
  }
  if (branch_type == BRANCH_CONDITIONAL) {
    basic_btb_conditional_history[cpu] <<= 1;
    if (taken) {
      basic_btb_conditional_history[cpu] |= 1;
    }
  }

  if (branch_type == BRANCH_RETURN) {
    // recalibrate call-return offset
    // if our return prediction got us into the right ball park, but not the
    // exactly correct byte target, then adjust our call instr size tracker
    uint64_t call_ip = pop_basic_btb_ras(cpu);
    uint64_t estimated_call_instr_size = basic_btb_abs_addr_dist(call_ip, branch_target);
    if (estimated_call_instr_size <= 10) {
      basic_btb_call_instr_sizes[cpu][basic_btb_call_size_tracker_hash(call_ip)] = estimated_call_instr_size;
    }
  } else if ((branch_type != BRANCH_INDIRECT) && (branch_type != BRANCH_INDIRECT_CALL)) {
    // use BTB
    auto btb_entry = basic_btb_find_entry(cpu, ip);

    if (btb_entry == NULL) {
      if ((branch_target != 0) && taken) {
        // no prediction for this entry so far, so allocate one
        uint64_t set = basic_btb_set_index(ip);
        auto repl_entry = basic_btb_get_lru_entry(cpu, set);

        repl_entry->ip_tag = ip;
        repl_entry->target = branch_target;
        repl_entry->always_taken = 1;
        basic_btb_update_lru(cpu, repl_entry);
      }
    } else {
      // update an existing entry
      btb_entry->target = branch_target;
      if (!taken) {
        btb_entry->always_taken = 0;
      }
    }
  }
}
