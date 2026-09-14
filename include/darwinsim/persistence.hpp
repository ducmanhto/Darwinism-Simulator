#pragma once

#include "darwinsim/simulation.hpp"

#include <sqlite3.h>

#include <string>

namespace darwinsim {

class PersistenceStore {
public:
    explicit PersistenceStore(const std::string& path);
    ~PersistenceStore();

      PersistenceStore(const PersistenceStore&) = delete;
      PersistenceStore& operator=(const PersistenceStore&) = delete;

      void record(const Simulation& simulation);

private:
    void exec(const char* sql);
      sqlite3* db_{nullptr};
};

}    // namespace darwinsim
