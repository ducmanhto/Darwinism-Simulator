#pragma once

#include "darwinsim/decision.hpp"
#include "darwinsim.grpc.pb.h"

namespace darwinsim {

rpc::DecisionInput to_rpc(const DecisionInput& input);
DecisionInput from_rpc(const rpc::DecisionInput& input, std::uint32_t granted_compute);
rpc::Action to_rpc(const Action& action);
Action from_rpc(const rpc::Action& action);

}    // namespace darwinsim
