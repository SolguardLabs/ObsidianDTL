import { existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const srcDir = join(root, "src");
const distDir = join(root, "dist");
const exeName = process.platform === "win32" ? "obsidian-dtl.exe" : "obsidian-dtl";
const exePath = join(distDir, exeName);
const sources = readdirSync(srcDir)
  .filter((name) => name.endsWith(".c"))
  .sort()
  .map((name) => join(srcDir, name));

mkdirSync(distDir, { recursive: true });

function run(command, args, options = {}) {
  const result = spawnSync(command, args, {
    cwd: root,
    encoding: "utf8",
    stdio: options.capture ? "pipe" : "inherit",
    shell: options.shell ?? false,
  });

  if (options.capture) {
    return result;
  }

  if (result.status !== 0) {
    process.exit(result.status ?? 1);
  }

  return result;
}

function findOnPath(name) {
  const result =
    process.platform === "win32"
      ? run("where.exe", [name], { capture: true })
      : run("sh", ["-lc", `command -v ${name}`], { capture: true });
  if (result.status === 0) {
    return result.stdout.split(/\r?\n/).find(Boolean)?.trim() ?? null;
  }
  return null;
}

function quoteCmd(value) {
  const text = String(value);
  if (!/[ \t&()]/.test(text)) {
    return text;
  }
  return `"${text.replace(/"/g, '""')}"`;
}

function buildWithUnixCompiler(compiler) {
  run(compiler, [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-O2",
    "-I",
    srcDir,
    "-o",
    exePath,
    ...sources,
  ]);
}

function findVsInstallPath() {
  const candidates = [
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe",
    "C:\\Program Files\\Microsoft Visual Studio\\Installer\\vswhere.exe",
  ];
  const vswhere = candidates.find((candidate) => existsSync(candidate));
  if (!vswhere) {
    return null;
  }
  const result = run(
    vswhere,
    [
      "-latest",
      "-products",
      "*",
      "-requires",
      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
      "-property",
      "installationPath",
    ],
    { capture: true },
  );
  if (result.status !== 0) {
    return null;
  }
  return result.stdout.split(/\r?\n/).find(Boolean)?.trim() ?? null;
}

function buildWithMsvc() {
  const installPath = findVsInstallPath();
  const vcvars = installPath ? join(installPath, "VC", "Auxiliary", "Build", "vcvars64.bat") : null;
  const clOnPath = findOnPath("cl.exe");
  const msvcSources = sources.map((source) => join("src", source.split(/[\\/]/).pop()));

  if (!clOnPath && (!vcvars || !existsSync(vcvars))) {
    console.error("No C compiler found. Install GCC/Clang or Visual Studio Build Tools with C++.");
    process.exit(127);
  }

  const commonArgs = [
    "/nologo",
    "/std:c11",
    "/W4",
    "/WX",
    "/Isrc",
    `/Fe:dist\\${exeName}`,
    "/Fodist\\",
    ...msvcSources,
  ];

  if (clOnPath) {
    run("cl.exe", commonArgs);
    return;
  }

  const cmdPath = join(distDir, "build-msvc.cmd");
  const lines = [
    "@echo off",
    `call ${quoteCmd(vcvars)} x64`,
    "if errorlevel 1 exit /b %errorlevel%",
    `cl.exe ${commonArgs.map(quoteCmd).join(" ")}`,
    "exit /b %errorlevel%",
    "",
  ];
  writeFileSync(cmdPath, lines.join("\r\n"), "utf8");
  run("cmd.exe", ["/d", "/c", cmdPath]);
}

const explicit = process.env.CC;
if (explicit) {
  buildWithUnixCompiler(explicit);
} else if (process.platform === "win32") {
  buildWithMsvc();
} else {
  const compiler = findOnPath("cc") || findOnPath("gcc") || findOnPath("clang");
  if (!compiler) {
    console.error("No C compiler found. Install cc, gcc or clang.");
    process.exit(127);
  }
  buildWithUnixCompiler(compiler);
}

console.log(exePath);
