function record(value, label) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw new TypeError(`${label} must be an object`);
  }
  return value;
}

function unsigned(value, label) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new RangeError(`${label} must be a safe unsigned integer`);
  }
  return BigInt(value);
}

function basisPoints(numerator, denominator) {
  return denominator === 0n ? 0 : Number((numerator * 10_000n) / denominator);
}

export function summarizeReport(input) {
  const report = record(input, "report");
  const domainStats = record(report.domain_stats, "domain_stats");
  const routeStats = record(report.routes, "routes");
  if (typeof report.scenario !== "string" || typeof report.conservation_ok !== "boolean") {
    throw new TypeError("report metadata is incomplete");
  }

  let totalAvailable = 0n;
  let totalPending = 0n;
  let totalFees = 0n;
  const domains = Object.entries(domainStats)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([id, raw]) => {
      const row = record(raw, `domain_stats.${id}`);
      const available = unsigned(row.available_liquidity, `${id}.available_liquidity`);
      const pending = unsigned(row.pending_outbound, `${id}.pending_outbound`);
      const fees = unsigned(row.fees, `${id}.fees`);
      totalAvailable += available;
      totalPending += pending;
      totalFees += fees;
      return {
        id,
        available: available.toString(),
        pending: pending.toString(),
        fees: fees.toString(),
        utilizationBps: basisPoints(pending, available + pending),
      };
    });

  const routes = Object.entries(routeStats)
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([id, raw]) => {
      const row = record(raw, `routes.${id}`);
      const deliveries = unsigned(row.deliveries, `${id}.deliveries`);
      const failures = unsigned(row.failures, `${id}.failures`);
      return {
        id,
        logicalOrigin: row.logical_origin,
        transportOrigin: row.transport_origin,
        active: row.active,
        deliveries: deliveries.toString(),
        failures: failures.toString(),
        successBps: basisPoints(deliveries, deliveries + failures),
      };
    });

  return {
    scenario: report.scenario,
    conservationOk: report.conservation_ok,
    stateDigest: report.state_digest,
    domains,
    routes,
    totals: {
      available: totalAvailable.toString(),
      pending: totalPending.toString(),
      fees: totalFees.toString(),
    },
  };
}
