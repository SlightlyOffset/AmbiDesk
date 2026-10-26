// Copies the project's source files into website/source so the site can display them.
// Run from anywhere: node website/sync-source.mjs
import { cpSync, mkdirSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const site = dirname(fileURLToPath(import.meta.url));
const root = join(site, "..");
const files = ["platformio.ini", "include/Config.h", "include/AppFSM.h", "include/WeatherData.h", "include/WeatherService.h",
  "include/DisplayEngine.h", "include/LcdDisplay.h", "include/Glyphs.h", "include/secrets.example.h", "src/main.cpp",
  "src/AppFSM.cpp", "src/WeatherService.cpp", "src/DisplayEngine.cpp", "src/LcdDisplay.cpp", "worker/src/index.js",
  "worker/dev-server.js", "worker/wrangler.toml"]; // secrets.h is never copied

rmSync(join(site, "source"), { recursive: true, force: true });
for (const f of files) {
  mkdirSync(dirname(join(site, "source", f)), { recursive: true });
  cpSync(join(root, f), join(site, "source", f));
}
console.log(`Copied ${files.length} files to website/source`);
