#pragma once

#include "darwinsim/simulation.hpp"

#include <string>

namespace darwinsim {

SimulationConfig load_config(const std::string& path);

}   // namespace darwinsim
