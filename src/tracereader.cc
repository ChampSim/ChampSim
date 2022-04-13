#include "tracereader.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

tracereader::tracereader(uint8_t cpu, std::string _ts, bool _rt) : cpu(cpu), trace_string(_ts), repeat_trace(_rt)
{
  std::string last_dot = trace_string.substr(trace_string.find_last_of("."));

  if (trace_string.substr(0, 4) == "http") {
    // Check file exists
    char testfile_command[4096];
    sprintf(testfile_command, "wget -q --spider %s", trace_string.c_str());
    FILE* testfile = popen(testfile_command, "r");
    if (pclose(testfile)) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "wget -qO- -o /dev/null %2$s | %1$s -dc";
  } else {
    std::ifstream testfile(trace_string);
    if (!testfile.good()) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "%1$s -dc %2$s";
  }

  if (last_dot[1] == 'g') // gzip format
    decomp_program = "gzip";
  else if (last_dot[1] == 'x') // xz
    decomp_program = "xz";
  else {
    std::cout << "ChampSim does not support traces other than gz or xz compression!" << std::endl;
    assert(0);
  }

  open(trace_string);
}

tracereader::~tracereader() { close(); }

template <typename T>
ooo_model_instr tracereader::read_single_instr()
{
  T trace_read_instr;

  while (!fread(&trace_read_instr, sizeof(T), 1, trace_file)) {
    // reached end of file for this trace
    std::cout << "*** Reached end of trace: " << trace_string;

    // close the trace file
    close();

    if (repeat_trace) {
      // re-open it
      std::cout << " -> re-open it" << std::endl;
      open(trace_string);
    } else {
      // return
      std::cout << " -> return" << std::endl;
      ooo_model_instr nop;
      nop.drained = true;
      return nop;
    }
  }

  // copy the instruction into the performance model's instruction format
  ooo_model_instr retval(cpu, trace_read_instr);
  return retval;
}

void tracereader::open(std::string trace_string)
{
  char gunzip_command[4096];
  sprintf(gunzip_command, cmd_fmtstr.c_str(), decomp_program.c_str(), trace_string.c_str());
  trace_file = popen(gunzip_command, "r");
  if (trace_file == NULL) {
    std::cerr << std::endl << "*** CANNOT OPEN TRACE FILE: " << trace_string << " ***" << std::endl;
    assert(0);
  }
}

void tracereader::close()
{
  if (trace_file != NULL) {
    pclose(trace_file);
  }
}

class cloudsuite_tracereader : public tracereader
{
  std::optional<ooo_model_instr> last_instr;

public:
  cloudsuite_tracereader(uint8_t cpu, std::string _tn, bool _rt) : tracereader(cpu, _tn, _rt) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<cloudsuite_instr>();

    if (!last_instr.has_value())
      last_instr = trace_read_instr;

    last_instr->branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr.value();

    last_instr = trace_read_instr;
    return retval;
  }
};

class input_tracereader : public tracereader
{
  std::optional<ooo_model_instr> last_instr;

public:
  input_tracereader(uint8_t cpu, std::string _tn, bool _rt) : tracereader(cpu, _tn, _rt) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<input_instr>();

    if (!repeat_trace && trace_read_instr.drained)
      return trace_read_instr;

    if (!last_instr.has_value())
      last_instr = trace_read_instr;

    last_instr->branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr.value();

    last_instr = trace_read_instr;
    return retval;
  }
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite, bool repeat_trace)
{
  if (is_cloudsuite) {
    return new cloudsuite_tracereader(cpu, fname, 1);
  } else {
    return new input_tracereader(cpu, fname, repeat_trace);
  }
}
