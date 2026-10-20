import assert from "node:assert/strict";
import test from "node:test";

import { summarizeReport } from "../../client/report-client.mjs";
import { runScenario } from "../helpers/obsidian-cli.js";

test("client summarizes domain liquidity and route availability", () => {
  const summary = summarizeReport(runScenario("direct"));
  const edge = summary.domains.find((domain) => domain.id === "edge");
  const direct = summary.routes.find((route) => route.id === "edge-direct");

  assert.equal(summary.conservationOk, true);
  assert.equal(edge.available, "5500000000");
  assert.equal(edge.pending, "1000000000");
  assert.equal(edge.utilizationBps, 1538);
  assert.equal(edge.fees, "20000");
  assert.equal(direct.successBps, 10_000);
});

test("client rejects unsafe JSON integer magnitudes", () => {
  const report = runScenario("snapshot");
  report.domain_stats.edge.available_liquidity = Number.MAX_SAFE_INTEGER + 1;
  assert.throws(() => summarizeReport(report), RangeError);
});
