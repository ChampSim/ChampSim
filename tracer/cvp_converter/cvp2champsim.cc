/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <sstream>
#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trace_instruction.h"

// defines for the paths for the various decompression programs and Apple/Linux differences

#ifdef __APPLE__
#define XZ_PATH "/opt/local/bin/xz"
#define GZIP_PATH "/usr/bin/gzip"
#define CAT_PATH "/bin/cat"
#define UINT64 uint64_t
#else
#define XZ_PATH "/usr/bin/xz"
#define GZIP_PATH "/bin/gzip"
#define CAT_PATH "/bin/cat"
#define UINT64 unsigned long long int
#endif

constexpr char TRANSLATED_REG_INSTRUCTION_POINTER = 64;
constexpr char TRANSLATED_REG_STACK_POINTER = 65;
constexpr char TRANSLATED_REG_FLAGS = 66;
constexpr char TRANSLATED_REG_ZERO = 67;

// For memory instructions that have multiple destination registers, adds all of them as destinations (unless base update registers if APPLY_IMP_BASE_UPDATE is enabled). For memory instructions that have no destination register, does not add any. 
bool APPLY_IMP_MEM_REGS = true;

// Adds an additional ALU instruction to perform the base update before the memory operation (pre-indexing increment) or after it (post-indexing increment). The base register is removed from the destination registers of the memory instruction.
bool APPLY_IMP_BASE_UPDATE = true;   

// Memory operations crossing cachelines will add the address of the second cacheline as a second memory source/destination. In addition, DCZVA will fix their size so that they do not cross cachelines (by definition, they touch a single cacheline but their address could be non aligned to a cacheline boundary).
bool APPLY_IMP_MEM_FOOTPRINT = true; 

// Fixes the bug by which some calls reading and writing to X30 were identified as returns.
bool APPLY_IMP_CALL_STACK = false;

// Adds the flag register as destination to ALU and FP instructions without one.
bool APPLY_IMP_FLAG_REG = false;

// Adds the source registers of branches in the CVP trace to the converted branches (with minor limitations described in the paper). Conditional branches that add a source register from the CVP trace will not add the flag register. Indirect jumps and calls that add a source register from the CVP trace will not add the REG_AX (originally used to convey the read others infomation to ChampSim).
bool APPLY_IMP_BRANCH_REGS = false;


// This improvement fixes the branch target for indirect calls with x30 as source and detination register, which is incorrect in the CVP traces. This improvement does not have any effect in ChampSim as ChampSim traces to not include the branch target. Thus, it is not described in the paper.
const bool APPLY_IMP_BRANCH_TARGET = false;


bool verbose = false;

// use non-cloudsuite ChampSim trace format
using trace_instr_format = input_instr;

void print (trace_instr_format tr) {
  std::cerr << "0x" << std::hex << tr.ip << std::dec << " ";
  if (tr.is_branch) {
    std::cerr << "B " << (bool) tr.branch_taken << " ";
  }
  std::cerr << "SR: ";
  for (int i=0; i < NUM_INSTR_SOURCES; i++) {
    if (tr.source_registers[i] != 0)
      std::cerr << (int) tr.source_registers[i] << " ";
  }
  std::cerr << "DR: ";
  for (int i=0; i < NUM_INSTR_DESTINATIONS; i++) {
    if (tr.destination_registers[i] != 0)
      std::cerr << (int) tr.destination_registers[i] << " ";
  }
  std::cerr << "SM: ";
  for (int i=0; i < NUM_INSTR_SOURCES; i++) {
    if (tr.source_memory[i] != 0)
      std::cerr << "0x" << std::hex << tr.source_memory[i] << std::dec << " ";
  }
  std::cerr << "DR: ";
  for (int i=0; i < NUM_INSTR_DESTINATIONS; i++) {
    if (tr.destination_memory[i] != 0)
      std::cerr << "0x" << std::hex << tr.destination_memory[i] << std::dec << " ";
  }
  std::cerr << std::endl;
}

// orginal instruction types from CVP-1 traces
typedef enum {
  aluInstClass = 0,
  loadInstClass = 1,
  storeInstClass = 2,
  condBranchInstClass = 3,
  uncondDirectBranchInstClass = 4,
  uncondIndirectBranchInstClass = 5,
  fpInstClass = 6,
  slowAluInstClass = 7,
  undefInstClass = 8
} InstClass;

const char *InstClassToString (InstClass t) {
  switch (t) {
    case aluInstClass:
      return "aluInstClass";
    case loadInstClass:
      return "loadInstClass";
    case storeInstClass:
      return "storeInstClass";
    case condBranchInstClass:
      return "condBranchInstClass";
    case uncondDirectBranchInstClass:
      return "uncondDirectBranchInstClass";
    case uncondIndirectBranchInstClass:
      return "uncondIndirectBranchInstClass";
    case fpInstClass:
      return "fpInstClass";
    case slowAluInstClass:
      return "slowAluInstClass";
    case undefInstClass:
      return "undefInstClass";
    default:
      assert(false);
  }
}

// branch types from CBP5
typedef enum {
  OPTYPE_OP = 2,
  OPTYPE_RET_UNCOND,
  OPTYPE_JMP_DIRECT_UNCOND,
  OPTYPE_JMP_INDIRECT_UNCOND,
  OPTYPE_CALL_DIRECT_UNCOND,
  OPTYPE_CALL_INDIRECT_UNCOND,
  OPTYPE_RET_COND,
  OPTYPE_JMP_DIRECT_COND,
  OPTYPE_JMP_INDIRECT_COND,
  OPTYPE_CALL_DIRECT_COND,
  OPTYPE_CALL_INDIRECT_COND,
  OPTYPE_ERROR,
  OPTYPE_MAX
} OpType;

const char* branch_names[] = {
    "none",
    "none",
    "OPTYPE_OP",
    "OPTYPE_RET_UNCOND",
    "OPTYPE_JMP_DIRECT_UNCOND",
    "OPTYPE_JMP_INDIRECT_UNCOND",
    "OPTYPE_CALL_DIRECT_UNCOND",
    "OPTYPE_CALL_INDIRECT_UNCOND",
    "OPTYPE_RET_COND",
    "OPTYPE_JMP_DIRECT_COND",
    "OPTYPE_JMP_INDIRECT_COND",
    "OPTYPE_CALL_DIRECT_COND",
    "OPTYPE_CALL_INDIRECT_COND",
    "OPTYPE_ERROR",
    "OPTYPE_MAX",
};


enum AddresssingMode : uint8_t {
  NoMode,
  BaseImmediateOffset,
  BaseRegOffset,
  BaseUpdate,
  BaseRegImmediate,
  PcRelative,
  DCZVA,
  Prefetch,
  Other,
};

struct AddrModeInfo {
  AddresssingMode m_mode = AddresssingMode::NoMode;
  uint8_t m_corrected_access_size = 0;
  uint8_t m_base_reg = 0;
  uint64_t m_mask = ~0;
  uint64_t m_last_ea = 0;
  bool m_load_store_pair = false;
  uint64_t m_last_base_reg_value = 0;

  // Used to make sure that the AddrModeInfo is for the same instruction of the same thread
  uint64_t m_mem_op_key;

  AddrModeInfo() = default;

  AddrModeInfo(AddresssingMode mode, uint8_t access_size, uint8_t base_reg, uint64_t key, bool pair = false, uint64_t mask = ~0)
  : m_mode(mode)
  , m_corrected_access_size(access_size)
  , m_base_reg(base_reg)
  , m_mem_op_key(key)
  , m_load_store_pair(pair)
  , m_mask(mask)  
  {
  }
};

// We will keep a register file up to date
UINT64 registers[256][2];
bool populated_registers[256];

bool first_time_assert = true;

// This is a cache to help us refine our addressing mode/pair vs single detection
std::unordered_map<uint64_t, AddrModeInfo> addr_mode_helper;

long long int counts[OPTYPE_MAX];

// Statistics
long long int cond_branches_depending_on_flag_register = 0;
long long int cond_branches_not_depending_on_flag_register = 0;
long long int alus_with_no_destination_register = 0;
long long int fps_with_no_destination_register = 0;
long long int indirect_calls_with_incorrect_target = 0;
long long int load_pairs = 0;
long long int store_pairs = 0;
long long int pre_indexing_base_update_load = 0;
long long int post_indexing_base_update_load = 0;
long long int pre_indexing_base_update_store = 0;
long long int post_indexing_base_update_store = 0;
long long int calls_incorrectly_classified_as_returns_fixed = 0;
long long int branches_now_including_original_trace_registers = 0;
long long int original_trace_registers_added_to_branches = 0;
long long int calls_that_could_not_add_all_original_destination_registers = 0;
long long int predicated_instructions_found = 0;
long long int indirect_calls_removing_reg_ax = 0;
long long int indirect_jumps_removing_reg_ax = 0;
long long int returns_with_added_trace_source_registers = 0;
long long int non_branches_reading_conflicting_x30 = 0;
bool last_x30_writer_was_call;
long long int insts_with_more_than_one_destination_imp = 0;
long long int additional_destination_regs_in_memory_instructions = 0;
long long int memory_instructions_not_adding_fake_destination_reg = 0;
long long int no_pair_crosses_cachelines = 0;
long long int pair_crosses_cachelines = 0;
long long int DCZVA_with_non_aligned_address_fixed = 0;


