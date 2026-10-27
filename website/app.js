// Set to your deployed Worker (or local dev-server) URL to start with live data.
const AGGREGATOR_URL = "";

// 12x8 weather icon bitmaps mirror include/Glyphs.h
const ICON_ROWS = 8;
const ICON_COLS = 12;
const MAX_SLOTS = 9;
const INSPECT_MS = 20000; // like the firmware: back to Now after 20 s without a press

const RAW_ICONS = [
  ["     ##     ","  #      #  ","    ####    "," # ###### # "," # ###### # ","    ####    ","  #      #  ","     ##     "],
  ["            ","    ####    ","  ########  "," ########## "," ########## ","  ########  ","            ","            "],
  ["    ###     ","  #######   "," #########  ","  #######   ","            "," #  #  #    ","  #  #  #   "," #  #  #    "],
  ["   ####     ","  #######   "," #########  ","    ##      ","   ##       ","  ####      ","    ##      ","   ##       "]
];

function parseIconRow(rowStr) {
  console.assert(typeof rowStr === "string", "parseIconRow: rowStr must be string");
  console.assert(rowStr.length === ICON_COLS, "parseIconRow: rowStr must match ICON_COLS");
  const row = [];
  const limit = Math.min(rowStr.length, ICON_COLS);
  for (let c = 0; c < limit; c++) {
    row.push(rowStr[c] === "#");
  }
  return row;
}

function parseIcon(rawIcon) {
  console.assert(Array.isArray(rawIcon), "parseIcon: rawIcon must be array");
  console.assert(rawIcon.length === ICON_ROWS, "parseIcon: rawIcon must match ICON_ROWS");
  const icon = [];
  const limit = Math.min(rawIcon.length, ICON_ROWS);
  for (let r = 0; r < limit; r++) {
    icon.push(parseIconRow(rawIcon[r]));
  }
  return icon;
}

const ICONS = [];
for (let i = 0; i < RAW_ICONS.length; i++) {
  ICONS.push(parseIcon(RAW_ICONS[i]));
}

const NAMES = ["แดดออก", "มีเมฆ", "ฝนตก", "ฟ้าผ่า/พายุ"];

const RAW_DEMO = [
  ["Now 14:00", 33, 0, 10],
  ["+1h 15:00", 34, 1, 30],
  ["+3h 17:00", 31, 2, 70],
  ["+5h 19:00", 29, 3, 85],
  ["+12h 02:00", 27, 1, 40],
  ["+1d 27/10", 33, 0, 10],
  ["+2d 28/10", 32, 2, 60],
  ["+3d 29/10", 32, 1, 20],
  ["+4d 30/10", 34, 0, 5]
];

const DEMO = [];
for (let i = 0; i < Math.min(RAW_DEMO.length, MAX_SLOTS); i++) {
  const d = RAW_DEMO[i];
  DEMO.push({ lbl: d[0], t: d[1], ico: d[2], pop: d[3] });
}

function $(id) {
  console.assert(typeof id === "string", "$: id must be string");
  console.assert(id.length > 0, "$: id must not be empty");
  return document.getElementById(id);
}

function num(v, d) {
  console.assert(typeof d === "number", "num: default must be number");
  const parsed = Number(v);
  const result = Number.isFinite(parsed) ? Math.round(parsed) : d;
  console.assert(Number.isFinite(result), "num: result must be finite");
  return result;
}

const canvasEl = $("m");
const ctx = canvasEl ? canvasEl.getContext("2d") : null;
let slots = DEMO;
let idx = 0;
let lastPress = 0;

// The matrix shows only the weather icon; temperature and rain chance go in the text below.
function frame() {
  console.assert(ctx != null, "frame: 2d context required");
  console.assert(idx >= 0 && idx < slots.length, "frame: slot index out of bounds");
  ctx.shadowBlur = 0;
  ctx.fillStyle = "#0d1014";
  ctx.fillRect(0, 0, 600, 400);

  const currentSlot = slots[idx];
  const iconIdx = currentSlot.ico;
  const icon = ICONS[iconIdx];

  for (let phase = 0; phase < 2; phase++) {
    const lit = phase === 1;
    for (let y = 0; y < ICON_ROWS; y++) {
      const row = icon[y];
      for (let x = 0; x < ICON_COLS; x++) {
        const pixelLit = row[x];
        if (pixelLit !== lit) continue;
        ctx.shadowColor = lit ? "#ff5a36" : "transparent";
        ctx.shadowBlur = lit ? 14 : 0;
        ctx.fillStyle = lit ? "#ff5a36" : "#272c36";
        ctx.beginPath();
        ctx.arc(x * 50 + 25, y * 50 + 25, 17, 0, 7);
        ctx.fill();
      }
    }
  }
}

