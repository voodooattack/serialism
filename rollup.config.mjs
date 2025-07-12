import { defineConfig } from "rollup";
import multiInput from "rollup-plugin-multi-input";
import commonjs from "@rollup/plugin-commonjs";
import { nodeResolve } from "@rollup/plugin-node-resolve";
import typescript from "@rollup/plugin-typescript";

const createConfig = (output) => {
  return defineConfig({
    input: "./src/serialism.ts",
    output: {
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
