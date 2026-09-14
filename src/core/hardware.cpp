#include "darwinsim/hardware.hpp"

#include <fstream>
#include <sstream>
#include <thread>

namespace darwinsim {
namespace {

std::string read_first_line(const char* path) {
    std::ifstream input(path);
    std::string line;
    if (input) std::getline(input, line);
    return line;
}

}   // namespace

HardwareCapacity read_cgroup_capacity(std::uint64_t credits_per_core) {
    HardwareCapacity capacity;
    capacity.cpu_cores = static_cast<double>(std::max(1U, std::thread::hardware_concurrency()));

     // cgroup v2: cpu.max is "quota period" or "max period".
     const std::string cpu_line = read_first_line("/sys/fs/cgroup/cpu.max");
     if (!cpu_line.empty()) {
         std::istringstream ss(cpu_line);
         std::string quota_token;
         std::uint64_t period = 0;
         ss >> quota_token >> period;
         if (quota_token != "max" && period > 0) {
             try {
                 const auto quota = static_cast<std::uint64_t>(std::stoull(quota_token));
                 capacity.cpu_cores = static_cast<double>(quota) / static_cast<double>(period);
             } catch (...) {
                 // Keep the hardware_concurrency fallback.
             }
         }
     }

     const std::string memory_line = read_first_line("/sys/fs/cgroup/memory.max");
     if (!memory_line.empty() && memory_line != "max") {
         try {
             capacity.memory_bytes = std::stoull(memory_line);
         } catch (...) {
         }
     }

     capacity.compute_credits_per_tick = static_cast<std::uint64_t>(
         std::max(1.0, capacity.cpu_cores) * static_cast<double>(credits_per_core));
     return capacity;
}

std::string hardware_mode_name(HardwareMode mode) {
    return mode == HardwareMode::Coupled ? "coupled" : "deterministic";
}

}   // namespace darwinsim
