#include "instruction.h"

#include <cstdio>
#include <string>

#include "circular_buffer.hpp"

class tracereader
{
    protected:
        FILE *trace_file = NULL;
        uint8_t cpu;
        std::string cmd_fmtstr;
        std::string decomp_program;
        std::string trace_string;

        champsim::circular_buffer<ooo_model_instr> instr_buffer{32};

        template <typename T>
        ooo_model_instr impl_get();

        template<typename T>
        ooo_model_instr read_single_instr();

    public:
        tracereader(const tracereader &other) = delete;
        tracereader(uint8_t cpu, std::string _ts);
        ~tracereader();
        void open(std::string trace_string);
        void close();

        virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

