#include "instruction.h"

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

class tracereader
{
    public:
        tracereader(uint8_t cpu, std::string _ts) : trace_string(_ts), cpu(cpu)  { }

    protected:
        std::string trace_string;
        static FILE* get_fptr(std::string fname);

        std::unique_ptr<FILE, decltype(&detail::pclose_file)> fp{get_fptr(trace_string), &detail::pclose_file};
#ifdef __GNUG__
        __gnu_cxx::stdio_filebuf<char> filebuf{fp.get(), std::ios::in};
#endif

        uint8_t cpu;
        bool eof_ = false;

        constexpr static std::size_t buffer_size = 128;
        constexpr static std::size_t refresh_thresh = 1;
        std::deque<ooo_model_instr> instr_buffer;

        template<typename T>
        void refresh_buffer();

        template <typename T>
        ooo_model_instr impl_get();
};

template <typename T>
class bulk_tracereader : public tracereader
{
    public:
        using tracereader::tracereader;
        friend class get_instr;
        friend class get_eof;
        friend class get_trace_string;
};

using supported_tracereader = std::variant<bulk_tracereader<input_instr>, bulk_tracereader<cloudsuite_instr>>;

struct get_instr
{
    ooo_model_instr operator()(bulk_tracereader<input_instr> &tr);
    ooo_model_instr operator()(bulk_tracereader<cloudsuite_instr> &tr);
};

struct get_eof
{
    bool operator()(const bulk_tracereader<input_instr> &tr);
    bool operator()(const bulk_tracereader<cloudsuite_instr> &tr);
};

struct get_trace_string
{
    std::string operator()(const bulk_tracereader<input_instr> &tr);
    std::string operator()(const bulk_tracereader<cloudsuite_instr> &tr);
};

supported_tracereader get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

