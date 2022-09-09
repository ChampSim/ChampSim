#include <cstdio>
#include <deque>
#include <memory>
#include <string>
#include <variant>

#ifdef __GNUG__
#include <ext/stdio_filebuf.h>
#endif

namespace detail
{
void pclose_file(FILE* f);
}

#include "instruction.h"

class tracereader
{
public:
  const std::string trace_string;
  tracereader(uint16_t asid, std::string _ts) : trace_string(_ts), asid(asid) {}
  virtual ~tracereader() = default;

  virtual ooo_model_instr operator()() = 0;
  bool eof() const;

protected:
  static FILE* get_fptr(std::string fname);

  std::unique_ptr<FILE, decltype(&detail::pclose_file)> fp{get_fptr(trace_string), &detail::pclose_file};
#ifdef __GNUG__
  __gnu_cxx::stdio_filebuf<char> filebuf{fp.get(), std::ios::in};
#endif

  uint16_t asid;
  bool eof_ = false;

  constexpr static std::size_t buffer_size = 128;
  constexpr static std::size_t refresh_thresh = 1;
  std::deque<ooo_model_instr> instr_buffer;

  template <typename T>
  void refresh_buffer();

  template <typename T>
  ooo_model_instr impl_get();
};

std::unique_ptr<tracereader> get_tracereader(std::string fname, uint16_t cpu, bool is_cloudsuite);