// one record from the CVP-1 trace file format
struct trace {
  UINT64 PC,  // program counter
      EA,     // effective address
      target; // branch target

  uint8_t access_size,
      taken, // branch was taken
      num_input_regs, num_output_regs, input_reg_names[256], output_reg_names[256];

  // output register values could be up to 128 bits each
  UINT64 output_reg_values[256][2];

  InstClass type; // instruction type

  AddresssingMode addr_mode;
  bool input_reg_base [256], output_reg_base [256];

  // is this a branch / load / store inst?
  bool isBranch () { return (type == uncondIndirectBranchInstClass || type == uncondDirectBranchInstClass || type == condBranchInstClass); }
  bool isLoad () const { return type == loadInstClass; }
  bool isStore () const { return type == storeInstClass; }


  // read a single record from the trace file, return true on success, false on EOF
  bool read(FILE* f) {

    // initialize
    PC = 0;
    EA = 0;
    target = 0;
    access_size = 0;
    taken = 0;
    type = undefInstClass;

    // get the PC
    int n;
    n = fread(&PC, 8, 1, f);
    if (!n)
      return false;
    if (feof(f))
      return false;
    assert(n == 1);

    // get the instruction type
    assert(fread(&type, 1, 1, f) == 1);

    // base on the type, read in different stuff
    switch (type) {
    case loadInstClass:
    case storeInstClass:
      // load or store? get the effective address and access size
      assert(fread(&EA, 8, 1, f) == 1);
      assert(fread(&access_size, 1, 1, f) == 1);
      break;

    case condBranchInstClass:
    case uncondDirectBranchInstClass:
    case uncondIndirectBranchInstClass:
      // branch? get "taken" and the target
      assert(fread(&taken, 1, 1, f) == 1);
      if (taken) {
        assert(fread(&target, 8, 1, f) == 1);
      } else {
        // if not taken, default target is fallthru, i.e. PC+4
        target = PC + 4;

        // this had better not be an unconditional branch (frickin ARM with its predicated branches)
        assert(type != uncondDirectBranchInstClass);
        assert(type != uncondIndirectBranchInstClass);
      }
      break;

    default:;
    }

    // get the number of input registers and their names
    assert(fread(&num_input_regs, 1, 1, f) == 1);
    for (int i = 0; i < num_input_regs; i++) {
      assert(fread(&input_reg_names[i], 1, 1, f) == 1);
    }

    // get the number of output registers and their names
    assert(fread(&num_output_regs, 1, 1, f) == 1);
    for (int i = 0; i < num_output_regs; i++) {
      assert(fread(&output_reg_names[i], 1, 1, f) == 1);
    }

    // read the output registers
    memset(output_reg_values, 0, sizeof(output_reg_values));
    for (int i = 0; i < num_output_regs; i++) {
      if (output_reg_names[i] <= 31) {
        // scalars
        assert(fread(&output_reg_values[i][0], 8, 1, f) == 1);
      } else if (output_reg_names[i] >= 32 && output_reg_names[i] < 64) {
        // SIMD
        assert(fread(&output_reg_values[i][0], 16, 1, f) == 1);
      } else
        assert(0);
    }

    // success!

    if (type != condBranchInstClass && type != uncondDirectBranchInstClass && type != uncondIndirectBranchInstClass) {
      if (last_x30_writer_was_call) {
        for (int i = 0; i < num_input_regs; i++) {
          if (input_reg_names[i] == 30) {
            non_branches_reading_conflicting_x30 ++;
            break;
          }
        }
      }
      for (int i = 0; i < num_output_regs; i++) {
        if (output_reg_names[i] == 30) {
          last_x30_writer_was_call = false;
          break;
        }
      }
    }

    return true;
  }

  void print () {
    std::cerr << "[PC: 0x" << std::hex << PC << std::dec << " Type: " << InstClassToString(type);

    if(type == InstClass::loadInstClass || type == InstClass::storeInstClass) {
      std::cerr << " ea: 0x" << std::hex << EA << std::dec << " size: " << (unsigned) access_size;
    }
    
    else if(type == InstClass::condBranchInstClass || type == InstClass::uncondDirectBranchInstClass || type == InstClass::uncondIndirectBranchInstClass) {
      std::cerr << " ( tkn:" << (unsigned) taken << " tar: 0x" << std::hex << target << ")" << std::dec;
    }
    
    std::cerr << " InRegs : { ";
    for (unsigned i = 0; i < num_input_regs; i++) {
	    std::cerr << (unsigned) input_reg_names[i] << " ";
	  }
    std::cerr << " } OutRegs : { ";
    for(unsigned i = 0; i < num_output_regs; i++) {
      if (output_reg_names[i] <= 31 || output_reg_names[i] >= 64) {
        std::cerr << (unsigned) output_reg_names[i] << " (" << std::hex << output_reg_values[i][0] << std::dec << ") ";
      }
      else { // SIMD
        std::cerr << (unsigned) output_reg_names[i] << " (hi: " << std::hex << output_reg_values[i][1] << std::dec << " lo: " << std::hex << output_reg_values[i][0] << std::dec << ") ";
      }
    }
    std::cerr << "} ]" << std::endl;
  }


  // Adds the flag register as destination to ALU and FP instructions without one
  void Improvement_flag_reg () {
    if((type == aluInstClass || type == fpInstClass) && num_output_regs == 0) {
      num_output_regs = 1;
      output_reg_names[0] = champsim::REG_FLAGS;
      // output_reg_values[0][0] = 0xdeadbeef;
      if (type == aluInstClass) {
        alus_with_no_destination_register ++;
      }
      else if (type == fpInstClass) {
        fps_with_no_destination_register ++;
      }
    }
  }


  // Fixes the branch target for indirect calls with x30 as source and detination register
  void Improvement_branch_target (uint64_t nextPC) {
    if(type == InstClass::uncondIndirectBranchInstClass && num_input_regs == 1 && num_output_regs == 1 && input_reg_names[0] == 30) {
      target = nextPC;
      indirect_calls_with_incorrect_target ++;
    }
  }


  // Create a key for memory operations. Used to identify different memory instructions that have the same PC (these instructions should from different threads/processes?)
  uint64_t get_mem_op_key () {
    uint64_t mask = isLoad();
    mask <<=3;
    mask |= num_input_regs;
    for (int i = 0; i < num_input_regs; i++) {
      mask <<=7;
      mask |= input_reg_names[i];
    }
    mask <<=3;
    mask |= num_output_regs;
    for (int i = 0; i < num_output_regs; i++) {
      mask <<=7;
      mask |= output_reg_names[i];
    }
    
    return (PC ^ mask);
  }

