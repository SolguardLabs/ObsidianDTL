import { readFileSync } from "node:fs";

import { summarizeReport } from "../client/report-client.mjs";

const path = process.argv[2];
if (!path) {
  console.error("Usage: node scripts/inspect-report.mjs <report.json|->");
  process.exit(2);
}

try {
  const source = path === "-" ? readFileSync(0, "utf8") : readFileSync(path, "utf8");
  const summary = summarizeReport(JSON.parse(source));
  process.stdout.write(`${JSON.stringify(summary, null, 2)}\n`);
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
}
