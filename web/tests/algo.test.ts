// Checks the browser (TypeScript) algorithms against hand-worked examples and against the
// team's C++ program: same seams, same carved pixels.
import { describe, expect, it } from 'vitest';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { PNG } from 'pngjs';
import { energyFromRows } from '../src/algo/energy';
import { dpVerticalSeam } from '../src/algo/dp';
import { greedyVerticalSeam } from '../src/algo/greedy';
import { graphCutVerticalSeam } from '../src/algo/graphcut';
import { Algorithm, carveSync, seamsInOriginal } from '../src/algo/carve';
import { RGBImage, fromRGBA } from '../src/algo/image';
import { GREEDY_TRAP } from '../src/presets';

const REPO = resolve(__dirname, '../..');

function loadPng(path: string): RGBImage {
  const png = PNG.sync.read(readFileSync(path));
  return fromRGBA(png.width, png.height, png.data);
}

function rng(seed: number) {
  return () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
}

describe('hand-checked examples', () => {
  it('greedy falls into the trap, DP and graph cut find the optimum', () => {
    const e = energyFromRows(GREEDY_TRAP);
    expect(greedyVerticalSeam(e).energy).toBe(19);
    expect(dpVerticalSeam(e).energy).toBe(8);
    expect(dpVerticalSeam(e).seam).toEqual([2, 3, 4, 3]);
    for (const s of ['dinic', 'edmonds_karp'] as const) {
      const g = graphCutVerticalSeam(e, s);
      expect(g.energy).toBe(8);
      expect(g.stats.maxFlow).toBeCloseTo(8, 9); // max-flow min-cut theorem
    }
  });

  it('graph cut and DP agree on the optimal cost for random grids', () => {
    const rand = rng(42);
    for (let t = 0; t < 200; t++) {
      const h = 2 + Math.floor(rand() * 6), w = 2 + Math.floor(rand() * 7);
      const rows = Array.from({ length: h }, () => Array.from({ length: w }, () => Math.floor(rand() * 50)));
      const e = energyFromRows(rows);
      const dp = dpVerticalSeam(e).energy;
      expect(graphCutVerticalSeam(e, 'dinic').energy).toBeCloseTo(dp, 6);
      expect(graphCutVerticalSeam(e, 'edmonds_karp').energy).toBeCloseTo(dp, 6);
      expect(greedyVerticalSeam(e).energy).toBeGreaterThanOrEqual(dp);
    }
  });

  it('maps removed seams back to the original image', () => {
    // 3x3 image, remove column 1 in every row, then row 0: originals are known.
    const w = 3, h = 3;
    const recs = [
      { vertical: true, seam: [1, 1, 1], energy: 0, ms: 0, wBefore: 3, hBefore: 3 },
      { vertical: false, seam: [0, 0], energy: 0, ms: 0, wBefore: 2, hBefore: 3 },
    ];
    const [v, hz] = seamsInOriginal(w, h, recs);
    expect([...v]).toEqual([1, 4, 7]);
    expect([...hz]).toEqual([0, 2]);
  });
});

// ---------------------------------------------------------------- C++ parity
// Runs bin/seam_carve (built with `make all`). Set SEAM_CARVE_CMD to override, e.g. on
// Windows: SEAM_CARVE_CMD="wsl -d Ubuntu -- ./bin/seam_carve".
const cmd = (process.env.SEAM_CARVE_CMD ?? './bin/seam_carve').split(' ');
const haveCpp = process.env.SEAM_CARVE_CMD !== undefined || existsSync(resolve(REPO, 'bin/seam_carve'));

