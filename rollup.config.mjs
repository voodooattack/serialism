import { defineConfig } from "rollup";
import { nodeResolve } from "@rollup/plugin-node-resolve";
import typescript from "@rollup/plugin-typescript";

const createConfig = (output) => {
  return defineConfig({
    input: "./src/index.ts",
    output: {
      exports: "named",
      ...output,
      preserveModules: true,
      sourcemap: true,
    },
    external: [
      "bindings",
      "nan",
    ],
    plugins: [
      nodeResolve(),
      typescript({
        tsconfig: "./tsconfig.json",
        outDir: output.dir,
        rootDir: "./src",
        declaration: true,
      }),
    ],
  });
};

export default [
  createConfig({
    dir: `./dist/esm`,
    format: "esm",
  }),
  createConfig({
    dir: `./dist/cjs`,
    format: "cjs",
  }),
];
