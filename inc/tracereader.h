#include "instruction.h"

#include <cstdio>
#include <deque>
#include <memory>
#include <string>

#ifdef __GNUG__
#include <ext/stdio_filebuf.h>
#include <iostream>
#endif

namespace detail
{
    void pclose_file(FILE* f);
}

class tracereader
{
    private:
        static FILE* get_fptr(std::string fname);

        std::unique_ptr<FILE, decltype(&detail::pclose_file)> fp;
#ifdef __GNUG__
        __gnu_cxx::stdio_filebuf<char> filebuf;
        std::istream trace_file{&filebuf};
#endif

        uint8_t cpu;
        bool eof_ = false;

        constexpr static std::size_t buffer_size = 128;
        constexpr static std::size_t refresh_thresh = 1;
        std::deque<ooo_model_instr> instr_buffer;

        template<typename T>
        void refresh_buffer();

    protected:
        template <typename T>
        ooo_model_instr impl_get();

    public:
        const std::string trace_string;

        tracereader(uint8_t cpu, std::string _ts);
        bool eof() const;

        virtual ooo_model_instr get() = 0;
};

std::unique_ptr<tracereader> get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