  // The addressing mode is used to determine the memory instructions that update the base register and the ones that are load/store pairs, and to fix the access size.
  // The access size is used by improvement mem-footprint but it is not part of the ChampSim traces.
  void Identify_addressing_mode_and_fix_access_size () {

    // Rearrange input and output regs for vector loads/stores such that the general purpose registers appear first
    if (isLoad() || isStore()) {
      if(num_input_regs > 1) {
        bool resort = false;
        for (int i = 0; i < num_input_regs; i++) {
          if (input_reg_names[i] > 31 && input_reg_names[i] < 64) { // SIMD
            resort = true;
            break;
          }
        }

        if (resort) {
          uint8_t *aux_input_reg_names;
          aux_input_reg_names = new uint8_t[num_input_regs];
          for (int i = 0; i < num_input_regs; i++) {
            aux_input_reg_names[i] = input_reg_names[i];
          }
          int n = 0;
          for (int i = 0; i < num_input_regs; i++) {
            assert(aux_input_reg_names[i] <= 67);
            if (aux_input_reg_names[i] < 32 || aux_input_reg_names[i] > 63) { // Not SIMD -> go firt
              input_reg_names[n] = aux_input_reg_names[i];
              n++;
            }
          }
          for (int i = 0; i < num_input_regs; i++) {
            if (aux_input_reg_names[i] > 31 && aux_input_reg_names[i] < 64) { // SIMD -> go after
              input_reg_names[n] = aux_input_reg_names[i];
              n++;
            }
          }
          assert(n == num_input_regs);
        }
      }

      if(num_output_regs > 1) {
        bool resort = false;
        for (int i = 0; i < num_output_regs; i++) {
          if (output_reg_names[i] > 31 && output_reg_names[i] < 64) { // SIMD
            resort = true;
            break;
          }
        }

        if (resort) {
          uint8_t *aux_output_reg_names;
          aux_output_reg_names = new uint8_t[num_output_regs];
          unsigned long long *aux_output_reg_values_l0 = new unsigned long long [num_output_regs];
          unsigned long long *aux_output_reg_values_hi = new unsigned long long [num_output_regs];

          for (int i = 0; i < num_output_regs; i++) {
            aux_output_reg_names[i] = output_reg_names[i];
            aux_output_reg_values_l0[i] = output_reg_values[i][0];
            aux_output_reg_values_hi[i] = output_reg_values[i][1];
          }
          int n = 0;
          for (int i = 0; i < num_output_regs; i++) {
            assert(aux_output_reg_names[i] <= 67);
            if (aux_output_reg_names[i] < 32 || aux_output_reg_names[i] > 63) { // Not SIMD -> go firt
              output_reg_names[n] = aux_output_reg_names[i];
              output_reg_values[n][0] = aux_output_reg_values_l0[i];
              output_reg_values[n][1] = aux_output_reg_values_hi[i];
              n++;
            }
          }
          for (int i = 0; i < num_output_regs; i++) {
            if (aux_output_reg_names[i] > 31 && aux_output_reg_names[i] < 64) { // SIMD go after
              output_reg_names[n] = aux_output_reg_names[i];
              output_reg_values[n][0] = aux_output_reg_values_l0[i];
              output_reg_values[n][1] = aux_output_reg_values_hi[i];
              n++;
            }
          }
          assert(n == num_output_regs);
        }
      }
    }

    uint64_t mem_op_key = get_mem_op_key ();
    
    if ((!isLoad() && !isStore() && addr_mode_helper.count(PC) != 0) || (addr_mode_helper.count(PC) != 0 && addr_mode_helper[PC].m_mem_op_key != mem_op_key)) {
      // Remove info if the instruction has the same PC but it is actually a different instruction
      addr_mode_helper.erase(PC);
    }

    if((isLoad() || isStore()) && addr_mode_helper.count(PC) == 0) {
      
      if(isLoad()) {

        // Case 1: Single dest reg
        if(num_output_regs == 1) {

          // Case 1.1: Single src reg
          if(num_input_regs == 1) {
            addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, access_size, input_reg_names[0], mem_op_key};
          }

          // Case 1.2: Two src regs
          else if(num_input_regs == 2) { 
            uint8_t outReg = output_reg_names[0];
                        
            uint8_t base_reg_index;
            for (base_reg_index = 0; base_reg_index < num_input_regs; base_reg_index++) {
              if (input_reg_names[base_reg_index] == outReg) {
                break;
              }
            }

            // Case 1.2.1: Source also in destination but single destination, so it cannot be Base Update.
            // The instruction is just overwriting one of the register used to compute the address
            if(base_reg_index != num_input_regs){
              addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key};
            }

            // Case 1.2.2: No source is also destination
            else {
              addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key};
            } 
          }

          // Case 1.2.3: No source register
          // Source 0 is bogus, but we are safe as this is not Base Update and we will not use it
          else if (num_input_regs == 0) {
            addr_mode_helper[PC] = {AddresssingMode::PcRelative, access_size, input_reg_names[0], mem_op_key};
          }
        }

        // Case 2: Two destination registers
        else if(num_output_regs == 2) {
          uint8_t num_integer_input_regs = 0;
          for (int i = 0; i < num_input_regs; i++) {
            if (input_reg_names[i] < 32 || input_reg_names[i] > 63) { // Not SIMD
              num_integer_input_regs ++;
            }
          }

          if (num_input_regs == 1 || num_integer_input_regs == 1) {
          
            uint8_t base_reg_index;
            for (base_reg_index = 0; base_reg_index < num_output_regs; base_reg_index++) {
              if (output_reg_names[base_reg_index] == input_reg_names[0]) {
                break;
              }
            }

            // Case 2.1: Single source register is also destination register
            if (base_reg_index != num_output_regs) {
              // For regular load/store: "Pre/Post indexed by an unscaled 9-bit signed immediate offset" hence -256 to 255 immediate
              auto base_reg_value = output_reg_values[base_reg_index][0];

              // If the EA equals the base_reg_value, it should be a load with base-update pre-increment
              const bool fits_unscaled_baseupdate_immediate = 
                (base_reg_value >= EA && ((base_reg_value - EA) <= 256)) ||  
                (base_reg_value < EA && ((EA - base_reg_value) <= 255));

              // Case 2.1.1: Value of destination register (base register) is within base update immediate
              // This is regular load with BaseUpdate addressing
              // Note that this is not bulletproof e.g. if it is actually ldp reg1, reg2, [reg1] and we have bad luck
              if(fits_unscaled_baseupdate_immediate) {
                addr_mode_helper[PC] = {AddresssingMode::BaseUpdate, access_size, input_reg_names[0], mem_op_key};
              }
            
              // Case 2.1.2: Value of destination register (base register) is not within base update immediate, this is load pair
              else {
                assert(num_input_regs == 1); // If there is an int register with SIMD registers, it should be base update... So make sure that if it is not a base update, there is a single source register
                addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
              }
            }

            // Case 2.2: Single source register not also destination register
            else {
              addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
            }
          }

          else {
            assert(num_input_regs == 2); // Two sources
            // LD1 with the register post index
            int base_reg_index = 0;
            for (base_reg_index = 0; base_reg_index < num_input_regs; base_reg_index++) {
              if (input_reg_names[base_reg_index] == output_reg_names[0]) {
                break;
              }
            }
            assert (base_reg_index < num_input_regs);
            addr_mode_helper[PC] = {AddresssingMode::BaseUpdate, access_size, output_reg_names[0], mem_op_key};
          }
        }

