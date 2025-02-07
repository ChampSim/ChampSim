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

#ifndef TRACE_INSTRUCTION_H
#define TRACE_INSTRUCTION_H

#include <limits>

// special registers that help us identify branches
namespace champsim
{
constexpr char REG_STACK_POINTER = 6;
constexpr char REG_FLAGS = 25;
constexpr char REG_INSTRUCTION_POINTER = 26;

constexpr const char* reg_names[] = {
    "invalid", "none",      "first",        "rdi",     "rsi",     "rbp",     "rsp",      "rbx",      "rdx",      "rcx",      "rax",      "r8",         "r9",
    "r10",     "r11",       "r12",          "r13",     "r14",     "r15",     "CS",       "SS",       "DS",       "ES",       "FS",       "GS",         "rflags",
    "rip",     "al",        "ah",           "ax",      "cl",      "ch",      "cx",       "dl",       "dh",       "dx",       "bl",       "bh",         "bx",
    "bp",      "si",        "di",           "sp",      "flags",   "ip",      "edi",      "dil",      "esi",      "sil",      "ebp",      "bpl",        "esp",
    "spl",     "ebx",       "edx",          "ecx",     "eax",     "eflags",  "eip",      "r8b",      "r8w",      "r8d",      "r9b",      "r9w",        "r9d",
    "r10b",    "r10w",      "r10d",         "r11b",    "r11w",    "r11d",    "r12b",     "r12w",     "r12d",     "r13b",     "r13w",     "r13d",       "r14b",
    "r14w",    "r14d",      "r15b",         "r15w",    "r15d",    "mm0",     "mm1",      "mm2",      "mm3",      "mm4",      "mm5",      "mm6",        "mm7",
    "xmm0",    "xmm1",      "xmm2",         "xmm3",    "xmm4",    "xmm5",    "xmm6",     "xmm7",     "xmm8",     "xmm9",     "xmm10",    "xmm11",      "xmm12",
    "xmm13",   "xmm14",     "xmm15",        "xmm16",   "xmm17",   "xmm18",   "xmm19",    "xmm20",    "xmm21",    "xmm22",    "xmm23",    "xmm24",      "xmm25",
    "xmm26",   "xmm27",     "xmm28",        "xmm29",   "xmm30",   "xmm31",   "ymm0",     "ymm1",     "ymm2",     "ymm3",     "ymm4",     "ymm5",       "ymm6",
    "ymm7",    "ymm8",      "ymm9",         "ymm10",   "ymm11",   "ymm12",   "ymm13",    "ymm14",    "ymm15",    "ymm16",    "ymm17",    "ymm18",      "ymm19",
    "ymm20",   "ymm21",     "ymm22",        "ymm23",   "ymm24",   "ymm25",   "ymm26",    "ymm27",    "ymm28",    "ymm29",    "ymm30",    "ymm31",      "zmm0",
    "zmm1",    "zmm2",      "zmm3",         "zmm4",    "zmm5",    "zmm6",    "zmm7",     "zmm8",     "zmm9",     "zmm10",    "zmm11",    "zmm12",      "zmm13",
    "zmm14",   "zmm15",     "zmm16",        "zmm17",   "zmm18",   "zmm19",   "zmm20",    "zmm21",    "zmm22",    "zmm23",    "zmm24",    "zmm25",      "zmm26",
    "zmm27",   "zmm28",     "zmm29",        "zmm30",   "zmm31",   "k0",      "k1",       "k2",       "k3",       "k4",       "k5",       "k6",         "k7",
    "mxcsr",   "mxcsrmask", "orig_rax",     "fpcw",    "fpsw",    "fptag",   "fpip_off", "fpip_sel", "fpopcode", "fpdp_off", "fpdp_sel", "fptag_full", "st0",
    "st1",     "st2",       "st3",          "st4",     "st5",     "st6",     "st7",      "dr0",      "dr1",      "dr2",      "dr3",      "dr4",        "dr5",
    "dr6",     "dr7",       "cr0",          "cr1",     "cr2",     "cr3",     "cr4",      "tssr",     "ldtr",     "tr",       "tr3",      "tr4",        "tr5",
    "tr6",     "tr7",       "status_flags", "df_flag", "gs_base", "fs_base", "g0",       "g1",       "g2",       "g3",       "g4",       "g5",         "g6",
    "g7",      "g8",        "g9",           "g10",     "g11",     "g12",     "g13",      "g14",      "g15"};
} // namespace champsim

// instruction format
constexpr std::size_t NUM_INSTR_DESTINATIONS_SPARC = 4;
constexpr std::size_t NUM_INSTR_DESTINATIONS = 2;
constexpr std::size_t NUM_INSTR_SOURCES = 4;

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): These classes are deliberately trivial
struct input_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip;

  // branch info
  unsigned char is_branch;
  unsigned char branch_taken;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES];           // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES];           // input memory
};

struct cloudsuite_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip;

  // branch info
  unsigned char is_branch;
  unsigned char branch_taken;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS_SPARC]; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES];                 // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS_SPARC]; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES];                 // input memory

  unsigned char asid[2];
};
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

#endif
