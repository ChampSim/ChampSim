#include "tracereader.h"

#include <algorithm>
#include <cstring>

void detail::pclose_file(FILE *f)
{
    pclose(f);
}

FILE* tracereader::get_fptr(std::string fname)
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

template<typename T>
void tracereader::refresh_buffer()
{
    std::array<T, buffer_size - refresh_thresh> trace_read_buf;
    std::array<char, std::size(trace_read_buf) * sizeof(T)> raw_buf;
    std::size_t bytes_read;

    // Read from trace file
#ifdef __GNUG__
    trace_file.read(std::data(raw_buf), std::size(raw_buf));
    bytes_read = trace_file.gcount();
    eof_ = trace_file.eof();
#else
    bytes_read = fread(std::data(raw_buf), sizeof(char), std::size(raw_buf), fp);
    eof_ = (bytes_left > 0);
#endif

    // Transform bytes into trace format instructions
    std::memcpy(std::data(trace_read_buf), std::data(raw_buf), bytes_read);

    // Inflate trace format into core model instructions
    auto cpu = this->cpu;
    auto begin = std::begin(trace_read_buf);
    auto end = std::next(begin, bytes_read/sizeof(T));
    std::transform(begin, end, std::back_inserter(instr_buffer), [cpu](T t){ return ooo_model_instr{cpu, t}; });

    // Set branch targets
    for (auto it = std::next(std::begin(instr_buffer)); it != std::end(instr_buffer); ++it)
        std::prev(it)->branch_target = it->ip;
}

bool tracereader::eof() const
{
    return eof_ && std::size(instr_buffer) <= refresh_thresh;
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

template <typename T>
class bulk_tracereader : public tracereader
{
    public:
        using tracereader::tracereader;

    ooo_model_instr get()
    {
        return impl_get<T>();
    }
};

std::unique_ptr<tracereader> get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
    if (is_cloudsuite)
        return std::make_unique<bulk_tracereader<cloudsuite_instr>>(cpu, fname);
    else
        return std::make_unique<bulk_tracereader<input_instr>>(cpu, fname);
}

