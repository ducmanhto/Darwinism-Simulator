#include "darwinsim/spatial_grid.hpp"

#include <algorithm>
#include <cmath>

namespace darwinsim {

SpatialGrid::SpatialGrid(double width, double height, double cell_size)
    : width_(width), height_(height), cell_size_(cell_size),
       cols_(std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(width / cell_size)))),
       rows_(std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(height / cell_size)))),
       buckets_(cols_ * rows_) {}

 std::size_t SpatialGrid::index_for(Vec2 p) const {
     const auto cx = std::min(cols_ - 1, static_cast<std::size_t>(std::max(0.0, p.x) / cell_size_));
     const auto cy = std::min(rows_ - 1, static_cast<std::size_t>(std::max(0.0, p.y) / cell_size_));
     return cy * cols_ + cx;
 }

 void SpatialGrid::rebuild(const std::vector<Animal>& animals,
                           const std::vector<PlantResource>& plants,
                           const std::vector<CarcassResource>& carcasses) {
     for (auto& bucket : buckets_) {
         bucket.animals.clear();
         bucket.plants.clear();
         bucket.carcasses.clear();
     }

      for (std::size_t i = 0; i < animals.size(); ++i) {
          if (animals[i].state.alive) {
              buckets_[index_for(animals[i].state.position)].animals.push_back(i);
          }
      }
      for (std::size_t i = 0; i < plants.size(); ++i) {
          if (plants[i].available) {
              buckets_[index_for(plants[i].position)].plants.push_back(i);
          }
      }
      for (std::size_t i = 0; i < carcasses.size(); ++i) {
          if (carcasses[i].available) {
              buckets_[index_for(carcasses[i].position)].carcasses.push_back(i);
          }
      }
 }

 std::vector<std::size_t> SpatialGrid::candidate_buckets(Vec2 p, double radius) const {
     const int min_x = std::max(0, static_cast<int>(std::floor((p.x - radius) / cell_size_)));
     const int max_x = std::min(static_cast<int>(cols_) - 1, static_cast<int>(std::floor((p.x + radius) / cell_size_)));
     const int min_y = std::max(0, static_cast<int>(std::floor((p.y - radius) / cell_size_)));
     const int max_y = std::min(static_cast<int>(rows_) - 1, static_cast<int>(std::floor((p.y + radius) / cell_size_)));

     std::vector<std::size_t> result;
     for (int y = min_y; y <= max_y; ++y) {
         for (int x = min_x; x <= max_x; ++x) {
             result.push_back(static_cast<std::size_t>(y) * cols_ + static_cast<std::size_t>(x));
         }
     }
     return result;
}

std::vector<std::size_t> SpatialGrid::nearby_animals(Vec2 p, double radius) const {
    std::vector<std::size_t> out;
    for (auto b : candidate_buckets(p, radius)) {
        out.insert(out.end(), buckets_[b].animals.begin(), buckets_[b].animals.end());
    }
    return out;
}

std::vector<std::size_t> SpatialGrid::nearby_plants(Vec2 p, double radius) const {
    std::vector<std::size_t> out;
    for (auto b : candidate_buckets(p, radius)) {
        out.insert(out.end(), buckets_[b].plants.begin(), buckets_[b].plants.end());
    }
    return out;
}

std::vector<std::size_t> SpatialGrid::nearby_carcasses(Vec2 p, double radius) const {
    std::vector<std::size_t> out;
    for (auto b : candidate_buckets(p, radius)) {
        out.insert(out.end(), buckets_[b].carcasses.begin(), buckets_[b].carcasses.end());
    }
    return out;
}

}   // namespace darwinsim
