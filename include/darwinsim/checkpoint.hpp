#pragma once

#include "darwinsim/simulation.hpp"

#include <string>

namespace darwinsim {

void save_checkpoint(const Simulation& simulation, const std::string& path);
void load_checkpoint(Simulation& simulation, const std::string& path);
std::uint64_t state_hash(const Simulation& simulation);

}   // namespace darwinsim
