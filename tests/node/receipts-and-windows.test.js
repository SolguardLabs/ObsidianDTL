import assert from "node:assert/strict";
import test from "node:test";

import { runScenario, runScript } from "../helpers/obsidian-cli.js";

test("expiry rechaza packets fuera de ventana temporal", () => {
  const report = runScenario("expiry");

  assert.equal(report.scenario, "expiry");
  assert.equal(report.packets.submitted, 1);
  assert.equal(report.packets.settled, 0);
  assert.equal(report.packets.rejected, 1);
  assert.equal(report.receipts.total, 0);
  assert.match(report.last_rejection, /expired/);
  assert.equal(report.balances.accounts.merchant, 100_000_000);
  assert.equal(report.conservation_ok, true);
});

test("duplicate contabiliza una sola entrega en la ruta primaria", () => {
  const report = runScenario("duplicate");

  assert.equal(report.scenario, "duplicate");
  assert.equal(report.packets.submitted, 1);
  assert.equal(report.packets.settled, 1);
  assert.equal(report.packets.rejected, 1);
  assert.equal(report.receipts.total, 1);
  assert.equal(report.routes["edge-direct"].deliveries, 1);
  assert.equal(report.routes["edge-direct"].failures, 1);
  assert.match(report.last_rejection, /nonce/);
  assert.equal(report.balances.accounts.merchant, 849_980_000);
  assert.equal(report.conservation_ok, true);
});

test("late marca confirmacion tardia sin descartar receipt valido", () => {
  const report = runScenario("late");

  assert.equal(report.scenario, "late");
  assert.equal(report.packets.settled, 1);
  assert.equal(report.packets.rejected, 0);
  assert.equal(report.receipts.total, 1);
  assert.equal(report.receipts.late, 1);
  assert.equal(report.receipt_log[0].accepted, true);
  assert.equal(report.receipt_log[0].late, true);
  assert.equal(report.conservation_ok, true);
});

test("TRY_DELIVER permite scripts con rechazos esperados", () => {
  const report = runScript([
    "SCENARIO scripted-reject",
    "PACKET pkt-script-002 edge core alice merchant 31 50000000 50000 4 release",
    "ADVANCE 5",
    "TRY_DELIVER pkt-script-002 edge-direct 0",
  ]);

  assert.equal(report.scenario, "scripted-reject");
  assert.equal(report.packets.settled, 0);
  assert.equal(report.packets.rejected, 1);
  assert.match(report.last_rejection, /expired/);
  assert.equal(report.conservation_ok, true);
});

