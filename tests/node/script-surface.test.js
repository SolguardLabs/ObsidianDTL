import assert from "node:assert/strict";
import test from "node:test";

import { runScript } from "../helpers/obsidian-cli.js";

test("EMPTY permite construir un fixture minimo desde cero", () => {
  const report = runScript([
    "EMPTY",
    "SCENARIO custom-empty",
    "DOMAIN branch Branch 1000000",
    "DOMAIN hub Hub 500000",
    "ACCOUNT operator Operator 0",
    "ACCOUNT receiver Receiver 0",
    "ACCOUNT relay Relay 0",
    "ROUTE branch-direct branch branch hub relay 25 10 1 1",
    "PACKET pkt-branch branch hub operator receiver 1 1000 25 10 release",
    "DELIVER pkt-branch branch-direct 1",
  ]);

  assert.equal(report.scenario, "custom-empty");
  assert.deepEqual(report.surface, { domains: 2, accounts: 3, routes: 1 });
  assert.equal(report.packets.submitted, 1);
  assert.equal(report.packets.settled, 1);
  assert.equal(report.balances.domains.branch, 999_000);
  assert.equal(report.balances.accounts.receiver, 975);
  assert.equal(report.balances.accounts.relay, 25);
  assert.equal(report.conservation_ok, true);
});

test("scripts pueden registrar rechazos de ruta inactiva", () => {
  const report = runScript([
    "SCENARIO inactive-route",
    "ROUTE edge-paused edge edge core relay-edge 20000 20 1 0",
    "PACKET pkt-paused edge core alice merchant 201 100000000 50000 10 release",
    "TRY_DELIVER pkt-paused edge-paused 0",
  ]);

  assert.equal(report.scenario, "inactive-route");
  assert.equal(report.packets.settled, 0);
  assert.equal(report.packets.rejected, 1);
  assert.equal(report.routes["edge-paused"].failures, 1);
  assert.equal(report.planner.health.inactive_routes, 1);
  assert.equal(report.planner.health.routes_with_failures, 1);
  assert.equal(report.planner.quotes["edge-paused"].accepted, false);
  assert.match(report.last_rejection, /inactive/);
  assert.equal(report.balances.accounts.merchant, 100_000_000);
  assert.equal(report.conservation_ok, true);
});

test("QUOTE selecciona una ruta sin mover balances", () => {
  const report = runScript([
    "SCENARIO quote-only",
    "QUOTE edge core 100000000 50000",
    "QUOTE treasury core 50000000 50000",
  ]);

  assert.equal(report.scenario, "quote-only");
  assert.equal(report.packets.submitted, 0);
  assert.equal(report.receipts.total, 0);
  assert.equal(report.events, 15);
  assert.equal(report.balances.domains.edge, 7_500_000_000);
  assert.equal(report.balances.accounts.merchant, 100_000_000);
  assert.equal(report.planner.health.active_routes, 3);
  assert.equal(report.planner.quotes["treasury-direct"].net_amount, 99_970_000);
  assert.equal(report.conservation_ok, true);
});
