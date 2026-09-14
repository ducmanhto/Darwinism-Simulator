#pragma once

#include "darwinsim/simulation.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

namespace darwinsim {

class HttpServer {
public:
    using CheckpointCallback = std::function<void()>;

      HttpServer(Simulation& simulation,
                 std::mutex& simulation_mutex,
                 std::atomic<bool>& paused,
                 CheckpointCallback checkpoint_callback,
                 unsigned short port = 8080);

      void run();

private:
    std::string state_json() const;
    std::string species_json() const;
    std::string animals_json(std::size_t limit) const;
    std::string terrain_json() const;

      Simulation& simulation_;
      std::mutex& simulation_mutex_;
      std::atomic<bool>& paused_;
      CheckpointCallback checkpoint_callback_;
      unsigned short port_;
};

}    // namespace darwinsim
