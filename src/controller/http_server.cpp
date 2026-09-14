#include "darwinsim/http_server.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
 #include <iostream>
 #include <sstream>
 #include <string>

 namespace darwinsim {
 namespace beast = boost::beast;
 namespace http = beast::http;
 namespace net = boost::asio;
 using tcp = net::ip::tcp;

 namespace {

 std::string json_escape(const std::string& value) {
     std::string out;
     for (char c : value) {
         if (c == '"' || c == '\\') out.push_back('\\');
         out.push_back(c);
     }
     return out;
 }

 std::size_t animal_limit_from_target(const std::string& target) {
     constexpr std::size_t kDefault = 500;
     constexpr std::size_t kMaximum = 5000;
     const auto pos = target.find("limit=");
     if (pos == std::string::npos) return kDefault;
     try {
         return std::clamp<std::size_t>(std::stoull(target.substr(pos + 6)), 1, kMaximum);
     } catch (...) {
         return kDefault;
     }
 }

 http::response<http::string_body> response(http::status status,
                                            const std::string& body,
                                            unsigned version) {
     http::response<http::string_body> res{status, version};
     res.set(http::field::server, "DarwinSim");
     res.set(http::field::content_type, "application/json");
     res.set(http::field::access_control_allow_origin, "*");
     res.body() = body;
     res.prepare_payload();
     return res;
 }

 }   // namespace

 HttpServer::HttpServer(Simulation& simulation,
                        std::mutex& simulation_mutex,
                        std::atomic<bool>& paused,
                        CheckpointCallback checkpoint_callback,
                        unsigned short port)
     : simulation_(simulation),
       simulation_mutex_(simulation_mutex),
       paused_(paused),
       checkpoint_callback_(std::move(checkpoint_callback)),
       port_(port) {}

 std::string HttpServer::state_json() const {
     std::lock_guard lock(simulation_mutex_);
     const auto s = simulation_.summary();
     std::ostringstream out;
     out << "{"
         << "\"tick\":" << s.tick << ','
         << "\"livingAnimals\":" << s.living_animals << ','
         << "\"totalCreated\":" << s.total_animals_created << ','
         << "\"speciesCount\":" << s.species_count << ','
         << "\"availablePlants\":" << s.available_plants << ','
         << "\"carcasses\":" << s.carcasses << ','
         << "\"births\":" << s.births << ','
         << "\"deaths\":" << s.deaths << ','
         << "\"speciationEvents\":" << s.speciation_events << ','
         << "\"hardwareMode\":\"" << hardware_mode_name(simulation_.config().hardware_mode) << "\","
         << "\"terrainSignature\":\"" << simulation_.world().terrain_signature() << "\","
         << "\"paused\":" << (paused_.load() ? "true" : "false")
         << "}";
     return out.str();
 }

 std::string HttpServer::species_json() const {
     std::lock_guard lock(simulation_mutex_);
     const auto species = simulation_.species_stats();
     std::ostringstream out;
     out << '[';
     bool first = true;
     for (const auto& s : species) {
         if (!first) out << ',';
         first = false;
         out << '{'
             << "\"id\":" << s.id << ','
             << "\"parentSpecies\":" << s.parent_species << ','
             << "\"birthTick\":" << s.birth_tick << ','
             << "\"population\":" << s.population << ','
             << "\"diversity\":" << std::fixed << std::setprecision(5) << s.diversity << ','
             << "\"compute\":" << s.centroid.compute_capacity << ','
             << "\"memory\":" << s.centroid.memory_capacity << ','
             << "\"sensory\":" << s.centroid.sensory_capacity << ','
             << "\"speed\":" << s.centroid.locomotion << ','
             << "\"aggression\":" << s.centroid.aggression << ','
             << "\"plantDigestion\":" << s.centroid.plant_digestion << ','
             << "\"meatDigestion\":" << s.centroid.meat_digestion
             << '}';
     }
    out << ']';
    return out.str();
}

std::string HttpServer::animals_json(std::size_t limit) const {
    std::lock_guard lock(simulation_mutex_);
    std::ostringstream out;
    out << '[';
    bool first = true;
    std::size_t count = 0;
    for (const auto& a : simulation_.animals()) {
        if (!a.state.alive) continue;
        if (count++ >= limit) break;
        if (!first) out << ',';
        first = false;
        out << '{'
            << "\"id\":" << a.id << ','
            << "\"speciesId\":" << a.species_id << ','
            << "\"x\":" << std::fixed << std::setprecision(2) << a.state.position.x << ','
            << "\"y\":" << a.state.position.y << ','
            << "\"energy\":" << a.state.energy << ','
            << "\"health\":" << a.state.health << ','
            << "\"age\":" << a.state.age << ','
            << "\"mass\":" << a.phenotype.mass << ','
            << "\"compute\":" << a.genome.compute_capacity << ','
            << "\"memory\":" << a.genome.memory_capacity << ','
            << "\"sensory\":" << a.genome.sensory_capacity << ','
            << "\"lifespanPotential\":" << a.phenotype.longevity_potential
            << '}';
    }
    out << ']';
    return out.str();
}

std::string HttpServer::terrain_json() const {
    std::lock_guard lock(simulation_mutex_);
    const auto& world = simulation_.world();
    const auto& config = world.config();
    const auto& cells = world.terrain_cells();

    std::ostringstream out;
    out << '{'
        << "\"width\":" << config.width << ','
        << "\"height\":" << config.height << ','
        << "\"cols\":" << std::max<std::uint32_t>(1, config.terrain.cols) << ','
        << "\"rows\":" << std::max<std::uint32_t>(1, config.terrain.rows) << ','
        << "\"signature\":\"" << world.terrain_signature() << "\",";

    out << "\"types\":[";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i != 0) out << ',';
        out << static_cast<unsigned>(cells[i].type);
    }
    out << "],\"regen\":[";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i != 0) out << ',';
        out << std::fixed << std::setprecision(3) << cells[i].plant_regen_multiplier;
    }
    out << "],\"movement\":[";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i != 0) out << ',';
        out << std::fixed << std::setprecision(3) << cells[i].movement_cost_multiplier;
    }
    out << "]}";
    return out.str();
}

