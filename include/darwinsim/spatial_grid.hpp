#pragma once

#include "darwinsim/animal.hpp"
#include "darwinsim/world.hpp"

#include <cstddef>
#include <vector>

namespace darwinsim {

class SpatialGrid {
public:
    SpatialGrid(double width, double height, double cell_size);

      void rebuild(const std::vector<Animal>& animals,
                   const std::vector<PlantResource>& plants,
                   const std::vector<CarcassResource>& carcasses);

      std::vector<std::size_t> nearby_animals(Vec2 position, double radius) const;
      std::vector<std::size_t> nearby_plants(Vec2 position, double radius) const;
      std::vector<std::size_t> nearby_carcasses(Vec2 position, double radius) const;

private:
    struct Bucket {
         std::vector<std::size_t> animals;
         std::vector<std::size_t> plants;
         std::vector<std::size_t> carcasses;
    };

      std::size_t index_for(Vec2 position) const;
      std::vector<std::size_t> candidate_buckets(Vec2 position, double radius) const;

      double width_;
      double height_;
      double cell_size_;
      std::size_t cols_;
      std::size_t rows_;
      std::vector<Bucket> buckets_;
};

}    // namespace darwinsim
