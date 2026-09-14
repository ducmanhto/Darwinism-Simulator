#include "darwinsim/decision.hpp"
#include "darwinsim/hardware.hpp"
#include "darwinsim/rpc_conversion.hpp"
#include "darwinsim.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <algorithm>
#include <cstdlib>
 #include <iostream>
 #include <memory>
 #include <string>
 #include <unistd.h>

 namespace {

 class DecisionWorkerService final : public darwinsim::rpc::DecisionWorker::Service {
 public:
     grpc::Status Decide(grpc::ServerContext*,
                         const darwinsim::rpc::DecisionBatch* request,
                         darwinsim::rpc::ActionBatch* response) override {
         darwinsim::HardwareCapacity capacity;
         if (request->hardware_coupled()) {
             capacity = darwinsim::read_cgroup_capacity(request->credits_per_core());
         } else {
             capacity = darwinsim::read_cgroup_capacity(request->credits_per_core());
             capacity.compute_credits_per_tick = request->deterministic_compute_credits();
         }

          std::uint64_t requested = 0;
          for (const auto& input : request->inputs()) requested += input.requested_compute();
          const double scale = requested == 0 ? 1.0 : std::min(
              1.0, static_cast<double>(capacity.compute_credits_per_tick) / static_cast<double>(requested));

          response->set_tick(request->tick());
          response->set_cpu_cores(capacity.cpu_cores);
          response->set_memory_bytes(capacity.memory_bytes);
          response->set_available_compute_credits(capacity.compute_credits_per_tick);

          for (const auto& input : request->inputs()) {
              const auto granted = static_cast<std::uint32_t>(
                  std::max(32.0, static_cast<double>(input.requested_compute()) * scale));
              auto local = darwinsim::from_rpc(input, granted);
              *response->add_actions() = darwinsim::to_rpc(darwinsim::DecisionEngine::decide(local));
          }
          return grpc::Status::OK;
      }

      grpc::Status Health(grpc::ServerContext*,
                          const darwinsim::rpc::Empty*,
                          darwinsim::rpc::HealthReply* response) override {
          char hostname[256]{};
          gethostname(hostname, sizeof(hostname) - 1);
          response->set_ok(true);
          response->set_hostname(hostname);
          return grpc::Status::OK;
      }
};

}    // namespace

int main() {
    const char* port_env = std::getenv("DARWINSIM_GRPC_PORT");
    const std::string address =
        std::string("0.0.0.0:") + (port_env ? port_env : "50051");

    DecisionWorkerService service;

    // Enable the standard grpc.health.v1.Health service.
    grpc::EnableDefaultHealthCheckService(true);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        address,
        grpc::InsecureServerCredentials());

    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(
        builder.BuildAndStart());

    if (!server) {
        std::cerr
            << "Failed to start DarwinSim decision worker on "
            << address << std::endl;
        return 1;
    }

    // Advertise the actual DecisionWorker RPC service as healthy.
    if (auto* health = server->GetHealthCheckService()) {
        health->SetServingStatus(
            "darwinsim.rpc.DecisionWorker",
            true);

        // Overall server health. Kubernetes can query this if no
        // service name is supplied.
        health->SetServingStatus("", true);
    }

    std::cout
        << "DarwinSim decision worker listening on "
        << address << std::endl;

    server->Wait();
    return 0;
}