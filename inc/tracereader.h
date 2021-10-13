#include "instruction.h"

#include <cstdio>
#include <string>

#ifdef __GNUG__
#include <ext/stdio_filebuf.h>
#include <iostream>
#endif

#include "circular_buffer.hpp"

class tracereader
{
    private:
        bool test_file(std::string fname) const;
        std::string popen_cmd(std::string fname) const;

        FILE *fp = NULL;
#ifdef __GNUG__
        __gnu_cxx::stdio_filebuf<char> filebuf;
        std::istream trace_file{&filebuf};
#endif

        uint8_t cpu;
        std::string trace_string;

        champsim::circular_buffer<ooo_model_instr> instr_buffer{32};

        template<typename T>
        ooo_model_instr read_single_instr();

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

