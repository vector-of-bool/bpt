#ifndef DDS_PROC_HPP_INCLUDED
#define DDS_PROC_HPP_INCLUDED

#include <string>
#include <vector>

namespace dds {

struct proc_result {
    int         signal = 0;
    int         retc   = 0;
    std::string output;

    bool okay() const noexcept { return retc == 0 && signal == 0; }
};

proc_result run_proc(const std::vector<std::string>& args);

}  // namespace dds

#endif  // DDS_PROC_HPP_INCLUDED