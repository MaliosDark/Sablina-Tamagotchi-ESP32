#!/usr/bin/env node
// Small HTTP server that scans WiFi via nmcli and serves results as JSON.
// Usage: node simulator/wifi-scan-server.js
// The simulator fetches http://localhost:3210/scan every ~8 seconds.

const http = require("http");
const { execSync } = require("child_process");

const PORT = 3210;

function scanWifi() {
  try {
    // Force a rescan first (may fail silently if no device)
    try { execSync("nmcli dev wifi rescan 2>/dev/null", { timeout: 5000 }); } catch (_) {}

    const raw = execSync(
      "nmcli -t -f SSID,SIGNAL,SECURITY,BSSID,FREQ dev wifi list 2>/dev/null",
      { timeout: 8000, encoding: "utf8" }
    );

    const lines = raw.trim().split("\n").filter(Boolean);
    const networks = lines.map((line) => {
      // nmcli -t uses ':' as separator but BSSID also contains ':'
      // Format: SSID:SIGNAL:SECURITY:BSSID:FREQ
      // We parse from the right since SSID can contain ':'
      const parts = line.split(":");
      if (parts.length < 5) return null;

      // FREQ is last, BSSID is 6 colon-separated hex values before FREQ
      const freq = parts[parts.length - 1];
      const bssid = parts.slice(parts.length - 7, parts.length - 1).join(":");
      const security = parts[parts.length - 8] || "";
      const signal = parseInt(parts[parts.length - 9], 10) || 0;
      const ssid = parts.slice(0, parts.length - 9).join(":");

      return { ssid, signal, security, bssid, freq };
    }).filter(Boolean);

    return { ok: true, networks, timestamp: Date.now() };
  } catch (err) {
    return { ok: false, error: err.message, networks: [], timestamp: Date.now() };
  }
}

const server = http.createServer((req, res) => {
  // Security: only accept requests from localhost origins (the simulator page).
  // This prevents other local apps or browser tabs from polling the WiFi scan API.
  const origin   = req.headers["origin"]   || "";
  const referer  = req.headers["referer"]  || "";
  const hostHdr  = req.headers["host"]     || "";
  const source   = origin || referer;
  const isLocalOrigin =
    source === "" ||                            // same-origin same-tab fetches
    source.startsWith("http://localhost") ||
    source.startsWith("http://127.");
  const isLocalHost =
    hostHdr.startsWith("localhost:") ||
    hostHdr.startsWith("127.") ||
    hostHdr === "localhost";

  if (!isLocalOrigin || !isLocalHost) {
    res.writeHead(403, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Forbidden" }));
    return;
  }

  res.setHeader("Access-Control-Allow-Origin", source || "http://localhost");
  res.setHeader("Access-Control-Allow-Methods", "GET");
  res.setHeader("Content-Type", "application/json");

  if (req.url === "/scan" && req.method === "GET") {
    const result = scanWifi();
    res.writeHead(200);
    res.end(JSON.stringify(result));
    return;
  }

  res.writeHead(404);
  res.end(JSON.stringify({ error: "Not found" }));
});

server.listen(PORT, "127.0.0.1", () => {
  console.log(`WiFi scan server running at http://127.0.0.1:${PORT}/scan`);
  console.log("The simulator will auto-connect when you enable Real WiFi.");
});
