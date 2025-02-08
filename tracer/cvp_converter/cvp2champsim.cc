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
#include <fstream>
#include <cstring>
#include <array>
#include <cassert>
#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./trace_instruction.h"

#include "./gzstream.h"

// defines for the paths for the various decompression programs and Apple/Linux differences

static inline constexpr int MAX_SRC = 4;
static inline constexpr int MAX_DST = 4;

static inline constexpr char TRANSLATED_REG_INSTRUCTION_POINTER = 64;
static inline constexpr char TRANSLATED_REG_STACK_POINTER = 65;
static inline constexpr char TRANSLATED_REG_FLAGS = 66;
static inline constexpr char TRANSLATED_REG_ZERO = 67;

static inline constexpr int MAX_REGS = 68; // highest register we could use is 67

bool verbose = false;

// use non-cloudsuite ChampSim trace format
using trace_instr_format = input_instr;

void print (trace_instr_format tr) {
  std::cerr << "0x" << std::hex << tr.ip << std::dec << " ";
  if (tr.is_branch) {
    std::cerr << "B " << (bool) tr.branch_taken << " ";
  }
  std::cerr << "SR: ";
  for (uint8_t i=0; i < NUM_INSTR_SOURCES; i++) {
    if (tr.source_registers[i] != 0)
      std::cerr << (int) tr.source_registers[i] << " ";
  }
  std::cerr << "DR: ";
  for (uint8_t i=0; i < NUM_INSTR_DESTINATIONS; i++) {
    if (tr.destination_registers[i] != 0)
      std::cerr << (int) tr.destination_registers[i] << " ";
  }
  std::cerr << "SM: ";
  for (uint8_t i=0; i < NUM_INSTR_SOURCES; i++) {
    if (tr.source_memory[i] != 0)
      std::cerr << "0x" << std::hex << tr.source_memory[i] << std::dec << " ";
  }
  std::cerr << "DR: ";
  for (uint8_t i=0; i < NUM_INSTR_DESTINATIONS; i++) {
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
  uint64_t m_mem_op_key = 0;

  AddrModeInfo() = default;

  AddrModeInfo(AddresssingMode mode, uint8_t access_size, uint8_t base_reg, uint64_t key, bool pair = false, uint64_t mask = ~0)
  : m_mode(mode)
  , m_corrected_access_size(access_size)
  , m_base_reg(base_reg)    
  , m_mask(mask)  
  , m_load_store_pair(pair)
  , m_mem_op_key(key)
  {
  }
};


// INT registers are registers 0 to 31. SIMD/FP registers are registers 32 to 63. Registers 64 to 67 used to map IP, SP, Flags, and zero (and differentiate them from the corresponding registesr used by branches)
enum Offset {
  vecOffset = 32,
  ccOffset = 64,
  invalidOffset = 255
};

struct Operand {
  bool m_valid;
  uint64_t m_value_lo;
  uint64_t m_value_hi;
  uint8_t m_reg_id;
  bool m_base_reg;

  bool valid() const { return m_valid; }
  bool isBaseReg() const { return m_base_reg; }
  uint64_t getValue() const { return m_value_lo; }
  uint64_t getValueHi() const { return m_value_hi; }
  bool isSimd() const { return m_reg_id >= Offset::vecOffset && m_reg_id < Offset::ccOffset; }
  bool isFlags() const { return m_reg_id == Offset::ccOffset; }
  uint8_t getName() const { return m_reg_id; }

  friend std::ostream& operator<<(std::ostream& stream, const Operand& op) {
    if(op.valid()) {
      stream << "[Reg:" << (int) op.getName() << " Val:" << std::hex;
      if(op.isSimd()) {
        stream << op.getValueHi();
      } 
      stream << op.getValue();
      if(op.isBaseReg()) {
        stream << " Breg";
      }
      stream << std::dec << "]";
    }
    else {
      stream << "[InvalidReg]";
    }
    return stream;
  }

  void copy_from (Operand op) {
    m_valid = op.m_valid;
    m_value_lo = op.m_value_lo;
    m_value_hi = op.m_value_hi;
    m_reg_id = op.m_reg_id;
    m_base_reg = op.m_base_reg;
  }
};

// This structure is used by CVP1's simulator.
// Adapt for your own needs.
struct Instruction {
  uint8_t m_type;
  uint64_t m_pc;
  bool m_taken;
  uint64_t m_next_pc;

  std::array<Operand, MAX_SRC> m_sources;
  std::array<Operand, MAX_DST> m_dests;

  uint64_t m_mem_ea;
  uint8_t m_mem_access_size;
  
  AddresssingMode m_addr_mode;

  bool isLoad() const { return m_type == InstClass::loadInstClass; }
  bool isStore() const { return m_type == InstClass::storeInstClass; }
  bool isBranch() const { 
    return m_type == InstClass::condBranchInstClass ||
      m_type == InstClass::uncondDirectBranchInstClass ||
      m_type == InstClass::uncondIndirectBranchInstClass;
  }

  int numInRegs() {
    return std::count_if(m_sources.begin(), m_sources.end(), [](const auto & in)
    {
      return in.valid();
    });
  }

  uint8_t baseUpdateReg() {
    if(auto it = std::find_if(m_sources.begin(), m_sources.end(), [](const auto & in)
    {
      return in.valid() && in.m_base_reg;
    }); it != m_sources.end())
    {
      return it->getName();
    }
    return Offset::invalidOffset;
  }

  int numOutRegs() {
    return std::count_if(m_dests.begin(), m_dests.end(), [](const auto & out)
    {
      return out.valid();
    });
  }

  void reset() {
    std::memset(this, 0, sizeof(Instruction));
  }
  
  bool is_base_update;
  bool is_guaranteed_base_update;
  bool ignore_hi_lane;
  bool is_kernel;

  friend std::ostream& operator<<(std::ostream& stream, const Instruction & inst) {
    static constexpr const char * cInfo[] = {"aluOp", "loadOp", "stOp", "condBrOp", "uncondDirBrOp", "uncondIndBrOp", "fpOp", "slowAluOp" };
    static constexpr const char * addrModeInfo[] = {"NoMode", "BaseImmOffset", "BaseRegOffset", "BaseUpdate", "DCZVA", "Prefetch" };

    stream << "[PC: 0x" << std::hex << inst.m_pc << std::dec << " type: "  << cInfo[inst.m_type];

    if(inst.isLoad() || inst.isStore()) {
      stream << " AddrMode:" << addrModeInfo[inst.m_addr_mode] << " EA: 0x" << std::hex << inst.m_mem_ea << std::dec << " MemSize: " << (int) inst.m_mem_access_size << "B";
    }
     
    if(inst.isBranch()) {
      stream << " Tkn:" << (int) inst.m_taken << " Targ: 0x" << std::hex << inst.m_next_pc << " " << std::dec;
    }
        
    stream << " - Sources : {";
    for(const auto & src : inst.m_sources) {
      if(src.valid()) {
        stream << " " << src;
      }
    }

    stream << " } - Dests : {";
    for(const auto & dst : inst.m_dests) {
      if(dst.valid()) {
        stream << " " << dst;
      }
    }
    
    stream << " }]";
    return stream;
  }

  // While CVP-1 encodes Aarch64 instructions, ChampSim follows x86 semantincs for branches. In addition, the CVP-1 traces do not include special purpose registers.
  // Thus, we translate registers 26 (IP), 6 (SP), and 25 (flags) from the CVP-1 traces to registers 64, 65, and 66, respectively, leaving registers 26, 6, and 25 free to convey the branch infomration to ChampSim.
  // Fuhtermore, register 0 is interpreted by ChampSim as "no register". Thus, we translate register 0 from the CVP-1 trace instructions into register 67. 

  void translate_registers () {

    for (auto it = m_sources.begin(); it != m_sources.end(); it++) {
      if (!it->valid()) {
        break;
      }

      switch (it->m_reg_id) {
        case champsim::REG_INSTRUCTION_POINTER:
          it->m_reg_id = TRANSLATED_REG_INSTRUCTION_POINTER;
          break;
        case champsim::REG_STACK_POINTER:
          it->m_reg_id = TRANSLATED_REG_STACK_POINTER;
          break;
        case champsim::REG_FLAGS:
          it->m_reg_id = TRANSLATED_REG_FLAGS;
          break;
        case 0:
          it->m_reg_id = TRANSLATED_REG_ZERO;
          break;
        default:
          break;
      }
    }

    for (auto it = m_dests.begin(); it != m_dests.end(); it++) {
      if (!it->valid()) {
        break;
      }

      switch (it->m_reg_id) {
        case champsim::REG_INSTRUCTION_POINTER:
          it->m_reg_id = TRANSLATED_REG_INSTRUCTION_POINTER;
          break;
        case champsim::REG_STACK_POINTER:
          it->m_reg_id = TRANSLATED_REG_STACK_POINTER;
          break;
        case champsim::REG_FLAGS:
          it->m_reg_id = TRANSLATED_REG_FLAGS;
          break;
        case 0:
          it->m_reg_id = TRANSLATED_REG_ZERO;
          break;
        default:
          break;
      }
    }
  }

  // Adds the flag register as destination to ALU and FP instructions without one
  void improvement_flag_reg () {
    if((m_type == aluInstClass || m_type == fpInstClass) && numOutRegs() == 0) {
      m_dests.at(0).m_valid = true;
      m_dests.at(0).m_reg_id = champsim::REG_FLAGS;
    }
  }

  // Fixes the branch target for indirect calls with x30 as source and detination register
  void improvement_branch_target (uint64_t nextPC) {
    if(m_type == InstClass::uncondIndirectBranchInstClass && numInRegs() == 1 && numOutRegs() == 1 && m_sources.at(0).m_reg_id == 30) {
      m_next_pc = nextPC;
    }
  }

  // Rearrange input and output regs for vector loads/stores such that the general purpose registers appear first
  void rearrange_registers () {
    if (isLoad() || isStore()) {
      if(numInRegs() > 1) {
        bool resort = false;
        for (int i = 0; i < numInRegs(); i++) {
          if (m_sources.at(i).isSimd()) {
            resort = true;
            break;
          }
        }

        if (resort) {
          std::array<Operand, MAX_SRC> aux_sources;
          for (int i = 0; i < numInRegs(); i++) {
            aux_sources.at(i).copy_from(m_sources.at(i));
          }
          
          int n = 0;
          for (int i = 0; i < numInRegs(); i++) {
            if (!aux_sources.at(i).isSimd()) {
              m_sources.at(n).copy_from(aux_sources.at(i));
              n++;
            }
          }
          for (int i = 0; i < numInRegs(); i++) {
            if (aux_sources.at(i).isSimd()) {
              m_sources.at(n).copy_from(aux_sources.at(i));
              n++;
            }
          }
        }
      }

      if(numOutRegs() > 1) {
        bool resort = false;
        for (int i = 0; i < numOutRegs(); i++) {
          if (m_dests.at(i).isSimd()) {
            resort = true;
            break;
          }
        }

        if (resort) {
          std::array<Operand, MAX_DST> aux_dests;
          for (int i = 0; i < numOutRegs(); i++) {
            aux_dests.at(i).copy_from(m_dests.at(i));
          }
          
          int n = 0;
          for (int i = 0; i < numOutRegs(); i++) {
            if (!aux_dests.at(i).isSimd()) {
              m_dests.at(n).copy_from(aux_dests.at(i));
              n++;
            }
          }
          for (int i = 0; i < numOutRegs(); i++) {
            if (aux_dests.at(i).isSimd()) {
              m_dests.at(n).copy_from(aux_dests.at(i));
              n++;
            }
          }
        }
      }
    }
  }

  // Create a key for memory operations. Used to identify different memory instructions that have the same PC (these instructions should from different threads/processes?)
  uint64_t get_mem_op_key () {
    uint64_t mask = isLoad();
    mask <<=3;
    mask |= numInRegs();
    for (int i = 0; i < numInRegs(); i++) {
      mask <<=7;
      mask |= m_sources.at(i).getName();
    }
    mask <<=3;
    mask |= numOutRegs();
    for (int i = 0; i < numOutRegs(); i++) {
      mask <<=7;
      mask |= m_dests.at(i).getName();
    }
    return (m_pc ^ mask);
  }

  // Check if this memory access crosses cachelines and get the address of the second cacheline
  bool improvement_mem_footprint_check_crosses_cachelines (uint64_t *next_cacheline_address) {
    assert(isLoad() || isStore());
    uint64_t cacheline_access_ini, cacheline_access_end;

    // Check if the memory access crosses cachelines and if so add the second cacheline address to the converted instruction  
    cacheline_access_ini = m_mem_ea & ~63lu;  // 0xffffffffffffffc0
    cacheline_access_end = (m_mem_ea + m_mem_access_size - 1) & ~63lu;

    if (cacheline_access_ini != cacheline_access_end) {
      *next_cacheline_address = cacheline_access_end;
      return true;
    }
    else {
      return false;
    }
  }
};


struct CVPTraceReader {
  // This is a cache to help us refine our addressing mode/ pair vs single detection
  std::unordered_map<uint64_t, AddrModeInfo> addr_mode_helper;
  std::array<uint64_t, 32> current_reg_values = {0};

  std::map<uint64_t, bool> code_pages, data_pages;
  std::map<uint64_t, uint64_t> remapped_pages;
  uint64_t bump_page = 0x1000;
  uint64_t num_cvp_insts = 0;
  bool last_cvp_inst = false;

  gz::igzstream * m_input;

  // Two instruction buffer to fix incorrect indirect call target
  // Note that given this implementation, the very last instruction of the trace
  // will never be returned to whoever uses CVPTraceReader
  std::array<Instruction, 2> m_inst_buffer;
  size_t m_current_index = 0;

  // We will keep an INT register file up to date
  std::array<uint64_t, MAX_REGS> registers;
  std::array<bool, MAX_REGS> populated_registers;

  uint64_t m_read_instr = 0;
  std::array<uint64_t, OPTYPE_MAX> counts;

  
  CVPTraceReader(const char * trace_name) {
    m_input = new gz::igzstream();
    m_input->open(trace_name, std::ios_base::in | std::ios_base::binary);

    if(m_input->bad() || m_input->fail()) {
      std::cerr << "Could not open the file" << std::endl;
      exit(1);
    }

    // Init the register file
    for (int i = 0; i < MAX_REGS; i++) {
      registers.at(i) = 0;
      populated_registers.at(i) = false;
    }
    // Init counts
    for (int i = 0; i < OPTYPE_MAX; i++) {
      counts.at(i) = 0;
    }

    preprocess_file();
    
    // m_input->close(); // closing and reopening gives an error
    delete m_input;
    m_input = new gz::igzstream();
    last_cvp_inst = false;
    num_cvp_insts = 0;

    m_input->open(trace_name, std::ios_base::in | std::ios_base::binary);

    if(m_input->bad() || m_input->fail()) {
      std::cerr << "Could not open the file" << std::endl;
      exit(1);
    }

    // Have to kickstart buffer 
    readTrace();
  }

  ~CVPTraceReader() {
    m_input->close();
    if(m_input)
      delete m_input;

    std::cout  << " Read " << m_read_instr << " instrs " << std::endl;
  }

  Instruction * readInstr() {
    if(readTrace()) {
      num_cvp_insts ++;
      return &m_inst_buffer[(m_current_index -1) & 0x1];  // Josue: added -1 to return the instruction from the previous iterations (we go one ahead always)
    }
    else {
      if (!last_cvp_inst) {
        num_cvp_insts ++;
        last_cvp_inst = true;
        return &m_inst_buffer[(m_current_index -1) & 0x1];
      }
      else {
        return nullptr;
      }
    }
  }

  uint64_t nextPC () {
    return m_inst_buffer[m_current_index & 0x1].m_pc;
  }

  // Read bytes from the trace and populate a buffer object.
  // Returns true if something was read from the trace, false if we the trace is over.
  bool readTrace() {
    // Trace Format :
    // Inst PC 				- 8 bytes
    // Inst Type			- 1 byte
    // If load/storeInst
    //   Effective Address 		- 8 bytes
    //   Access Size (one reg)		- 1 byte
    // If branch
    //   Taken 				- 1 byte
    //   If Taken
    //     Target			- 8 bytes
    // Num Input Regs 			- 1 byte
    // Input Reg Names 			- 1 byte each
    // Num Output Regs 			- 1 byte
    // Output Reg Names			- 1 byte each
    // Output Reg Values
    //   If INT (0 to 31) or FLAG (64) 	- 8 bytes each
    //   If SIMD (32 to 63)		- 16 bytes each

    auto & instr = m_inst_buffer[++m_current_index & 0x1];
    instr.reset();

    m_input->read((char*) &instr.m_pc, sizeof(instr.m_pc));

    if(m_input->eof())
      return false;

    instr.m_next_pc = instr.m_pc + 4;
    m_input->read((char*) &instr.m_type, sizeof(instr.m_type));

    assert(instr.m_type != undefInstClass);

    if(instr.isBranch()) {
      // Fill everything, then fix indirect call to x30 later if needed
      m_input->read((char*) &instr.m_taken, sizeof(instr.m_taken));
      if(instr.m_taken) {
        m_input->read((char*) &instr.m_next_pc, sizeof(instr.m_next_pc));
      }
    }

    if(instr.isLoad() || instr.isStore()) {
      m_input->read((char*) &instr.m_mem_ea, sizeof(instr.m_mem_ea));
      // We will fix the access size after we know number of inputs/outputs
      m_input->read((char*) &instr.m_mem_access_size, sizeof(instr.m_mem_access_size));
    }

    uint8_t num_in_regs = 0;
    m_input->read((char*) &num_in_regs, sizeof(num_in_regs));

    for(auto i = 0; i != num_in_regs; i++) {
      m_input->read((char*) &instr.m_sources[i].m_reg_id, sizeof(instr.m_sources[i].m_reg_id));
      instr.m_sources[i].m_valid = true;
    }

    uint8_t num_out_regs = 0;
    m_input->read((char*) &num_out_regs, sizeof(num_out_regs));

    for(auto i = 0; i != num_out_regs; i++) {
      m_input->read((char*) &instr.m_dests[i].m_reg_id, sizeof(instr.m_dests[i].m_reg_id));
      instr.m_dests[i].m_valid = true;
    }

    for(auto i = 0; i != num_out_regs; i++) {
      m_input->read((char*) &instr.m_dests[i].m_value_lo, sizeof(instr.m_dests[i].m_value_lo));
      if(instr.m_dests[i].isSimd()) {
        m_input->read((char*) &instr.m_dests[i].m_value_hi, sizeof(instr.m_dests[i].m_value_hi));
      }
      instr.m_dests[i].m_valid = true;
    }
    return true;
  }

  void preprocess_file() {
    
    std::cerr << "preprocessing to find code and data pages..." << std::endl;

    Instruction *t;    
    int count = 0;

    // Have to kickstart buffer 
    readTrace();

    for (t = readInstr(); t != nullptr; t = readInstr()) {
      // if (!code_pages[t->m_pc >> 12]) {
      //   std::cerr << count << " " << t->m_pc << std::endl;
      // }
      code_pages[t->m_pc >> 12] = true;
      if (t->m_type == loadInstClass || t->m_type == storeInstClass)
        data_pages[t->m_mem_ea >> 12] = true;
      count++;
      if (count % 10000000 == 0) {
        std::cerr << ".";        
        if (count % 600000000 == 0) {
          std::cerr << std::endl;
        }
      }
    }
    std::cerr << code_pages.size() << " code pages, " << data_pages.size() << " data pages" << std::endl;
  }


  // The addressing mode is used to determine the memory instructions that update the base register and the ones that are load/store pairs, and to fix the access size.
  // The access size is used by improvement mem-footprint but it is not part of the ChampSim traces.
  void identify_addressing_mode_and_fix_access_size () {

    Instruction * cvp_inst = &m_inst_buffer[(m_current_index -1) & 0x1];

    // Rearrange input and output regs for vector loads/stores such that the general purpose registers appear first
    cvp_inst->rearrange_registers();

    uint64_t mem_op_key = cvp_inst->get_mem_op_key ();
    
    if ((!cvp_inst->isLoad() && !cvp_inst->isStore() && addr_mode_helper.count(cvp_inst->m_pc) != 0) || (addr_mode_helper.count(cvp_inst->m_pc) != 0 && addr_mode_helper[cvp_inst->m_pc].m_mem_op_key != mem_op_key)) {
      // Remove info if the instruction has the same PC but it is actually a different instruction
      addr_mode_helper.erase(cvp_inst->m_pc);
    }

    if((cvp_inst->isLoad() || cvp_inst->isStore()) && addr_mode_helper.count(cvp_inst->m_pc) == 0) {
      
      if(cvp_inst->isLoad()) {

        // Case 1: Single dest reg
        if(cvp_inst->numOutRegs() == 1) {

          // Case 1.1: Single src reg
          if(cvp_inst->numInRegs() == 1) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
          }

          // Case 1.2: Two src regs
          else if(cvp_inst->numInRegs() == 2) { 
            uint8_t outReg = cvp_inst->m_dests.at(0).getName();

            if(auto base_reg = std::find_if(cvp_inst->m_sources.begin(), cvp_inst->m_sources.end(), [outReg](const auto & in)
            {
              return in.valid() && in.getName() == outReg;
            }); base_reg != cvp_inst->m_sources.end())
            {
              // Case 1.2.1: Source also in destination but single destination, so it cannot be Base Update.
              // The instruction is just overwriting one of the register used to compute the address  
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            }            
            else {
              // Case 1.2.2: No source is also destination
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            } 
          }

          // Case 1.2.3: No source register
          // Source 0 is bogus, but we are safe as this is not Base Update and we will not use it
          else if (cvp_inst->numInRegs() == 0) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::PcRelative, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
          }
        }

        // Case 2: Two destination registers
        else if(cvp_inst->numOutRegs() == 2) {
          uint8_t num_integer_input_regs = 0;
          for (int i = 0; i < cvp_inst->numInRegs(); i++) {
            if (!cvp_inst->m_sources.at(i).isSimd()) {
              num_integer_input_regs ++;
            }
          }

          if (cvp_inst->numInRegs() == 1 || num_integer_input_regs == 1) {
          
            if(auto base_reg = std::find_if(cvp_inst->m_dests.begin(), cvp_inst->m_dests.end(), [&](const auto & out)
            {
              return out.valid() && out.getName() == cvp_inst->m_sources[0].getName();
            }); base_reg != cvp_inst->m_dests.end())
            {
              // Case 2.1: Single source register is also destination register

              // For regular load/store: "Pre/Post indexed by an unscaled 9-bit signed immediate offset" hence -256 to 255 immediate
              auto base_reg_value = base_reg->getValue();

              // If the EA equals the base_reg_value, it should be a load with base-update pre-increment
              const bool fits_unscaled_baseupdate_immediate = 
                (base_reg_value >= cvp_inst->m_mem_ea && ((base_reg_value - cvp_inst->m_mem_ea) <= 256)) ||  
                (base_reg_value < cvp_inst->m_mem_ea && ((cvp_inst->m_mem_ea - base_reg_value) <= 255));

              // Case 2.1.1: Value of destination register (base register) is within base update immediate
              // This is regular load with BaseUpdate addressing
              // Note that this is not bulletproof e.g. if it is actually ldp reg1, reg2, [reg1] and we have bad luck
              if(fits_unscaled_baseupdate_immediate) {
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseUpdate, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
              }
            
              // Case 2.1.2: Value of destination register (base register) is not within base update immediate, this is load pair
              else {
                assert(cvp_inst->numInRegs() == 1); // If there is an int register with SIMD registers, it should be base update... So make sure that if it is not a base update, there is a single source register
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
              }
            }

            // Case 2.2: Single source register not also destination register
            else {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
            }
          }

          else {
            assert(cvp_inst->numInRegs() == 2); // Two sources
            // LD* with the register post index

            auto outReg = cvp_inst->m_dests[0].getName();
            if(auto base_reg = std::find_if(cvp_inst->m_sources.begin(), cvp_inst->m_sources.end(), [&](const auto & in)
            {
              return in.valid() && in.getName() == outReg;
            }); base_reg != cvp_inst->m_sources.end())
            {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseUpdate, cvp_inst->m_mem_access_size, cvp_inst->m_dests[0].getName(), mem_op_key};
            }
            else {
              assert (false);
            }
          }
        }

        // Case 3: Three destination registers, load pair, or SIMD load multiple with base update
        else if(cvp_inst->numOutRegs() >= 3) {
          // We assume that the base register (if any) will be the first destination so make sure that there is a single one (at least, non SIMD)
          uint8_t num_integer_dest_regs = 0;
          for (int i = 0; i < cvp_inst->numOutRegs(); i++) {
            if (!cvp_inst->m_dests.at(i).isSimd()) {
              num_integer_dest_regs ++;
            }
          }
          assert(cvp_inst->numInRegs() == 1 || num_integer_dest_regs == 1);
          bool base_update = false;

          for (int i = 0; i < cvp_inst->numInRegs(); i++) {
            if (cvp_inst->m_sources.at(i).getName() == cvp_inst->m_dests.at(0).getName()) {
              base_update = true;
            }
          } 
          if (base_update) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseUpdate, (uint8_t) (cvp_inst->m_mem_access_size * (cvp_inst->numOutRegs() - 1)), cvp_inst->m_dests.at(0).getName(), mem_op_key, true};
          }
          else {
            // Definitively, not base update
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size * cvp_inst->numOutRegs()), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};            
          }
        }

        // Case 4: Load with zero destination registers (= prefetch)
        else if(cvp_inst->numOutRegs() == 0) {
          addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::Prefetch, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
        }
      }

      else {
        assert(cvp_inst->isStore());

        // Case 1: One source
        if(cvp_inst->numInRegs() == 1) {
          // Case 1.1 DCZVA have to align EA to 64B because
          // There are no alignment restrictions on this address and traces were captured with 64B cache line size
          if(cvp_inst->m_mem_access_size == 64) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::DCZVA, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key, false, ~63lu};
          }

          // Case 1.2 Specific ops
          else {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
          }
        }

        // Case 2: Two sources
        else if (cvp_inst->numInRegs() == 2) {
          
          // Case 2.1: No destination registers
          if(cvp_inst->numOutRegs() == 0) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
          }

          else {
            assert(cvp_inst->numOutRegs() == 1);

            const bool has_base_update = std::find_if(cvp_inst->m_sources.begin(), cvp_inst->m_sources.end(), [&](const auto & in)
            {
              return in.valid() && in.getName() == cvp_inst->m_dests[0].getName();
            }) != cvp_inst->m_sources.end();

            // Case 2.2: One destination register, differs from two sources (strex?)
            if(!has_base_update) {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            }

            // Case 2.3: One destination register, same as one of the sources
            else if(has_base_update) {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseUpdate, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            }
          }
        }

        // Case 3: Three sources or more (SIMD store multiple)
        else if(cvp_inst->numInRegs() >= 3) {
          
          // Case 3.2: No outputs
          if(cvp_inst->numOutRegs() == 0) {
            // Make sure that the first register is not SIMD as we are going to assume it might be the base register
            assert(!cvp_inst->m_sources.at(0).isSimd()); //Not SIMD

            auto base_reg_value = registers.at(cvp_inst->m_sources.at(0).getName()); 

            // Can't really make a decision here, wait until next time
            if(base_reg_value == 0x0) {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::NoMode, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            }

            else {
              // For store pair: 
              // "Pre-indexed by a scaled 7-bit signed immediate offset."
              // "Post-indexed by a scaled 7-bit signed immediate offset."
              const int scaling_factor = cvp_inst->m_mem_access_size;
              const bool fits_stp_immediate_offset = 
                ((cvp_inst->m_mem_ea >= base_reg_value) && ((cvp_inst->m_mem_ea - base_reg_value) <= ((uint64_t) (63 * scaling_factor)))) ||
                ((cvp_inst->m_mem_ea < base_reg_value) && ((base_reg_value - cvp_inst->m_mem_ea) <= ((uint64_t) (64 * scaling_factor))));

              // Case 3.2.1: store pair BaseImmediateOffset if value of base register is within immediate offset of EA
              // Note that this is not bulletproof
              if(fits_stp_immediate_offset) {
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
              }

              // Case 3.2.2: regular store using two registers for address
              else {
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
              }
            }
          }

          else { // cvp_inst->numOutRegs() != 0
            // We should only have a single destination register
          
            // Case 3.1: >=3 sources, 1 destination
            if (cvp_inst->numOutRegs() == 1) {

              if (cvp_inst->m_dests.at(0).getValue() == 0 || cvp_inst->m_dests.at(0).getValue() == 1) {
                // Case 3.1.1: Output value is 0/1 then it is (most probably) STXP (Store Exclusive Pair writes 0/1 to indicate sucess or no write performed)
                // The addressing mode is BaseRegImmediate, but because it is a pair, the data transfer size is 2x.
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegImmediate, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
              }

              else {
                // Case 3.1.2: Output value is not 0/1 then it is Store Pair with Base update
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseUpdate, (uint8_t) (cvp_inst->m_mem_access_size  * (cvp_inst->numInRegs() - 1)), cvp_inst->m_dests.at(0).getName(), mem_op_key, true};
              }
            }

            // Case 3.3: >=3 sources, 2 destinations
            else {
              // Make sure source registers are not SIMD... otherwise it could be ST4?
              for (int i = 0; i < cvp_inst->numInRegs(); i++) {
                if (cvp_inst->m_sources.at(i).isSimd()) {
                  assert(false);
                }
              }

              // Looks like Compare and Swap Pair, which reads into two consecutive regs, compares with two consecutive regs, and stores two consecutive regs. 
              // That's pretty specific... and not base update
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::Other, (uint8_t) (cvp_inst->m_mem_access_size * cvp_inst->numInRegs()), cvp_inst->m_dests.at(0).getName(), mem_op_key, true};
            }
          }
        }
      }
    }
               
    if(cvp_inst->isLoad() || cvp_inst->isStore()) {
      auto info = addr_mode_helper.find(cvp_inst->m_pc);
      assert(info != addr_mode_helper.end());

      // Attempt to fix non categorized stores with three sources (Case 3.2)
      if(info->second.m_mode == AddresssingMode::NoMode) {

        assert(!cvp_inst->m_sources.at(0).isSimd());
        auto base_reg_value = registers.at(cvp_inst->m_sources.at(0).getName());
        const bool ea_valid = info->second.m_last_ea != 0x0;
        const bool same_ea = info->second.m_last_ea == cvp_inst->m_mem_ea;

        
        if(ea_valid && same_ea){

          if (base_reg_value != info->second.m_last_base_reg_value) {
            // If sameE EA and base register changed, that's strange... We should not come here.
            std::cerr << "Strange... Inst: " << *cvp_inst << std::endl;
            assert(false);
          }
          
          // Base register did not change since last time, and address is the same, this is three source store with single base register, hence store pair
          if (base_reg_value == 0x0) {
            addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
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

            if (registers.at(cvp_inst->m_sources.at(0).getName()) + registers.at(cvp_inst->m_sources.at(1).getName()) == cvp_inst->m_mem_ea) {
              // In some (most?) cases it looks like base + potetial offset give EA as an overflow accident
              // Anyway it does not affect the accuracy of our conversion as ChampSim traces to not include access size and, in any case, this instruction is not a base update
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key, true};              
            }

            else {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};              
            }
          }

          else {
            // Base reg did not change but EA changed, so it's using a second register for address
            if (base_reg_value == 0x0) {
              addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
            }

            // Base register value did change, which caused EA to change since last time
            // We have to redo the check for case 3.2.1 vs 3.2.2
            else {

              // Actually, we never came here...
              assert(false);
            
              // For store pair :
              // "Pre-indexed by a scaled 7-bit signed immediate offset."
              // "Post-indexed by a scaled 7-bit signed immediate offset."
              const int scaling_factor = cvp_inst->m_mem_access_size;
              const bool fits_stp_immediate_offset =
                ((cvp_inst->m_mem_access_size >= base_reg_value) && ((cvp_inst->m_mem_access_size - base_reg_value) <= ((uint64_t) (63 * scaling_factor)))) ||
                ((cvp_inst->m_mem_access_size < base_reg_value) && ((base_reg_value - cvp_inst->m_mem_access_size) <= ((uint64_t) (64 * scaling_factor))));

              // Case 3.2.1: store pair BaseImmediateOffset if value of base register is within immediate offset of EA
              // Note that this is not bulletproof
              if(fits_stp_immediate_offset) {
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseImmediateOffset, (uint8_t) (cvp_inst->m_mem_access_size << 1), cvp_inst->m_sources.at(0).getName(), mem_op_key, true};
              }

              // Case 3.2.2: regular store using two registers for address
              else {
                addr_mode_helper[cvp_inst->m_pc] = {AddresssingMode::BaseRegOffset, cvp_inst->m_mem_access_size, cvp_inst->m_sources.at(0).getName(), mem_op_key};
              }
            }
          }
        }

        else {
          // ! ea_valid. We will try to classify this instruction next time
        }
      }

      cvp_inst->m_mem_access_size = info->second.m_corrected_access_size;
      AddresssingMode addr_mode = info->second.m_mode;

      info->second.m_last_ea = cvp_inst->m_mem_ea;
      info->second.m_last_base_reg_value = registers.at(cvp_inst->m_sources.at(0).getName());

      // Double check we have identified an integer register as base update
      if (addr_mode == AddresssingMode::BaseUpdate && (info->second.m_base_reg >= 32 && info->second.m_base_reg < 64)) {
        std::cerr << "Num cvp insts: " << num_cvp_insts << std::endl;
        std::cerr << *cvp_inst << std::endl;        
      }
      assert(addr_mode != AddresssingMode::BaseUpdate || (info->second.m_base_reg < 32 || info->second.m_base_reg >= 64)); // Not SIMD

      // Marking destinstion register as base register is only useful to control its latency in a timing simulator 
      // (base reg is available after ALU cycle, not after load to use cycle)
      // So, only mark it if it is an actual BaseUpdate and not just BaseReg or BaseImm that happens to load data in the base register
      if(addr_mode == AddresssingMode::BaseUpdate) {
        for(auto & dst : cvp_inst->m_dests) {
          if(dst.getName() == info->second.m_base_reg) {
            dst.m_base_reg = true;
          }
        }
      }

      // We could not classify the instruction this time as the candidate base register was uninitialized. Try next time.
      if (addr_mode == AddresssingMode::NoMode && !populated_registers.at(cvp_inst->m_sources.at(0).getName())) {
        addr_mode_helper.erase(cvp_inst->m_pc);
      }
    }
  }

  // Align effective address of DCZVA stores to a cacheline boundary
  void improvement_mem_footprint_align_DCZVA_address (Instruction *inst) {
    assert(inst->isLoad() || inst->isStore());
    auto info = addr_mode_helper.find(inst->m_pc);
    if (info == addr_mode_helper.end()) {
      return; // We will try again next time
    }
       
    // To align address for DCZVA
    inst->m_mem_ea &= info->second.m_mask;
    info->second.m_last_ea = inst->m_mem_ea;
  }

  void generate_converted_instruction () {
    trace_instr_format ct;

    Instruction * cvp_inst = &m_inst_buffer[(m_current_index -1) & 0x1];

    ct.ip = cvp_inst->m_pc;
    ct.is_branch = false;

    // we are going to figure out the op type
    OpType c = OPTYPE_OP;

    // if this is a branch then do more stuff
    if (cvp_inst->isBranch()) {
      ct.is_branch = true;

      // t.print();

      // if this is a conditional branch then it's direct and we're done figuring out the type
      if (cvp_inst->m_type == condBranchInstClass) {
        c = OPTYPE_JMP_DIRECT_COND;
      } 

      else {
        // this is some other kind of branch. it should have a non-zero target
        assert(cvp_inst->m_next_pc);

        // On ARM, returns are an indirect jump to X30. They do not write to any general-purpose register
        if (cvp_inst->numOutRegs() == 0 && cvp_inst->numInRegs() == 1 && cvp_inst->m_sources.at(0).getName() == 30) {

          // yes. it's a return.
          c = OPTYPE_RET_UNCOND;
        }

        // On ARM, calls link the return address in register X30
        else if (cvp_inst->numOutRegs() == 1 && cvp_inst->m_dests.at(0).getName() == 30) {

          // is it indirect?
          if (cvp_inst->m_type == uncondIndirectBranchInstClass) {
            c = OPTYPE_CALL_INDIRECT_UNCOND;
          }
          else {
            c = OPTYPE_CALL_DIRECT_UNCOND;
          }
        } 
          
        // no X30? then it's just an unconditional jump
        else {
            
          // is it indirect?
          if (cvp_inst->m_type == uncondIndirectBranchInstClass) {
            c = OPTYPE_JMP_INDIRECT_UNCOND;
          }
          else {
            c = OPTYPE_JMP_DIRECT_UNCOND;
          }
        }
      }

      counts.at(c)++;

      // OK now make a branch instruction out of this bad boy
      memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
      memset(ct.source_registers, 0, sizeof(ct.source_registers));
      memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
      memset(ct.source_memory, 0, sizeof(ct.source_memory));
      
      switch (c) {
      case OPTYPE_JMP_DIRECT_UNCOND:
        ct.branch_taken = cvp_inst->m_taken;
        
        // Writes IP only
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        
        assert(cvp_inst->numInRegs() == 0);  // Does not have any source register
        assert(cvp_inst->numOutRegs() == 0); // nor any destination register in the CVP-1 trace
        break;

      case OPTYPE_JMP_DIRECT_COND:
        ct.branch_taken = cvp_inst->m_taken;
        
        // reads FLAGS or other, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        // turns out pin records conditional direct branches as also reading IP. whatever.
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        
        if (cvp_inst->numInRegs() == 1) {            
          // Most likely those are cb(n)z and tb(n)z which jump based on the content of a general-purpose register.
          // We will add more sources later but should not add flags.
          ct.source_registers[1] = cvp_inst->m_sources.at(0).getName();

        }
        else if (cvp_inst->numInRegs() == 2) {
          // Predicated insts (csel, csneg, etc.). These should add flags.
          assert(false); // Actually, we never came here.
          ct.source_registers[1] = champsim::REG_FLAGS;
          ct.source_registers[2] = cvp_inst->m_sources.at(0).getName();
          ct.source_registers[3] = cvp_inst->m_sources.at(1).getName();
        }
        else {             
          assert(cvp_inst->numInRegs() == 0);
          // Conditional branches without a source register in the CVP-1 trace should read from the flag register
          ct.source_registers[1] = champsim::REG_FLAGS;
        }
        assert(cvp_inst->numOutRegs() == 0); // Does not have any destination register in the CVP-1 trace
        break;

      case OPTYPE_CALL_INDIRECT_UNCOND:
        ct.branch_taken = true;

        // reads something else, reads IP, reads SP, writes SP, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.source_registers[1] = champsim::REG_STACK_POINTER;
        
        assert(cvp_inst->numInRegs() == 1);   
        ct.source_registers[2] = cvp_inst->m_sources.at(0).getName();
          
        // This is a known limitation described in the paper. 
        // We cannot add a third destination register without modifying the maximum number of destination registers in ChampSim
        assert(cvp_inst->numOutRegs() == 1 && cvp_inst->m_dests.at(0).getName() == 30);
        break;

      case OPTYPE_CALL_DIRECT_UNCOND:
        ct.branch_taken = true;

        // reads IP, reads SP, writes SP, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        ct.source_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.source_registers[1] = champsim::REG_STACK_POINTER;

        assert (cvp_inst->numInRegs() == 0); // It does not have any source register in the CVP-1 trace
        // This is a known limitation described in the paper. 
        // We cannot add a third destination register without modifying the maximum number of destination registers in ChampSim
        assert(cvp_inst->numOutRegs() == 1 && cvp_inst->m_dests.at(0).getName() == 30);
        break;

      case OPTYPE_JMP_INDIRECT_UNCOND:
        ct.branch_taken = true;

        // reads something else, writes IP
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        
        assert(cvp_inst->numInRegs() == 1);  
        ct.source_registers[0] = cvp_inst->m_sources.at(0).getName();

        assert(cvp_inst->numOutRegs() == 0); // It does not have any destination register in the CVP-1 trace
        break;

      case OPTYPE_RET_UNCOND:
        ct.branch_taken = true;

        // reads SP, writes SP, writes IP
        ct.source_registers[0] = champsim::REG_STACK_POINTER;
        ct.destination_registers[0] = champsim::REG_INSTRUCTION_POINTER;
        ct.destination_registers[1] = champsim::REG_STACK_POINTER;
        
        assert(cvp_inst->numInRegs() == 1);  
        ct.source_registers[1] = cvp_inst->m_sources.at(0).getName();

        assert(cvp_inst->numOutRegs() == 0); // It does not have any destination register in the CVP-1 trace
        break;

      default:
        assert(0);
      }

      // print(ct);
      fwrite(&ct, sizeof(ct), 1, stdout); // write a branch trace
    }

    else { // Non-branches

      if (addr_mode_helper.count(cvp_inst->m_pc) != 0 && addr_mode_helper[cvp_inst->m_pc].m_mode == BaseUpdate) {

        uint32_t base_reg_name = addr_mode_helper[cvp_inst->m_pc].m_base_reg;

        if(auto base_reg = std::find_if(cvp_inst->m_dests.begin(), cvp_inst->m_dests.end(), [base_reg_name](const auto & out)
          {
            return out.valid() && out.getName() == base_reg_name;
          }); base_reg != cvp_inst->m_dests.end()) 
        {

          if (base_reg->getValue() == cvp_inst->m_mem_ea) {
                  
            // Pre-indexing base update increment
            memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
            memset(ct.source_registers, 0, sizeof(ct.source_registers));
            memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
            memset(ct.source_memory, 0, sizeof(ct.source_memory));
            
            assert(base_reg != 0);
            ct.destination_registers[0] = base_reg->getName();
            ct.source_registers[0] = base_reg->getName();
            // print(ct);
            fwrite(&ct, sizeof(ct), 1, stdout); // write the pre-increment alu
            ct.ip += 2;
          }
        }
        else { assert(false); }
      }

      memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
      memset(ct.source_registers, 0, sizeof(ct.source_registers));
      memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
      memset(ct.source_memory, 0, sizeof(ct.source_memory));
      counts.at(OPTYPE_OP)++;
      if (cvp_inst->numInRegs() > (int) NUM_INSTR_SOURCES) {  // At least, some SIMD stores with five source registers... Mentioned it as limitation
        for (int i = NUM_INSTR_SOURCES; i < MAX_SRC; i++) {
          cvp_inst->m_sources.at(i).m_valid = false;
        }
        assert(cvp_inst->numInRegs() == NUM_INSTR_SOURCES);  
      }
      
      assert(cvp_inst->numOutRegs() <= 1 || (cvp_inst->m_type == loadInstClass || cvp_inst->m_type == storeInstClass));
      
      int b = 0;  
      for (int a=0; a<cvp_inst->numOutRegs(); a++) {          
        int x = cvp_inst->m_dests.at(a).getName();

        if (addr_mode_helper.count(cvp_inst->m_pc) != 0 && addr_mode_helper[cvp_inst->m_pc].m_mode == BaseUpdate && x == addr_mode_helper[cvp_inst->m_pc].m_base_reg) {
          // If we are aplying the base-update improvement, we don't want to add the base register as a destination of the memory instruction
          continue;
        }          

        assert(x != 0);
        ct.destination_registers[b] = x;
        b++;
      }
      
      for (int i = 0; i < cvp_inst->numInRegs(); i++) {
        int x = cvp_inst->m_sources.at(i).getName();
        assert(x != 0);
        ct.source_registers[i] = x;
      }
    
     
      bool exapnds_two_cachelines = false;
      uint64_t next_cacheline_address;
      if (cvp_inst->isLoad() || cvp_inst->isStore()) {

        // Check if this memory access crosses cachelines and get the address of the second cacheline
        exapnds_two_cachelines = cvp_inst->improvement_mem_footprint_check_crosses_cachelines(&next_cacheline_address);

        // Align effective address of DCZVA stores to a cacheline boundary
        improvement_mem_footprint_align_DCZVA_address(cvp_inst);

      }

      switch (cvp_inst->m_type) {
        case loadInstClass:
          ct.source_memory[0] = transform(cvp_inst->m_mem_ea);
          if (exapnds_two_cachelines) {
            ct.source_memory[1] = transform(next_cacheline_address);
          }
          break;
        case storeInstClass:
          ct.destination_memory[0] = transform(cvp_inst->m_mem_ea);
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

      if (addr_mode_helper.count(cvp_inst->m_pc) != 0 && addr_mode_helper[cvp_inst->m_pc].m_mode == BaseUpdate) {
        uint32_t base_reg_name = addr_mode_helper[cvp_inst->m_pc].m_base_reg;

        if(auto base_reg = std::find_if(cvp_inst->m_dests.begin(), cvp_inst->m_dests.end(), [base_reg_name](const auto & out)
          {
            return out.valid() && out.getName() == base_reg_name;
          }); base_reg != cvp_inst->m_dests.end()) 
        {
        
          if (base_reg->getValue() != cvp_inst->m_mem_ea) {
                  
            // Post-indexing base update increment
            memset(ct.destination_registers, 0, sizeof(ct.destination_registers));
            memset(ct.source_registers, 0, sizeof(ct.source_registers));
            memset(ct.destination_memory, 0, sizeof(ct.destination_memory));
            memset(ct.source_memory, 0, sizeof(ct.source_memory));
            
            assert(base_reg != 0);
            ct.destination_registers[0] = base_reg->getName();
            ct.source_registers[0] = base_reg->getName();
            ct.ip += 2;
            // print(ct);
            fwrite(&ct, sizeof(ct), 1, stdout); // write the pre-increment alu
          }
        }
        else { assert(false); }
      }  
    }

    // Update the register values. Sometimes we used them to infer the addressing mode.
    // cvp_inst->print();

    for (int i = 0; i < cvp_inst->numOutRegs(); i++) {
      int x = cvp_inst->m_dests.at(i).getName();
      registers.at(x) = cvp_inst->m_dests.at(i).getValue();      
      populated_registers.at(x) = true;
    }

    if (verbose) {
      static long long int n = 0;
      std::cerr << ++n << " " << cvp_inst->m_pc << " ";
      if (c == OPTYPE_OP) {
        switch (cvp_inst->m_type) {
        case loadInstClass:
          std::cerr << "LOAD (0x" <<  cvp_inst->m_mem_ea << ")";
          break;
        case storeInstClass:
          std::cerr << "STORE (0x" << cvp_inst->m_mem_ea << ")";
          break;
        case aluInstClass:
          std::cerr << "ALU";
          break;
        case fpInstClass:
          std::cerr << "FP";
          break;
        case slowAluInstClass:
          std::cerr << "SLOWALU";
          break;
        }
        for (int i = 0; i < cvp_inst->numInRegs(); i++)
          std::cerr << " I " << cvp_inst->m_sources.at(i).getName();
        for (int i = 0; i < cvp_inst->numOutRegs(); i++)
          std::cerr << " O " << cvp_inst->m_dests.at(i).getName();
      } else {
        std::cerr << branch_names[c] << " " << cvp_inst->m_next_pc;
      }
      std::cerr << std::endl;
    }
  }

  // take an address representing data and make sure it doesn't overlap with code
  uint64_t transform(uint64_t a) {
    static int num_allocs = 0;
    uint64_t page = a >> 12;
    uint64_t new_page = page;
    if (code_pages.find(page) != code_pages.end()) {
      new_page = remapped_pages[page];
      if (new_page == 0) {
        num_allocs++;
        std::cerr << "[" << num_allocs << "]";
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

};


// one record from the CVP-1 trace file format
struct trace {
  uint64_t PC,  // program counter
      EA,     // effective address
      target; // branch target

  uint8_t access_size,
      taken, // branch was taken
      num_input_regs, num_output_regs, input_reg_names[256], output_reg_names[256];

  // output register values could be up to 128 bits each
  uint64_t output_reg_values[256][2];

  std::array<Operand, MAX_SRC> m_sources;
  std::array<Operand, MAX_DST> m_dests;

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
};


int main(int argc, char** argv) {

  if(argc < 2) {
    std::cerr << "Expecting path to trace file, exiting..." << std::endl;
   return -1;
  }

  CVPTraceReader reader(argv[1]);
  
  // number of records read so far
  long long int n = 0;

  // loop getting records until we're done
  for (Instruction * cvp_inst = reader.readInstr(); cvp_inst != nullptr; cvp_inst = reader.readInstr()) {

    // one more record
    n++;

    // print something to entertain the user while they wait
    if (n % 1000000 == 0) {
      std::cerr << n << " instructions" << std::endl;
    }

    // if (n == 100000) {
    //   break;
    // }

    // here we can print the CVP-1 instructio before we make any change
    // std::cerr << *cvp_inst << std::endl;

    cvp_inst->translate_registers();

    cvp_inst->improvement_flag_reg();

    cvp_inst->improvement_branch_target (reader.nextPC());

    reader.identify_addressing_mode_and_fix_access_size ();

    reader.generate_converted_instruction();

  }

  std::cerr << "converted " << n << " instructions" << std::endl;
  OpType lim = OPTYPE_MAX;
  for (int i = 2; i < (int)lim; i++) {
    if (reader.counts.at(i))
      std::cerr << branch_names[i] << " " << reader.counts.at(i) << " " << 100 * reader.counts.at(i) / (double)n << std::endl;
  }
  std::cerr << std::endl;
  return 0;
}