        // Case 3: Three destination registers, load pair, or SIMD load multiple with base update
        else if(num_output_regs >= 3) {
          // We assume that the base register (if any) will be the first destination so make sure that there is a single one (at least, non SIMD)
          uint8_t num_integer_dest_regs = 0;
          for (int i = 0; i < num_output_regs; i++) {
            if (output_reg_names[i] < 32 || output_reg_names[i] > 63) { // Not SIMD
              num_integer_dest_regs ++;
            }
          }
          assert(num_input_regs == 1 || num_integer_dest_regs == 1);
          bool base_update = false;
          for (int i = 0; i < num_input_regs; i++) {
            if (input_reg_names[i] == output_reg_names[0]) {
              base_update = true;
            }
          } 
          if (base_update) {
            addr_mode_helper[PC] = {AddresssingMode::BaseUpdate, (uint8_t) (access_size * (num_output_regs - 1)), output_reg_names[0], mem_op_key, true};
          }
          else {
            // Definitively, not base update
            addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size * num_output_regs), input_reg_names[0], mem_op_key, true};            
          }
        }

        // Case 4: Load with zero destination registers (= prefetch)
        else if(num_output_regs == 0) {
          addr_mode_helper[PC] = {AddresssingMode::Prefetch, access_size, input_reg_names[0], mem_op_key};
        }
      }

      else {
        assert(isStore());

        // Case 1: One source
        if(num_input_regs == 1) {
          // Case 1.1 DCZVA have to align EA to 64B because
          // There are no alignment restrictions on this address and traces were captured with 64B cache line size
          if(access_size == 64) {
            addr_mode_helper[PC] = {AddresssingMode::DCZVA, access_size, input_reg_names[0], mem_op_key, false, ~63lu};
          }

          // Case 1.2 Specific ops
          else {
            addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, access_size, input_reg_names[0], mem_op_key};
          }
        }

        // Case 2: Two sources
        else if (num_input_regs == 2) {
          
          // Case 2.1: No destination registers
          if(num_output_regs == 0) {
            addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, access_size, input_reg_names[0], mem_op_key};
          }

          else {
            assert(num_output_regs == 1);

            uint8_t base_reg_index;
            for (base_reg_index = 0; base_reg_index < num_input_regs; base_reg_index++) {
              if (input_reg_names[base_reg_index] == output_reg_names[0]) {
                break;
              }
            }
          
            // Case 2.2: One destination register, differs from two sources (strex?)
            if(num_output_regs == 1 && base_reg_index == num_input_regs) { // !has_base_update
              addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, access_size, input_reg_names[0], mem_op_key};
            }

            // Case 2.3: One destination register, same as one of the sources
            else if(base_reg_index != num_input_regs) { // has_base_update              
            assert(base_reg_index == 0); // As we resort the int / SIMD registers so that the integer ones come first, the base register should always be at position 0
              addr_mode_helper[PC] = {AddresssingMode::BaseUpdate, access_size, input_reg_names[base_reg_index], mem_op_key};
            }

            else { assert(false); }
          }
        }

        // Case 3: Three sources or more (SIMD store multiple)
        else if(num_input_regs >= 3) {
          
          // Case 3.2: No outputs
          if(num_output_regs == 0) {
            // Make sure that the first register is not SIMD as we are going to assume it might be the base register
            assert(input_reg_names[0] <= 31 || input_reg_names[0] >= 64); //Not SIMD

            auto base_reg_value = registers[input_reg_names[0]][0]; 

            // Can't really make a decision here, wait until next time
            if(base_reg_value == 0x0) {
              addr_mode_helper[PC] = {AddresssingMode::NoMode, access_size, input_reg_names[0], mem_op_key};
            }

            else {
              // For store pair: 
              // "Pre-indexed by a scaled 7-bit signed immediate offset."
              // "Post-indexed by a scaled 7-bit signed immediate offset."
              const int scaling_factor = access_size;
              const bool fits_stp_immediate_offset = 
                ((EA >= base_reg_value) && ((EA - base_reg_value) <= ((uint64_t) (63 * scaling_factor)))) ||
                ((EA < base_reg_value) && ((base_reg_value - EA) <= ((uint64_t) (64 * scaling_factor))));

              // Case 3.2.1: store pair BaseImmediateOffset if value of base register is within immediate offset of EA
              // Note that this is not bulletproof
              if(fits_stp_immediate_offset) {
                addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
              }

              // Case 3.2.2: regular store using two registers for address
              else {
                addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key};
              }
            }
          }

          else { // num_output_regs != 0
            // We should only have a single destination register
          
            // Case 3.1: >=3 sources, 1 destination
            if (num_output_regs == 1) {

              if (output_reg_values[0][0] == 0 || output_reg_values[0][0] == 1) {
                // Case 3.1.1: Output value is 0/1 then it is (most probably) STXP (Store Exclusive Pair writes 0/1 to indicate sucess or no write performed)
                // The addressing mode is BaseRegImmediate, but because it is a pair, the data transfer size is 2x.
                addr_mode_helper[PC] = {AddresssingMode::BaseRegImmediate, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
              }

              else {
                // Case 3.1.2: Output value is not 0/1 then it is Store Pair with Base update
                addr_mode_helper[PC] = {AddresssingMode::BaseUpdate, (uint8_t) (access_size  * (num_input_regs - 1)), output_reg_names[0], mem_op_key, true};
              }
            }

            // Case 3.3: >=3 sources, 2 destinations
            else {
              // Make sure source registers are not SIMD... otherwise it could be ST4?
              for (int i = 0; i < num_input_regs; i++) {
                if (input_reg_names[i] > 31 && input_reg_names[i] < 64) {
                  assert(false);
                }
              }

              // Looks like Compare and Swap Pair, which reads into two consecutive regs, compares with two consecutive regs, and stores two consecutive regs. 
              // That's pretty specific... and not base update
              addr_mode_helper[PC] = {AddresssingMode::Other, (uint8_t) (access_size * num_input_regs), output_reg_names[0], mem_op_key, true};
            }
          }
        }
      }
    }
               
    if(isLoad() || isStore()) {
      auto info = addr_mode_helper.find(PC);
      assert(info != addr_mode_helper.end());

      // Attempt to fix non categorized stores with three sources (Case 3.2)
      if(info->second.m_mode == AddresssingMode::NoMode) {

        assert(input_reg_names[0] <= 31 || input_reg_names[0] >= 64); // No SIMD register
        auto base_reg_value = registers[input_reg_names[0]][0];
        const bool ea_valid = info->second.m_last_ea != 0x0;
        const bool same_ea = info->second.m_last_ea == EA;

        
        if(ea_valid && same_ea){

          if (base_reg_value != info->second.m_last_base_reg_value) {
            // If sameE EA and base register changed, that's strange... We should not come here.
            print();
            assert(false);
          }
          
          // Base register did not change since last time, and address is the same, this is three source store with single base register, hence store pair
          if (base_reg_value == 0x0) {
            addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
          }

          else {
            // This cannot happen because if base reg value is not 0, we could have categorized it
            assert(false);
          }
        }

        
        else if(ea_valid && !same_ea) {

          if (base_reg_value != info->second.m_last_base_reg_value) {
            // If EA changed and the base register changed, then this is likely BaseImm STP...
            // But if base + potential offset (register index 1) gives EA, it's BaseRegOffset STR where both base and offset reg changed.

            if (registers[input_reg_names[0]][0] + registers[input_reg_names[1]][0] == EA) {
              // In some (most?) cases it looks like base + potetial offset give EA as an overflow accident
              // Anyway it does not affect the accuracy of our conversion as ChampSim traces to not include access size and, in any case, this instruction is not a base update
              addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key, true};              
            }

            else {
              addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};              
            }
          }

          else {
            // Base reg did not change but EA changed, so it's using a second register for address
            if (base_reg_value == 0x0) {
              addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key};
            }

            // Base register value did change, which caused EA to change since last time
            // We have to redo the check for case 3.2.1 vs 3.2.2
            else {

              // Actually, we never came here...
              assert(false);
            
              // For store pair :
              // "Pre-indexed by a scaled 7-bit signed immediate offset."
              // "Post-indexed by a scaled 7-bit signed immediate offset."
              const int scaling_factor = access_size;
              const bool fits_stp_immediate_offset =
                ((EA >= base_reg_value) && ((EA - base_reg_value) <= ((uint64_t) (63 * scaling_factor)))) ||
                ((EA < base_reg_value) && ((base_reg_value - EA) <= ((uint64_t) (64 * scaling_factor))));

              // Case 3.2.1: store pair BaseImmediateOffset if value of base register is within immediate offset of EA
              // Note that this is not bulletproof
              if(fits_stp_immediate_offset) {
                addr_mode_helper[PC] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (access_size << 1), input_reg_names[0], mem_op_key, true};
              }

              // Case 3.2.2: regular store using two registers for address
              else {
                addr_mode_helper[PC] = {AddresssingMode::BaseRegOffset, access_size, input_reg_names[0], mem_op_key};
              }
            }
          }
        }

        else {
          // ! ea_valid. We will try to classify this instruction next time
        }
      }

      access_size = info->second.m_corrected_access_size;
      addr_mode = info->second.m_mode;

      info->second.m_last_ea = EA;
      info->second.m_last_base_reg_value = registers[input_reg_names[0]][0];

      // Double check we have identified an integer register as base update
      assert(addr_mode != AddresssingMode::BaseUpdate || (info->second.m_base_reg < 32 || info->second.m_base_reg >= 64)); // Not SIMD

      // Marking destinstion register as base register is only useful to control its latency in a timing simulator 
      // (base reg is available after ALU cycle, not after load to use cycle)
      // So, only mark it if it is an actual BaseUpdate and not just BaseReg or BaseImm that happens to load data in the base register
      if(addr_mode == AddresssingMode::BaseUpdate) {
        for(uint32_t reg = 0; reg < num_output_regs; reg++) {
          if(output_reg_names[reg] == info->second.m_base_reg) {
            output_reg_base[reg] = true;
          }
        }
      }

      // We could not classify the instruction this time as the candidate base register was uninitialized. Try next time.
      if (addr_mode == AddresssingMode::NoMode && !populated_registers[input_reg_names[0]]) {
        addr_mode_helper.erase(PC);
      }

      if (info->second.m_load_store_pair) {  
        if (isLoad()) {
          load_pairs ++;
        }
        else { // store
          store_pairs ++;
        }
      }
    }
  }

  bool Improvement_mem_footprint_check_crosses_cachelines (uint64_t *next_cacheline_address) {
    assert(isLoad() || isStore());
    uint64_t cacheline_access_ini, cacheline_access_end;

    // Check if the memory access crosses cachelines and if so add the second cacheline address to the converted instruction  
    cacheline_access_ini = EA & ~63lu;  // 0xffffffffffffffc0
    cacheline_access_end = (EA + access_size - 1) & ~63lu;

    if (cacheline_access_ini != cacheline_access_end) {
      if (addr_mode_helper.count(PC) != 0 && addr_mode_helper[PC].m_load_store_pair) {
        pair_crosses_cachelines ++;
      }
      else {
        no_pair_crosses_cachelines ++;
      }
      *next_cacheline_address = cacheline_access_end;
      return true;
    }
    else {
      return false;
    }
  }

  void Improvement_mem_footprint_align_DCZVA_address () {
    assert(isLoad() || isStore());
    auto info = addr_mode_helper.find(PC);
    if (info == addr_mode_helper.end()) {
      return; // We will try again next time
    }
       
    // To align address for DCZVA
    uint64_t old_EA = EA;
    EA &= info->second.m_mask;
    if (old_EA != EA) {
      DCZVA_with_non_aligned_address_fixed++;
    }
    info->second.m_last_ea = EA;
  }


  // While CVP-1 encodes Aarch64 instructions, ChampSim follows x86 semantincs for branches. In addition, the CVP-1 traces do not include special purpose registers.
  // Thus, we translate registers 26 (IP), 6 (SP), and 25 (flags) from the CVP-1 traces to registers 64, 65, and 66, respectively, leaving registers 26, 6, and 25 free to convey the branch infomration to ChampSim.
  // Fuhtermore, register 0 is interpreted by ChampSim as "no register". Thus, we translate register 0 from the CVP-1 trace instructions into register 67. 

  void Translate_registers () {
    for (int i = 0; i < num_output_regs; i++) {
      switch (output_reg_names[i]) {
        case champsim::REG_INSTRUCTION_POINTER:
          output_reg_names[i] = TRANSLATED_REG_INSTRUCTION_POINTER;
          break;
        case champsim::REG_STACK_POINTER:
          output_reg_names[i] = TRANSLATED_REG_STACK_POINTER;
          break;
        case champsim::REG_FLAGS:
          output_reg_names[i] = TRANSLATED_REG_FLAGS;
          break;
        case 0:
          output_reg_names[i] = TRANSLATED_REG_ZERO;
          break;
        default:
          break;
      }
    }

    for (int i = 0; i < num_input_regs; i++) {
      switch (input_reg_names[i]) {
        case champsim::REG_INSTRUCTION_POINTER:
          input_reg_names[i] = TRANSLATED_REG_INSTRUCTION_POINTER;
          break;
        case champsim::REG_STACK_POINTER:
          input_reg_names[i] = TRANSLATED_REG_STACK_POINTER;
          break;
        case champsim::REG_FLAGS:
          input_reg_names[i] = TRANSLATED_REG_FLAGS;
          break;
        case 0:
          input_reg_names[i] = TRANSLATED_REG_ZERO;
          break;
        default:
          break;
      }
    }
  }
};


