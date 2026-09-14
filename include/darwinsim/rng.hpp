#pragma once

#include <cstdint>

namespace darwinsim {

enum class RandomDomain : std::uint64_t {
    Initialization = 1,
    Decision = 2,
    Movement = 3,
    Combat = 4,
    MateChoice = 5,
    Crossover = 6,
    Mutation = 7,
    ResourceSpawn = 8,
    ResolutionOrder = 9,
    Speciation = 10,
    Terrain = 11
};

class DeterministicRng {
public:
    static std::uint64_t hash(std::uint64_t seed,
                                std::uint64_t entity,
                                std::uint64_t tick,
                                RandomDomain domain,
                                std::uint64_t sample = 0);

      static double uniform01(std::uint64_t seed,
                              std::uint64_t entity,
                              std::uint64_t tick,
                              RandomDomain domain,
                              std::uint64_t sample = 0);

      static double uniform(std::uint64_t seed,
                            std::uint64_t entity,
                            std::uint64_t tick,
                            RandomDomain domain,
                            double low,
                            double high,
                            std::uint64_t sample = 0);

      static double normal(std::uint64_t seed,
                           std::uint64_t entity,
                           std::uint64_t tick,
                           RandomDomain domain,
                           double mean,
                           double sigma,
                           std::uint64_t sample = 0);
};

}    // namespace darwinsim
