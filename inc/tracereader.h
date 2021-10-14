#include "instruction.h"

#include <cstdio>
#include <deque>
#include <string>

#ifdef __GNUG__
#include <ext/stdio_filebuf.h>
#include <iostream>
#endif

class tracereader
{
    private:
        bool test_file(std::string fname) const;
        FILE* get_fptr(std::string fname) const;

        FILE *fp = NULL;
#ifdef __GNUG__
        __gnu_cxx::stdio_filebuf<char> filebuf;
        std::istream trace_file{&filebuf};
#endif

        uint8_t cpu;
        std::string trace_string;

        constexpr static std::size_t buffer_size = 128;
        constexpr static std::size_t refresh_thresh = 1;
        std::deque<ooo_model_instr> instr_buffer;

        template<typename T>
        void refresh_buffer();

    protected:
        template <typename T>
        ooo_model_instr impl_get();

    public:
        tracereader(const tracereader &other) = delete;
        tracereader(uint8_t cpu, std::string _ts);
        ~tracereader();
        void open(std::string trace_string);
        void close();

        virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

