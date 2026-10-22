import http from "node:http";
import os from "node:os";
import worker from "./src/index.js";

const PORT = 3000;

function getLocalIps() {
  const interfaces = os.networkInterfaces();
  const list = [];
  for (const name of Object.keys(interfaces)) {
    for (const iface of interfaces[name]) {
      if (iface.family === "IPv4" && !iface.internal) {
        list.push({ name, address: iface.address });
      }
    }
  }
  return list;
}

const server = http.createServer(async (req, res) => {
  const clientIp = req.socket.remoteAddress;
  const fullUrl = `http://localhost:${PORT}${req.url}`;
  const request = new Request(fullUrl, {
    method: req.method,
    headers: req.headers
  });

  try {
    const workerRes = await worker.fetch(request, {}, {});
    const bodyText = await workerRes.text();

    const headers = Object.fromEntries(workerRes.headers.entries());
    headers["Content-Length"] = Buffer.byteLength(bodyText);
    headers["Connection"] = "close";

    res.writeHead(workerRes.status, headers);
    res.end(bodyText);

    const timestamp = new Date().toLocaleTimeString();
    console.log(`[${timestamp}] ${req.method} ${req.url} from ${clientIp} -> ${workerRes.status} ${bodyText}`);
  } catch (err) {
    console.error(`Error handling request:`, err);
    res.writeHead(500, { "Content-Type": "text/plain" });
    res.end("Internal Server Error: " + err.message);
  }
});

server.listen(PORT, "0.0.0.0", () => {
  const ips = getLocalIps();
  console.log("==================================================");
  console.log("      AmbiDesk Local Micro-Aggregator Server      ");
  console.log("==================================================");
  console.log(`Listening on: http://0.0.0.0:${PORT}\n`);
  console.log("Detected local network interfaces:");
  for (const item of ips) {
    console.log(`  - ${item.name}: http://${item.address}:${PORT}/`);
  }
  const preferred = ips.find(i => i.name.toLowerCase().includes("wi-fi") || i.name.toLowerCase().includes("wireless")) || ips.find(i => i.address.startsWith("192.168.1.")) || ips[0];
  const targetIp = preferred ? preferred.address : "127.0.0.1";

  console.log("\nCopy these settings into your include/secrets.h:");
  console.log(`  #define AGGREGATOR_HOST     "${targetIp}"`);
  console.log(`  #define AGGREGATOR_PORT     ${PORT}`);
  console.log(`  #define AGGREGATOR_PATH     "/"`);
  console.log(`  #define AGGREGATOR_USE_SSL  false`);
  console.log("==================================================");
});