function describe() {
  console.assert(idx >= 0 && idx < slots.length, "describe: idx within bounds");
  console.assert(NAMES.length === 4, "describe: NAMES must have 4 labels");
  const s = slots[idx];
  const weatherName = NAMES[s.ico] || "";
  const readoutEl = $("readout");
  readoutEl.textContent = `${s.lbl}: ${s.t}°C, ${weatherName}, โอกาสฝนตก ${s.pop}%`;

  const chipsEl = $("chips");
  const chipButtons = Array.from(chipsEl.children);
  const maxChips = Math.min(chipButtons.length, MAX_SLOTS);
  for (let i = 0; i < maxChips; i++) {
    const btn = chipButtons[i];
    btn.setAttribute("aria-pressed", String(i === idx));
  }
}

function select(i) {
  console.assert(typeof i === "number", "select: i must be a number");
  console.assert(slots.length > 0, "select: slots must not be empty");
  const clamped = Math.max(0, Math.min(Math.floor(i), slots.length - 1));
  idx = clamped;
  lastPress = Date.now();
  describe();
  frame();
}

function handleChipClick(e) {
  console.assert(e != null, "handleChipClick: event required");
  const target = e.currentTarget;
  console.assert(target != null, "handleChipClick: currentTarget required");
  const slotIdx = Number(target.dataset.idx);
  if (Number.isFinite(slotIdx)) {
    select(slotIdx);
  }
}

function buildChips() {
  console.assert(Array.isArray(slots), "buildChips: slots must be array");
  console.assert(slots.length > 0, "buildChips: slots must not be empty");
  const chipsEl = $("chips");
  const buttons = [];
  const count = Math.min(slots.length, MAX_SLOTS);
  for (let i = 0; i < count; i++) {
    const s = slots[i];
    const b = document.createElement("button");
    b.type = "button";
    b.dataset.idx = String(i);
    const labelParts = s.lbl.split(" ");
    b.textContent = labelParts[0];
    b.title = s.lbl;
    b.onclick = handleChipClick;
    buttons.push(b);
  }
  chipsEl.replaceChildren(...buttons);
  idx = 0;
  lastPress = 0;
  describe();
  frame();
}

function normalise(j) {
  console.assert(j != null && typeof j === "object", "normalise: j must be an object");
  console.assert(MAX_SLOTS === 9, "normalise: MAX_SLOTS must be 9");
  const rawRows = Array.isArray(j.slots) && j.slots.length ? j.slots : [
    { lbl: "Now", t: j.cur_t, ico: j.cur_icon, pop: 0 },
    { lbl: "+1h", t: j.cur_t, ico: j.nxt_icon, pop: j.pop }
  ];
  const out = [];
  const limit = Math.min(rawRows.length, MAX_SLOTS);
  for (let i = 0; i < limit; i++) {
    const r = rawRows[i];
    const rawLbl = r.lbl != null ? String(r.lbl) : "";
    const lbl = rawLbl.slice(0, 10) || "Slot";
    const t = num(r.t, 0);
    const rawIco = num(r.ico, 1);
    const ico = Math.min(Math.max(rawIco, 0), 3);
    const rawPop = num(r.pop, 0);
    const pop = Math.min(Math.max(rawPop, 0), 100);
    out.push({ lbl, t, ico, pop });
  }
  return out;
}

async function connect(url) {
  const st = $("status");
  console.assert(typeof url === "string", "connect: url must be string");
  console.assert(st != null, "connect: status element required");
  if (!url) {
    slots = DEMO;
    buildChips();
    st.textContent = "แสดงข้อมูลตัวอย่าง";
    return;
  }
  st.textContent = "กำลังเชื่อมต่อ…";
  try {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) {
      throw new Error("HTTP " + res.status);
    }
    const data = await res.json();
    slots = normalise(data);
    buildChips();
    const parsedUrl = new URL(url);
    const host = parsedUrl.host;
    st.textContent = "ข้อมูลจริงจาก " + host;
    try {
      localStorage.setItem("ambidesk-url", url);
    } catch {}
  } catch (e) {
    slots = DEMO;
    buildChips();
    st.textContent = `เชื่อมต่อ ${url} ไม่สำเร็จ (${e.message}) กรุณาตรวจสอบ URL หรือสถานะเซิร์ฟเวอร์ — แสดงข้อมูลตัวอย่าง`;
  }
}