void HttpServer::run() {
    try {
         net::io_context ioc{1};
         tcp::acceptor acceptor{ioc, {tcp::v4(), port_}};
         std::cout << "DarwinSim HTTP API listening on 0.0.0.0:" << port_ << std::endl;
         for (;;) {
             tcp::socket socket{ioc};
             acceptor.accept(socket);
             beast::flat_buffer buffer;
             http::request<http::string_body> req;
             http::read(socket, buffer, req);

              const std::string target(req.target());
              http::response<http::string_body> res;
              if (req.method() == http::verb::get && target == "/api/state") {
                  res = response(http::status::ok, state_json(), req.version());
              } else if (req.method() == http::verb::get && target == "/api/species") {
                  res = response(http::status::ok, species_json(), req.version());
              } else if (req.method() == http::verb::get && target.rfind("/api/animals", 0) == 0) {
                  res = response(http::status::ok, animals_json(animal_limit_from_target(target)), req.version());
              } else if (req.method() == http::verb::get && target == "/api/terrain") {
                  res = response(http::status::ok, terrain_json(), req.version());
              } else if (req.method() == http::verb::post && target == "/api/control/pause") {
                  paused_.store(true);
                  res = response(http::status::ok, "{\"ok\":true,\"paused\":true}", req.version());
              } else if (req.method() == http::verb::post && target == "/api/control/resume") {
                  paused_.store(false);
                  res = response(http::status::ok, "{\"ok\":true,\"paused\":false}", req.version());
              } else if (req.method() == http::verb::post && target == "/api/control/checkpoint") {
                  checkpoint_callback_();
                  res = response(http::status::ok, "{\"ok\":true}", req.version());
              } else if (req.method() == http::verb::get && target == "/healthz") {
                  res = response(http::status::ok, "{\"ok\":true}", req.version());
              } else {
                  res = response(http::status::not_found, "{\"error\":\"not found\"}", req.version());
              }

              http::write(socket, res);
              beast::error_code ec;
              socket.shutdown(tcp::socket::shutdown_send, ec);
         }
     } catch (const std::exception& ex) {
         std::cerr << "HTTP server error: " << ex.what() << std::endl;
     }
}

}   // namespace darwinsim
