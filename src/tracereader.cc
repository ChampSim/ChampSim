#include "tracereader.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <functional>
#include <fstream>

std::istream& operator>>(std::istream& istrm, input_instr &instr)
{
    return istrm.read(reinterpret_cast<char*>(&instr), sizeof(input_instr));
}

std::istream& operator>>(std::istream& istrm, cloudsuite_instr &instr)
{
    return istrm.read(reinterpret_cast<char*>(&instr), sizeof(cloudsuite_instr));
}

tracereader::tracereader(uint8_t cpu, std::string _ts) :
    fp(get_fptr(_ts)),
#ifdef __GNUG__
    filebuf(fp, std::ios::in),
#endif
    cpu(cpu), trace_string(_ts)
{
    if (!test_file(trace_string))
    {
        std::cerr << "TRACE FILE NOT FOUND" << std::endl;
        assert(0);
    }
}

tracereader::~tracereader()
{
    close();
}

bool tracereader::test_file(std::string fname) const
{
    if (fname.substr(0,4) == "http")
    {
        char testfile_command[4096];
        sprintf(testfile_command, "wget -q --spider %s", fname.c_str());
        FILE *testfile = popen(testfile_command, "r");
        return pclose(testfile);
    }
    else
    {
        std::ifstream testfile(fname);
        bool result = testfile.good();
        if (result)
            testfile.close();
        return result;
    }
    return false;
}

FILE* tracereader::get_fptr(std::string fname) const
{
    std::string cmd_fmtstr = "%1$s %2$s";
    if (fname.substr(0,4) == "http")
        cmd_fmtstr = "wget -qO- -o /dev/null %2$s | %1$s";

    std::string decomp_program = "cat";
    if (fname.back() == 'z')
    {
        std::string last_dot = fname.substr(fname.find_last_of("."));
        if (last_dot[1] == 'g') // gzip format
            decomp_program = "gzip -dc";
        else if (last_dot[1] == 'x') // xz
            decomp_program = "xz -dc";
    }

    char gunzip_command[4096];
    sprintf(gunzip_command, cmd_fmtstr.c_str(), decomp_program.c_str(), fname.c_str());
    return popen(gunzip_command, "r");
}

void tracereader::open(std::string trace_string)
{
    if (!test_file(trace_string))
    {
        std::cerr << "TRACE FILE NOT FOUND" << std::endl;
        assert(0);
    }

    bool fail = false;
    fp = get_fptr(trace_string);
    fail = (fp == NULL);
#ifdef __GNUG__
    filebuf = __gnu_cxx::stdio_filebuf<char>{fp, std::ios::in};
#endif

    if (fail)
    {
        std::cerr << std::endl << "*** CANNOT OPEN TRACE FILE: " << trace_string << " ***" << std::endl;
        assert(0);
    }
}

void tracereader::close()
{
    if (fp != NULL)
        pclose(fp);
}

template<typename T>
ooo_model_instr tracereader::read_single_instr()
{
    T trace_read_instr;
    bool need_reopen = false;

#ifdef __GNUG__
    trace_file >> trace_read_instr;
    need_reopen = trace_file.fail();
#else
    need_reopen = !fread(&trace_read_instr, sizeof(T), 1, fp);
#endif

    if (need_reopen)
    {
        // reached end of file for this trace
        std::cout << "*** Reached end of trace: " << trace_string << std::endl;

        // close the trace file and re-open it
        close();
        open(trace_string);

#ifdef __GNUG__
        trace_file >> trace_read_instr;
#else
        auto read = fread(&trace_read_instr, sizeof(T), 1, fp);
        assert(read > 0);
#endif
    }

    // copy the instruction into the performance model's instruction format
    return ooo_model_instr{cpu, trace_read_instr};
}

template <typename T>
ooo_model_instr tracereader::impl_get()
{
    if (instr_buffer.occupancy() <= 1)
    {
        std::generate_n(std::back_inserter(instr_buffer), std::size(instr_buffer)-instr_buffer.occupancy(), std::bind(&tracereader::read_single_instr<T>, this));
        for (auto it = std::next(std::begin(instr_buffer)); it != std::end(instr_buffer); ++it)
            std::prev(it)->branch_target = it->ip; // set branch targets
    }

    auto retval = instr_buffer.front();
    instr_buffer.pop_front();
    return retval;
}

class cloudsuite_tracereader : public tracereader
{
    public:
        using tracereader::tracereader;

    ooo_model_instr get()
    {
        return impl_get<cloudsuite_instr>();
    }
};

class input_tracereader : public tracereader
{
    public:
        using tracereader::tracereader;

    ooo_model_instr get()
    {
        return impl_get<input_instr>();
    }
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
    if (is_cloudsuite)
        return new cloudsuite_tracereader(cpu, fname);
    else
        return new input_tracereader(cpu, fname);
}

