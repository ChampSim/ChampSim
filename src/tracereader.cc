#include "tracereader.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <functional>
#include <fstream>

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
void tracereader::refresh_buffer()
{
    std::array<T, buffer_size - refresh_thresh> trace_read_buf;
    std::array<char, std::size(trace_read_buf) * sizeof(T)> raw_buf;
    bool need_reopen = false;
    std::size_t bytes_left = std::size(raw_buf);

    // Attempt to fill the buffer from an open trace
#ifdef __GNUG__
    trace_file.read(std::data(raw_buf), std::size(raw_buf));
    bytes_left -= trace_file.gcount();
    need_reopen = trace_file.eof();
#else
    bytes_left -= fread(std::data(raw_buf), sizeof(char), std::size(raw_buf), fp);
    need_reopen = (bytes_left > 0);
#endif

    // If there was an error, assume it is due to EOF
    if (need_reopen)
    {
        std::cout << "*** Reached end of trace: " << trace_string << std::endl;

        // Close the trace file and re-open it
        close();
        open(trace_string);

        // Attempt to fill the buffer from the reopened trace
        auto startpos = std::next(std::data(raw_buf), std::size(raw_buf) - bytes_left);
#ifdef __GNUG__
        trace_file.read(startpos, bytes_left);
        bytes_left -= trace_file.gcount();
#else
        bytes_left -= fread(startpos, sizeof(char), bytes_left, fp);
#endif
    }

    assert(bytes_left == 0);

    // Transform bytes into trace format instructions
    std::memcpy(std::data(trace_read_buf), std::data(raw_buf), std::size(raw_buf));

    // Inflate trace format into core model instructions
    auto cpu = this->cpu;
    std::transform(std::begin(trace_read_buf), std::end(trace_read_buf), std::back_inserter(instr_buffer), [cpu](T t){ return ooo_model_instr{cpu, t}; });

    // Set branch targets
    for (auto it = std::next(std::begin(instr_buffer)); it != std::end(instr_buffer); ++it)
        std::prev(it)->branch_target = it->ip;
}

template <typename T>
ooo_model_instr tracereader::impl_get()
{
    if (std::size(instr_buffer) <= refresh_thresh)
        refresh_buffer<T>();

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

