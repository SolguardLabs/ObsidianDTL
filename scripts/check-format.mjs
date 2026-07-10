import { readFileSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { dirname } from "node:path";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const include = ["src", "tests", "scripts"];
const allowed = new Set([".c", ".h", ".js", ".mjs"]);
const failures = [];

function walk(dir) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) {
      walk(full);
      continue;
    }
    const dot = entry.name.lastIndexOf(".");
    const ext = dot >= 0 ? entry.name.slice(dot) : "";
    if (!allowed.has(ext)) {
      continue;
    }
    const text = readFileSync(full, "utf8");
    if (text.includes("\t")) {
      failures.push(`${full}: contains tab characters`);
    }
    if (!text.endsWith("\n")) {
      failures.push(`${full}: missing trailing newline`);
    }
    const lines = text.split(/\r?\n/);
    lines.forEach((line, index) => {
      if (line.length > 140) {
        failures.push(`${full}:${index + 1}: line exceeds 140 characters`);
      }
    });
  }
}

for (const dir of include) {
  walk(join(root, dir));
}

if (failures.length > 0) {
  console.error(failures.join("\n"));
  process.exit(1);
}

console.log("format checks passed");

