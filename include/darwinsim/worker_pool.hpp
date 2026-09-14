#pragma once

#include "darwinsim/decision.hpp"
#include "darwinsim/simulation.hpp"
#include "darwinsim.grpc.pb.h"

#include <memory>
#include <string>
#include <vector>

namespace darwinsim {

class WorkerPool {
public:
    WorkerPool(std::vector<std::string> endpoints, const SimulationConfig& config);

      bool distributed() const { return !workers_.empty(); }
      std::vector<Action> decide(const std::vector<DecisionInput>& inputs, std::uint64_t tick);

private:
    struct WorkerClient {
         std::string endpoint;
         std::unique_ptr<rpc::DecisionWorker::Stub> stub;
    };

      std::vector<WorkerClient> workers_;
      SimulationConfig config_;
};

std::vector<std::string> worker_endpoints_from_env();

}    // namespace darwinsim
