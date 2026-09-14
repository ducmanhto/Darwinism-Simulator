#include "darwinsim/worker_pool.hpp"

#include "darwinsim/rpc_conversion.hpp"

#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

namespace darwinsim {

std::vector<std::string> worker_endpoints_from_env() {
    if (const char* explicit_endpoints = std::getenv("DARWINSIM_WORKERS")) {
        std::vector<std::string> endpoints;
        std::stringstream ss(explicit_endpoints);
        std::string token;
        while (std::getline(ss, token, ',')) if (!token.empty()) endpoints.push_back(token);
        return endpoints;
    }

     const char* count_env = std::getenv("DARWINSIM_WORKER_COUNT");
     const int count = count_env ? std::max(0, std::atoi(count_env)) : 0;
     const char* service_env = std::getenv("DARWINSIM_WORKER_SERVICE");
     const std::string service = service_env ? service_env : "darwinsim-worker";
     const char* port_env = std::getenv("DARWINSIM_GRPC_PORT");
     const std::string port = port_env ? port_env : "50051";

     std::vector<std::string> endpoints;
     for (int i = 0; i < count; ++i) {
         endpoints.push_back(service + "-" + std::to_string(i) + "." + service + ":" + port);
     }
     return endpoints;
}

WorkerPool::WorkerPool(std::vector<std::string> endpoints, const SimulationConfig& config)
    : config_(config) {
    for (auto& endpoint : endpoints) {
        WorkerClient worker;
        worker.endpoint = endpoint;
        worker.stub = rpc::DecisionWorker::NewStub(
            grpc::CreateChannel(endpoint, grpc::InsecureChannelCredentials()));
        workers_.push_back(std::move(worker));
    }
}

std::vector<Action> WorkerPool::decide(const std::vector<DecisionInput>& inputs, std::uint64_t tick) {
    if (workers_.empty()) {
        std::vector<Action> local;
        local.reserve(inputs.size());
        for (const auto& input : inputs) local.push_back(DecisionEngine::decide(input));
        return local;
    }

     std::vector<rpc::DecisionBatch> batches(workers_.size());
     for (auto& batch : batches) {
         batch.set_tick(tick);
         batch.set_hardware_coupled(config_.hardware_mode == HardwareMode::Coupled);
         batch.set_deterministic_compute_credits(config_.deterministic_compute_credits / workers_.size());
         batch.set_credits_per_core(config_.credits_per_core);
     }

     for (std::size_t i = 0; i < inputs.size(); ++i) {
         *batches[i % workers_.size()].add_inputs() = to_rpc(inputs[i]);
     }

     std::vector<std::future<std::vector<Action>>> futures;
     futures.reserve(workers_.size());
     for (std::size_t i = 0; i < workers_.size(); ++i) {
         futures.push_back(std::async(std::launch::async, [&, i]() {
             rpc::ActionBatch response;
             grpc::ClientContext context;
             const auto status = workers_[i].stub->Decide(&context, batches[i], &response);
             if (!status.ok()) {
                 throw std::runtime_error("worker " + workers_[i].endpoint + " failed: " + status.error_message());
             }
             std::vector<Action> actions;
             actions.reserve(static_cast<std::size_t>(response.actions_size()));
             for (const auto& a : response.actions()) actions.push_back(from_rpc(a));
             return actions;
         }));
     }

     std::vector<Action> out;
     out.reserve(inputs.size());
     for (auto& future : futures) {
         auto batch_actions = future.get();
         out.insert(out.end(), batch_actions.begin(), batch_actions.end());
     }
     return out;
}

}   // namespace darwinsim