std::map<UINT64, bool> code_pages, data_pages;
std::map<UINT64, UINT64> remapped_pages;
UINT64 bump_page = 0x1000;

// this string will contain the trace file name, or "-" if we want to read from standard input
char tracefilename[1000];

namespace { constexpr char REG_AX = 56; }

auto open_trace_file(void) {
  // read from standard input?
  if (!strcmp(tracefilename, "-")) {
    fprintf(stderr, "reading from standard input\n");
    fflush(stderr);
    return stdin;
  }

  // see what kind of file this is by reading the magic number
  auto magic_tester = fopen(tracefilename, "r");
  if (!magic_tester) {
    perror(tracefilename);
    exit(1);
  }

  // read six bytes from the beginning of the file
  unsigned char s[6];
  int n = fread(s, 1, 6, magic_tester);
  fclose(magic_tester);
  assert(n == 6);

  constexpr auto cmd_size = 5 + std::max({sizeof(XZ_PATH), sizeof(GZIP_PATH), sizeof(CAT_PATH)}) + sizeof(tracefilename);
  char cmd[cmd_size];

  // is this the magic number for XZ compression?
  if (s[0] == 0xfd && s[1] == '7' && s[2] == 'z' && s[3] == 'X' && s[4] == 'Z' && s[5] == 0) {

    // it is an XZ file or doing a good impression of one
    fprintf(stderr, "opening xz file \"%s\"\n", tracefilename);
    fflush(stderr);

    // start up an xz decompression and open a pipe to our standard input
    sprintf(cmd, "%s -dc %s", XZ_PATH, tracefilename);
  }

  // check for the magic number for GZIP compression
  else if (s[0] == 0x1f && s[1] == 0x8b) {

    // it is a GZ file
    fprintf(stderr, "opening gz file \"%s\"\n", tracefilename);
    fflush(stderr);

    // open a pipe to a gzip decompression process
    sprintf(cmd, "%s -dc %s", GZIP_PATH, tracefilename);
  } 
  
  else {
    // no magic number? maybe it's uncompressed?
    fprintf(stderr, "opening file \"%s\"\n", tracefilename);
    fflush(stderr);

    // use Unix cat to open and read this file. we could just fopen the file
    // but then we're pclosing it later so that could get weird
    sprintf(cmd, "%s %s", CAT_PATH, tracefilename);
  }

  auto f = popen(cmd, "r");
  if (f)
    return f;

  perror(cmd);
  return f;
}

void preprocess_file(void) {
  trace t;
  fprintf(stderr, "preprocessing to find code and data pages...\n");
  fflush(stderr);
  FILE* f = open_trace_file();
  if (!f)
    return;
  int count = 0;
  for (;;) {
    bool good = t.read(f);
    if (!good)
      break;
    code_pages[t.PC >> 12] = true;
    if (t.type == loadInstClass || t.type == storeInstClass)
      data_pages[t.EA >> 12] = true;
    count++;
    if (count % 10000000 == 0) {
      fprintf(stderr, ".");
      fflush(stderr);
      if (count % 600000000 == 0) {
        fprintf(stderr, "\n");
        fflush(stderr);
      }
    }
  }
  pclose(f);
  fprintf(stderr, "%ld code pages, %ld data pages\n", code_pages.size(), data_pages.size());
  fflush(stderr);
}

// take an address representing data and make sure it doesn't overlap with code
UINT64 transform(UINT64 a) {
  static int num_allocs = 0;
  UINT64 page = a >> 12;
  UINT64 new_page = page;
  if (code_pages.find(page) != code_pages.end()) {
    new_page = remapped_pages[page];
    if (new_page == 0) {
      num_allocs++;
      fprintf(stderr, "[%d]", num_allocs);
      fflush(stderr);
      // allocate a new page
      new_page = bump_page;
      for (;;) {
        if (code_pages.find(new_page) != code_pages.end() || data_pages.find(new_page) != data_pages.end())
          new_page++;
        else
          break;
      }
      bump_page = new_page + 1;
      remapped_pages[page] = new_page;
    }
  }
  a = new_page << 12 | (a & 0xfff);
  return a;
}


void print_usage() {
  std::cerr << "Usage: cvp2champsim [-t trace_file] [-v] [-i imp_list]" << std::endl;
  std::cerr << "       imp_list: a comma-separated list of improvements" << std::endl;
  std::cerr << "Available improvements: imp_mem-regs imp_base-update imp_mem-footprint imp_call-stack imp_flag-reg imp_branch-regs" << std::endl;
  std::cerr << "Alternatively, we can select All_imps Memory_imps Branch_imps No_imp to enable all improvements, all memory improvements, or all branch improvements, respectively." << std::endl;
  std::cerr << "Default: All improvements" << std::endl;
} 