function handleSubmit(e) {
  console.assert(e != null, "handleSubmit: event required");
  console.assert(typeof e.preventDefault === "function", "handleSubmit: preventDefault required");
  e.preventDefault();
  const inputEl = $("url");
  const rawUrl = inputEl ? inputEl.value : "";
  connect(rawUrl.trim());
}

function checkInspectTimeout() {
  const now = Date.now();
  console.assert(typeof now === "number", "checkInspectTimeout: now must be number");
  console.assert(typeof lastPress === "number", "checkInspectTimeout: lastPress must be number");
  if (idx > 0 && now - lastPress > INSPECT_MS) {
    select(0);
  }
}

// Source code viewer: files are copied into website/source by sync-source.mjs.
const FILES = [
  "src/main.cpp", "src/AppFSM.cpp", "src/WeatherService.cpp", "src/DisplayEngine.cpp", "src/LcdDisplay.cpp",
  "include/Config.h", "include/AppFSM.h", "include/WeatherData.h", "include/WeatherService.h", "include/DisplayEngine.h",
  "include/LcdDisplay.h", "include/Glyphs.h", "include/secrets.example.h", "worker/src/index.js", "worker/dev-server.js",
  "worker/wrangler.toml", "platformio.ini"
];
const REPO = "SlightlyOffset/AmbiDesk";

async function showFile(p) {
  console.assert(typeof p === "string" && p.length > 0, "showFile: p must be non-empty string");
  console.assert(FILES.includes(p), "showFile: p must be in FILES allowlist");
  const code = $("code");
  code.textContent = "กำลังโหลด…";

  const filesEl = $("files");
  const buttons = Array.from(filesEl.children);
  const buttonLimit = Math.min(buttons.length, FILES.length);
  for (let i = 0; i < buttonLimit; i++) {
    const b = buttons[i];
    const isCurrent = b.dataset.p === p;
    b.setAttribute("aria-pressed", String(isCurrent));
  }

  const ghLink = $("gh");
  ghLink.href = `https://github.com/${REPO}/blob/main/${p}`;

  const candidates = ["source/" + p, `https://raw.githubusercontent.com/${REPO}/main/${p}`];
  const candidateLimit = Math.min(candidates.length, 2);
  for (let i = 0; i < candidateLimit; i++) {
    const u = candidates[i];
    try {
      const r = await fetch(u);
      if (r.ok) {
        code.textContent = await r.text();
        return;
      }
    } catch {}
  }
  code.textContent = `โหลด ${p} ไม่ได้ ให้รัน node website/sync-source.mjs แล้วเปิดเว็บผ่านเซิร์ฟเวอร์ หรือเปิดไฟล์บน GitHub`;
}

function handleFileClick(e) {
  console.assert(e != null, "handleFileClick: event required");
  const target = e.currentTarget;
  console.assert(target != null, "handleFileClick: currentTarget required");
  const p = target.dataset.p;
  if (p) {
    showFile(p);
  }
}

function initFileButtons() {
  console.assert(Array.isArray(FILES), "initFileButtons: FILES must be array");
  console.assert(FILES.length === 17, "initFileButtons: FILES length must be 17");
  const filesContainer = $("files");
  const fileButtons = [];
  const maxFiles = Math.min(FILES.length, 20);
  for (let i = 0; i < maxFiles; i++) {
    const filePath = FILES[i];
    const b = document.createElement("button");
    b.type = "button";
    b.dataset.p = filePath;
    b.textContent = filePath;
    b.onclick = handleFileClick;
    fileButtons.push(b);
  }
  filesContainer.replaceChildren(...fileButtons);
}

function initApp() {
  console.assert(Array.isArray(DEMO), "initApp: DEMO array initialized");
  console.assert(DEMO.length === MAX_SLOTS, "initApp: DEMO length matches MAX_SLOTS");
  const formEl = $("src");
  if (formEl) {
    formEl.onsubmit = handleSubmit;
  }
  setInterval(checkInspectTimeout, 1000);

  let saved = "";
  try {
    saved = localStorage.getItem("ambidesk-url") || "";
  } catch {}
  const searchParams = new URLSearchParams(location.search);
  const start = searchParams.get("api") || AGGREGATOR_URL || saved;
  const urlInput = $("url");
  if (urlInput) {
    urlInput.value = start;
  }
  buildChips();
  if (start) {
    connect(start);
  }

  initFileButtons();
  showFile(FILES[0]);
}

initApp();
