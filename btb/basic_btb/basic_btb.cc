
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include "ooo_cpu.h"

#define BASIC_BTB_SETS 256
#define BASIC_BTB_WAYS 8
#define BASIC_BTB_INDIRECT_SIZE 1024
#define BASIC_BTB_RAS_SIZE 32

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
 * different ISAs could use different sizes for call instructions.
 * Furthermore, if the ISA you're looking at has variable-sized call
 * instructions, then the following solution would be insufficient
 * to always get it right.
 */
uint64_t basic_btb_last_return_target[NUM_CPUS];
uint64_t basic_btb_ras_call_return_offset[NUM_CPUS];

uint64_t basic_btb_set_index(uint64_t ip) { return (ip >> 2) % BASIC_BTB_SETS; }

BASIC_BTB_ENTRY *basic_btb_find_entry(uint8_t cpu, uint64_t ip) {
  uint64_t set = basic_btb_set_index(ip);
  for (uint32_t i = 0; i < BASIC_BTB_WAYS; i++) {
    if (basic_btb[cpu][set][i].ip_tag == ip) {
      return &(basic_btb[cpu][set][i]);
    }
  }

  return NULL;
}

BASIC_BTB_ENTRY *basic_btb_get_lru_entry(uint8_t cpu, uint64_t set) {
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

void basic_btb_update_lru(uint8_t cpu, BASIC_BTB_ENTRY *btb_entry) {
  btb_entry->lru = basic_btb_lru_counter[cpu];
  basic_btb_lru_counter[cpu]++;
}

uint64_t basic_btb_indirect_hash(uint8_t cpu, uint64_t ip) {
  uint64_t hash = (ip >> 2) ^ (basic_btb_conditional_history[cpu]);
  return hash % BASIC_BTB_INDIRECT_SIZE;
}

void push_basic_btb_ras(uint8_t cpu, uint64_t ip) {
  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] =
      ip + basic_btb_ras_call_return_offset[cpu];
  basic_btb_ras_index[cpu]++;
  if (basic_btb_ras_index[cpu] == BASIC_BTB_RAS_SIZE) {
    basic_btb_ras_index[cpu] = 0;
  }
}

uint64_t pop_basic_btb_ras(uint8_t cpu) {
  basic_btb_ras_index[cpu]--;
  if (basic_btb_ras_index[cpu] == -1) {
    basic_btb_ras_index[cpu] += BASIC_BTB_RAS_SIZE;
  }

  uint64_t target = basic_btb_ras[cpu][basic_btb_ras_index[cpu]];
  basic_btb_ras[cpu][basic_btb_ras_index[cpu]] = 0;

  return target;
}

void O3_CPU::initialize_btb() {
  std::cout << "Basic BTB sets: " << BASIC_BTB_SETS
            << " ways: " << BASIC_BTB_WAYS
            << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
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
  basic_btb_last_return_target[cpu] = 0;
  basic_btb_ras_call_return_offset[cpu] = 0;
}

uint64_t O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type,
                                uint8_t &always_taken) {
  if (branch_type != BRANCH_CONDITIONAL) {
    always_taken = true;
  }

  if ((branch_type == BRANCH_DIRECT_CALL) ||
      (branch_type == BRANCH_INDIRECT_CALL)) {
    // add something to the RAS
    push_basic_btb_ras(cpu, ip);
  }

  if (branch_type == BRANCH_RETURN) {
    // pop something off the RAS
    uint64_t target = pop_basic_btb_ras(cpu);
    // record the latest prediction from the RAS
    basic_btb_last_return_target[cpu] = target;

    return target;
  } else if ((branch_type == BRANCH_INDIRECT) ||
             (branch_type == BRANCH_INDIRECT_CALL)) {
    return basic_btb_indirect[cpu][basic_btb_indirect_hash(cpu, ip)];
  } else {
    // use BTB for all other branches + direct calls
    auto btb_entry = basic_btb_find_entry(cpu, ip);

    if (btb_entry == NULL) {
      // no prediction for this IP
      always_taken = 1;
      return 0;
    }

    always_taken = btb_entry->always_taken;
    basic_btb_update_lru(cpu, btb_entry);

    return btb_entry->target;
  }

  return 0;
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken,
                        uint8_t branch_type) {
  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) ||
      (branch_type == BRANCH_INDIRECT_CALL)) {
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
    // if our return prediction got us into the right cache line, but not the
    // right byte target, then adjust our offset up or down, accordingly
    if ((basic_btb_last_return_target[cpu] >> 6) == (branch_target >> 6)) {
      if (basic_btb_last_return_target[cpu] > branch_target) {
        // current offset is too high, so lower it
        basic_btb_ras_call_return_offset[cpu]--;
      } else if (basic_btb_last_return_target[cpu] < branch_target) {
        // current offset is too low, so increase it
        basic_btb_ras_call_return_offset[cpu]++;
      }
    }
  } else {
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