int main(int argc, char** argv) {

  // defaults to reading from standard input
  strcpy(tracefilename, "-");

  // defaults to all improvements enabled
  APPLY_IMP_MEM_REGS = true;
  APPLY_IMP_BASE_UPDATE = true;
  APPLY_IMP_MEM_FOOTPRINT = true;
  APPLY_IMP_CALL_STACK = true;
  APPLY_IMP_FLAG_REG = true;
  APPLY_IMP_BRANCH_REGS = true;

  for (int i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-t") == 0) { // tracefile options
      i++;
      if (i == argc) {
        print_usage();
        return 1;
      }
      else {
        strcpy(tracefilename, argv[i]);
      }
    }
  
    else if (strcmp(argv[i], "-v") == 0) {
      verbose = true;      
    }

    else if (strcmp(argv[i], "-i") == 0) {
      i++;
      
      APPLY_IMP_MEM_REGS = false;
      APPLY_IMP_BASE_UPDATE = false;
      APPLY_IMP_MEM_FOOTPRINT = false;
      APPLY_IMP_CALL_STACK = false;
      APPLY_IMP_FLAG_REG = false;
      APPLY_IMP_BRANCH_REGS = false;

      std::stringstream input_options (argv[i]);
      while (input_options.good()) {
        std::string option;
        getline(input_options, option, ',');

        if (option.compare("No_imp") == 0) {
          std::cerr << "NO improvement enabled!" << std::endl;
        }
        else if (option.compare("All_imps") == 0) {
          APPLY_IMP_MEM_REGS = true;
          APPLY_IMP_BASE_UPDATE = true;
          APPLY_IMP_MEM_FOOTPRINT = true;
          APPLY_IMP_CALL_STACK = true;
          APPLY_IMP_FLAG_REG = true;
          APPLY_IMP_BRANCH_REGS = true;
          std::cerr << "ALL improvements enabled!" << std::endl;
        }
        else if (option.compare("Memory_imps") == 0) {
          APPLY_IMP_MEM_REGS = true;
          APPLY_IMP_BASE_UPDATE = true;
          APPLY_IMP_MEM_FOOTPRINT = true;
          std::cerr << "MEMORY improvements enabled!" << std::endl;
        }
        else if (option.compare("All_but_mem-footprint") == 0) {
          APPLY_IMP_MEM_REGS = true;
          APPLY_IMP_BASE_UPDATE = true;
          APPLY_IMP_CALL_STACK = true;
          APPLY_IMP_FLAG_REG = true;
          APPLY_IMP_BRANCH_REGS = true;
          std::cerr << "All but imp_mem-footprint improvements enabled!" << std::endl;
        } 
        else if (option.compare("Branch_imps") == 0) {
          APPLY_IMP_CALL_STACK = true;
          APPLY_IMP_FLAG_REG = true;
          APPLY_IMP_BRANCH_REGS = true;
          std::cerr << "BRANCH improvements enabled!" << std::endl;
        }
        else if (option.compare("imp_mem-regs") == 0) {
          APPLY_IMP_MEM_REGS = true;
          std::cerr << "Improvement MEM-REGS enabled!" << std::endl;
        }
        else if (option.compare("imp_base-update") == 0) {
          APPLY_IMP_BASE_UPDATE = true;
          std::cerr << "Improvement BASE-UPDATE enabled!" << std::endl;
        }
        else if (option.compare("imp_mem-footprint") == 0) {
          APPLY_IMP_MEM_FOOTPRINT = true;
          std::cerr << "Improvement MEM-FOOTPRINT enabled!" << std::endl;
        }
        else if (option.compare("imp_call-stack") == 0) {
          APPLY_IMP_CALL_STACK = true;
          std::cerr << "Improvement CALL-STACK enabled!" << std::endl;
        }
        else if (option.compare("imp_flag-reg") == 0) {
          APPLY_IMP_FLAG_REG = true;
          std::cerr << "Improvement FLAG-REG enabled!" << std::endl;
        }
        else if (option.compare("imp_branch-regs") == 0) {
          APPLY_IMP_BRANCH_REGS = true;
          std::cerr << "Improvement BRANCH-REGS enabled!" << std::endl;
        } 
        else {
          std::cerr << "Unknow improvement " << option << std::endl;
          print_usage();
          return 1;
        }
      }
    }

    else {
      print_usage();
      return 1;
    }
  }  

  for (int i = 0; i < 256; i++) {
    populated_registers[i] = false;
  }

  trace t;

  memset(registers, 0, sizeof(registers));

  preprocess_file();

  // open the trace file
  FILE* f = open_trace_file();

  if (!f) {
    return 1;
  }

  // number of records read so far
  long long int n = 1;
  trace nextt;

  bool artificially_added_output;

  // Read the first instruction
  bool good = t.read(f);

  if (!good) {
    fprintf(stderr, "Failed reading the first instruction. Is the trace file correct?");
    return 1;
  }

  // loop getting records until we're done
  for (;;) {

    artificially_added_output = false;

    // one more record
    n++;

    // print something to entertain the user while they wait
    if (n % 1000000 == 0) {
      fprintf(stderr, "%lld instructions\n", n);
      fflush(stderr);
    }

    // read a record from the trace file
    good = nextt.read(f);

    if (good && nextt.PC == t.PC) {
      fprintf(stderr, "hmm, that's weird\n");
    }

    // if (n == 100000) {
    //   break;
    // }

    // here we can print the CVP-1 instructio before we make any change
    // t.print();
    

    t.Translate_registers();

    if (APPLY_IMP_FLAG_REG) {
      t.Improvement_flag_reg();
    }

    if (APPLY_IMP_BRANCH_TARGET) {
      t.Improvement_branch_target (nextt.PC);
    }

    t.Identify_addressing_mode_and_fix_access_size ();


    trace_instr_format ct;
    ct.ip = t.PC;
    ct.is_branch = false;

    // we are going to figure out the op type
    OpType c = OPTYPE_OP;

    // if this is a branch then do more stuff
    if (t.isBranch()) {
      ct.is_branch = true;

      // t.print();

      // if this is a conditional branch then it's direct and we're done figuring out the type
      if (t.type == condBranchInstClass) {
        c = OPTYPE_JMP_DIRECT_COND;
      } 

      else {
        // this is some other kind of branch. it should have a non-zero target
        assert(t.target);

        if (APPLY_IMP_CALL_STACK) {

          if (t.num_input_regs == 1 && t.input_reg_names[0] == 30) {
            calls_incorrectly_classified_as_returns_fixed ++;
          }
          
          // On ARM, returns are an indirect jump to X30. They do not write to any general-purpose register
          if (t.num_output_regs == 0 && t.num_input_regs == 1 && t.input_reg_names[0] == 30) {

            // yes. it's a return.
            c = OPTYPE_RET_UNCOND;
            calls_incorrectly_classified_as_returns_fixed --;
          }

          // On ARM, calls link the return address in register X30
          else if (t.num_output_regs == 1 && t.output_reg_names[0] == 30) {

            // is it indirect?
            if (t.type == uncondIndirectBranchInstClass) {
              c = OPTYPE_CALL_INDIRECT_UNCOND;
            }
            else {
              c = OPTYPE_CALL_DIRECT_UNCOND;
            }

            last_x30_writer_was_call = true;
          } 
          
          // no X30? then it's just an unconditional jump
          else {
            
            // is it indirect?
            if (t.type == uncondIndirectBranchInstClass) {
              c = OPTYPE_JMP_INDIRECT_UNCOND;
            }
            else {
              c = OPTYPE_JMP_DIRECT_UNCOND;
            }
          }
        }

        else { // ! APPLY_IMP_CALL_STACK

          // This is the identification of branches in the original cvp2champsim. 
          // We keep it to allow enabling the different improvements individually.

          // on ARM, calls link the return address in register X30. let's see if this
          // instruction is doing that; if so, it's a call or wants us to believe it is
          if (t.num_output_regs == 1 && t.output_reg_names[0] == 30) {

            // is it indirect?
            if (t.type == uncondIndirectBranchInstClass)
              c = OPTYPE_CALL_INDIRECT_UNCOND;
            else
              c = OPTYPE_CALL_DIRECT_UNCOND;
          } 
          
          else {
            // no X30? then it's just an unconditional jump
            // is it indirect?
            if (t.type == uncondIndirectBranchInstClass)
              c = OPTYPE_JMP_INDIRECT_UNCOND;
            else
              c = OPTYPE_JMP_DIRECT_UNCOND;
          }

          // on ARM, returns are an indirect jump to X30. let's see if we're doing this
          if (t.num_input_regs == 1)
            if (t.input_reg_names[0] == 30) {
              // yes. it's a return.
              c = OPTYPE_RET_UNCOND;
            }
        }
      }

      counts[c]++;

      // OK now make a branch instruction out of this bad boy

      int reg_sources = 0;
      int reg_dests = 0;

      memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
      memset(ct.source_registers, 0, sizeof(ct.source_registers));
      memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
      memset(ct.source_memory, 0, sizeof(ct.source_memory));
      
      switch (c) {
      case OPTYPE_JMP_DIRECT_UNCOND:
        ct.branch_taken = t.taken;
        
        // Writes IP only
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        reg_dests = 1;
        
        assert(t.num_input_regs == 0);  // Does not have any source register
        assert(t.num_output_regs == 0); // nor any destination register in the CVP-1 trace
        break;

      case OPTYPE_JMP_DIRECT_COND:
        ct.branch_taken = t.taken;
        
        // reads FLAGS or other, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        reg_dests = 1;
        // turns out pin records conditional direct branches as also reading IP. whatever.
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        
        if (!APPLY_IMP_BRANCH_REGS) {
          ct.source_registers[1] = champsim::REG_FLAGS;
          reg_sources = 2;
          cond_branches_depending_on_flag_register ++;
        }
        else { // APPLY_IMP_BRANCH_REGS
          if (t.num_input_regs == 1) {            
            // Most likely those are cb(n)z and tb(n)z which jump based on the content of a general-purpose register.
            // We will add more sources later but should not add flags.
            ct.source_registers[1] = t.input_reg_names[0];
            reg_sources = 2;

            branches_now_including_original_trace_registers ++;
            original_trace_registers_added_to_branches ++;
            cond_branches_not_depending_on_flag_register ++;
          }
          else if (t.num_input_regs == 2) {
            // Predicated insts (csel, csneg, etc.). These should add flags.
            assert(false); // Actually, we never came here.
            ct.source_registers[1] = champsim::REG_FLAGS;
            ct.source_registers[2] = t.input_reg_names[0];
            ct.source_registers[3] = t.input_reg_names[1];            
            reg_sources = 4;

            branches_now_including_original_trace_registers ++;
            original_trace_registers_added_to_branches += 2;
            predicated_instructions_found ++;
            cond_branches_depending_on_flag_register ++;            
          }
          else {             
            assert(t.num_input_regs == 0);
            // Conditional branches without a source register in the CVP-1 trace should read from the flag register
            ct.source_registers[1] = champsim::REG_FLAGS;
            reg_sources = 2;

            cond_branches_depending_on_flag_register ++;
          }
        }
        assert(t.num_output_regs == 0); // Does not have any destination register in the CVP-1 trace
        break;

      case OPTYPE_CALL_INDIRECT_UNCOND:
        ct.branch_taken = true;

        // reads something else, reads IP, reads SP, writes SP, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        reg_dests = 2;
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.source_registers[1] = champsim::REG_STACK_POINTER;
        
        if (!APPLY_IMP_BRANCH_REGS) {
          ct.source_registers[2] = ::REG_AX; // Without improvement branch-regs, add register AX to "read something else"
          reg_sources = 3;
        }
        else { // APPLY_IMP_BRANCH_REGS
          assert(t.num_input_regs == 1);   
          ct.source_registers[2] = t.input_reg_names[0];
          reg_sources = 3;
          
          branches_now_including_original_trace_registers ++;
          original_trace_registers_added_to_branches ++;
          indirect_calls_removing_reg_ax ++;
        }
        
        // This is a known limitation described in the paper. 
        // We cannot add a third destination register without modifying the maximum number of destination registers in ChampSim
        assert(t.num_output_regs == 1 && t.output_reg_names[0] == 30);
        calls_that_could_not_add_all_original_destination_registers ++; 
        break;

      case OPTYPE_CALL_DIRECT_UNCOND:
        ct.branch_taken = true;

        // reads IP, reads SP, writes SP, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        reg_dests = 2;
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.source_registers[1] = champsim::REG_STACK_POINTER;
        reg_sources = 2;

        assert (t.num_input_regs == 0); // It does not have any source register in the CVP-1 trace
        // This is a known limitation described in the paper. 
        // We cannot add a third destination register without modifying the maximum number of destination registers in ChampSim
        assert(t.num_output_regs == 1 && t.output_reg_names[0] == 30);
        calls_that_could_not_add_all_original_destination_registers ++;
        break;

      case OPTYPE_JMP_INDIRECT_UNCOND:
        ct.branch_taken = true;

        // reads something else, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        reg_dests = 1;
        
        if (!APPLY_IMP_BRANCH_REGS) {
          ct.source_registers[0] = ::REG_AX; // Without improvement branch-regs, add register AX to "read something else"
          reg_sources = 1;
        }
        else { // APPLY_IMP_BRANCH_REGS
          assert(t.num_input_regs == 1);  
          ct.source_registers[0] = t.input_reg_names[0];
          reg_sources = 1;

          branches_now_including_original_trace_registers ++;
          original_trace_registers_added_to_branches ++;
          indirect_jumps_removing_reg_ax ++;
        }
        assert(t.num_output_regs == 0); // It does not have any destination register in the CVP-1 trace
        break;

      case OPTYPE_RET_UNCOND:
        ct.branch_taken = true;

        // reads SP, writes SP, writes IP
        ct.source_registers[0] = champsim::REG_STACK_POINTER;
        reg_sources = 1;
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        reg_dests = 2;
        
        if (APPLY_IMP_BRANCH_REGS) {
          assert(t.num_input_regs == 1);  
          ct.source_registers[1] = t.input_reg_names[0];
          reg_sources = 2;

          branches_now_including_original_trace_registers ++;
          original_trace_registers_added_to_branches ++;
          returns_with_added_trace_source_registers ++;
        }

        if (APPLY_IMP_CALL_STACK) {
          assert(t.num_output_regs == 0); // It does not have any destination register in the CVP-1 trace
        }
        else {
          // but if we identify returns incorrectly, it might have one... which we cannot add.
          // This is a known limitation described in the paper.
          calls_that_could_not_add_all_original_destination_registers ++;  // it is a return but anyway it should have been a call...
        }
        break;

      default:
        assert(0);
      }

      // print(ct);
      fwrite(&ct, sizeof(ct), 1, stdout); // write a branch trace
    }

    else { // Non-branches

      if (APPLY_IMP_BASE_UPDATE && addr_mode_helper.count(t.PC) != 0 && addr_mode_helper[t.PC].m_mode == BaseUpdate) {

        uint32_t base_reg = addr_mode_helper[t.PC].m_base_reg;

        int8_t base_reg_index;
        for (base_reg_index = 0; base_reg_index < t.num_output_regs; base_reg_index++) {
          if (t.output_reg_names[base_reg_index] == base_reg) {
            break;
          }
        }
        
        assert(base_reg_index < t.num_output_regs);

        if (t.output_reg_values[base_reg_index][0] == t.EA) {
                
          // Pre-indexing base update increment
          memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
          memset(ct.source_registers, 0, sizeof(ct.source_registers));
          memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
          memset(ct.source_memory, 0, sizeof(ct.source_memory));
          
          assert(base_reg != 0);
          ct.destination_registers[0] = base_reg;
          ct.source_registers[0] = base_reg;
          // print(ct);
          fwrite(&ct, sizeof(ct), 1, stdout); // write the pre-increment alu
          ct.ip += 2;
          
          if (t.isLoad()) {
            pre_indexing_base_update_load ++;
          }
          else {
            pre_indexing_base_update_store ++;
          }
        }
      }

      memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
      memset(ct.source_registers, 0, sizeof(ct.source_registers));
      memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
      memset(ct.source_memory, 0, sizeof(ct.source_memory));
      counts[OPTYPE_OP]++;
      if (t.num_input_regs > NUM_INSTR_SOURCES) {
        t.num_input_regs = NUM_INSTR_SOURCES;
        // assert(false);  // At least, some SIMD stores with five source registers... Mentioned it as limitation
      }
      
      assert(t.num_output_regs <= 1 || (t.type == loadInstClass || t.type == storeInstClass));
      
      // if (t.num_output_regs == 0 && (t.type == loadInstClass || t.type == storeInstClass)) {
      if (t.num_output_regs == 0) {
        if (!APPLY_IMP_MEM_REGS) {
          // The original cvp2champsim converter forced all instructions to have at least a single destination register, adding register X0 when needed.
          t.num_output_regs = 1;
          t.output_reg_names[0] = TRANSLATED_REG_ZERO;
          artificially_added_output = true;
        }
        else {
          if (t.type == loadInstClass || t.type == storeInstClass) {
            memory_instructions_not_adding_fake_destination_reg ++;
          }
          else {
            assert(!APPLY_IMP_FLAG_REG); // We can only have non-memory instructions with no destination register if the flag-reg improvements is not enabled
          }
        }
      }

      int n_out_regs = t.num_output_regs;
      int b = 0;  
      if (t.num_output_regs > 1 && (t.type == loadInstClass || t.type == storeInstClass)) {
        // The original cvp2champsim converter only conveyed the first destination register from the CVP trace instruction to the
        if (!APPLY_IMP_MEM_REGS){
          n_out_regs = 1;
        } 
        
        if (n_out_regs > 1) {
          insts_with_more_than_one_destination_imp ++;
          additional_destination_regs_in_memory_instructions += t.num_output_regs -1;
        }
      }
      
      for (int a=0; a<n_out_regs; a++) {          
        int x = t.output_reg_names[a];

        if (APPLY_IMP_BASE_UPDATE && addr_mode_helper.count(t.PC) != 0 && addr_mode_helper[t.PC].m_mode == BaseUpdate && x == addr_mode_helper[t.PC].m_base_reg) {
          // If we are aplying the base-update improvement, we don't want to add the base register as a destination of the memory instruction
          if (APPLY_IMP_MEM_REGS) {
            continue;
          }
          else {              
            if (t.num_output_regs > 1) {
              // If there are more output registers, we will add the next one
              x = t.output_reg_names[1];
            }
            else {
              // Otherwise, we will add register zero
              x = TRANSLATED_REG_ZERO;
            }
          }
        }          

        assert(x != 0);
        ct.destination_registers[b] = x;
        b++;
      }
      

      for (int i = 0; i < t.num_input_regs; i++) {
        int x = t.input_reg_names[i];          
        assert(x != 0);
        ct.source_registers[i] = x;
      }
    
     
      bool exapnds_two_cachelines = false;
      uint64_t next_cacheline_address;
      if (APPLY_IMP_MEM_FOOTPRINT && (t.isLoad() || t.isStore())) {

        // Check if this memory access crosses cachelines and get the address of the second cacheline
        exapnds_two_cachelines = t.Improvement_mem_footprint_check_crosses_cachelines(&next_cacheline_address);

        // Align effectiva address of DCZVA stores to a cacheline boundary
        t.Improvement_mem_footprint_align_DCZVA_address();

      }

      switch (t.type) {
        case loadInstClass:
          ct.source_memory[0] = transform(t.EA);
          if (exapnds_two_cachelines) {
            ct.source_memory[1] = transform(next_cacheline_address);
          }
          break;
        case storeInstClass:
          ct.destination_memory[0] = transform(t.EA);
          if (exapnds_two_cachelines) {
            ct.destination_memory[1] = transform(next_cacheline_address);
          }
          break;
        case aluInstClass:
        case fpInstClass:
        case slowAluInstClass:
          break;
        case uncondDirectBranchInstClass:
        case condBranchInstClass:
        case uncondIndirectBranchInstClass:
        case undefInstClass:
          assert(0);
      }
      // print(ct);
      fwrite(&ct, sizeof(ct), 1, stdout); // write a non-branch trace    

      if (APPLY_IMP_BASE_UPDATE && addr_mode_helper.count(t.PC) != 0 && addr_mode_helper[t.PC].m_mode == BaseUpdate) {
        uint32_t base_reg = addr_mode_helper[t.PC].m_base_reg;

        int8_t base_reg_index;
        for (base_reg_index = 0; base_reg_index < t.num_output_regs; base_reg_index++) {
          if (t.output_reg_names[base_reg_index] == base_reg) {
            break;
          }
        }
        
        assert(base_reg_index < t.num_output_regs);

        if (t.output_reg_values[base_reg_index][0] != t.EA) {
                
          // Post-indexing base update increment
          memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
          memset(ct.source_registers, 0, sizeof(ct.source_registers));
          memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
          memset(ct.source_memory, 0, sizeof(ct.source_memory));
          
          assert(base_reg != 0);
          ct.destination_registers[0] = base_reg;
          ct.source_registers[0] = base_reg;
          ct.ip += 2;
          // print(ct);
          fwrite(&ct, sizeof(ct), 1, stdout); // write the pre-increment alu
          if (t.isLoad()) {
            post_indexing_base_update_load ++;
          }
          else {
            post_indexing_base_update_store ++;
          }
        }
      }  
    }

    // Update the register values. Sometimes we used them to infer the addressing mode.
    // t.print();

    if (!artificially_added_output) {
      for (int i = 0; i < t.num_output_regs; i++) {
        int x = t.output_reg_names[i];
        registers[x][0] = t.output_reg_values[i][0];
        registers[x][1] = t.output_reg_values[i][1];
        populated_registers[x] = true;
      }
    }

    if (verbose) {
      static long long int n = 0;
      fprintf(stderr, "%lld %llx ", ++n, t.PC);
      if (c == OPTYPE_OP) {
        switch (t.type) {
        case loadInstClass:
          fprintf(stderr, "LOAD (0x%llx)", t.EA);
          break;
        case storeInstClass:
          fprintf(stderr, "STORE (0x%llx)", t.EA);
          break;
        case aluInstClass:
          fprintf(stderr, "ALU");
          break;
        case fpInstClass:
          fprintf(stderr, "FP");
          break;
        case slowAluInstClass:
          fprintf(stderr, "SLOWALU");
          break;
        }
        for (int i = 0; i < t.num_input_regs; i++)
          fprintf(stderr, " I%d", t.input_reg_names[i]);
        for (int i = 0; i < t.num_output_regs; i++)
          fprintf(stderr, " O%d", t.output_reg_names[i]);
      } else {
        fprintf(stderr, "%s %llx", branch_names[c], t.target);
      }
      fprintf(stderr, "\n");
    }

    // are we done? then stop.


    if (!good) {
      break;
    }

    t = nextt;
  }

  fprintf(stderr, "converted %lld instructions\n", n);
  OpType lim = OPTYPE_MAX;
  for (int i = 2; i < (int)lim; i++) {
    if (counts[i])
      fprintf(stderr, "%s %lld %f%%\n", branch_names[i], counts[i], 100 * counts[i] / (double)n);
  }

  
  fprintf(stderr, "\n");
  
  if (APPLY_IMP_FLAG_REG) {
    fprintf(stderr, "Improvement FLAG-REG. Applied to %lld (%.2f%%) alu instructions and %lld (%.2f%%) FP instructions. %lld branches depend on flags (%.2f%% of cond branches).\n", 
      alus_with_no_destination_register, 
      100 * alus_with_no_destination_register / (double) n,
      fps_with_no_destination_register,
      100 * fps_with_no_destination_register / (double) n, 
      cond_branches_depending_on_flag_register, 
      100 * cond_branches_depending_on_flag_register / (double) counts[OPTYPE_JMP_DIRECT_COND]
    );
  }
  
  if (APPLY_IMP_BRANCH_TARGET) {
    fprintf(stderr, "Improvement BRANCH-TARGET. (No actual effect on ChampSim traces).  Applied to %lld (%.2f%%) instructions\n", 
      indirect_calls_with_incorrect_target, 
      100 * indirect_calls_with_incorrect_target / (double) n
    );
  }

  if (APPLY_IMP_BASE_UPDATE) {
    fprintf(stderr, "Improvement BASE-UPDATE. Load pairs: %lld (%.2f%% instructions). Store pairs: %lld (%.2f%% instructions). Pre-indexing base update load: %lld (%.2f%% instructions). Post-indexing base update load: %lld (%.2f%% instructions). Pre-indexing base update store: %lld (%.2f%% instructions). Post-indexing base update store: %lld (%.2f%% instrctions).\n", 
      load_pairs, 
      100 * load_pairs / (double) n, 
      store_pairs, 
      100 * store_pairs / (double) n, 
      pre_indexing_base_update_load, 
      100 * pre_indexing_base_update_load / (double) n, 
      post_indexing_base_update_load, 
      100 * post_indexing_base_update_load / (double) n, 
      pre_indexing_base_update_store, 
      100 * pre_indexing_base_update_store / (double) n, 
      post_indexing_base_update_store, 
      100 * post_indexing_base_update_store / (double) n);    
  }  

  if (APPLY_IMP_CALL_STACK) {
    fprintf(stderr, "Improvement CALL-STACK. Applied to %lld (%.2f%% instructions) that have been identified as additional calls\n", calls_incorrectly_classified_as_returns_fixed, 100 * calls_incorrectly_classified_as_returns_fixed / (double) n);
  }

  if (APPLY_IMP_BRANCH_REGS) {
    fprintf(stderr, "Improvement BRANCH-REGS. Applied to %lld branches (%.2f%% instrucctions), which add %.2f registers per branch. The maximum number of destination regisetrs leaves %lld calls (%.2f%% instrucctions) that could not add one destination registers per call. %lld conditional branches replace a dependency from flags with a dependency from other source registers (%.2f%% of cond branches). Indirect calls that replace reg_ax: %lld (%.2f%% instructions). Indirect jump that replace reg_ax: %lld (%.2f%% instructions). Returns with regs from the CVP-1 trace addeds: %lld (%.2f%% instructions). Predicated inststructions as conditional branches: %lld. Non-branch instructions reading conflicting X30: %lld (%.2f%% instructions).\n", 
      branches_now_including_original_trace_registers, 
      100 * branches_now_including_original_trace_registers / (double) n, 
      original_trace_registers_added_to_branches / (double) branches_now_including_original_trace_registers, 
      calls_that_could_not_add_all_original_destination_registers, 
      100 * calls_that_could_not_add_all_original_destination_registers / (double) n,
      cond_branches_not_depending_on_flag_register, 
      100 * cond_branches_not_depending_on_flag_register / (double) counts[OPTYPE_JMP_DIRECT_COND],
      indirect_calls_removing_reg_ax, 
      100 * indirect_calls_removing_reg_ax / (double) n, 
      indirect_jumps_removing_reg_ax, 
      100 * indirect_jumps_removing_reg_ax / (double) n, 
      returns_with_added_trace_source_registers, 
      100 * returns_with_added_trace_source_registers / (double) n,
      predicated_instructions_found,
      non_branches_reading_conflicting_x30, 
      100 * non_branches_reading_conflicting_x30 / (double) n
    );
  }

  if (APPLY_IMP_MEM_REGS) {
    fprintf(stderr, "Improvement MEM-REGS. Applied to %lld (%.2f%%) instructions with more than 1 destination register, adding  %.2f destination registers per instruction. In addition, applied to %lld (%.2f%%) instructions that had no destination register and it did not add any for them.\n", 
      insts_with_more_than_one_destination_imp, 
      100 * insts_with_more_than_one_destination_imp / (double) n, 
      additional_destination_regs_in_memory_instructions / (double) insts_with_more_than_one_destination_imp, 
      memory_instructions_not_adding_fake_destination_reg,  
      100 * memory_instructions_not_adding_fake_destination_reg / (double) n
    );
  }

  if (APPLY_IMP_MEM_FOOTPRINT) {
    fprintf(stderr, "Improvement MEM-FOOTPRINT. Load/store pairs that cross cachelines: %lld (%.2f%% instructions). Non Load/Store pairs that cross cachelines: %lld (%.2f%% instructions). DCZVA with non-aligned address: %lld\n",
      pair_crosses_cachelines, 
      100 * pair_crosses_cachelines / (double) n, 
      no_pair_crosses_cachelines, 
      100 * no_pair_crosses_cachelines / (double) n,
      DCZVA_with_non_aligned_address_fixed
    );

  }

  // close the pipe, if any
  if (f != stdin) {
    pclose(f);
  }
  return 0;
}
