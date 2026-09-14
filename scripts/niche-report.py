#!/usr/bin/env python3
"""Print a lightweight species-by-biome occupancy report from the DarwinSim API."""

from __future__ import annotations

import json
import os
import urllib.request
from collections import Counter, defaultdict

BASE_URL = os.environ.get("DARWINSIM_URL", "http://localhost:8088").rstrip("/")
BIOMES = {0: "river", 1: "plains", 2: "desert", 3: "highlands"}


def get_json(path: str):
    with urllib.request.urlopen(BASE_URL + path, timeout=10) as response:
        return json.load(response)


def main() -> None:
    terrain = get_json("/api/terrain")
    animals = get_json("/api/animals?limit=5000")

    width = float(terrain["width"])
    height = float(terrain["height"])
    cols = int(terrain["cols"])
    rows = int(terrain["rows"])
    types = terrain["types"]

    occupancy: dict[int, Counter[str]] = defaultdict(Counter)
    positions: dict[int, list[tuple[float, float]]] = defaultdict(list)

    for animal in animals:
        x = max(0.0, min(width, float(animal["x"])))
        y = max(0.0, min(height, float(animal["y"])))
        col = min(cols - 1, int(x / max(width, 1.0) * cols))
        row = min(rows - 1, int(y / max(height, 1.0) * rows))
        biome = BIOMES.get(int(types[row * cols + col]), "unknown")
        species_id = int(animal["speciesId"])
        occupancy[species_id][biome] += 1
        positions[species_id].append((x, y))

    print(f"api={BASE_URL} terrain_signature={terrain['signature']} sampled_animals={len(animals)}")
    print("species,pop_sample,river,plains,desert,highlands,dominant_biome,mean_x,mean_y")
    for species_id in sorted(occupancy):
        counts = occupancy[species_id]
        population = sum(counts.values())
        dominant = counts.most_common(1)[0][0] if population else "none"
        pts = positions[species_id]
        mean_x = sum(x for x, _ in pts) / max(1, len(pts))
        mean_y = sum(y for _, y in pts) / max(1, len(pts))
        print(
            f"{species_id},{population},{counts['river']},{counts['plains']},"
            f"{counts['desert']},{counts['highlands']},{dominant},{mean_x:.2f},{mean_y:.2f}"
        )


if __name__ == "__main__":
    main()
