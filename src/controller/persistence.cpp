#include "darwinsim/persistence.hpp"

#include "darwinsim/checkpoint.hpp"

#include <stdexcept>

namespace darwinsim {

PersistenceStore::PersistenceStore(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        const std::string error = db_ ? sqlite3_errmsg(db_) : "unknown SQLite error";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("SQLite open failed: " + error);
    }
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("CREATE TABLE IF NOT EXISTS experiment_stats ("
         "tick INTEGER PRIMARY KEY, living INTEGER, total_created INTEGER, species INTEGER, "
         "plants INTEGER, carcasses INTEGER, births INTEGER, deaths INTEGER, speciations INTEGER, state_hash TEXT);");
    exec("CREATE TABLE IF NOT EXISTS species_stats ("
         "tick INTEGER, species_id INTEGER, parent_species INTEGER, birth_tick INTEGER, population INTEGER, "
         "diversity REAL, compute REAL, memory REAL, sensory REAL, speed REAL, aggression REAL, "
         "plant_digestion REAL, meat_digestion REAL, PRIMARY KEY(tick, species_id));");
}

PersistenceStore::~PersistenceStore() {
    if (db_) sqlite3_close(db_);
}

void PersistenceStore::exec(const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "SQLite error";
        sqlite3_free(error);
        throw std::runtime_error(message);
    }
}

void PersistenceStore::record(const Simulation& simulation) {
    const auto s = simulation.summary();
    sqlite3_stmt* stmt = nullptr;
    const char* insert_stats =
        "INSERT OR REPLACE INTO experiment_stats "
        "(tick,living,total_created,species,plants,carcasses,births,deaths,speciations,state_hash) "
        "VALUES(?,?,?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db_, insert_stats, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(s.tick));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(s.living_animals));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(s.total_animals_created));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(s.species_count));
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(s.available_plants));
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(s.carcasses));
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(s.births));
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(s.deaths));
    sqlite3_bind_int64(stmt, 9, static_cast<sqlite3_int64>(s.speciation_events));
    const std::string hash = std::to_string(state_hash(simulation));
    sqlite3_bind_text(stmt, 10, hash.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    sqlite3_finalize(stmt);

    const char* insert_species =
        "INSERT OR REPLACE INTO species_stats "
        "(tick,species_id,parent_species,birth_tick,population,diversity,compute,memory,sensory,speed,aggression,plant_digestion, meat_digestion) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);";
    for (const auto& species : simulation.species_stats()) {
        if (sqlite3_prepare_v2(db_, insert_species, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(db_));
        }
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(s.tick));
        sqlite3_bind_int(stmt, 2, static_cast<int>(species.id));
        sqlite3_bind_int(stmt, 3, static_cast<int>(species.parent_species));
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(species.birth_tick));
         sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(species.population));
         sqlite3_bind_double(stmt, 6, species.diversity);
         sqlite3_bind_double(stmt, 7, species.centroid.compute_capacity);
         sqlite3_bind_double(stmt, 8, species.centroid.memory_capacity);
         sqlite3_bind_double(stmt, 9, species.centroid.sensory_capacity);
         sqlite3_bind_double(stmt, 10, species.centroid.locomotion);
         sqlite3_bind_double(stmt, 11, species.centroid.aggression);
         sqlite3_bind_double(stmt, 12, species.centroid.plant_digestion);
         sqlite3_bind_double(stmt, 13, species.centroid.meat_digestion);
         if (sqlite3_step(stmt) != SQLITE_DONE) {
             sqlite3_finalize(stmt);
             throw std::runtime_error(sqlite3_errmsg(db_));
         }
         sqlite3_finalize(stmt);
     }
}

}   // namespace darwinsim
