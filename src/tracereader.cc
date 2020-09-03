#include "tracereader.h"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>

tracereader::tracereader(std::string _ts) : trace_string(_ts)
{
    std::string last_dot = trace_string.substr(trace_string.find_last_of("."));

    if (trace_string.substr(0,4) == "http")
    {
        // Check file exists
        char testfile_command[4096];
        sprintf(testfile_command, "wget -q --spider %s", trace_string.c_str());
        FILE *testfile = popen(testfile_command, "r");
        if (pclose(testfile))
        {
            std::cerr << "TRACE FILE NOT FOUND" << std::endl;
            assert(0);
        }
        cmd_fmtstr = "wget -qO- %2$s | %1$s -dc";
    }
    else
    {
        std::ifstream testfile(trace_string);
        if (!testfile.good())
        {
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

tracereader::~tracereader()
{
    close();
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
    if (trace_file != NULL)
    {
        pclose(trace_file);
    }
}

class cloudsuite_tracereader : public tracereader
{
    uint8_t cpu;

    public:
    cloudsuite_tracereader(uint8_t cpu, std::string _tn) : tracereader(_tn), cpu(cpu) {}

    ooo_model_instr get()
    {
        cloudsuite_instr current_cloudsuite_instr;
        while (!fread(&current_cloudsuite_instr, sizeof(cloudsuite_instr), 1, trace_file))
        {
            // reached end of file for this trace
            std::cout << "*** Reached end of trace: " << trace_string << std::endl;

            // close the trace file and re-open it
            close();
            open(trace_string);
        }

        // copy the instruction into the performance model's instruction format
        return ooo_model_instr(cpu, current_cloudsuite_instr);
    }
};

class input_tracereader : public tracereader
{
    uint8_t cpu;

    input_instr last_instr;
    bool initialized;

    public:
    input_tracereader(uint8_t cpu, std::string _tn) : tracereader(_tn), cpu(cpu) {}

    ooo_model_instr get()
    {
        input_instr trace_read_instr;

        while (!fread(&trace_read_instr, sizeof(input_instr), 1, trace_file))
        {
            // reached end of file for this trace
            std::cout << "*** Reached end of trace: " << trace_string << std::endl;

            // close the trace file and re-open it
            close();
            open(trace_string);
        }

        if (!initialized)
        {
            last_instr = trace_read_instr;
            initialized = true;
        }

        // copy the instruction into the performance model's instruction format
        ooo_model_instr retval(cpu, last_instr);

        retval.branch_target = trace_read_instr.ip;
        last_instr = trace_read_instr;
        return retval;
    }
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
    if (is_cloudsuite)
    {
        return new cloudsuite_tracereader(cpu, fname);
    }
    else
    {
        return new input_tracereader(cpu, fname);
    }
}

