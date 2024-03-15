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

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <fstream>
#include <iostream>
#include <random>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "../../inc/trace_instruction.h"
#include "pin.H"

using trace_instr_format_t = bytecode_instr;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 instrCount = 0;

std::ofstream outfile;

trace_instr_format_t curr_instr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace", "specify file name for Champsim tracer output");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0", "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "1000000", "How many instructions to trace");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
            << "Specify the output trace file with -o" << std::endl
            << "Specify the number of instructions to skip before tracing with -s" << std::endl
            << "Specify the number of instructions to trace with -t" << std::endl
            << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

// Addresses used for knowing mainModule memory range
ADDRINT mainModuleBase = 0;
ADDRINT mainModuleHigh = 0;
int seenBytecodes = 0;

// Callback for loaded images - to find the base and high of the program, and thus calculate offsets
VOID Image(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img)) {
        mainModuleBase = IMG_LowAddress(img);
        mainModuleHigh = IMG_HighAddress(img);
    }
}

void ResetCurrentInstruction(VOID* ip)
{
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
  curr_instr.load_type = LOAD_TYPE::NOT_LOAD;
}

BOOL ShouldWrite()
{
  ++instrCount;
  return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction()
{
  typename decltype(outfile)::char_type buf[sizeof(trace_instr_format_t)];
  std::memcpy(buf, &curr_instr, sizeof(trace_instr_format_t));
  outfile.write(buf, sizeof(trace_instr_format_t));
}

void BranchOrNot(UINT32 taken)
{
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r)
{
  auto set_end = std::find(begin, end, 0);
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r;
}

// Determine what type of load the memory read is; currently just randomly
// assigns it to be data or bytecode.
void DataOrBytecode(BOOL bytecodeLoad) {
  if (bytecodeLoad) {
    std::cout << "\r" << "Seen bytecodes: " << seenBytecodes++ << std::endl;
    curr_instr.load_type = LOAD_TYPE::BYTECODE;
  } else {
    curr_instr.load_type = LOAD_TYPE::STANDARD_DATA;
  }
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
  // begin each instruction with this function
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

  // instrument branch instructions
  if (INS_IsBranch(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

  if (INS_IsMemoryRead(ins)) {
    static const std::vector<ADDRINT> byteCodeLoadAddresses = {
        0x25a652,0x25a6c8,0x25a782,0x25a88c,0x25a96b,0x25aa46,0x25ab19,0x25ac58,
        0x25adab,0x25aef2,0x25b029,0x25b17e,0x25b269,0x25b30d,0x25b3b7,0x25b461,0x25b52c,
        0x25b5c8,0x25b6b3,0x25b7c2,0x25b917,0x25ba64,0x25bbb6,0x25bd0b,0x25be93,0x25c036,
        0x25c188,0x25c2b0,0x25c377,0x25c4d6,0x25c709,0x25c8d4,0x25cc20,0x25cd0d,0x25ce2c,0x25cef3,
        0x25d051,0x25d1ce,0x25d2f0,0x25d406,0x25d7ba,0x25d962,0x25dab7,0x25dcbd,0x25dec5,0x25dfdc,
        0x25e202,0x25e4a3,0x25e645,0x25e7a0,0x25e88a,0x25ea38,0x25eb4a,0x25ec9d,0x25edea,0x25ef3e,
        0x25f154,0x25f302,0x25f417,0x25f595,0x25f68b,0x25f782,0x25f83b,0x25f959,0x25fc1f,
        0x25fe26,0x25ff27,0x2600ed,0x2601f2,0x2602a0,0x26034a,0x260469,0x260659,0x260740,0x260878,
        0x26099d,0x260ac6,0x260bef,0x260cfe,0x260e1e,0x260f92,0x261104,0x261326,0x261404,
        0x26170e,0x2617f3,0x261942,0x261a69,0x261b65,0x261c47,0x261d32,0x261efc,
        0x26211d,0x2622c5,0x2623e9,0x2624ff,0x262784,0x262a4f,0x262bcf,0x262cda,
        0x262e2d,0x2630df,0x263398,0x2635c4,0x26373e,0x263960,0x263ba2,0x263d32,0x263e46,0x263f7e,0x26401a,
        0x2640d0,0x26418d,0x2641fc,0x26428f,0x2643d2,0x264515,0x264582,0x264615,
        0x264755,0x26487e,0x264912,0x2649d2,0x264aae,0x264bab,0x264cc2,0x264d2f,0x264dc6,
        0x264eda,0x264f47,0x264fde,0x2650ee,0x265209,0x26537a,0x2654ff,0x2655ec,0x2656ca,0x265791,0x265858,
        0x265957,0x265a6c,0x265b9e,0x265d65,0x265f4a,0x266094,0x2661f5,0x26636b,0x2664b9,0x26670a,0x2668f1,
        0x266b05,0x266d18,0x266e49,0x266fad,0x26705f,0x267107,0x267465,0x267663,0x267797,0x26782b,0x267dc4,
        0x267f86,0x268136,0x2683c8,0x2685ef,0x268972,0x268cc7,0x268ece,0x26915d,0x2693b2,0x269668,0x2699a4,
        0x269bb5,0x269f00,0x26a216,0x26a4be,0x26a8ca,0x26ab47,0x26ac58,0x26adaa,0x26af02,0x26af64,0x26b024
      };

    ADDRINT insAddr = INS_Address(ins) - mainModuleBase;
    if (std::find_if(byteCodeLoadAddresses.begin(), byteCodeLoadAddresses.end(),
        [insAddr](ADDRINT byteCodeLoadAddress) {
            return byteCodeLoadAddress == insAddr;
      }) != byteCodeLoadAddresses.end()) {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) DataOrBytecode, IARG_BOOL, true, IARG_END);
      } else {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) DataOrBytecode, IARG_BOOL, false, IARG_END);
      }
  }

  // instrument register reads
  UINT32 readRegCount = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < readRegCount; i++) {
    UINT32 regNum = INS_RegR(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>, IARG_PTR, curr_instr.source_registers, IARG_PTR,
                   curr_instr.source_registers + NUM_INSTR_SOURCES, IARG_UINT32, regNum, IARG_END);
  }

  // instrument register writes
  UINT32 writeRegCount = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < writeRegCount; i++) {
    UINT32 regNum = INS_RegW(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>, IARG_PTR, curr_instr.destination_registers, IARG_PTR,
                   curr_instr.destination_registers + NUM_INSTR_DESTINATIONS, IARG_UINT32, regNum, IARG_END);
  }

  // instrument memory reads and writes
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Iterate over each memory operand of the instruction.
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>, IARG_PTR, curr_instr.source_memory, IARG_PTR,
                     curr_instr.source_memory + NUM_INSTR_SOURCES, IARG_MEMORYOP_EA, memOp, IARG_END);
    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>, IARG_PTR, curr_instr.destination_memory, IARG_PTR,
                     curr_instr.destination_memory + NUM_INSTR_DESTINATIONS, IARG_MEMORYOP_EA, memOp, IARG_END);
  }

  // finalize each instruction with this function
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v) { outfile.close(); }

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid
  if (PIN_Init(argc, argv))
    return Usage();

  outfile.open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
  if (!outfile) {
    std::cout << "Couldn't open output trace file. Exiting." << std::endl;
    exit(1);
  }

  // Fix base adress to enable calculation of offset
  IMG_AddInstrumentFunction(Image, 0);
  
  // Register function to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
