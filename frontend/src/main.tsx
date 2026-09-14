import React, { useEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import './styles.css';

type State = {
  tick: number;
  livingAnimals: number;
  totalCreated: number;
  speciesCount: number;
  availablePlants: number;
  carcasses: number;
  births: number;
  deaths: number;
  speciationEvents: number;
  hardwareMode: string;
  terrainSignature: string;
  paused: boolean;
};

type Species = {
  id: number;
  parentSpecies: number;
  birthTick: number;
  population: number;
  diversity: number;
  compute: number;
  memory: number;
  sensory: number;
  speed: number;
  aggression: number;
  plantDigestion: number;
  meatDigestion: number;
};

type Animal = {
  id: number;
  speciesId: number;
  x: number;
  y: number;
  energy: number;
  health: number;
  age: number;
  mass: number;
  compute: number;
  memory: number;
  sensory: number;
  lifespanPotential: number;
};

type Terrain = {
  width: number;
  height: number;
  cols: number;
  rows: number;
  signature: string;
  types: number[];
  regen: number[];
  movement: number[];
};

async function json<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetch(url, init);
  if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
  return response.json() as Promise<T>;
}

function Metric({ label, value }: { label: string; value: React.ReactNode }) {
  return <div className="metric"><span>{label}</span><strong>{value}</strong></div>;
}

function TerrainCanvas({ terrain }: { terrain: Terrain | null }) {
  const ref = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    if (!terrain || !ref.current) return;
    const canvas = ref.current;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const cellWidth = canvas.width / terrain.cols;
    const cellHeight = canvas.height / terrain.rows;
    const palette = ['#214e69', '#284d30', '#6b5b35', '#48515d'];

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    for (let row = 0; row < terrain.rows; ++row) {
      for (let col = 0; col < terrain.cols; ++col) {
        const index = row * terrain.cols + col;
        const type = terrain.types[index] ?? 1;
        const fertility = Math.min(2.8, Math.max(0.0, terrain.regen[index] ?? 1.0));
        ctx.globalAlpha = 0.48 + Math.min(0.42, fertility * 0.10);
        ctx.fillStyle = palette[type] ?? palette[1];
        ctx.fillRect(col * cellWidth, row * cellHeight, Math.ceil(cellWidth + 0.5), Math.ceil(cellHeight + 0.5));
      }
    }
    ctx.globalAlpha = 1.0;
  }, [terrain]);

  return <canvas ref={ref} className="terrain" width={768} height={768} aria-hidden="true" />;
}

function World({ animals, terrain }: { animals: Animal[]; terrain: Terrain | null }) {
  const points = useMemo(() => animals.slice(0, 400), [animals]);
  const width = terrain?.width ?? 512;
  const height = terrain?.height ?? 512;

  return (
    <div className="world" aria-label="Ecosystem overview with terrain">
      <TerrainCanvas terrain={terrain} />
      {points.map((a) => (
        <div
          key={a.id}
          className="animal"
          title={`Animal ${a.id} / Species ${a.speciesId}`}
          style={{
            left: `${Math.min(99, Math.max(0, a.x / width * 100))}%`,
            top: `${Math.min(99, Math.max(0, a.y / height * 100))}%`
          }}
        >
          {a.speciesId % 10}
        </div>
      ))}
      <div className="terrain-legend" aria-label="Terrain legend">
        <span><i className="river" />River</span>
        <span><i className="plains" />Plains</span>
        <span><i className="desert" />Desert</span>
        <span><i className="highlands" />Highlands</span>
      </div>
    </div>
  );
}

function App() {
  const [state, setState] = useState<State | null>(null);
  const [species, setSpecies] = useState<Species[]>([]);
  const [animals, setAnimals] = useState<Animal[]>([]);
  const [terrain, setTerrain] = useState<Terrain | null>(null);
  const [error, setError] = useState('');

  async function refresh() {
    try {
      const [s, sp, a] = await Promise.all([
        json<State>('/api/state'),
        json<Species[]>('/api/species'),
        json<Animal[]>('/api/animals?limit=500')
      ]);
      setState(s);
      setSpecies(sp);
      setAnimals(a);
      setError('');

      if (!terrain || terrain.signature !== s.terrainSignature) {
        setTerrain(await json<Terrain>('/api/terrain'));
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  }

  useEffect(() => {
    void refresh();
    const id = window.setInterval(() => void refresh(), 1000);
    return () => window.clearInterval(id);
    // The terrain signature check inside refresh handles static-map changes.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  async function command(commandName: 'pause' | 'resume' | 'checkpoint') {
    await json(`/api/control/${commandName}`, { method: 'POST' });
    await refresh();
  }

  return (
    <main>
      <header>
        <div>
          <p className="eyebrow">Distributed artificial evolution</p>
          <h1>DarwinSim</h1>
        </div>
        <div className="controls">
          <button onClick={() => void command(state?.paused ? 'resume' : 'pause')}>
            {state?.paused ? 'Resume' : 'Pause'}
          </button>
          <button onClick={() => void command('checkpoint')}>Checkpoint</button>
        </div>
      </header>

      {error && <p className="error">API error: {error}</p>}

      <section className="metrics">
        <Metric label="Tick" value={state?.tick ?? '-'} />
        <Metric label="Living" value={state?.livingAnimals ?? '-'} />
        <Metric label="Species" value={state?.speciesCount ?? '-'} />
        <Metric label="Births" value={state?.births ?? '-'} />
        <Metric label="Deaths" value={state?.deaths ?? '-'} />
        <Metric label="Speciation events" value={state?.speciationEvents ?? '-'} />
        <Metric label="Plants" value={state?.availablePlants ?? '-'} />
        <Metric label="Hardware mode" value={state?.hardwareMode ?? '-'} />
      </section>

      <section className="panel">
        <div className="panel-title">
          <h2>Ecosystem</h2>
          <span>terrain heatmap + up to 400 living animals</span>
        </div>
        <World animals={animals} terrain={terrain} />
      </section>

      <section className="panel">
        <div className="panel-title"><h2>Species</h2><span>centroid traits and population</span></div>
        <div className="table-wrap">
          <table>
            <thead>
              <tr>
                <th>ID</th><th>Population</th><th>Diversity</th><th>Compute</th><th>Memory</th>
                <th>Sensory</th><th>Speed</th><th>Plant</th><th>Meat</th>
              </tr>
            </thead>
            <tbody>
              {species.map((s) => (
                <tr key={s.id}>
                  <td>{s.id}</td><td>{s.population}</td><td>{s.diversity.toFixed(3)}</td>
                  <td>{s.compute.toFixed(2)}</td><td>{s.memory.toFixed(2)}</td><td>{s.sensory.toFixed(2)}</td>
                  <td>{s.speed.toFixed(2)}</td><td>{s.plantDigestion.toFixed(2)}</td><td>{s.meatDigestion.toFixed(2)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </main>
  );
}

createRoot(document.getElementById('root')!).render(
  <React.StrictMode><App /></React.StrictMode>
);
