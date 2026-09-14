#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace darwinsim {

using AnimalId = std::uint64_t;
using SpeciesId = std::uint32_t;
using ResourceId = std::uint64_t;

struct Vec2 {
    double x{0.0};
    double y{0.0};
};

inline double distance(Vec2 a, Vec2 b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline Vec2 clamp_position(Vec2 p, double width, double height) {
    p.x = std::clamp(p.x, 0.0, width);
    p.y = std::clamp(p.y, 0.0, height);
    return p;
}

enum class ActionType : std::uint8_t {
    Rest = 0,
    Move = 1,
    EatPlant = 2,
    EatCarcass = 3,
    Flee = 4,
    Attack = 5,
    Mate = 6,
    Explore = 7
};

inline std::string to_string(ActionType action) {
    switch (action) {
          case ActionType::Rest: return "rest";
          case ActionType::Move: return "move";
          case ActionType::EatPlant: return "eat_plant";
          case ActionType::EatCarcass: return "eat_carcass";
          case ActionType::Flee: return "flee";
          case ActionType::Attack: return "attack";
          case ActionType::Mate: return "mate";
          case ActionType::Explore: return "explore";
      }
      return "unknown";
}

}    // namespace darwinsim
