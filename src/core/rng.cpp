#include "darwinsim/rng.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace darwinsim {
namespace {

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
     x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
     x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
     return x ^ (x >> 31U);
}

}   // namespace

std::uint64_t DeterministicRng::hash(std::uint64_t seed,
                                     std::uint64_t entity,
                                     std::uint64_t tick,
                                     RandomDomain domain,
                                     std::uint64_t sample) {
    std::uint64_t x = splitmix64(seed);
    x ^= splitmix64(entity + 0x100000001b3ULL);
    x ^= splitmix64(tick + 0x9e3779b9ULL);
    x ^= splitmix64(static_cast<std::uint64_t>(domain));
    x ^= splitmix64(sample + 0xd6e8feb86659fd93ULL);
    return splitmix64(x);
}

double DeterministicRng::uniform01(std::uint64_t seed,
                                   std::uint64_t entity,
                                   std::uint64_t tick,
                                   RandomDomain domain,
                                   std::uint64_t sample) {
    constexpr double denom = static_cast<double>(1ULL << 53U);
    return static_cast<double>(hash(seed, entity, tick, domain, sample) >> 11U) / denom;
}

double DeterministicRng::uniform(std::uint64_t seed,
                                 std::uint64_t entity,
                                 std::uint64_t tick,
                                 RandomDomain domain,
                                 double low,
                                 double high,
                                 std::uint64_t sample) {
    return low + (high - low) * uniform01(seed, entity, tick, domain, sample);
}

double DeterministicRng::normal(std::uint64_t seed,
                                std::uint64_t entity,
                                std::uint64_t tick,
                                RandomDomain domain,
                                double mean,
                                double sigma,
                                std::uint64_t sample) {
    // Explicit Box-Muller rather than std::normal_distribution so results are
    // stable across standard-library implementations.
    const double u1 = std::max(1e-12, uniform01(seed, entity, tick, domain, sample * 2U));
    const double u2 = uniform01(seed, entity, tick, domain, sample * 2U + 1U);
    const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
    return mean + sigma * z;
}

}   // namespace darwinsim
