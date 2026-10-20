import { existsSync, mkdtempSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const exePath = join(
  root,
  "dist",
  process.platform === "win32" ? "obsidian-dtl.exe" : "obsidian-dtl",
);

let built = false;

function needsBuild() {
  if (!existsSync(exePath)) {
    return true;
  }
  const binaryMtime = statSync(exePath).mtimeMs;
  const sourceDir = join(root, "src");
  const newestSource = readdirSync(sourceDir)
    .filter((name) => name.endsWith(".c") || name.endsWith(".h"))
    .map((name) => statSync(join(sourceDir, name)).mtimeMs)
    .reduce((max, value) => Math.max(max, value), 0);
  return newestSource > binaryMtime;
}

export function buildBinary() {
  if (built) {
    return exePath;
  }
  if (!needsBuild()) {
    built = true;
    return exePath;
  }
  const result = spawnSync(process.execPath, [join(root, "scripts", "build.mjs")], {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    throw new Error(`build failed\nSTDOUT:\n${result.stdout}\nSTDERR:\n${result.stderr}`);
  }
  built = true;
  return exePath;
}

export function runScenario(name = "direct") {
  const exe = buildBinary();
  const result = spawnSync(exe, [name], {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    throw new Error(
      `scenario ${name} failed\nSTDOUT:\n${result.stdout}\nSTDERR:\n${result.stderr}`,
    );
  }
  return JSON.parse(result.stdout);
}

export function runScript(lines) {
  const exe = buildBinary();
  const dir = mkdtempSync(join(tmpdir(), "obsidian-dtl-"));
  const scriptPath = join(dir, "scenario.obs");
  writeFileSync(scriptPath, `${lines.join("\n")}\n`, "utf8");
  const result = spawnSync(exe, ["script", scriptPath], {
    cwd: root,
    encoding: "utf8",
  });
  if (result.status !== 0) {
    throw new Error(`script failed\nSTDOUT:\n${result.stdout}\nSTDERR:\n${result.stderr}`);
  }
  return JSON.parse(result.stdout);
}

export function expectDigest(value) {
  if (!/^0x[0-9a-f]{16}$/.test(value)) {
    throw new Error(`invalid digest ${value}`);
  }
}
