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

        template<typename T>
        ooo_model_instr read_single_instr();

        virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint16_t asid, bool is_cloudsuite);

