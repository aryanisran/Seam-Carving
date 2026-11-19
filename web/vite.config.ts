import { defineConfig } from 'vitest/config';
import react from '@vitejs/plugin-react';

// BASE_PATH is set by the GitHub Pages workflow (e.g. /Graph-Cut/); local dev uses '/'.
export default defineConfig({
  base: process.env.BASE_PATH ?? '/',
  plugins: [react()],
  server: { fs: { allow: ['..'] } }, // sample images live in ../samples
  test: { environment: 'node', include: ['tests/**/*.test.ts'] },
});
