#include "darwinsim/checkpoint.hpp"
#include "darwinsim/config.hpp"
#include "darwinsim/http_server.hpp"
#include "darwinsim/persistence.hpp"
#include "darwinsim/simulation.hpp"
#include "darwinsim/worker_pool.hpp"

#include <atomic>
#include <chrono>
 #include <cstdlib>
 #include <filesystem>
 #include <iostream>
 #include <mutex>
 #include <string>
 #include <thread>

 namespace {

 std::string env_or(const char* name, const std::string& fallback) {
     if (const char* value = std::getenv(name)) return value;
     return fallback;
 }

 }   // namespace

 int main() {
     try {
         const std::string config_path = env_or("DARWINSIM_CONFIG", "/etc/darwinsim/default.yaml");
         const std::string data_dir = env_or("DARWINSIM_DATA_DIR", "/var/lib/darwinsim");
         auto config = darwinsim::load_config(config_path);
         if (const char* mode = std::getenv("DARWINSIM_HARDWARE_MODE")) {
              config.hardware_mode = std::string(mode) == "coupled"
                  ? darwinsim::HardwareMode::Coupled
                  : darwinsim::HardwareMode::Deterministic;
         }
         darwinsim::Simulation simulation(config);
         simulation.initialize();
         std::filesystem::create_directories(data_dir);

         const std::string checkpoint_path = data_dir + "/latest.chk";
         if (std::filesystem::exists(checkpoint_path)) {
             try {
                 darwinsim::load_checkpoint(simulation, checkpoint_path);
                 std::cout << "Restored " << checkpoint_path << " at tick " << simulation.tick() << std::endl;
             } catch (const std::exception& ex) {
                 std::cerr << "Checkpoint restore skipped: " << ex.what() << std::endl;
             }
         }

         darwinsim::PersistenceStore persistence(data_dir + "/darwinsim.sqlite");
         darwinsim::WorkerPool workers(darwinsim::worker_endpoints_from_env(), config);
         std::cout << (workers.distributed() ? "Distributed decision workers enabled" : "No workers configured; using local decisions") << std::endl;

         std::mutex simulation_mutex;
         std::atomic<bool> paused{false};
         std::atomic<bool> running{true};

         auto checkpoint = [&]() {
             std::lock_guard lock(simulation_mutex);
             darwinsim::save_checkpoint(simulation, checkpoint_path);
             persistence.record(simulation);
             std::cout << "checkpoint tick=" << simulation.tick() << " path=" << checkpoint_path << std::endl;
         };

         const unsigned short port = static_cast<unsigned short>(std::stoi(env_or("DARWINSIM_HTTP_PORT", "8080")));
         darwinsim::HttpServer http(simulation, simulation_mutex, paused, checkpoint, port);
         std::thread http_thread([&] { http.run(); });
         http_thread.detach();

         const int sleep_ms = std::stoi(env_or("DARWINSIM_TICK_SLEEP_MS", "5"));
         std::vector<darwinsim::DecisionInput> pending_inputs;
         std::uint64_t pending_tick = 0;
         bool have_pending_tick = false;

         while (running.load()) {
             if (paused.load()) {
                 std::this_thread::sleep_for(std::chrono::milliseconds(100));
                 continue;
             }

             try {
                 if (!have_pending_tick) {
                     std::lock_guard lock(simulation_mutex);
                     pending_tick = simulation.tick();
                     // In distributed mode each worker performs its own capacity allocation.
                     // Give prepare_tick a huge logical capacity so it does not pre-throttle.
                     pending_inputs = simulation.prepare_tick(workers.distributed() ? UINT64_MAX / 4 : 0);
                     have_pending_tick = true;
                 }

                 auto actions = workers.decide(pending_inputs, pending_tick);
                 {
                     std::lock_guard lock(simulation_mutex);
                     simulation.apply_actions(std::move(actions));
                     have_pending_tick = false;
                     pending_inputs.clear();
                     if (config.ticks_per_checkpoint > 0 && simulation.tick() % config.ticks_per_checkpoint == 0) {
                         darwinsim::save_checkpoint(simulation, checkpoint_path);
                     }
                    if (simulation.tick() % 100 == 0) {
                        persistence.record(simulation);
                        const auto s = simulation.summary();
                        std::cout << "tick=" << s.tick << " living=" << s.living_animals
                                  << " species=" << s.species_count << " births=" << s.births
                                  << " deaths=" << s.deaths << std::endl;
                    }
                }
            } catch (const std::exception& ex) {
                // The prepared inputs are intentionally retained. A restarted worker receives
                // exactly the same tick, so failure does not silently change evolution.
                std::cerr << "tick retry: " << ex.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }

            if (sleep_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "darwinsim-controller: " << ex.what() << std::endl;
        return 1;
    }
}