describe.skipIf(!haveCpp)('matches the C++ implementation', () => {
  const tmp = 'web/.tmp-test';
  mkdirSync(resolve(REPO, tmp), { recursive: true });

  /** Run bin/seam_carve on a PNG and compare seams, seam energies and the final pixels with the TS port. */
  function compareWithCpp(pngPath: string, image: RGBImage, algorithm: Algorithm, solver: string, metric: string, cols: number, rows: number, tag: string) {
    execFileSync(cmd[0], [...cmd.slice(1),
      '-i', pngPath, '-o', `${tmp}/${tag}_out.png`,
      '--remove-cols', String(cols), '--remove-rows', String(rows),
      '--algorithm', algorithm, '--solver', solver, '--energy', metric,
      '--export-seams', `${tmp}/${tag}.json`], { cwd: REPO, stdio: 'pipe' });

    const cpp = JSON.parse(readFileSync(resolve(REPO, `${tmp}/${tag}.json`), 'utf8')).seams;
    const js = carveSync(image, cols, rows, { algorithm, solver: solver as never, metric: metric as never });
    expect(js.records.map((r) => (r.vertical ? 'vertical' : 'horizontal'))).toEqual(cpp.map((s: { orientation: string }) => s.orientation));
    expect(js.records.map((r) => r.seam)).toEqual(cpp.map((s: { indices: number[] }) => s.indices));
    // the C++ JSON prints 6 significant digits
    js.records.forEach((r, i) => expect(Math.abs(r.energy - cpp[i].energy_sum)).toBeLessThan(1e-5 * Math.max(1, r.energy)));
    js.records.forEach((r, i) => {
      if (r.flow) expect(Math.abs(r.flow.maxFlow - cpp[i].max_flow)).toBeLessThan(1e-5 * Math.max(1, r.flow.maxFlow));
    });

    const out = loadPng(resolve(REPO, `${tmp}/${tag}_out.png`));
    const fin = js.images.at(-1)!;
    expect([fin.w, fin.h]).toEqual([out.w, out.h]);
    expect(Buffer.from(fin.data).equals(Buffer.from(out.data))).toBe(true);
  }

  const ALL: [Algorithm, string][] = [['dp', 'dinic'], ['greedy', 'dinic'], ['graph_cut', 'dinic'], ['graph_cut', 'edmonds_karp']];

  describe('sample image samples/synthetic_tower.png', () => {
    const image = loadPng(resolve(REPO, 'samples/synthetic_tower.png'));
    const cases = [...ALL.map(([a, s]) => [a, s, 'dual'] as const), ['dp', 'dinic', 'sobel'] as const, ['greedy', 'dinic', 'sobel'] as const, ['graph_cut', 'dinic', 'sobel'] as const];
    it.each(cases)('%s (%s, %s energy): identical seams, energies and pixels', (algorithm, solver, metric) => {
      compareWithCpp('samples/synthetic_tower.png', image, algorithm, solver, metric, 6, 3, `tower_${algorithm}_${solver}_${metric}`);
    }, 120000);
  });

  // Random images built from only 3 colours: lots of equal energies, so any difference in
  // tie-breaking between the TypeScript port and the C++ code would show up here.
  describe('random tie-heavy images', () => {
    const shapes: [number, number, number][] = [[14, 9, 1], [9, 14, 2], [17, 11, 3]];
    for (const [w, h, seed] of shapes) {
      const rand = rng(seed * 7919);
      const palette = [[10, 10, 10], [200, 60, 60], [10, 10, 10], [240, 240, 240]];
      const rgba = new Uint8Array(w * h * 4);
      for (let i = 0; i < w * h; i++) {
        const p = palette[Math.floor(rand() * palette.length)];
        rgba.set([p[0], p[1], p[2], 255], i * 4);
      }
      const png = new PNG({ width: w, height: h });
      png.data = Buffer.from(rgba);
      const path = `${tmp}/random_${w}x${h}.png`;
      writeFileSync(resolve(REPO, path), PNG.sync.write(png));
      const image = fromRGBA(w, h, rgba);
      for (const metric of ['dual', 'sobel']) {
        it.each(ALL)(`${w}x${h} seed ${seed}, %s (%s), ${metric}`, (algorithm, solver) => {
          compareWithCpp(path, image, algorithm, solver, metric, Math.floor(w / 2), Math.floor(h / 3), `rand${seed}_${algorithm}_${solver}_${metric}`);
        }, 120000);
      }
    }
  });
});
