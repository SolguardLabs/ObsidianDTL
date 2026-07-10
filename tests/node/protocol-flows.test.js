import assert from "node:assert/strict";
import test from "node:test";

import { expectDigest, runScenario, runScript } from "../helpers/obsidian-cli.js";

test("snapshot inicializa superficie operacional estable", () => {
  const report = runScenario("snapshot");

  assert.equal(report.scenario, "snapshot");
  assert.deepEqual(report.surface, { domains: 3, accounts: 6, routes: 3 });
  assert.equal(report.packets.submitted, 0);
  assert.equal(report.receipts.total, 0);
  assert.equal(report.conservation_ok, true);
  assert.equal(report.balances.domains.edge, 7_500_000_000);
  assert.equal(report.domain_stats.edge.available_liquidity, 7_500_000_000);
  assert.equal(report.planner.health.active_routes, 3);
  assert.equal(report.planner.health.inactive_routes, 0);
  assert.equal(report.planner.quotes["edge-direct"].accepted, true);
  assert.equal(report.planner.quotes["edge-direct"].net_amount, 99_980_000);
  expectDigest(report.planner.route_digest);
  expectDigest(report.state_digest);
});

test("direct liquida un packet primario y conserva suministro", () => {
  const report = runScenario("direct");

  assert.equal(report.scenario, "direct");
  assert.equal(report.packets.submitted, 1);
  assert.equal(report.packets.settled, 1);
  assert.equal(report.packets.rejected, 0);
  assert.equal(report.receipts.total, 1);
  assert.equal(report.receipts.late, 0);
  assert.equal(report.routes["edge-direct"].deliveries, 1);
  assert.equal(report.balances.domains.edge, 6_500_000_000);
  assert.equal(report.balances.accounts.merchant, 1_099_980_000);
  assert.equal(report.balances.accounts["relay-edge"], 20_000);
  assert.equal(report.domain_stats.edge.available_liquidity, 5_500_000_000);
  assert.equal(report.planner.health.total_pending, 1_000_000_000);
  assert.equal(report.conservation_ok, true);
  expectDigest(report.packet_log[0].digest);
  expectDigest(report.receipt_log[0].digest);
});

test("fallback usa una ruta secundaria autorizada sin romper accounting global", () => {
  const report = runScenario("fallback");

  assert.equal(report.scenario, "fallback");
  assert.equal(report.packets.settled, 1);
  assert.equal(report.packets.rejected, 0);
  assert.equal(report.routes["edge-fallback"].deliveries, 1);
  assert.equal(report.receipt_log[0].route_id, "edge-fallback");
  assert.equal(report.balances.accounts.merchant, 1_599_975_000);
  assert.equal(report.balances.accounts["relay-core"], 25_000);
  assert.equal(report.planner.quotes["edge-fallback"].transport_origin, "core");
  assert.equal(report.conservation_ok, true);
});

test("routing selecciona rutas automaticamente segun prioridad y fee envelope", () => {
  const report = runScenario("routing");

  assert.equal(report.scenario, "routing");
  assert.equal(report.packets.submitted, 2);
  assert.equal(report.packets.settled, 2);
  assert.equal(report.routes["edge-direct"].deliveries, 2);
  assert.equal(report.routes["edge-fallback"].deliveries, 0);
  assert.deepEqual(
    report.receipt_log.map((receipt) => receipt.route_id),
    ["edge-direct", "edge-direct"],
  );
  assert.equal(report.balances.accounts.merchant, 649_960_000);
  assert.equal(report.planner.health.total_pending, 550_000_000);
  assert.equal(report.domain_stats.edge.available_liquidity, 6_400_000_000);
  assert.equal(report.conservation_ok, true);
});

test("script permite escenarios externos reproducibles", () => {
  const report = runScript([
    "SCENARIO scripted",
    "PACKET pkt-script-001 edge core alice merchant 21 100000000 50000 10 release",
    "DELIVER pkt-script-001 edge-direct 0",
  ]);

  assert.equal(report.scenario, "scripted");
  assert.equal(report.packets.submitted, 1);
  assert.equal(report.packets.settled, 1);
  assert.equal(report.packets.rejected, 0);
  assert.equal(report.receipts.total, 1);
  assert.equal(report.balances.accounts.merchant, 199_980_000);
  assert.equal(report.conservation_ok, true);
});
