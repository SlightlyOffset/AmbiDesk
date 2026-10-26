// Set to your deployed Worker (or local dev-server) URL to start with live data.
const AGGREGATOR_URL = "";

// Bitmaps and 3x5 font mirror include/Glyphs.h
const ICONS = [
  ["     ##     ","  #      #  ","    ####    "," # ###### # "," # ###### # ","    ####    ","  #      #  ","     ##     "],
  ["            ","    ####    ","  ########  "," ########## "," ########## ","  ########  ","            ","            "],
  ["    ###     ","  #######   "," #########  ","  #######   ","            "," #  #  #    ","  #  #  #   "," #  #  #    "],
  ["   ####     ","  #######   "," #########  ","    ##      ","   ##       ","  ####      ","    ##      ","   ##       "]
].map(r => r.map(s => [...s].map(c => c === "#")));
const NAMES = ["แดดออก", "มีเมฆ", "ฝนตก", "ฟ้าผ่า/พายุ"];
const INSPECT_MS = 20000; // like the firmware: back to Now after 20 s without a press
const DEMO = [["Now 14:00",33,0,10],["+1h 15:00",34,1,30],["+3h 17:00",31,2,70],["+5h 19:00",29,3,85],
  ["+12h 02:00",27,1,40],["+1d 27/10",33,0,10],["+2d 28/10",32,2,60],["+3d 29/10",32,1,20],["+4d 30/10",34,0,5]]
  .map(([lbl, t, ico, pop]) => ({ lbl, t, ico, pop }));

const $ = id => document.getElementById(id);
const ctx = $("m").getContext("2d");
let slots = DEMO, idx = 0, lastPress = 0;

// The matrix shows only the weather icon; temperature and rain chance go in the text below.
function frame() {
  ctx.shadowBlur = 0;
  ctx.fillStyle = "#0d1014";
  ctx.fillRect(0, 0, 600, 400);
  const icon = ICONS[slots[idx].ico];
  for (const lit of [false, true]) for (let y = 0; y < 8; y++) for (let x = 0; x < 12; x++) {
    if (icon[y][x] !== lit) continue;
    ctx.shadowColor = lit ? "#ff5a36" : "transparent";
    ctx.shadowBlur = lit ? 14 : 0;
    ctx.fillStyle = lit ? "#ff5a36" : "#272c36";
    ctx.beginPath(); ctx.arc(x * 50 + 25, y * 50 + 25, 17, 0, 7); ctx.fill();
  }
}

function describe() {
  const s = slots[idx];
  $("readout").textContent = `${s.lbl}: ${s.t}°C, ${NAMES[s.ico]}, โอกาสฝนตก ${s.pop}%`;
  [...$("chips").children].forEach((b, i) => b.setAttribute("aria-pressed", i === idx));
}

function select(i) { idx = i; lastPress = Date.now(); describe(); frame(); }

function buildChips() {
  $("chips").replaceChildren(...slots.map((s, i) => {
    const b = document.createElement("button");
    b.type = "button"; b.textContent = s.lbl.split(" ")[0]; b.title = s.lbl;
    b.onclick = () => select(i);
    return b;
  }));
  idx = 0; lastPress = 0; describe(); frame();
}

const num = (v, d) => Number.isFinite(+v) ? Math.round(+v) : d;
function normalise(j) {
  const rows = Array.isArray(j.slots) && j.slots.length ? j.slots
    : [{ lbl: "Now", t: j.cur_t, ico: j.cur_icon, pop: 0 }, { lbl: "+1h", t: j.cur_t, ico: j.nxt_icon, pop: j.pop }];
  return rows.slice(0, 9).map(r => ({ lbl: String(r.lbl ?? "").slice(0, 10) || "Slot",
    t: num(r.t, 0), ico: Math.min(Math.max(num(r.ico, 1), 0), 3), pop: Math.min(Math.max(num(r.pop, 0), 0), 100) }));
}

async function connect(url) {
  const st = $("status");
  if (!url) { slots = DEMO; buildChips(); st.textContent = "แสดงข้อมูลตัวอย่าง"; return; }
  st.textContent = "กำลังเชื่อมต่อ…";
  try {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error("HTTP " + res.status);
    slots = normalise(await res.json()); buildChips();
    st.textContent = "ข้อมูลจริงจาก " + new URL(url).host;
    try { localStorage.setItem("ambidesk-url", url); } catch {}
  } catch (e) {
    slots = DEMO; buildChips();
    st.textContent = `Couldn’t reach ${url} (${e.message}). Check the URL and that the aggregator is running. แสดงข้อมูลตัวอย่าง`;
  }
}

$("src").onsubmit = e => { e.preventDefault(); connect($("url").value.trim()); };
setInterval(() => { if (idx && Date.now() - lastPress > INSPECT_MS) select(0); }, 1000);

let saved = ""; try { saved = localStorage.getItem("ambidesk-url") || ""; } catch {}
const start = new URLSearchParams(location.search).get("api") || AGGREGATOR_URL || saved;
$("url").value = start;
buildChips();
if (start) connect(start);

// Source code viewer: files are copied into website/source by sync-source.mjs.
const FILES = ["src/main.cpp","src/AppFSM.cpp","src/WeatherService.cpp","src/DisplayEngine.cpp","src/LcdDisplay.cpp",
  "include/Config.h","include/AppFSM.h","include/WeatherData.h","include/WeatherService.h","include/DisplayEngine.h",
  "include/LcdDisplay.h","include/Glyphs.h","include/secrets.example.h","worker/src/index.js","worker/dev-server.js",
  "worker/wrangler.toml","platformio.ini"];
const REPO = "SlightlyOffset/AmbiDesk";
async function showFile(p) {
  const code = $("code");
  code.textContent = "กำลังโหลด…";
  [...$("files").children].forEach(b => b.setAttribute("aria-pressed", b.dataset.p === p));
  $("gh").href = `https://github.com/${REPO}/blob/main/${p}`;
  for (const u of ["source/" + p, `https://raw.githubusercontent.com/${REPO}/main/${p}`]) {
    try { const r = await fetch(u); if (r.ok) { code.textContent = await r.text(); return; } } catch {}
  }
  code.textContent = `โหลด ${p} ไม่ได้ ให้รัน node website/sync-source.mjs แล้วเปิดเว็บผ่านเซิร์ฟเวอร์ หรือเปิดไฟล์บน GitHub`;
}
$("files").replaceChildren(...FILES.map(p => {
  const b = document.createElement("button");
  b.type = "button"; b.dataset.p = p; b.textContent = p; b.onclick = () => showFile(p);
  return b;
}));
showFile(FILES[0]);
