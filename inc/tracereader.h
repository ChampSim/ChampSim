#include "instruction.h"

#include <cstdio>
#include <string>

class tracereader
{
    protected:
        FILE *trace_file = NULL;
        std::string cmd_fmtstr;
        std::string decomp_program;
        std::string trace_string;

    public:
        tracereader(const tracereader &other) = delete;
        tracereader(std::string _ts);
        ~tracereader();
        void open(std::string trace_string);
        void close();
        virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

