#pragma once

#include <cstdint>
#include <string>

namespace darwinsim {

enum class HardwareMode : std::uint8_t { Deterministic = 0, Coupled = 1 };

struct HardwareCapacity {
    double cpu_cores{1.0};
    std::uint64_t memory_bytes{512ULL * 1024ULL * 1024ULL};
    std::uint64_t compute_credits_per_tick{250000};
};

HardwareCapacity read_cgroup_capacity(std::uint64_t credits_per_core = 250000);
std::string hardware_mode_name(HardwareMode mode);

}    // namespace darwinsim
