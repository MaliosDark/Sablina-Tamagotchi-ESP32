const canvas = document.getElementById("lcd");
const ctx = canvas.getContext("2d");

const SCREEN = { w: 320, h: 172 };
const GAME = { w: 128, h: 128 };
const LEFT_X = 0;
let LEFT_W = 236;
let RIGHT_X = LEFT_X + LEFT_W;
const RIGHT_W_BASE = SCREEN.w - 236;
let RIGHT_W = RIGHT_W_BASE;
const GAME_TOPBAR_H = 12;
const GAME_X = LEFT_X + 1;
const GAME_Y = GAME_TOPBAR_H + 2;
let GAME_W = LEFT_W - 2;
const GAME_H = SCREEN.h - GAME_Y - 1;
let PANEL_X = RIGHT_X + 4;
const PANEL_Y = 6;
let PANEL_W = RIGHT_W - 8;
const PANEL_H = SCREEN.h - PANEL_Y - 2;

let panelHidden = false;

function activeGameW() { return GAME_W; }

function applyLayoutVars(hidden) {
  if (hidden) {
    LEFT_W = SCREEN.w;
    GAME_W = SCREEN.w - 2;
    RIGHT_X = SCREEN.w;
    RIGHT_W = 0;
    PANEL_X = SCREEN.w;
    PANEL_W = 0;
  } else {
    LEFT_W = 236;
    GAME_W = LEFT_W - 2;
    RIGHT_X = LEFT_X + LEFT_W;
    RIGHT_W = RIGHT_W_BASE;
    PANEL_X = RIGHT_X + 4;
    PANEL_W = RIGHT_W - 8;
  }
}

function setLayoutPanelHidden(hidden) {
  panelHidden = hidden;
  if (hidden) {
    LEFT_W = SCREEN.w;
    GAME_W = SCREEN.w - 2;
    RIGHT_X = SCREEN.w;
    RIGHT_W = 0;
    PANEL_X = SCREEN.w;
    PANEL_W = 0;
  } else {
    LEFT_W = 236;
    GAME_W = LEFT_W - 2;
    RIGHT_X = LEFT_X + LEFT_W;
    RIGHT_W = RIGHT_W_BASE;
    PANEL_X = RIGHT_X + 4;
    PANEL_W = RIGHT_W - 8;
  }
}

const SPRITES = window.ORIGINAL_SPRITES || {};
const spriteCache = {};
const spriteCanvasCache = {};
const STORE_KEY = "sablina_sim_state_v4";

// ── Security: HTML escaping helper ───────────────────────────────
// Use instead of raw string interpolation whenever injecting into innerHTML.
function escHtml(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

/* ── Visual feedback state (sound & vibration indicators) ── */
let _soundIndicatorUntilMs = 0;
let _vibrateIndicatorUntilMs = 0;

/* ── BLE Peer Communication via BroadcastChannel ── */
const BLE_CHANNEL_NAME = "sablina_ble_peer";
let bleChannel = null;
let blePeer = null;          // { name, mood, stage, activity, battery, timestamp }
let blePeerInteraction = ""; // current interaction text
let blePeerInteractionMs = 0;
let blePeerConversation = null;
const BLE_PEER_TIMEOUT = 8000; // peer considered gone after 8s no message
const BLE_BROADCAST_INTERVAL = 3000;
let _lastBleBroadcastMs = 0;

function setBlePeerConversation(localText, peerText, nowMs = performance.now()) {
  blePeerConversation = {
    localText: localText || "",
    peerText: peerText || "",
    startedMs: nowMs,
  };
}

function activeBlePeerConversation(nowMs = performance.now()) {
  return blePeerConversation && (nowMs - blePeerConversation.startedMs) < 4500;
}

function ensureSocialMemory(peerLike) {
  if (!peerLike || !peerLike.id) return null;
  const id = String(peerLike.id);
  if (!state.socialMemory[id]) {
    state.socialMemory[id] = {
      id,
      name: peerLike.name || "Nearby Sablina",
      encounters: 0,
      chats: 0,
      affinity: 0,
      giftsGiven: 0,
      giftsReceived: 0,
      lastGift: "",
      lastSeenMs: performance.now(),
    };
  }
  state.socialMemory[id].name = peerLike.name || state.socialMemory[id].name;
  state.socialMemory[id].lastSeenMs = performance.now();
  return state.socialMemory[id];
}

function socialBondLabel(memory) {
  if (!memory) return "NONE";
  if (memory.affinity >= 75 && memory.encounters >= 6) return "BESTIE";
  if (memory.affinity >= 50 && memory.encounters >= 4) return "ALLY";
  if (memory.affinity >= 25 && memory.encounters >= 2) return "FRIEND";
  return "NEW";
}

function noteSocialEncounter(peerLike, justDetected = false) {
  const memory = ensureSocialMemory(peerLike);
  if (!memory) return null;
  if (justDetected) {
    memory.encounters += 1;
    memory.affinity = clamp(memory.affinity + 2, 0, 100);
    persistState();
  }
  return memory;
}

function socialGiftKindFromAction(action, fromLine = "", text = "") {
  if (action === "giftSnack") return "snack";
  if (action === "giftRest") return "rest";
  if (action === "giftClean") return "clean";
  if (action === "giftCoin") return "coin";
  const lower = `${fromLine} ${text}`.toLowerCase();
  if (lower.includes("snack gift")) return "snack";
  if (lower.includes("rest gift")) return "rest";
  if (lower.includes("clean gift")) return "clean";
  if (lower.includes("coin gift")) return "coin";
  return "";
}

function applyIncomingGift(giftKind) {
  if (giftKind === "snack") {
    state.hun = clamp(state.hun + 10, 0, 100);
    sfxFeed();
  } else if (giftKind === "rest") {
    state.fat = clamp(state.fat + 10, 0, 100);
    sfxSleep();
  } else if (giftKind === "clean") {
    state.cle = clamp(state.cle + 10, 0, 100);
    sfxHatch();
  } else if (giftKind === "coin") {
    state.exp = clamp(state.exp + 8, 0, 9999);
    sfxCoin();
  }
}

function registerIncomingSocialInteraction(peerLike, action, fromLine = "", text = "") {
  const memory = ensureSocialMemory(peerLike);
  if (!memory) return null;
  memory.chats += 1;
  memory.affinity = clamp(memory.affinity + 1, 0, 100);
  const giftKind = socialGiftKindFromAction(action, fromLine, text);
  if (giftKind) {
    memory.giftsReceived += 1;
    memory.lastGift = giftKind;
    memory.affinity = clamp(memory.affinity + 4, 0, 100);
    applyIncomingGift(giftKind);
  }
  persistState();
  return memory;
}

function registerOutgoingSocialInteraction(peerLike, action, fromLine = "", text = "") {
  const memory = ensureSocialMemory(peerLike);
  if (!memory) return null;
  memory.chats += 1;
  memory.affinity = clamp(memory.affinity + 1, 0, 100);
  const giftKind = socialGiftKindFromAction(action, fromLine, text);
  if (giftKind) {
    memory.giftsGiven += 1;
    memory.lastGift = giftKind;
    memory.affinity = clamp(memory.affinity + 3, 0, 100);
  }
  persistState();
  return memory;
}

function initBleChannel() {
  try {
    bleChannel = new BroadcastChannel(BLE_CHANNEL_NAME);
    bleChannel.onmessage = (e) => {
      if (!state.ble) return;
      const d = e.data;
      if (d && d.type === "sablina_peer" && d.id !== _bleInstanceId) {
        const wasKnownPeer = blePeer && blePeer.id === d.id && (performance.now() - blePeer.timestamp) < BLE_PEER_TIMEOUT;
        blePeer = {
          id: d.id,
          name: d.name,
          mood: d.mood,
          stage: d.stage,
          activity: d.activity,
          battery: d.battery,
          hatched: d.hatched,
          alive: d.alive,
          timestamp: performance.now(),
        };
        noteSocialEncounter(blePeer, !wasKnownPeer);
      }
      if (d && d.type === "sablina_interact" && d.targetId === _bleInstanceId) {
        blePeerInteraction = d.text;
        blePeerInteractionMs = performance.now();
        registerIncomingSocialInteraction({ id: d.fromId, name: d.fromName }, d.action, d.fromLine, d.text);
        setBlePeerConversation(
          `${state.name}: ${d.toLine || "I heard you."}`,
          `${d.fromName || "Nearby Sablina"}: ${d.fromLine || d.text}`,
          blePeerInteractionMs,
        );
        // React to incoming interaction
        switch (d.action) {
          case "giftSnack":
          case "giftRest":
          case "giftClean":
          case "giftCoin":
            triggerVibration(180);
            break;
          case "play":
            state.fat = clamp(state.fat + 3, 0, 100);
            sfxPlay(); triggerVibration(200);
            break;
          case "explore":
            state.traits.curiosity = clamp(state.traits.curiosity + 1, 0, 100);
            sfxHatch();
            break;
          case "shareFood":
            state.hun = clamp(state.hun + 4, 0, 100);
            sfxFeed();
            break;
          case "comfort":
            state.fat = clamp(state.fat + 4, 0, 100);
            state.traits.stress = clamp(state.traits.stress - 1, 0, 100);
            sfxSleep();
            break;
          case "help":
            state.fat = clamp(state.fat + 3, 0, 100);
            sfxHatch();
            break;
          case "joke":
            state.fat = clamp(state.fat + 2, 0, 100);
            sfxCoin();
            break;
          default: // greet
            state.fat = clamp(state.fat + 1, 0, 100);
            sfxHatch();
            break;
        }
      }
    };
  } catch (_) { /* BroadcastChannel not supported */ }
}

const _bleInstanceId = Math.random().toString(36).slice(2, 10);

function bleBroadcastState(nowMs) {
  if (!bleChannel || !state.ble) return;
  if (nowMs - _lastBleBroadcastMs < BLE_BROADCAST_INTERVAL) return;
  _lastBleBroadcastMs = nowMs;
  bleChannel.postMessage({
    type: "sablina_peer",
    id: _bleInstanceId,
    name: state.name,
    mood: state.mood,
    stage: state.stage,
    activity: state.activity,
    battery: Math.round(state.battery),
    hatched: state.hatched,
    alive: state.alive,
  });
}

function bleSendInteraction(action, text, fromLine = "", toLine = "") {
  if (!bleChannel || !blePeer) return;
  bleChannel.postMessage({
    type: "sablina_interact",
    targetId: blePeer.id,
    fromId: _bleInstanceId,
    fromName: state.name,
    action,
    text,
    fromLine,
    toLine,
  });
}

function isPeerAlive() {
  return blePeer && (performance.now() - blePeer.timestamp) < BLE_PEER_TIMEOUT;
}

const screens = {
  HOME: "home",
  PET_STATUS: "pet_status",
  ENVIRONMENT: "environment",
  FOREST: "forest",
  SYSTEM: "system",
  SHOP: "shop",
  ACHIEVEMENTS: "achievements",
  TOOLS: "tools",
  STATS: "stats",
  WIFI_FOOD: "wifi_food",
};

const SHOP_FOODS = [
  { sprite: "f01", name: "Rice Ball", cost: 2, hunger: 8 },
  { sprite: "f02", name: "Sandwich", cost: 3, hunger: 10 },
  { sprite: "f03", name: "Juice", cost: 2, hunger: 6 },
  { sprite: "f04", name: "Cake", cost: 5, hunger: 14 },
  { sprite: "f05", name: "Cookie", cost: 1, hunger: 5 },
  { sprite: "f06", name: "Candy", cost: 1, hunger: 4 },
  { sprite: "f07", name: "Popcorn", cost: 2, hunger: 7 },
  { sprite: "f08", name: "Donut", cost: 3, hunger: 9 },
  { sprite: "f09", name: "Pizza", cost: 4, hunger: 12 },
  { sprite: "f10", name: "Burger", cost: 4, hunger: 12 },
  { sprite: "f11", name: "Ice Cream", cost: 3, hunger: 7 },
  { sprite: "f12", name: "Ramen", cost: 5, hunger: 15 },
  { sprite: "f13", name: "Apple", cost: 1, hunger: 5 },
  { sprite: "f14", name: "Banana", cost: 1, hunger: 5 },
  { sprite: "f15", name: "Milk", cost: 2, hunger: 7 },
  { sprite: "f16", name: "Bread", cost: 2, hunger: 8 },
  { sprite: "f17", name: "Egg", cost: 2, hunger: 7 },
  { sprite: "f18", name: "Fish", cost: 4, hunger: 11 },
  { sprite: "f19", name: "Steak", cost: 6, hunger: 18 },
  { sprite: "f20", name: "Salad", cost: 3, hunger: 9 },
  { sprite: "f21", name: "Soup", cost: 3, hunger: 10 },
  { sprite: "f22", name: "Tea", cost: 1, hunger: 4 },
  { sprite: "pancho", name: "Pancho", cost: 5, hunger: 14 },
  { sprite: "sushi", name: "Sushi", cost: 6, hunger: 16 },
  { sprite: "watercooler", name: "Water", cost: 0, hunger: 3 },
  { sprite: "warn", name: "Medicine", cost: 10, hunger: 5, isMedicine: true },
];

const state = {
  name: "Sablina",
  hun: 72,
  fat: 65,
  cle: 80,
  exp: 42,
  battery: 100,
  charging: false,
  wifi: true,
  ble: true,
  llm: true,
  bubble: "",
  targetRoom: "LIVING ROOM",
  alive: true,
  hatched: false,
  hatchFrame: 0,
  hatchTickMs: 0,
  deathMs: 0,

  screen: screens.HOME,
  scene: "home",
  sceneFrame: 0,
  sceneTickMs: 0,
  sceneStartMs: 0,
  petFrame: 0,
  petTickMs: 0,
  forestFrame: 0,
  forestTickMs: 0,
  darkMode: false,

  systemPage: 0,
  shopScroll: 0,
  shopCursor: 0,
  iconsLastInteractMs: performance.now(),
  roomTheme: "roomwhite",
  galleryIndex: 0,
  statsPage: 0,
  toolsPage: 0,
  achievePage: 0,
  realWifi: false,
  realWifiNetworks: [],
  llmScreenUntilMs: 0,

  // Lifetime counters for achievements
  lifetime: {
    foodEaten: 0,
    handshakes: 0,
    pmkidCaptures: 0,
    deauths: 0,
    wifiScans: 0,
    networksFound: 0,
    coinsEarned: 0,
    coinsSpent: 0,
    gamesPlayed: 0,
    gamesWon: 0,
    cleans: 0,
    sleeps: 0,
    pets: 0,
    maxNetworks: 0,
    daysAlive: 0,
  },

  // Audit log: last 30 WiFi events
  auditLog: [],

  miniGame: {
    active: false,
    targetX: 0,
    cursorX: 64,
    speed: 1.5,
    dir: 1,
    score: 0,
    round: 0,
    maxRounds: 5,
    resultMs: 0,
    lastHit: false,
  },

  bleStats: {
    deviceCount: 0,
    lastScanMs: 0,
  },

  socialMemory: {},

  vibrating: false,
  vibrateUntilMs: 0,

  mood: "CALM",
  stage: "BABY",
  prevStage: "BABY",   // tracks last stage to detect transitions
  activity: "IDLE",

  ageMinutes: 0,
  ageHours: 0,
  ageDays: 0,

  wifiStats: {
    netCount: 4,
    strongCount: 2,
    hiddenCount: 1,
    openCount: 1,
    wpaCount: 3,
    avgRSSI: -71,
    scanRunning: false,
  },

  traits: {
    curiosity: 70,
    activity: 60,
    stress: 40,
  },

  controls: {
    tftBrightnessIndex: 1,
    ledBrightnessIndex: 1,
    soundEnabled: true,
    neoPixelsEnabled: true,
    autoSleep: true,
    autoSaveMs: 30000,
  },

  engine: {
    lastLogicMs: 0,
    lastHungerMs: 0,
    lastHappyMs: 0,
    lastHealthMs: 0,
    lastAgeMs: 0,
    lastWifiScanMs: 0,
    lastBleScanMs: 0,
    lastBatteryMs: 0,
    lastTraitMs: 0,
    lastCoinBonusMs: 0,
    lastDecisionMs: 0,
    decisionEveryMs: 10000,
    activityUntilMs: 0,
    activityStepMs: 0,
    lastAutoSaveMs: 0,
    lastLocalThoughtMs: 0,
    bootMs: performance.now(),
    zeroStatMs: 0,
    criticalStatMs: 0,   // timestamp when any single stat first hit critical (≤5)
    sickMs: 0,           // timestamp when pet became sick (0 = healthy)
    lastEvolveMs: 0,     // timestamp of last stage evolution (for celebration)
  },

  twoBtn: {
    cursor: 0,
  },

  // ── WiFi Food – Sablina's hunting grounds ─────────────────────
  wifiFood: {
    mode: "IDLE",            // IDLE, SCAN, PROBES, DEAUTH, HANDSHAKE, PMKID, BEACON
    channel: 1,
    hopping: false,
    lastHopMs: 0,
    selectedAP: 0,
    menuIdx: 0,
    totalPackets: 0,
    mgmtPackets: 0,
    dataPackets: 0,
    eapolPackets: 0,
    deauthsSent: 0,
    probes: 0,               // probe requests caught
    probeSnacks: 0,          // food earned from probes
    beaconsSent: 0,          // fake beacons spammed
    confused: 0,             // clients confused by beacon spam
    aps: [],                 // {bssid, ssid, rssi, channel, encryption, clients, handshake, pmkid}
    clients: [],             // {mac, apBssid, rssi, packets}
    handshakes: [],          // {apBssid, clientMac, ssid, messages, complete}
    pmkids: [],              // {apBssid, ssid, pmkid}
    lastTickMs: 0,
  },

  // ── ReAct Agent state ───────────────────────────────────────────
  agent: {
    currentTool: "none",       // last tool selected by the agent
    lastThought: "",           // simulated LLM flavor text
    lastResult: "",            // short result description
    cycleCount: 0,             // how many agent cycles have run
    lastCycleMs: 0,            // timestamp of last cycle
    cyclePeriodMs: 12000,      // normal cycle period
    urgentPeriodMs: 4000,      // period when needs are critical
  },
};

const refs = {
  petName: document.getElementById("petName"),
  hun: document.getElementById("hun"),
  fat: document.getElementById("fat"),
  cle: document.getElementById("cle"),
  exp: document.getElementById("exp"),

  hunVal: document.getElementById("hunVal"),
  fatVal: document.getElementById("fatVal"),
  cleVal: document.getElementById("cleVal"),
  expVal: document.getElementById("expVal"),

  wifi: document.getElementById("wifi"),
  ble: document.getElementById("ble"),
  llm: document.getElementById("llm"),
  darkMode: document.getElementById("darkMode"),
  bubble: document.getElementById("bubble"),

  netCount: document.getElementById("netCount"),
  strongCount: document.getElementById("strongCount"),
  hiddenCount: document.getElementById("hiddenCount"),
  openCount: document.getElementById("openCount"),
  wpaCount: document.getElementById("wpaCount"),
  avgRssi: document.getElementById("avgRssi"),

  curiosity: document.getElementById("curiosity"),
  activityTrait: document.getElementById("activityTrait"),
  stress: document.getElementById("stress"),

  autoSleep: document.getElementById("autoSleep"),
  autoSaveMs: document.getElementById("autoSaveMs"),
  soundEnabled: document.getElementById("soundEnabled"),
  neoPixelsEnabled: document.getElementById("neoPixelsEnabled"),
  tftBrightnessIndex: document.getElementById("tftBrightnessIndex"),
  ledBrightnessIndex: document.getElementById("ledBrightnessIndex"),

  realWifi: document.getElementById("realWifi"),
  charging: document.getElementById("charging"),

  moodBadge: document.getElementById("moodBadge"),
  stageBadge: document.getElementById("stageBadge"),
  activityBadge: document.getElementById("activityBadge"),
  ageBadge: document.getElementById("ageBadge"),
  btnA: document.getElementById("btnA"),
  btnB: document.getElementById("btnB"),
  sidebarToggle: document.getElementById("sidebarToggle"),
};

function clamp(v, min, max) {
  return Math.max(min, Math.min(max, v));
}

function pick(list) {
  return list[Math.floor(Math.random() * list.length)];
}

function randomInt(min, max) {
  return Math.floor(min + Math.random() * (max - min + 1));
}

function inferRoomFromStateAndText(text) {
  const msg = String(text || "").toLowerCase();

  if (state.hun < 30 || msg.includes("hungry") || msg.includes("food") || msg.includes("eat")) {
    return "KITCHEN";
  }
  if (state.cle < 30 || msg.includes("clean") || msg.includes("dirty") || msg.includes("wash")) {
    return "BATHROOM";
  }
  if (state.fat < 30 || msg.includes("sleep") || msg.includes("tired") || msg.includes("rest")) {
    return "BEDROOM";
  }
  if (msg.includes("play") || msg.includes("game") || msg.includes("fun")) {
    return "PLAYROOM";
  }
  if (msg.includes("scan") || msg.includes("discover") || msg.includes("network")) {
    return "LAB";
  }
  return "LIVING ROOM";
}

function rgb565ToRgb(v) {
  const r5 = (v >> 11) & 0x1f;
  const g6 = (v >> 5) & 0x3f;
  const b5 = v & 0x1f;
  return {
    r: Math.round((r5 * 255) / 31),
    g: Math.round((g6 * 255) / 63),
    b: Math.round((b5 * 255) / 31),
  };
}

function buildImageData(cacheKey, w, h, pixels, transparentZero) {
  if (spriteCache[cacheKey]) return spriteCache[cacheKey];

  const img = ctx.createImageData(w, h);
  for (let i = 0; i < pixels.length; i++) {
    const px = pixels[i] >>> 0;
    const { r, g, b } = rgb565ToRgb(px);
    const p = i * 4;
    img.data[p] = r;
    img.data[p + 1] = g;
    img.data[p + 2] = b;
    img.data[p + 3] = transparentZero && px === 0 ? 0 : 255;
  }

  spriteCache[cacheKey] = img;
  return img;
}

function getSpriteImageData(name) {
  const s = SPRITES[name];
  if (!s || !Array.isArray(s.data)) return null;
  return buildImageData(name, s.w, s.h, s.data, true);
}

function getFrameImageData(name, frame) {
  const s = SPRITES[name];
  if (!s || !Array.isArray(s.frames) || s.frames.length === 0) return null;
  const idx = ((frame % s.frames.length) + s.frames.length) % s.frames.length;
  const key = `${name}:f${idx}`;
  return buildImageData(key, s.w, s.h, s.frames[idx], false);
}

function drawFrame(name, frame, x, y) {
  const img = getFrameImageData(name, frame);
  if (!img) return;
  ctx.putImageData(img, x, y);
}

function drawSprite(name, x, y) {
  const img = getSpriteImageData(name);
  if (!img) return;
  ctx.putImageData(img, x, y);
}

function gameToScreenX(gx) {
  return GAME_X + Math.round((gx * GAME_W) / GAME.w);
}

function gameToScreenY(gy) {
  return GAME_Y + Math.round((gy * GAME_H) / GAME.h);
}

function gameToScreenW(gw) {
  return Math.max(1, Math.round((gw * GAME_W) / GAME.w));
}

function gameToScreenH(gh) {
  return Math.max(1, Math.round((gh * GAME_H) / GAME.h));
}

function getSpriteCanvas(name) {
  const s = SPRITES[name];
  const img = getSpriteImageData(name);
  if (!s || !img) return null;
  if (spriteCanvasCache[name]) return spriteCanvasCache[name];

  const c = document.createElement("canvas");
  c.width = s.w;
  c.height = s.h;
  const cctx = c.getContext("2d");
  cctx.putImageData(img, 0, 0);
  spriteCanvasCache[name] = c;
  return c;
}

function getFrameCanvas(name, frame) {
  const s = SPRITES[name];
  if (!s || !Array.isArray(s.frames) || s.frames.length === 0) return null;
  const idx = ((frame % s.frames.length) + s.frames.length) % s.frames.length;
  const key = `${name}:f${idx}`;
  if (spriteCanvasCache[key]) return spriteCanvasCache[key];

  const img = getFrameImageData(name, idx);
  if (!img) return null;
  const c = document.createElement("canvas");
  c.width = s.w;
  c.height = s.h;
  const cctx = c.getContext("2d");
  cctx.putImageData(img, 0, 0);
  spriteCanvasCache[key] = c;
  return c;
}

function drawFrameGame(name, frame, gx, gy) {
  const s = SPRITES[name];
  const c = getFrameCanvas(name, frame);
  if (!s || !c) return;
  const dx = gameToScreenX(gx);
  const dy = gameToScreenY(gy);
  const dw = gameToScreenW(s.w);
  const dh = gameToScreenH(s.h);
  ctx.drawImage(c, dx, dy, dw, dh);
}

function drawSpriteGame(name, gx, gy) {
  const s = SPRITES[name];
  const c = getSpriteCanvas(name);
  if (!s || !c) return;
  const dx = gameToScreenX(gx);
  const dy = gameToScreenY(gy);
  const dw = gameToScreenW(s.w);
  const dh = gameToScreenH(s.h);
  ctx.drawImage(c, dx, dy, dw, dh);
}

function drawSpriteGameSmall(name, gx, gy, gSize) {
  const s = SPRITES[name];
  const c = getSpriteCanvas(name);
  if (!s || !c) return;
  const dx = gameToScreenX(gx);
  const dy = gameToScreenY(gy);
  const dw = gameToScreenW(gSize);
  const dh = gameToScreenH(gSize);
  ctx.drawImage(c, dx, dy, dw, dh);
}

function singleSceneFrames(sceneName) {
  if (sceneName === "eat") {
    return [
      "sablinaeat",
      "sablinaeat2",
      "sablinaeat3",
      "sablinaeat4",
      "sablinaeat5",
      "sablinaeat6",
      "sablinaeat7",
      "sablinaeat8",
      "sablinaeat9",
      "sablinaeat10",
      "sablinaeat11",
    ];
  }
  return null;
}

function currentSceneConfig() {
  const scene = state.scene;
  // Use project-editor override only when it has a mapping for this scene
  const m = window._sceneMapOverride;
  if (m && m[scene]) {
    if (scene === "eat") return { sequence: singleSceneFrames("eat"), y: 0, interval: 130 };
    const name = m[scene];
    const intervals = { clean: 180, play: 180, sleep: 190, shake: 180, pet: 160, hunt: 120, discover: 160, rest: 190, walk: 150, color: 160, garden: 180, baby: 200, box: 200 };
    const xvals = { pet: 1 };
    const yvals = { eat: 0, pet: 1 };
    return { name, x: xvals[scene] || 0, y: yvals[scene] ?? 12, interval: intervals[scene] || 180 };
  }
  // Built-in fallback,always works regardless of override
  if (scene === "clean")    return { name: "cleangif",   y: 12, interval: 180 };
  if (scene === "play")     return { name: "gamegif",    y: 12, interval: 180 };
  if (scene === "sleep")    return { name: "sleepgif",   y: 12, interval: 190 };
  if (scene === "shake")    return { name: "gardengif",  y: 12, interval: 180 };
  if (scene === "pet")      return { name: "colorgif",   x: 1,  y: 1,  interval: 160 };
  if (scene === "eat")      return { sequence: singleSceneFrames("eat"), y: 0, interval: 130 };
  if (scene === "hunt")     return { name: "eatgif",     y: 12, interval: 120 };
  if (scene === "discover") return { name: "gardengif",  y: 12, interval: 160 };
  if (scene === "rest")     return { name: "sleepgif",   y: 12, interval: 190 };
  // Extended scenes
  if (scene === "walk")     return { name: "gamewalk",   y: 12, interval: 150 };
  if (scene === "color")    return { name: "colorgif",   y: 1,  interval: 160 };
  if (scene === "garden")   return { name: "gardengif",  y: 12, interval: 180 };
  if (scene === "baby")     return { name: "baby",       y: 0,  interval: 200 };
  if (scene === "box")      return { name: "box",        y: 0,  interval: 200 };
  return null;
}

// ── Character sprite selection: only uses UI overrides; default is always sablinagif ──
// Set window._charSpriteOverride (string) to lock to a specific sprite.
// Set window._moodSpriteMap  (object) to override per-mood  e.g. {"EXCITED":"gamewalk"}
// Set window._activitySpriteMap (object) to override per-activity

function getPetSpriteName() {
  // 1. Hard override from Animation Controls panel
  if (window._charSpriteOverride) return window._charSpriteOverride;
  // 2. Activity override (only if user explicitly set one via UI)
  if (window._activitySpriteMap && state.activity !== "IDLE") {
    const spr = window._activitySpriteMap[state.activity];
    if (spr) return spr;
  }
  // 3. Mood override (only if user explicitly set one via UI)
  if (window._moodSpriteMap) {
    const spr = window._moodSpriteMap[state.mood];
    if (spr) return spr;
  }
  // 4. Default: always sablinagif
  return "sablinagif";
}

function updatePetFrame(nowMs) {
  const spriteName = getPetSpriteName();
  const frameLen = Array.isArray(SPRITES[spriteName]?.frames) ? SPRITES[spriteName].frames.length : 1;
  const moodFast = state.mood === "EXCITED" || state.mood === "CURIOUS";
  const interval = moodFast ? 120 : 180;

  if (state.petTickMs === 0) {
    state.petTickMs = nowMs;
    return;
  }
  if (nowMs - state.petTickMs < interval) return;

  state.petTickMs = nowMs;
  state.petFrame = (state.petFrame + 1) % Math.max(1, frameLen);
}

function updateSceneFrame(nowMs, cfg) {
  const frameLen = Array.isArray(cfg.sequence)
    ? cfg.sequence.length
    : Array.isArray(SPRITES[cfg.name]?.frames)
      ? SPRITES[cfg.name].frames.length
      : 1;
  if (state.sceneTickMs === 0) {
    state.sceneTickMs = nowMs;
    return;
  }
  if (nowMs - state.sceneTickMs < cfg.interval) return;
  state.sceneTickMs = nowMs;
  state.sceneFrame = (state.sceneFrame + 1) % Math.max(1, frameLen);
}

function setScene(sceneName) {
  state.scene = sceneName;
  state.sceneFrame = 0;
  state.sceneTickMs = 0;
  state.sceneStartMs = performance.now();
}

function drawCurrentScene(cfg) {
  if (Array.isArray(cfg.sequence) && cfg.sequence.length > 0) {
    const frameName = cfg.sequence[state.sceneFrame % cfg.sequence.length];
    drawSpriteGame(frameName, cfg.x || 0, cfg.y || 0);
    return;
  }

  const hasFrames = Array.isArray(SPRITES[cfg.name]?.frames);
  if (hasFrames) {
    drawFrameGame(cfg.name, state.sceneFrame, cfg.x || 0, cfg.y || 0);
    return;
  }

  drawSpriteGame(cfg.name, cfg.x || 0, cfg.y || 0);
}

function applyHunt() {
  const n = state.wifiStats.netCount;
  let hungerDelta = 0;
  let happyDelta = 0;
  let healthDelta = 0;

  if (n === 0) {
    hungerDelta = -15;
    happyDelta = -10;
    healthDelta = -5;
  } else {
    hungerDelta = Math.min(35, n * 2 + state.wifiStats.strongCount * 3);
    const variety = state.wifiStats.hiddenCount * 2 + state.wifiStats.openCount;
    happyDelta = Math.min(30, variety * 3 + Math.floor((state.wifiStats.avgRSSI + 100) / 3));
    if (state.wifiStats.avgRSSI > -75) healthDelta += 5;
    if (state.wifiStats.avgRSSI > -65) healthDelta += 5;
    if (state.wifiStats.strongCount > 5) healthDelta += 3;
  }

  state.hun = clamp(state.hun + hungerDelta, 0, 100);
  state.fat = clamp(state.fat + happyDelta, 0, 100);
  state.cle = clamp(state.cle + healthDelta, 0, 100);
  state.exp = clamp(state.exp + Math.max(1, Math.floor(n / 3)), 0, 9999);
}

function applyDiscover() {
  const n = state.wifiStats.netCount;
  let happyDelta = 0;
  let hungerDelta = 0;

  if (n === 0) {
    happyDelta = -5;
    hungerDelta = -3;
  } else {
    const curiosity = state.wifiStats.hiddenCount * 4 + state.wifiStats.openCount * 3 + state.wifiStats.netCount;
    happyDelta = Math.min(35, Math.floor(curiosity / 2));
    hungerDelta = -5;
  }

  state.fat = clamp(state.fat + happyDelta, 0, 100);
  state.hun = clamp(state.hun + hungerDelta, 0, 100);
  state.exp = clamp(state.exp + Math.max(1, n), 0, 9999);
}

/* ── Audio Engine (simulates piezo buzzer + PAM8302 amp) ── */
let audioCtx = null;
function getAudioCtx() {
  if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
  return audioCtx;
}

function playTone(freq, durationMs, vol) {
  if (!state.controls.soundEnabled) return;
  _soundIndicatorUntilMs = performance.now() + (durationMs || 100) + 200;
  try {
    const ac = getAudioCtx();
    const osc = ac.createOscillator();
    const gain = ac.createGain();
    osc.type = "square";
    osc.frequency.value = freq;
    gain.gain.value = vol || 0.08;
    osc.connect(gain);
    gain.connect(ac.destination);
    osc.start();
    gain.gain.exponentialRampToValueAtTime(0.001, ac.currentTime + durationMs / 1000);
    osc.stop(ac.currentTime + durationMs / 1000 + 0.05);
  } catch (_) {}
}

function sfxFeed() { playTone(880, 80); setTimeout(() => playTone(1100, 80), 100); }
function sfxClean() { playTone(600, 60); setTimeout(() => playTone(800, 60), 80); setTimeout(() => playTone(1000, 60), 160); }
function sfxPlay() { playTone(1200, 50); setTimeout(() => playTone(1400, 50), 70); setTimeout(() => playTone(1600, 80), 140); }
function sfxSleep() { playTone(400, 200, 0.04); setTimeout(() => playTone(300, 300, 0.03), 250); }
function sfxDeath() { playTone(440, 300, 0.1); setTimeout(() => playTone(330, 300, 0.08), 350); setTimeout(() => playTone(220, 500, 0.06), 700); }
function sfxHatch() { playTone(600, 100); setTimeout(() => playTone(800, 100), 120); setTimeout(() => playTone(1200, 200), 240); }
function sfxCoin() { playTone(1300, 60); setTimeout(() => playTone(1600, 100), 80); }
function sfxWarn() { playTone(200, 150, 0.1); setTimeout(() => playTone(200, 150, 0.1), 200); }
function sfxMiniHit() { playTone(1000, 50); setTimeout(() => playTone(1400, 80), 60); }
function sfxMiniMiss() { playTone(300, 150, 0.1); }
function sfxVibrate() { playTone(100, 80, 0.05); }

/* ── Vibration simulation ── */
function triggerVibration(ms) {
  state.vibrating = true;
  state.vibrateUntilMs = performance.now() + (ms || 200);
  _vibrateIndicatorUntilMs = performance.now() + (ms || 200) + 300;
  sfxVibrate();
}

function updateVibration(nowMs) {
  if (state.vibrating && nowMs > state.vibrateUntilMs) {
    state.vibrating = false;
  }
}

/* ── BLE Scanning simulation ── */
function maybeBleScan(nowMs) {
  if (!state.ble) {
    state.bleStats.deviceCount = 0;
    blePeer = null;
    return;
  }
  if (nowMs - state.bleStats.lastScanMs < 10000) return;
  state.bleStats.lastScanMs = nowMs;
  // If a real peer is connected, count includes it
  const baseDev = randomInt(0, 12);
  state.bleStats.deviceCount = isPeerAlive() ? baseDev + 1 : baseDev;
}

// ── ReAct Agent: Observe → Think → Decide → Act ────────────────────────────
// Mirrors the IDF agent_task in app_main.c.
// Deterministic tool selector + simulated LLM flavor text.
const AGENT_THOUGHTS = {
  feed_pet:       ["I'm so hungry right now.", "Need snacks ASAP!", "Time for food."],
  sleep_pet:      ["Feeling tired...", "A little rest would be nice.", "Eyes getting heavy~"],
  clean_pet:      ["I should freshen up.", "Time for a bath.", "Feeling a bit messy."],
  play_game:      ["Let's have some fun!", "Game time!", "I want to play something."],
  hunt_wifi:      ["I sense wireless signals nearby.", "Time to collect some data.", "The air is full of packets."],
  deauth_target:  ["That network looks crowded.", "A little interference never hurt.", "Clearing the channel."],
  beacon_spam:    ["Filling the air with imaginary networks.", "So many fake SSIDs...", "Beacon parade!"],
  check_stats:    ["How am I doing today?", "Let me check my status.", "Stats check time."],
  peer_interact:  ["I sense a nearby friend!", "Someone's close by.", "Time to say hi."],
};

function agentDecideTool() {
  const { hun, fat, cle, exp } = state;
  const wifi = state.wifi;
  const netCount = state.wifiStats.netCount;
  const peerVisible = isPeerAlive();
  const lastTool = state.agent.currentTool;
  const uptime = performance.now() - state.engine.bootMs;

  // Critical needs first
  if (hun < 30) return "feed_pet";
  if (fat < 30) return "sleep_pet";
  if (cle < 30) return "clean_pet";

  // BLE peer in range – greet (avoid back-to-back repeats)
  if (peerVisible && lastTool !== "peer_interact") return "peer_interact";

  // WiFi Food: earn coins when low
  if (wifi && exp < 20) return "hunt_wifi";

  // WiFi Food: rotate deauth / beacon_spam
  if (wifi && netCount > 0 && exp >= 20) {
    const slot = Math.floor(uptime / 45000) % 3;
    if (slot === 1) return "deauth_target";
    if (slot === 2) return "beacon_spam";
  }

  // Moderate needs
  if (hun < 50) return "feed_pet";
  if (fat < 50) return "sleep_pet";
  if (cle < 50) return "clean_pet";

  // Default: play or check stats
  const minuteSlot = Math.floor(uptime / 60000) % 5;
  if (minuteSlot === 0) return "check_stats";
  return "play_game";
}

// Helper: trigger a timed activity + scene from the agent, only if currently IDLE
function agentTriggerActivity(activity, scene, room, durationMs) {
  if (state.activity !== "IDLE") return;
  state.activity = activity;
  state.engine.activityUntilMs = performance.now() + durationMs;
  state.engine.activityStepMs  = performance.now();
  setScene(scene);
  state.targetRoom = room;
}

function agentExecuteTool(tool) {
  switch (tool) {
    case "feed_pet": {
      const gain = 18 + randomInt(0, 4);
      state.hun = Math.min(100, state.hun + gain);
      agentTriggerActivity("EAT", "eat", "KITCHEN", randomInt(8000, 14000));
      return `feed_pet: ate (+${gain} HUN)`;
    }
    case "sleep_pet": {
      const gain = 20 + randomInt(0, 4);
      state.fat = Math.min(100, state.fat + gain);
      agentTriggerActivity("REST", "rest", "BEDROOM", randomInt(10000, 18000));
      return `sleep_pet: rested (+${gain} HAP)`;
    }
    case "clean_pet": {
      const gain = 24 + randomInt(0, 4);
      state.cle = Math.min(100, state.cle + gain);
      agentTriggerActivity("CLEAN", "clean", "BATHROOM", randomInt(8000, 14000));
      return `clean_pet: washed (+${gain} HP)`;
    }
    case "play_game": {
      state.hun = Math.max(0, state.hun - 3);
      state.fat = Math.max(0, state.fat - 4);
      state.cle = Math.max(0, state.cle - 2);
      state.exp = Math.min(9999, state.exp + 2);
      agentTriggerActivity("PLAY", "play", "PLAYROOM", randomInt(8000, 14000));
      return "play_game: played (+2 coins)";
    }
    case "hunt_wifi": {
      const found = state.wifiStats.netCount || randomInt(2, 5);
      const earned = Math.min(10, found);
      state.exp = Math.min(9999, state.exp + earned);
      state.wifiStats.netCount = found;
      agentTriggerActivity("HUNT", "hunt", "LAB", randomInt(8000, 14000));
      return `hunt_wifi: ${found} nets, +${earned} coins`;
    }
    case "deauth_target": {
      if (state.wifiStats.netCount === 0) {
        const found = randomInt(2, 5);
        state.wifiStats.netCount = found;
        state.exp = Math.min(9999, state.exp + 3);
        agentTriggerActivity("DISCOVER", "discover", "LAB", randomInt(8000, 12000));
        return `deauth: scanned first (${found} nets, +3 coins)`;
      }
      state.exp = Math.min(9999, state.exp + 3);
      agentTriggerActivity("DISCOVER", "discover", "LAB", randomInt(8000, 12000));
      return `deauth_target: AP#${randomInt(0, state.wifiStats.netCount - 1)} (+3 coins)`;
    }
    case "beacon_spam": {
      const ssids = ["Free_Snacks_WiFi", "Sablina_Network", "Not_A_Trap_5G", "FBI_Van", "SablinaSignal"];
      const idx = Math.floor(performance.now() / 11003) % ssids.length;
      state.exp = Math.min(9999, state.exp + 2);
      agentTriggerActivity("SHAKE", "shake", "PLAYROOM", randomInt(8000, 12000));
      return `beacon_spam: "${ssids[idx]}" (+2 coins)`;
    }
    case "check_stats":
      return `H:${state.hun} HAP:${state.fat} HP:${state.cle} coins:${state.exp} nets:${state.wifiStats.netCount}`;
    case "peer_interact": {
      if (!isPeerAlive()) return "peer_interact: no peer";
      // Delegate to the existing social interaction system
      maybeBleAutoInteract(performance.now());
      return `peer: said hi to ${blePeer?.name || "nearby"}`;
    }
    default:
      return "no-op";
  }
}

function runAgentReActStep(nowMs) {
  const ag = state.agent;
  const urgent = state.hun < 30 || state.fat < 30 || state.cle < 30;
  const period = urgent ? ag.urgentPeriodMs : ag.cyclePeriodMs;
  if (nowMs - ag.lastCycleMs < period) return;
  ag.lastCycleMs = nowMs;
  ag.cycleCount++;

  // DECIDE (deterministic)
  const tool = agentDecideTool();
  ag.currentTool = tool;

  // THINK (simulated LLM flavor text)
  const thoughts = AGENT_THOUGHTS[tool] || ["..."];
  ag.lastThought = thoughts[Math.floor(Math.random() * thoughts.length)];
  // Sync thought to bubble so the displayed message matches what the agent is doing
  state.bubble = ag.lastThought;

  // ACT
  ag.lastResult = agentExecuteTool(tool);

  // Skip peer_interact if it was already handled by maybeBleAutoInteract
  // (agentExecuteTool calls it directly for peer_interact)
}

/* ── BLE autonomous interaction between peers ── */
let _lastBleAutoInteractMs = 0;
function maybeBleAutoInteract(nowMs) {
  if (!state.ble || !isPeerAlive()) return;
  // Interval: 12-25s depending on curiosity trait
  const interval = 25000 - (state.traits.curiosity / 100) * 13000;
  if (nowMs - _lastBleAutoInteractMs < interval) return;
  _lastBleAutoInteractMs = nowMs;

  const peer = blePeer;
  const memory = ensureSocialMemory(peer);
  const affinity = memory?.affinity || 0;
  const N = state.name;
  const P = peer.name;

  // Decide action based on mood + stats (autonomous decision)
  let action, text;
  let localLine = affinity >= 50 ? "Missed you." : "I found you nearby.";
  let peerLine = "I see you too.";
  if (affinity >= 25 && state.hun < 45) {
    action = "giftSnack";
    text = `${N} sends a snack gift to ${P}`;
    localLine = affinity >= 50 ? "I saved snacks." : "Snack gift.";
    peerLine = "That helps a lot.";
  } else if (affinity >= 25 && state.fat < 45) {
    action = "giftRest";
    text = `${N} sends a rest gift to ${P}`;
    localLine = "Rest gift.";
    peerLine = "I needed that calm.";
  } else if (affinity >= 25 && state.cle < 45) {
    action = "giftClean";
    text = `${N} sends a clean gift to ${P}`;
    localLine = "Clean gift.";
    peerLine = "Everything feels lighter.";
  } else if (affinity >= 50 && state.exp < 12) {
    action = "giftCoin";
    text = `${N} sends a coin gift to ${P}`;
    localLine = "Coin gift.";
    peerLine = "I will use it well.";
  } else if (state.hun < 40 && state.exp > 0) {
    action = "shareFood";
    text = `${N} shares some food with ${P}`;
    localLine = "I brought snacks for both of us.";
    peerLine = "Perfect. I was getting hungry.";
  } else if (state.fat < 40) {
    action = "comfort";
    text = `${N} snuggles up to ${P}`;
    localLine = "I need a calm moment.";
    peerLine = "Stay close. We can slow down together.";
  } else if (state.mood === "EXCITED" || state.mood === "HAPPY") {
    action = "play";
    text = pick([
      `${N} and ${P} play tag!`,
      `${N} challenges ${P} to a dance-off!`,
      `${N} and ${P} do a high-five!`,
    ]);
    localLine = pick([
      "Want to race toward the brightest signal?",
      "Let's make this more fun.",
      "Ready for a quick game?",
    ]);
    peerLine = pick([
      "Absolutely. I'm in.",
      "Only if I get the first point.",
      "Let's go.",
    ]);
  } else if (state.mood === "CURIOUS") {
    action = "explore";
    text = pick([
      `${N} and ${P} explore the forest together`,
      `${N} shows ${P} a hidden path`,
      `${N} and ${P} discover a secret spot!`,
    ]);
    localLine = "I found a strange signal trail.";
    peerLine = "Lead the way. I want to see it.";
  } else if (state.mood === "BORED") {
    action = "joke";
    text = pick([
      `${N} tells ${P} a silly joke`,
      `${N} makes a funny face at ${P}`,
      `${N} and ${P} stare at clouds together`,
    ]);
    localLine = "I need something funny right now.";
    peerLine = "Then don't stop. That was good.";
  } else if (peer.mood === "SICK" || peer.mood === "HUNGRY") {
    action = "help";
    text = `${N} tries to cheer up ${P}`;
    localLine = "You look low. Need a hand?";
    peerLine = "Thanks. That helps a lot.";
  } else {
    action = "greet";
    text = pick([
      `${N} waves at ${P}!`,
      `${N} says hi to ${P}`,
      `${N} bumps noses with ${P}`,
      `${N} and ${P} share a moment`,
    ]);
    localLine = pick([
      affinity >= 50 ? "Missed you." : "Hi there.",
      "Good to see another Sablina around.",
      affinity >= 25 ? "You feel familiar now." : "Nice signal weather today.",
    ]);
    peerLine = pick([
      "Hi. I was hoping to find you.",
      "Same here. It feels less lonely.",
      "Agreed. The air feels lively today.",
    ]);
  }

  bleSendInteraction(action, text, localLine, peerLine);
  registerOutgoingSocialInteraction(peer, action, localLine, text);
  state.bubble = text;
  setBlePeerConversation(`${N}: ${localLine}`, `${P}: ${peerLine}`, nowMs);

  // Effects
  switch (action) {
    case "giftSnack":
      state.exp = Math.max(0, state.exp - 1);
      triggerVibration(160);
      break;
    case "giftRest":
      state.fat = clamp(state.fat + 2, 0, 100);
      triggerVibration(160);
      break;
    case "giftClean":
      state.cle = clamp(state.cle + 2, 0, 100);
      triggerVibration(160);
      break;
    case "giftCoin":
      state.exp = Math.max(0, state.exp - 2);
      triggerVibration(160);
      break;
    case "play":
      state.fat = clamp(state.fat + 4, 0, 100);
      state.traits.activity = clamp(state.traits.activity + 1, 0, 100);
      sfxPlay(); triggerVibration(150);
      break;
    case "explore":
      state.traits.curiosity = clamp(state.traits.curiosity + 1, 0, 100);
      state.fat = clamp(state.fat + 2, 0, 100);
      sfxHatch();
      break;
    case "shareFood":
      state.exp = Math.max(0, state.exp - 1);
      state.hun = clamp(state.hun + 5, 0, 100);
      sfxFeed();
      break;
    case "comfort":
      state.fat = clamp(state.fat + 5, 0, 100);
      state.traits.stress = clamp(state.traits.stress - 2, 0, 100);
      sfxSleep();
      break;
    case "help":
      state.fat = clamp(state.fat + 2, 0, 100);
      sfxHatch();
      break;
    case "joke":
      state.fat = clamp(state.fat + 3, 0, 100);
      sfxCoin();
      break;
    default: // greet
      state.fat = clamp(state.fat + 1, 0, 100);
      sfxHatch();
      break;
  }
}

/* ── Visual indicators (drawn on canvas) ── */
function drawSoundIndicator(nowMs) {
  if (nowMs > _soundIndicatorUntilMs) return;
  const alpha = Math.min(1, (_soundIndicatorUntilMs - nowMs) / 300);
  const cx = SCREEN.w - 16;
  const cy = 6;

  ctx.save();
  ctx.globalAlpha = alpha;
  // Speaker icon
  ctx.fillStyle = "#00e5ff";
  ctx.fillRect(cx - 4, cy - 2, 4, 5);
  ctx.beginPath();
  ctx.moveTo(cx, cy - 4);
  ctx.lineTo(cx + 4, cy - 7);
  ctx.lineTo(cx + 4, cy + 7);
  ctx.lineTo(cx, cy + 4);
  ctx.closePath();
  ctx.fill();
  // Sound waves
  const t = (nowMs % 600) / 600;
  ctx.strokeStyle = "#00e5ff";
  ctx.lineWidth = 1;
  for (let i = 0; i < 3; i++) {
    const r = 5 + i * 3 + t * 3;
    const a = alpha * (1 - i * 0.3);
    ctx.globalAlpha = a;
    ctx.beginPath();
    ctx.arc(cx + 4, cy, r, -0.6, 0.6);
    ctx.stroke();
  }
  ctx.restore();
}

function drawVibrateIndicator(nowMs) {
  if (nowMs > _vibrateIndicatorUntilMs) return;
  const alpha = Math.min(1, (_vibrateIndicatorUntilMs - nowMs) / 400);
  const t = (nowMs % 200) / 200;

  ctx.save();
  ctx.globalAlpha = alpha * 0.6;
  ctx.strokeStyle = "#ff5555";
  ctx.lineWidth = 2;

  // Vibration waves on left side of screen
  const cx = 10;
  const cy = 6;
  for (let i = 0; i < 3; i++) {
    const off = Math.sin((t + i * 0.3) * Math.PI * 2) * 2;
    ctx.beginPath();
    ctx.moveTo(cx - 3 + off, cy - 5 + i * 3);
    ctx.lineTo(cx + 3 + off, cy - 5 + i * 3);
    ctx.stroke();
  }

  // Pulsing border
  ctx.globalAlpha = alpha * 0.3;
  ctx.strokeStyle = "#ff5555";
  ctx.lineWidth = 1;
  ctx.strokeRect(GAME_X + 1, GAME_Y + 1, GAME_W - 2, GAME_H - 2);
  ctx.restore();
}

function drawBlePeerIndicator(nowMs) {
  if (!state.ble || !isPeerAlive()) return;

  const peer = blePeer;
  const pulse = 0.5 + 0.5 * Math.sin(nowMs / 400);

  // BLE icon (bottom-right of game area)
  const bx = GAME_X + GAME_W - 28;
  const by = GAME_Y + GAME_H - 14;

  ctx.save();
  // BLE symbol background
  ctx.globalAlpha = 0.7 + pulse * 0.3;
  ctx.fillStyle = "#4488ff";
  ctx.beginPath();
  ctx.arc(bx + 5, by + 5, 7, 0, Math.PI * 2);
  ctx.fill();

  // "B" letter for Bluetooth
  ctx.fillStyle = "#fff";
  ctx.font = "bold 8px monospace";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText("B", bx + 5, by + 5);

  // Peer name
  ctx.globalAlpha = 1;
  ctx.fillStyle = "#4488ff";
  ctx.font = "6px monospace";
  ctx.textAlign = "right";
  ctx.fillText(peer.name, bx - 3, by + 5);
  ctx.restore();

  // Show interaction text
  if (blePeerInteraction && (nowMs - blePeerInteractionMs) < 4000) {
    const fadeAlpha = Math.min(1, (4000 - (nowMs - blePeerInteractionMs)) / 1000);
    ctx.save();
    ctx.globalAlpha = fadeAlpha;
    ctx.fillStyle = "#4488ff";
    ctx.font = "7px monospace";
    ctx.textAlign = "center";
    ctx.fillText(blePeerInteraction, GAME_X + GAME_W / 2, GAME_Y + GAME_H - 2);
    ctx.restore();
  }
}

/* ── Battery drain simulation ── */
function updateBattery(nowMs) {
  if (nowMs - state.engine.lastBatteryMs < 15000) return;
  state.engine.lastBatteryMs = nowMs;

  if (state.charging) {
    state.battery = clamp(state.battery + 2, 0, 100);
    return;
  }

  let drain = 0.3;
  if (state.wifi) drain += 0.2;
  if (state.ble) drain += 0.15;
  if (state.controls.neoPixelsEnabled) drain += 0.1;
  if (state.activity !== "IDLE") drain += 0.15;
  const brightness = [0.05, 0.1, 0.2];
  drain += brightness[state.controls.tftBrightnessIndex] || 0.1;

  state.battery = clamp(state.battery - drain, 0, 100);
}

/* ── Trait auto-evolution ── */
function updateTraits(nowMs) {
  if (nowMs - state.engine.lastTraitMs < 30000) return;
  state.engine.lastTraitMs = nowMs;

  if (state.activity === "HUNT" || state.activity === "DISCOVER") {
    state.traits.curiosity = clamp(state.traits.curiosity + 1, 0, 100);
  } else if (state.traits.curiosity > 50) {
    state.traits.curiosity = clamp(state.traits.curiosity - 0.5, 0, 100);
  }

  if (state.activity === "PLAY" || state.activity === "SHAKE") {
    state.traits.activity = clamp(state.traits.activity + 1, 0, 100);
  } else if (state.activity === "REST" || state.activity === "IDLE") {
    state.traits.activity = clamp(state.traits.activity - 0.3, 0, 100);
  }

  if (state.hun < 20 || state.cle < 20) {
    state.traits.stress = clamp(state.traits.stress + 2, 0, 100);
  } else if (state.fat > 70 && state.hun > 60) {
    state.traits.stress = clamp(state.traits.stress - 1, 0, 100);
  }
}

/* ── Coins economy ── */
function updateCoinBonus(nowMs) {
  if (nowMs - state.engine.lastCoinBonusMs < 120000) return;
  state.engine.lastCoinBonusMs = nowMs;

  if (state.wifiStats.netCount > 5) {
    state.exp = clamp(state.exp + 1, 0, 9999);
  }
  if (state.bleStats.deviceCount > 3) {
    state.exp = clamp(state.exp + 1, 0, 9999);
  }
  if (state.fat > 80 && state.hun > 60 && state.cle > 60) {
    state.exp = clamp(state.exp + 2, 0, 9999);
  }
}

/* ── Death mechanic ── */
function updateDeath(nowMs) {
  if (!state.alive) return;

  // Any single stat critically low (≤5) triggers the death countdown
  const critical = state.hun <= 5 || state.fat <= 5 || state.cle <= 5;
  if (critical) {
    if (state.engine.criticalStatMs === 0) {
      state.engine.criticalStatMs = nowMs;
      const who = state.hun <= 5 ? "starving" : state.fat <= 5 ? "exhausted" : "filthy";
      state.bubble = `${state.name} is ${who}, critical condition!`;
      sfxWarn();
      triggerVibration(500);
    }
    const elapsed = nowMs - state.engine.criticalStatMs;
    if (elapsed > 20000 && elapsed <= 30000 && state.bubble.indexOf("final") === -1) {
      state.bubble = `${state.name} is in FINAL WARNING! Care now!`;
      sfxWarn();
    }
    if (elapsed > 60000) {
      state.alive = false;
      state.deathMs = nowMs;
      state.activity = "IDLE";
      setScene("home");
      state.bubble = `${state.name} has passed away...`;
      sfxDeath();
      triggerVibration(1000);
      persistState();
    }
  } else {
    state.engine.criticalStatMs = 0;
    // Legacy zeroStatMs reset for compatibility
    state.engine.zeroStatMs = 0;
  }
}

/* ── Birth / Hatching sequence ── */
function updateHatching(nowMs) {
  if (state.hatched) return;

  if (state.hatchTickMs === 0) {
    state.hatchTickMs = nowMs;
    state.bubble = "An egg is hatching...";
    return;
  }

  const elapsed = nowMs - state.hatchTickMs;
  state.hatchFrame = Math.floor(elapsed / 400) % 4;

  if (elapsed > 5000) {
    state.hatched = true;
    state.bubble = `${state.name} is born! Welcome!`;
    sfxHatch();
    triggerVibration(300);
    persistState();
  }
}

function drawHatchingScreen() {
  ctx.fillStyle = "#0a0e1a";
  ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);

  const cx = GAME_X + Math.floor(GAME_W / 2);
  const cy = GAME_Y + Math.floor(GAME_H / 2);

  const wobble = Math.sin(performance.now() / 150) * (state.hatchFrame + 1) * 2;

  ctx.save();
  ctx.translate(cx + wobble, cy);

  const eggW = 28;
  const eggH = 36;
  ctx.fillStyle = "#f8e8d0";
  ctx.beginPath();
  ctx.ellipse(0, 0, eggW, eggH, 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = "#c8a880";
  ctx.lineWidth = 2;
  ctx.stroke();

  if (state.hatchFrame >= 1) {
    ctx.strokeStyle = "#8b6c4a";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(-8, -10); ctx.lineTo(-3, 5); ctx.lineTo(6, -15);
    ctx.stroke();
  }
  if (state.hatchFrame >= 2) {
    ctx.beginPath();
    ctx.moveTo(5, -5); ctx.lineTo(12, 8); ctx.lineTo(-2, 12);
    ctx.stroke();
  }
  if (state.hatchFrame >= 3) {
    ctx.fillStyle = "#ffd700";
    ctx.beginPath();
    ctx.arc(0, -eggH + 10, 8, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = "#222";
    ctx.beginPath();
    ctx.arc(-3, -eggH + 9, 2, 0, Math.PI * 2);
    ctx.arc(3, -eggH + 9, 2, 0, Math.PI * 2);
    ctx.fill();
  }

  ctx.restore();

  ctx.fillStyle = "#ffdc8a";
  ctx.font = "9px monospace";
  ctx.textAlign = "center";
  ctx.fillText("Hatching...", cx, GAME_Y + GAME_H - 12);
  ctx.textAlign = "left";
}

/* ── Death screen ── */
function drawDeathScreen() {
  ctx.fillStyle = "#080808";
  ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);

  const cx = GAME_X + Math.floor(GAME_W / 2);
  const cy = GAME_Y + Math.floor(GAME_H / 2) - 16;

  // Tombstone
  ctx.fillStyle = "#445";
  ctx.fillRect(cx - 12, cy + 10, 24, 30);
  ctx.fillRect(cx - 20, cy + 16, 40, 8);
  ctx.beginPath();
  ctx.arc(cx, cy + 10, 12, Math.PI, 0, false);
  ctx.fill();

  ctx.fillStyle = "#ffe06a";
  ctx.font = "9px monospace";
  ctx.textAlign = "center";
  ctx.fillText(state.name, cx, cy);

  // Cause of death
  const age = `${state.ageDays}d ${state.ageHours}h`;
  ctx.fillStyle = "#778";
  ctx.font = "7px monospace";
  ctx.fillText(`Lived: ${age}  Stage: ${state.stage}`, cx, cy + 52);
  ctx.fillStyle = "#889";
  ctx.fillText("Rest in peace", cx, cy + 62);
  ctx.fillStyle = "#668";
  ctx.fillText("Long press A to revive", cx, cy + 74);
  ctx.textAlign = "left";
}

/* ── Mini-game: Catch the Signal ── */
function startMiniGame() {
  state.miniGame.active = true;
  state.miniGame.score = 0;
  state.miniGame.round = 0;
  state.miniGame.cursorX = 64;
  state.miniGame.speed = 1.5;
  state.miniGame.dir = 1;
  state.miniGame.resultMs = 0;
  state.miniGame.lastHit = false;
  nextMiniGameRound();
}

function nextMiniGameRound() {
  state.miniGame.round++;
  if (state.miniGame.round > state.miniGame.maxRounds) {
    endMiniGame();
    return;
  }
  state.miniGame.targetX = randomInt(20, 108);
  state.miniGame.cursorX = randomInt(0, 30);
  state.miniGame.dir = 1;
  state.miniGame.speed = 1.2 + state.miniGame.round * 0.3;
  state.miniGame.resultMs = 0;
}

function miniGameTap() {
  if (state.miniGame.resultMs > 0) return;
  const dist = Math.abs(state.miniGame.cursorX - state.miniGame.targetX);
  if (dist < 10) {
    state.miniGame.score++;
    state.miniGame.lastHit = true;
    sfxMiniHit();
  } else {
    state.miniGame.lastHit = false;
    sfxMiniMiss();
  }
  state.miniGame.resultMs = performance.now();
}

function endMiniGame() {
  state.miniGame.active = false;
  const coins = state.miniGame.score * 3;
  state.exp = clamp(state.exp + coins, 0, 9999);
  state.fat = clamp(state.fat + state.miniGame.score * 2, 0, 100);
  state.hun = clamp(state.hun - 5, 0, 100);
  state.lifetime.gamesPlayed++;
  state.lifetime.coinsEarned += coins;
  if (state.miniGame.score === state.miniGame.maxRounds) state.lifetime.gamesWon++;
  state.bubble = `Game over! ${state.miniGame.score}/${state.miniGame.maxRounds} hits! +${coins} coins`;
  if (coins > 0) sfxCoin();
  triggerVibration(200);
  persistState();
}

function updateMiniGame(nowMs) {
  if (!state.miniGame.active) return;

  if (state.miniGame.resultMs > 0) {
    if (nowMs - state.miniGame.resultMs > 800) {
      nextMiniGameRound();
    }
    return;
  }

  state.miniGame.cursorX += state.miniGame.speed * state.miniGame.dir;
  if (state.miniGame.cursorX >= 128 || state.miniGame.cursorX <= 0) {
    state.miniGame.dir *= -1;
  }
  state.miniGame.cursorX = clamp(state.miniGame.cursorX, 0, 128);
}

function drawMiniGame() {
  ctx.fillStyle = "#0a0e1a";
  ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);

  const mg = state.miniGame;

  ctx.fillStyle = "#ffdc8a";
  ctx.font = "9px monospace";
  ctx.textAlign = "center";
  const cx = GAME_X + Math.floor(GAME_W / 2);
  ctx.fillText(`CATCH THE SIGNAL  ${mg.round}/${mg.maxRounds}`, cx, GAME_Y + 14);

  const barY = GAME_Y + Math.floor(GAME_H / 2) - 4;
  const barH = 8;
  ctx.fillStyle = "#1f2941";
  ctx.fillRect(GAME_X + 4, barY, GAME_W - 8, barH);

  const tgtX = GAME_X + 4 + Math.round(((GAME_W - 8) * mg.targetX) / 128);
  ctx.fillStyle = "#66e08b";
  ctx.fillRect(tgtX - 8, barY - 2, 16, barH + 4);

  const curX = GAME_X + 4 + Math.round(((GAME_W - 8) * mg.cursorX) / 128);
  // Draw gamewalk sprite as the moving character
  const gwS = SPRITES.gamewalk;
  if (gwS && gwS.frames) {
    const gwFrame = Math.floor(performance.now() / 120) % gwS.frames.length;
    const gwC = getFrameCanvas("gamewalk", gwFrame);
    if (gwC) {
      const gwW = gameToScreenW(gwS.w);
      const gwH = gameToScreenH(gwS.h);
      ctx.drawImage(gwC, curX - gwW / 2, barY - gwH + 2, gwW, gwH);
    }
  } else {
    ctx.fillStyle = "#ff5555";
    ctx.fillRect(curX - 2, barY - 6, 4, barH + 12);
  }

  if (mg.resultMs > 0) {
    ctx.fillStyle = mg.lastHit ? "#66e08b" : "#ff5555";
    ctx.fillText(mg.lastHit ? "HIT!" : "MISS!", cx, barY + 30);
  }

  ctx.fillStyle = "#ffe06a";
  ctx.fillText(`Score: ${mg.score}`, cx, GAME_Y + GAME_H - 10);
  ctx.textAlign = "left";
}

/* ── Sickness mechanic ── */
function updateSickness(nowMs) {
  if (!state.alive) return;
  const wasSick = state.engine.sickMs > 0;
  // Triggers: cleanliness critically low OR all stats poor for a sustained period
  const sicknessCondition = state.cle < 15 || (state.cle < 25 && state.hun < 25 && state.fat < 25);
  if (sicknessCondition) {
    if (!wasSick) state.engine.sickMs = nowMs;
    // Prolonged sickness (>2 min) drains stats faster
    if (nowMs - state.engine.sickMs > 120000) {
      state.hun = Math.max(0, state.hun - 1);
      state.cle = Math.max(0, state.cle - 1);
    }
  } else {
    // Self-recovery if all stats rise above threshold
    if (wasSick && state.cle >= 50 && state.hun >= 40 && state.fat >= 40) {
      state.engine.sickMs = 0;
    }
  }
}

function isSick() { return state.engine.sickMs > 0; }

function applyMedicine() {
  state.engine.sickMs = 0;
  state.cle = Math.min(100, state.cle + 30);
  state.hun = Math.min(100, state.hun + 10);
  state.fat = Math.min(100, state.fat + 10);
  state.bubble = `${state.name} took her medicine and feels better!`;
  sfxFeed();
  triggerVibration(200);
}

function updateMood(nowMs) {
  // SICK mood: active sickness OR cleanliness threshold
  if (isSick() || state.cle < 25 || (!state.wifi && nowMs - state.engine.lastWifiScanMs > 60000)) {
    state.mood = "SICK";
    return;
  }
  if (state.hun < 25) {
    state.mood = "HUNGRY";
    return;
  }
  if (state.fat > 80 && (state.wifiStats.netCount > 8 || state.bleStats.deviceCount > 5)) {
    state.mood = "EXCITED";
    return;
  }
  if (state.fat > 60 && (state.wifiStats.netCount > 0 || state.bleStats.deviceCount > 0)) {
    state.mood = "HAPPY";
    return;
  }
  if (state.wifiStats.netCount === 0 && state.bleStats.deviceCount === 0 && nowMs - state.engine.lastWifiScanMs > 30000) {
    state.mood = "BORED";
    return;
  }
  if (state.wifiStats.hiddenCount > 0 || state.wifiStats.openCount > 0 || state.bleStats.deviceCount > 3) {
    state.mood = "CURIOUS";
    return;
  }
  state.mood = "CALM";
}

function updateEvolution() {
  const ageMin = state.ageDays * 1440 + state.ageHours * 60 + state.ageMinutes;
  const avg = Math.floor((state.hun + state.fat + state.cle) / 3);

  let newStage;
  if (ageMin >= 180 && avg > 40) {
    newStage = "ELDER";
  } else if (ageMin >= 60 && avg > 45) {
    newStage = "ADULT";
  } else if (ageMin >= 20 && avg > 35) {
    newStage = "TEEN";
  } else if (ageMin >= 8 && avg > 25) {
    newStage = "CHILD";
  } else {
    newStage = "BABY";
  }

  if (newStage !== state.stage && state.stage !== "EGG") {
    state.prevStage = state.stage;
    state.stage = newStage;
    state.engine.lastEvolveMs = performance.now();
    state.bubble = `${state.name} evolved into a ${newStage}!`;
    sfxHatch();
    triggerVibration(400);
  }
}

function maybeRandomWifiScan(nowMs) {
  if (!state.wifi) {
    state.wifiStats.scanRunning = false;
    state.wifiStats.netCount = 0;
    state.wifiStats.strongCount = 0;
    state.wifiStats.hiddenCount = 0;
    state.wifiStats.openCount = 0;
    state.wifiStats.wpaCount = 0;
    state.wifiStats.avgRSSI = -100;
    return;
  }

  if (nowMs - state.engine.lastWifiScanMs < 8000) return;
  state.engine.lastWifiScanMs = nowMs;
  state.wifiStats.scanRunning = true;
  state.lifetime.wifiScans++;

  if (state.realWifi && state.realWifiNetworks.length > 0) {
    const nets = state.realWifiNetworks;
    state.wifiStats.netCount = nets.length;
    state.wifiStats.strongCount = nets.filter(n => n.signal > 60).length;
    state.wifiStats.hiddenCount = nets.filter(n => !n.ssid || n.ssid === "--").length;
    state.wifiStats.openCount = nets.filter(n => !n.security || n.security === "" || n.security === "--").length;
    state.wifiStats.wpaCount = nets.filter(n => n.security && n.security !== "" && n.security !== "--").length;
    const signals = nets.map(n => n.signal || 0);
    const avgSignal = signals.length > 0 ? Math.round(signals.reduce((a,b) => a + b, 0) / signals.length) : 0;
    state.wifiStats.avgRSSI = clamp(avgSignal - 100, -100, -30);
    state.wifiStats.scanRunning = false;
    return;
  }

  const total = randomInt(0, 14);
  const hidden = randomInt(0, Math.min(4, total));
  const open = randomInt(0, Math.min(4, total - hidden));
  const strong = randomInt(0, Math.min(6, total));

  state.wifiStats.netCount = total;
  state.wifiStats.hiddenCount = hidden;
  state.wifiStats.openCount = open;
  state.wifiStats.strongCount = strong;
  state.wifiStats.wpaCount = Math.max(0, total - open);
  state.wifiStats.avgRSSI = total === 0 ? -100 : randomInt(-91, -53);
  state.wifiStats.scanRunning = false;
  if (total > state.lifetime.maxNetworks) state.lifetime.maxNetworks = total;
  state.lifetime.networksFound += total;
}

function decideNextActivity(nowMs) {
  if (state.activity !== "IDLE") return;
  if (nowMs - state.engine.lastDecisionMs < state.engine.decisionEveryMs) return;

  state.engine.lastDecisionMs = nowMs;
  state.engine.decisionEveryMs = randomInt(6000, 12000);

  let desireEat = (100 - state.hun) * 2;
  let desireHunt = (100 - state.hun) + Math.floor(state.traits.curiosity / 2);
  if (state.wifiStats.netCount === 0) desireHunt = Math.floor(desireHunt / 2);

  let desireDisc =
    state.traits.curiosity +
    state.wifiStats.hiddenCount * 10 +
    state.wifiStats.openCount * 6 +
    state.wifiStats.netCount * 2 +
    randomInt(0, 20);
  if (state.wifiStats.netCount === 0) desireDisc = Math.floor(desireDisc / 2);

  let desireClean = (100 - state.cle) + Math.floor(state.traits.stress / 3);
  let desireRest = (100 - state.cle) + Math.floor(state.traits.stress / 2);
  let desirePlay = 20 + Math.floor(state.traits.curiosity / 3) + randomInt(0, 15);
  let desireShake = 10 + randomInt(0, 20);
  let desireIdle = 5;

  if (state.hun < 20) {
    desireEat += 50;
    desireRest -= 10;
  }
  if (state.mood === "HUNGRY") {
    desireEat += 60;
    desireHunt += 5;
    desireRest -= 10;
    desirePlay -= 15;
  }
  if (state.mood === "SICK") {
    desireClean += 35;
    desirePlay -= 20;
    desireShake -= 15;
  }
  if (state.mood === "CURIOUS") {
    desireDisc += 15;
    desirePlay += 10;
  }
  if (state.mood === "SICK") {
    desireRest += 20;
    desireDisc -= 10;
  }
  if (state.mood === "EXCITED") {
    desireDisc += 10;
    desireHunt += 5;
    desirePlay += 20;
    desireShake += 15;
  }
  if (state.mood === "BORED") {
    desireDisc += 10;
    desireHunt += 5;
    desirePlay += 15;
    desireShake += 10;
  }
  if (state.mood === "HAPPY") {
    desirePlay += 15;
    desireShake += 10;
  }

  const desires = { EAT: desireEat, HUNT: desireHunt, DISCOVER: desireDisc, CLEAN: desireClean, REST: desireRest, PLAY: desirePlay, SHAKE: desireShake, IDLE: desireIdle };
  for (const k of Object.keys(desires)) desires[k] = Math.max(desires[k], 0);

  let best = desires.IDLE;
  let chosen = "IDLE";
  for (const [act, val] of Object.entries(desires)) {
    if (val > best) { best = val; chosen = act; }
  }

  if (chosen === "IDLE") return;

  state.activity = chosen;
  state.engine.activityUntilMs = nowMs + randomInt(8000, 20000);
  state.engine.activityStepMs = nowMs;

  const actMap = {
    EAT: { scene: "eat", room: "KITCHEN", nav: null },
    HUNT: { scene: "hunt", room: "LAB", nav: screens.FOREST },
    DISCOVER: { scene: "discover", room: "PLAYROOM", nav: screens.FOREST },
    CLEAN: { scene: "clean", room: "BATHROOM", nav: null },
    REST: { scene: "rest", room: "BEDROOM", nav: null },
    PLAY: { scene: "play", room: "PLAYROOM", nav: null },
    SHAKE: { scene: "play", room: "PLAYROOM", nav: null },
  };
  const cfg = actMap[chosen];
  if (cfg) {
    setScene(cfg.scene);
    state.targetRoom = cfg.room;
    if (cfg.nav && state.screen === screens.HOME) {
      llmNavigateScreen(cfg.nav, 5000);
    }
  }
}

function updateActivity(nowMs) {
  if (state.activity === "IDLE") return;
  if (nowMs < state.engine.activityUntilMs) return;

  if (state.activity === "HUNT") applyHunt();
  if (state.activity === "DISCOVER") applyDiscover();
  if (state.activity === "EAT") {
    state.hun = clamp(state.hun + 16, 0, 100);
    state.fat = clamp(state.fat + 4, 0, 100);
  }
  if (state.activity === "CLEAN") {
    state.cle = clamp(state.cle + 15, 0, 100);
    state.fat = clamp(state.fat - 1, 0, 100);
  }
  if (state.activity === "REST") {
    state.hun = clamp(state.hun - 3, 0, 100);
    state.fat = clamp(state.fat + 10, 0, 100);
    state.cle = clamp(state.cle + 15, 0, 100);
  }
  if (state.activity === "PLAY") {
    state.fat = clamp(state.fat - 5, 0, 100);
    state.hun = clamp(state.hun - 4, 0, 100);
    state.exp = clamp(state.exp + 3, 0, 9999);
  }
  if (state.activity === "SHAKE") {
    state.fat = clamp(state.fat + 5, 0, 100);
    state.hun = clamp(state.hun - 3, 0, 100);
    state.exp = clamp(state.exp + 2, 0, 9999);
    triggerVibration(150);
  }

  state.activity = "IDLE";
  setScene("home");
}

function logicTick(nowMs) {
  if (!state.alive) return;
  if (!state.hatched) { updateHatching(nowMs); return; }

  if (state.engine.lastLogicMs === 0) {
    state.engine.lastLogicMs = nowMs;
    state.engine.lastHungerMs = nowMs;
    state.engine.lastHappyMs = nowMs;
    state.engine.lastHealthMs = nowMs;
    state.engine.lastAgeMs = nowMs;
    state.engine.lastWifiScanMs = nowMs;
    state.engine.lastBleScanMs = nowMs;
    state.engine.lastBatteryMs = nowMs;
    state.engine.lastTraitMs = nowMs;
    state.engine.lastCoinBonusMs = nowMs;
    state.engine.lastAutoSaveMs = nowMs;
    state.engine.lastLocalThoughtMs = nowMs;
    return;
  }

  if (state.battery <= 0) {
    state.bubble = "Battery dead! Plug in to charge.";
    // Still tick the battery charger and auto-save even when dead,so charging recovers the pet
    updateBattery(nowMs);
    if (nowMs - state.engine.lastAutoSaveMs >= state.controls.autoSaveMs) {
      state.engine.lastAutoSaveMs = nowMs;
      persistState();
    }
    return;
  }

  if (state.miniGame.active) {
    updateMiniGame(nowMs);
    return;
  }

  if (nowMs - state.engine.lastHungerMs >= 5000) {
    state.hun = clamp(state.hun - 2, 0, 100);
    state.engine.lastHungerMs = nowMs;
  }

  if (nowMs - state.engine.lastHappyMs >= 7000) {
    const badWifi = state.wifiStats.netCount === 0 && nowMs - state.engine.lastWifiScanMs > 30000;
    state.fat = clamp(state.fat - (badWifi ? 3 : 1), 0, 100);
    state.engine.lastHappyMs = nowMs;
  }

  if (nowMs - state.engine.lastHealthMs >= 10000) {
    const low = state.hun < 20 || state.fat < 20;
    state.cle = clamp(state.cle - (low ? 2 : 1), 0, 100);
    state.engine.lastHealthMs = nowMs;
  }

  if (nowMs - state.engine.lastAgeMs >= 60000) {
    state.ageMinutes += 1;
    if (state.ageMinutes >= 60) {
      state.ageMinutes -= 60;
      state.ageHours += 1;
    }
    if (state.ageHours >= 24) {
      state.ageHours -= 24;
      state.ageDays += 1;
    }
    state.engine.lastAgeMs = nowMs;
  }

  maybeRandomWifiScan(nowMs);
  maybeBleScan(nowMs);
  bleBroadcastState(nowMs);
  runAgentReActStep(nowMs);
  maybeBleAutoInteract(nowMs);
  wifiFoodTick(nowMs);
  updateBattery(nowMs);
  updateTraits(nowMs);
  updateCoinBonus(nowMs);
  updateVibration(nowMs);
  updateSickness(nowMs);
  updateMood(nowMs);
  state.targetRoom = inferRoomFromStateAndText(state.bubble);
  updateEvolution();
  updateDeath(nowMs);
  if (!state.alive) return;
  updateLocalThought(nowMs);
  decideNextActivity(nowMs);
  updateActivity(nowMs);
  updateLlmScreenReturn(nowMs);

  if (nowMs - state.engine.lastAutoSaveMs >= state.controls.autoSaveMs) {
    state.engine.lastAutoSaveMs = nowMs;
    persistState();
  }

  if (state.controls.autoSleep && state.activity === "IDLE" && state.fat < 25) {
    state.activity = "REST";
    state.engine.activityUntilMs = nowMs + randomInt(6000, 10000);
    setScene("rest");
  }

  // Night auto-sleep
  if (isNightTime() && state.activity === "IDLE" && Math.random() < 0.001) {
    state.activity = "REST";
    state.engine.activityUntilMs = nowMs + randomInt(15000, 30000);
    setScene("rest");
    state.bubble = `${state.name} is sleeping... zzz`;
    sfxSleep();
  }
}

function llmNavigateScreen(screenKey, durationMs) {
  if (state.llmScreenUntilMs > performance.now()) return;
  if (
    screenKey === screens.PET_STATUS ||
    screenKey === screens.STATS ||
    screenKey === screens.ENVIRONMENT ||
    screenKey === screens.SYSTEM
  ) return;
  // Don't navigate away from HOME while resting/sleeping
  if ((state.scene === "sleep" || state.scene === "rest") && screenKey !== screens.HOME) return;
  state.screen = screenKey;
  state.llmScreenUntilMs = performance.now() + durationMs;
  if (screenKey === screens.FOREST) resetShowcaseFrames();
}

function updateLocalThought(nowMs) {
  if (nowMs - state.engine.lastLocalThoughtMs < 6000) return;
  state.engine.lastLocalThoughtMs = nowMs;

  // TinyLLM simulator, mimics stories260K.bin narrative style
  let thought = "";
  let navScreen = null;
  let navDuration = 5000;
  let selfAction = null;

  const nets = state.wifiStats.netCount;
  const rssi = state.wifiStats.avgRSSI;
  const bleDev = state.bleStats.deviceCount;
  const hour = new Date().getHours();
  const isNight = hour >= 22 || hour < 6;
  const isMorning = hour >= 6 && hour < 10;
  const N = state.name;

  // Time-of-day behavior
  if (isNight && state.activity === "IDLE" && Math.random() < 0.2) {
    thought = pick([
      `${N} yawns. It's time to sleep.`,
      "The stars are out. Time to rest.",
      `${N} feels sleepy under the quiet sky.`,
    ]);
    selfAction = "REST"; navScreen = screens.HOME;
  } else if (isMorning && state.activity === "IDLE" && Math.random() < 0.15) {
    thought = pick([
      `${N} wakes up. A new adventure!`,
      "The sun is warm. Time to explore!",
      `${N} stretches and looks for signals.`,
    ]);
    selfAction = pick(["HUNT", "DISCOVER"]);
    navScreen = screens.FOREST; navDuration = 5000;
  }

  // Mood-driven narrative + autonomous actions
  if (!thought && state.mood === "HUNGRY") {
    thought = pick([
      `${N} feels a rumble in her tummy.`,
      `"I need food," ${N} thinks sadly.`,
      `${N} walks to the kitchen to eat.`,
      `Her hunger is at ${state.hun}%. Not good.`,
      `${N} dreams about a big warm meal.`,
      `${N} needs energy badly.`,
    ]);
    if (Math.random() < 0.45) { selfAction = "EAT"; navScreen = screens.HOME; }
  } else if (!thought && state.mood === "SICK") {
    thought = pick([
      `${N} feels unwell and needs care.`,
      "Everything aches. A bath would help.",
      `${N} goes to clean up and feel better.`,
      `"I need rest," ${N} whispers softly.`,
      `${N} is tired and a bit dirty.`,
    ]);
    selfAction = pick(["CLEAN", "REST"]);
  } else if (!thought && state.mood === "EXCITED") {
    thought = pick([
      `${N} jumps with joy! So many signals!`,
      `"Look! ${nets} networks!" she exclaims.`,
      `${N} runs around the room with energy.`,
      "The forest is calling. Time to explore!",
      `${N} wants to play a fun game!`,
      `"This is the best day!" says ${N}.`,
    ]);
    selfAction = pick(["HUNT", "DISCOVER", "PLAY", "SHAKE"]);
    if (Math.random() < 0.3) { navScreen = screens.FOREST; navDuration = 5000; }
  } else if (!thought && state.mood === "CURIOUS") {
    thought = pick([
      `${N} notices a strange signal nearby.`,
      `"What is that?" ${N} wonders aloud.`,
      `${N} finds ${state.wifiStats.hiddenCount} hidden networks.`,
      "Something is beeping in the distance.",
      `${N} carefully follows the signal trail.`,
      "The forest has secrets to discover.",
    ]);
    selfAction = pick(["DISCOVER", "HUNT"]);
    if (Math.random() < 0.35) { navScreen = screens.FOREST; navDuration = 5000; }
  } else if (!thought && state.mood === "BORED") {
    thought = pick([
      `${N} sighs. Nothing is happening.`,
      `"I wish I had something to do."`,
      `${N} looks out the window at the forest.`,
      "Maybe a walk will cheer her up.",
      `${N} looks for something fun to do.`,
      `${N} wants an adventure!`,
    ]);
    selfAction = pick(["PLAY", "HUNT", "DISCOVER"]);
    if (Math.random() < 0.2) { navScreen = screens.FOREST; navDuration = 5000; }
  } else if (!thought && state.mood === "HAPPY") {
    thought = pick([
      `${N} smiles and wags her tail.`,
      `"Everything is wonderful!" says ${N}.`,
      `${N} hums a little song to herself.`,
      "Life is good. The stats are healthy.",
      `${N} looks at her ${state.exp} coins proudly.`,
    ]);
    if (Math.random() < 0.2) selfAction = pick(["PLAY", "SHAKE"]);
  }

  // Activity-driven narrative
  if (!thought && state.activity === "HUNT") {
    thought = pick([
      `${N} scans the air. ${nets} networks!`,
      `Signal strength: ${rssi}dB. Searching...`,
      `${N} finds ${state.wifiStats.strongCount} strong signals.`,
      "The antenna beeps. New APs detected!",
      `${N} is deep in the WiFi forest.`,
      state.wifiStats.openCount > 0 ? `Open network spotted!` : "All secured. Keep probing.",
    ]);
    navScreen = screens.FOREST; navDuration = 5000;
  } else if (!thought && state.activity === "DISCOVER") {
    thought = pick([
      `${N} analyzes a mysterious packet.`,
      "A new network appears on the map!",
      `${N} decodes the hidden SSID carefully.`,
      "Discovery mode active. Mapping signals.",
      `${N} finds something interesting nearby.`,
    ]);
    navScreen = screens.FOREST; navDuration = 4500;
  } else if (!thought && state.activity === "REST") {
    thought = pick([
      "Zzz... sweet dreams.", `${N} sleeps peacefully.`,
      "Quiet and restful. Recharging energy.",
      `${N} dreams about a big forest.`,
    ]);
  } else if (!thought && state.activity === "EAT") {
    thought = pick([
      "Nom nom! Delicious!", `${N} eats happily.`,
      "The food is yummy and warm.",
      `${N} licks her lips. More please!`,
    ]);
  } else if (!thought && state.activity === "CLEAN") {
    thought = pick([
      "Scrub scrub! Almost clean!", `${N} takes a nice bath.`,
      "The warm water feels so good.",
      `${N} is getting sparkly clean.`,
    ]);
  } else if (!thought && state.activity === "PLAY") {
    thought = pick([
      `${N} plays a fun game!`, "Haha! This is so fun!",
      `${N} jumps and laughs happily.`,
      "The game makes her feel alive!",
    ]);
  } else if (!thought && state.activity === "SHAKE") {
    thought = pick([
      `${N} shakes with excitement!`,
      "Shake shake! Full of energy!",
      `${N} does a happy dance.`,
    ]);
  }

  // Idle narrative with self-initiated exploration
  if (!thought) {
    const pool = [
      `${N} sits quietly in her room.`,
      `${N} counts her ${state.exp} coins.`,
      `"I wonder what's in the forest."`,
      `${N} checks her health. ${state.cle}% HP.`,
      `The room is cozy. ${N} feels at peace.`,
      nets > 5 ? `${N} senses ${nets} networks nearby.` : `It's quiet. Only ${nets} signals.`,
      rssi > -60 ? `${N} detects a strong signal!` : "The signals seem far away.",
      `${N} is a ${state.stage}. Growing every day!`,
      state.ageDays > 0 ? `${N} is ${state.ageDays} day(s) old now.` : `${N} is still very young.`,
      `${N} stretches and yawns lazily.`,
      `${N} thinks about changing her room.`,
      bleDev > 0 ? `${N} senses ${bleDev} Bluetooth devices!` : `No BLE devices near ${N}.`,
      state.battery < 30 ? `${N} feels her battery getting low...` : `Battery at ${Math.round(state.battery)}%. Feeling good!`,
      `${N} wants to play a mini-game!`,
      state.traits.curiosity > 70 ? `${N}'s curiosity drives her to explore.` : `${N} relaxes in her ${state.roomTheme.replace("room","")} room.`,
    ];
    thought = pick(pool);

    // Autonomous decisions when idle
    if (Math.random() < 0.2 && state.hun < 50) {
      selfAction = "EAT"; navScreen = screens.HOME;
    } else if (Math.random() < 0.15 && nets > 3) {
      selfAction = "HUNT"; navScreen = screens.FOREST; navDuration = 5000;
    } else if (Math.random() < 0.12) {
      selfAction = pick(["PLAY", "DISCOVER", "SHAKE"]);
    } else if (Math.random() < 0.08) {
      selfAction = "CLEAN";
    }

    // Navigate to relevant screens from idle thoughts
    if (thought.includes("forest")) { navScreen = screens.FOREST; navDuration = 5000; }
    else if (thought.includes("achieve") || thought.includes("badge")) { navScreen = screens.ACHIEVEMENTS; navDuration = 4000; }
    else if (thought.includes("health")) { navScreen = screens.PET_STATUS; navDuration = 4000; }
  }

  // WiFi event interjections
  if (nets > 10 && Math.random() < 0.15) {
    thought = `${N} gasps! ${nets} networks everywhere!`;
  }

  // Stage-specific narrative
  if (state.stage === "ELDER" && Math.random() < 0.08) {
    thought = pick([
      `${N} remembers the old forest days.`,
      "Wisdom comes slowly, like good WiFi.",
      `${N} has grown wise over the years.`,
    ]);
    navScreen = screens.FOREST; navDuration = 5000;
  } else if (state.stage === "BABY" && Math.random() < 0.08) {
    thought = pick([
      `Little ${N} is learning about the world.`,
      `${N} looks at everything with wonder.`,
      "So many new things to discover!",
    ]);
  }

  state.bubble = thought;
  state.targetRoom = inferRoomFromStateAndText(thought);

  // Apply self-initiated action (like IDF room_flow)
  if (selfAction && state.activity === "IDLE") {
    state.activity = selfAction;
    state.engine.activityUntilMs = nowMs + randomInt(8000, 18000);
    state.engine.activityStepMs = nowMs;
    const actCfg = {
      EAT: { scene: "eat", room: "KITCHEN" },
      HUNT: { scene: "hunt", room: "LAB" },
      DISCOVER: { scene: "discover", room: "PLAYROOM" },
      CLEAN: { scene: "clean", room: "BATHROOM" },
      REST: { scene: "rest", room: "BEDROOM" },
      PLAY: { scene: "play", room: "PLAYROOM" },
      SHAKE: { scene: "shake", room: "PLAYROOM" },
    };
    const ac = actCfg[selfAction];
    if (ac) { setScene(ac.scene); state.targetRoom = ac.room; }
  }

  if (navScreen && state.screen === screens.HOME) {
    llmNavigateScreen(navScreen, navDuration);
  }
}

function updateLlmScreenReturn(nowMs) {
  if (state.llmScreenUntilMs > 0 && nowMs > state.llmScreenUntilMs) {
    if (state.screen !== screens.HOME) {
      state.screen = screens.HOME;
    }
    state.llmScreenUntilMs = 0;
  }
}

function panelHeader() {
  if (state.screen !== screens.HOME) {
    if (state.screen === screens.PET_STATUS) return "\u2620 SOUL CARD";
    if (state.screen === screens.ENVIRONMENT) return "ENVIRONMENT";
    if (state.screen === screens.FOREST) {
      if (state.activity === "HUNT") return "HUNTING WIFI...";
      if (state.activity === "DISCOVER") return "DISCOVERING...";
      return "EXPLORING...";
    }
    if (state.screen === screens.SYSTEM) {
      const pages = ["SYSTEM INFO", "CONTROLS", "SETTINGS", "DIAGNOSTICS"];
      return pages[state.systemPage % pages.length];
    }
    if (state.screen === screens.SHOP) return "SHOP";
    if (state.screen === screens.ACHIEVEMENTS) return "ACHIEVEMENTS";
    if (state.screen === screens.TOOLS) {
      const tp = ["WIFI AUDIT", "BLE SCAN", "SIGNAL METER", "NETWORK LIST", "AUDIT LOG"];
      return tp[state.toolsPage % tp.length];
    }
    if (state.screen === screens.STATS) {
      const sp = ["PET VITALS", "TIME & AGE", "ENVIRONMENT", "SYSTEM", "LIFETIME"];
      return sp[state.statsPage % sp.length];
    }
    if (state.screen === screens.WIFI_FOOD) {
      const mode = state.wifiFood.mode;
      if (mode === "SCAN")     return "☠ SNIFFING...";
      if (mode === "PROBES")   return "☠ LURKING...";
      if (mode === "DEAUTH")   return "☠ FEEDING!";
      if (mode === "HANDSHAKE") return "☠ CATCHING...";
      if (mode === "PMKID")    return "☠ HUNTING...";
      if (mode === "BEACON")   return "☠ SPAMMING!";
      return "☠ WIFI FOOD";
    }
  }

  if (state.activity === "HUNT") return "HUNTING WIFI...";
  if (state.activity === "DISCOVER") return "DISCOVERING...";
  if (state.activity === "REST") return "RESTING...";
  return "IDLE";
}

function twoButtonMenuItems() {
  if (state.screen === screens.HOME) {
    return [
      { label: "status", cmd: "screen:pet_status", gx: 2, gy: 0, gs: 22 },
      { label: "food", cmd: "feed", gx: 34, gy: 0, gs: 22 },
      { label: "sleep", cmd: "sleep", gx: 66, gy: 0, gs: 22 },
      { label: "clean", cmd: "clean", gx: 98, gy: 0, gs: 22 },
      { label: "shop", cmd: "screen:shop", gx: 2, gy: 106, gs: 22 },
      { label: "achieve", cmd: "screen:achievements", gx: 34, gy: 106, gs: 22 },
      { label: "tools", cmd: "screen:tools", gx: 66, gy: 106, gs: 22 },
      { label: "stats", cmd: "screen:stats", gx: 98, gy: 106, gs: 22 },
    ];
  }

  return [
    { label: "home",         cmd: "screen:home" },
    { label: "soul card",    cmd: "screen:pet_status" },
    { label: "shop",         cmd: "screen:shop" },
    { label: "achievements", cmd: "screen:achievements" },
    { label: "tools",        cmd: "screen:tools" },
    { label: "stats",        cmd: "screen:stats" },
    { label: "forest",       cmd: "screen:forest" },
    { label: "wifi hunt",    cmd: "screen:wifi_food" },
  ];
}

function isShowcaseScreen() {
  return state.screen === screens.FOREST;
}

function resetShowcaseFrames() {
  state.forestFrame = 0;
  state.forestTickMs = 0;
}

function updateForestFrame(nowMs) {
  const frameLen = Array.isArray(SPRITES.forest1?.frames) ? SPRITES.forest1.frames.length : 1;
  if (state.forestTickMs === 0) {
    state.forestTickMs = nowMs;
    return;
  }
  if (nowMs - state.forestTickMs < 440) return;
  state.forestTickMs = nowMs;
  state.forestFrame = (state.forestFrame + 1) % Math.max(1, frameLen);
}


function syncTwoButtonLabel() {
  const items = twoButtonMenuItems();
  if (state.twoBtn.cursor >= items.length) state.twoBtn.cursor = 0;
}

function twoButtonNext() {
  state.iconsLastInteractMs = performance.now();
  const items = twoButtonMenuItems();
  if (items.length === 0) return;

  if (state.screen === screens.SHOP) {
    state.shopCursor = (state.shopCursor + 1) % SHOP_FOODS.length;
    return;
  }
  if (state.screen === screens.SYSTEM) {
    state.systemPage = (state.systemPage + 1) % 4;
    return;
  }
  if (state.screen === screens.ACHIEVEMENTS) {
    state.achievePage = (state.achievePage + 1) % 4;
    return;
  }
  if (state.screen === screens.TOOLS) {
    state.toolsPage = (state.toolsPage + 1) % 5;
    return;
  }
  if (state.screen === screens.STATS) {
    state.statsPage = (state.statsPage + 1) % 5;
    return;
  }
  if (state.screen === screens.WIFI_FOOD) {
    // BTN B = cycle action menu
    const menuLen = wifiFoodMenuItems(state.wifiFood).length;
    state.wifiFood.menuIdx = (state.wifiFood.menuIdx + 1) % menuLen;
    return;
  }

  state.twoBtn.cursor = (state.twoBtn.cursor + 1) % items.length;
  syncTwoButtonLabel();
}

function twoButtonSelect() {
  state.iconsLastInteractMs = performance.now();
  if (state.screen === screens.SHOP) {
    const food = SHOP_FOODS[state.shopCursor];
    if (food && state.exp >= food.cost) {
      state.exp -= food.cost;
      state.lifetime.coinsSpent += food.cost;
      if (food.isMedicine) {
        applyMedicine();
      } else {
        state.lifetime.foodEaten++;
        state.hun = clamp(state.hun + food.hunger, 0, 100);
        state.fat = clamp(state.fat + 2, 0, 100);
        setScene("eat");
        state.targetRoom = "KITCHEN";
        state.bubble = `Yummy ${food.name}! +${food.hunger} hunger`;
        state.activity = "EAT";
        state.engine.activityUntilMs = performance.now() + 6000;
        state.engine.activityStepMs = performance.now();
        sfxFeed();
        sfxCoin();
      }
    } else if (food) {
      state.bubble = `Need ${food.cost} coins for ${food.name}!`;
    }
    persistState();
    return;
  }
  if (state.screen === screens.WIFI_FOOD) {
    // BTN A = execute selected menu item
    const items = wifiFoodMenuItems(state.wifiFood);
    const item = items[state.wifiFood.menuIdx % items.length];
    if (item) applyCommand(item.cmd);
    return;
  }
  const items = twoButtonMenuItems();
  if (items.length === 0) return;
  const item = items[state.twoBtn.cursor] || items[0];
  applyCommand(item.cmd);
  persistState();
  syncTwoButtonLabel();
}

function twoButtonHome() {
  if (!state.alive) {
    applyCommand("revive");
    return;
  }
  if (state.miniGame.active) {
    state.miniGame.active = false;
    state.bubble = "Game cancelled.";
  }
  state.screen = screens.HOME;
  state.activity = "IDLE";
  state.llmScreenUntilMs = 0;
  state.iconsLastInteractMs = performance.now();
  setScene("home");
  syncTwoButtonLabel();
}

function bindShortLongPress(element, onShort, onLong) {
  let downAt = 0;
  let consumedLong = false;
  let timer = null;
  const LONG_MS = 650;

  const start = () => {
    downAt = Date.now();
    consumedLong = false;
    timer = setTimeout(() => {
      consumedLong = true;
      onLong();
    }, LONG_MS);
  };

  const end = () => {
    if (timer) clearTimeout(timer);
    timer = null;
    if (consumedLong) return;
    if (Date.now() - downAt < LONG_MS) onShort();
  };

  element.addEventListener("mousedown", start);
  element.addEventListener("touchstart", start, { passive: true });
  element.addEventListener("mouseup", end);
  element.addEventListener("mouseleave", end);
  element.addEventListener("touchend", end);
}

function drawInfoPanel() {
  const x = PANEL_X + 5;
  const w = PANEL_W - 10;

  ctx.fillStyle = "#0f1424";
  ctx.fillRect(x, PANEL_Y + 4, w, PANEL_H - 8);
  ctx.strokeStyle = "#33486d";
  ctx.strokeRect(x, PANEL_Y + 4, w, PANEL_H - 8);

  ctx.fillStyle = "#dce9ff";
  ctx.font = "bold 9px monospace";
  ctx.fillText(state.name.slice(0, 8), x + 6, PANEL_Y + 16);
  ctx.font = "8px monospace";
  ctx.fillStyle = "#c0d4ff";
  ctx.fillText(state.mood, x + 6, PANEL_Y + 27);
  ctx.fillStyle = "#8899bb";
  ctx.fillText(`${state.stage} ${state.ageDays}d${state.ageHours}h`, x + 6, PANEL_Y + 37);
  // LLM indicator on header line
  ctx.fillStyle = state.llm ? "#4ae08b" : "#555";
  ctx.font = "6px monospace";
  ctx.textAlign = "right";
  ctx.fillText("LLM", x + w - 6, PANEL_Y + 16);
  ctx.textAlign = "left";

  const drawBar = (label, value, y, color) => {
    ctx.fillStyle = "#8899bb";
    ctx.font = "7px monospace";
    ctx.fillText(label, x + 6, y);
    ctx.fillStyle = "#1f2941";
    ctx.fillRect(x + 6, y + 3, w - 12, 5);
    ctx.fillStyle = color;
    ctx.fillRect(x + 6, y + 3, Math.round(((w - 12) * value) / 100), 5);
  };

  drawBar("HUN", state.hun, PANEL_Y + 46, "#f2b93b");
  drawBar("HAP", state.fat, PANEL_Y + 62, "#57d2ff");
  drawBar("HP", state.cle, PANEL_Y + 78, "#66e08b");

  ctx.font = "8px monospace";
  ctx.fillStyle = "#ffe06a";
  ctx.fillText(`Coins: ${state.exp}`, x + 6, PANEL_Y + 96);

  ctx.fillStyle = "#dce9ff";
  ctx.font = "7px monospace";
  ctx.fillText(`WiFi: ${state.wifiStats.netCount} nets`, x + 6, PANEL_Y + 110);
  ctx.fillText(`BLE: ${state.bleStats.deviceCount} dev`, x + 6, PANEL_Y + 120);

  ctx.fillStyle = "#a0b8e0";
  const batCol = state.battery > 50 ? "#66e08b" : state.battery > 20 ? "#f2b93b" : "#ff5555";
  ctx.fillText(`Bat: ${Math.round(state.battery)}%`, x + 6, PANEL_Y + 130);
  ctx.fillStyle = "#1f2941";
  ctx.fillRect(x + 6 + 36, PANEL_Y + 123, w - 12 - 36, 5);
  ctx.fillStyle = batCol;
  ctx.fillRect(x + 6 + 36, PANEL_Y + 123, Math.round(((w - 12 - 36) * state.battery) / 100), 5);

  const act = state.activity === "IDLE" ? "Idle" : state.activity.charAt(0) + state.activity.slice(1).toLowerCase();
  ctx.fillText(`Act: ${act}`, x + 6, PANEL_Y + 141);

  if (isPeerAlive()) {
    const peerMemory = state.socialMemory[String(blePeer.id)] || null;
    ctx.fillStyle = "#4488ff";
    ctx.fillText(`Peer:${blePeer.name.slice(0,5)}`, x + 6, PANEL_Y + 151);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#9be0ff";
    ctx.fillText(`Bond:${socialBondLabel(peerMemory)}`, x + 6, PANEL_Y + 159);
  }

  // ReAct agent status (always visible)
  const ag = state.agent;
  if (ag.currentTool && ag.currentTool !== "none") {
    const agY = isPeerAlive() ? PANEL_Y + 170 : PANEL_Y + 151;
    ctx.font = "6px monospace";
    ctx.fillStyle = "#f0c040";
    ctx.fillText(`AI:${ag.currentTool}`, x + 6, agY);
    if (ag.lastResult) {
      ctx.fillStyle = "#99cc99";
      const resultShort = ag.lastResult.slice(0, 20);
      ctx.fillText(resultShort, x + 6, agY + 9);
    }
    // Cycle counter in corner
    ctx.fillStyle = "#556677";
    ctx.textAlign = "right";
    ctx.fillText(`#${ag.cycleCount}`, x + w - 6, agY);
    ctx.textAlign = "left";
  }

  if (state.charging) {
    ctx.fillStyle = "#66e08b";
    ctx.font = "6px monospace";
    ctx.textAlign = "right";
    ctx.fillText("CHG", x + w - 6, PANEL_Y + 27);
    ctx.textAlign = "left";
  }
  if (!state.alive) {
    ctx.fillStyle = "#ff5555";
    ctx.font = "8px monospace";
    ctx.fillText("DEAD", x + 6, PANEL_Y + 156);
  }
}

function iconsVisible(nowMs) {
  return (nowMs || performance.now()) - state.iconsLastInteractMs < 15000;
}

function drawBubbleOnLeft(nowMs) {
  if (activeBlePeerConversation(nowMs)) {
    const convo = blePeerConversation;
    const bx = gameToScreenX(2);
    const by = gameToScreenY(66);
    const bw = gameToScreenW(124);
    const bh = gameToScreenH(44);

    ctx.fillStyle = "rgba(8, 14, 26, 0.96)";
    ctx.fillRect(bx, by, bw, bh);
    ctx.strokeStyle = "#9ac4ff";
    ctx.strokeRect(bx, by, bw, bh);

    ctx.font = "7px monospace";
    const charsPerLine = Math.floor((bw - 10) / 4.3) | 0;  // ~4.3px per char at 7px mono

    function wrapLine(text, maxChars) {
      if (text.length <= maxChars) return [text];
      const cut = text.lastIndexOf(" ", maxChars);
      const pos = cut > 8 ? cut : maxChars;
      return [text.slice(0, pos), text.slice(pos).trim()];
    }

    const localLines = wrapLine(convo.localText || "", charsPerLine);
    const peerLines  = wrapLine(convo.peerText  || "", charsPerLine);

    // Local speaker row (blue bg)
    ctx.fillStyle = "#1a3a60";
    ctx.fillRect(bx + 2, by + 2, bw - 4, 8 + (localLines.length > 1 ? 9 : 0));
    ctx.fillStyle = "#ffe06a";
    ctx.fillText(localLines[0], bx + 5, by + 9);
    if (localLines[1]) ctx.fillText(localLines[1], bx + 5, by + 18);

    // Peer speaker row (teal bg)
    const peerRowY = by + 2 + 10 + (localLines.length > 1 ? 9 : 0);
    ctx.fillStyle = "#0b3d38";
    ctx.fillRect(bx + 2, peerRowY, bw - 4, 8 + (peerLines.length > 1 ? 9 : 0));
    ctx.fillStyle = "#67f0e2";
    ctx.fillText(peerLines[0], bx + 5, peerRowY + 7);
    if (peerLines[1]) ctx.fillText(peerLines[1], bx + 5, peerRowY + 16);
    return;
  }

  if (!state.bubble) return;

  const txt = (state.bubble || "");
  const maxLine = 26;
  const twoLines = txt.length > maxLine;
  const hideIcons = !iconsVisible(nowMs);

  let byGame;
  if (hideIcons) {
    byGame = twoLines ? 104 : 112;
  } else {
    byGame = twoLines ? 74 : 82;
  }

  const bx = gameToScreenX(2);
  const by = gameToScreenY(byGame);
  const bw = gameToScreenW(124);
  const bh = gameToScreenH(twoLines ? 24 : 15);

  ctx.fillStyle = "rgba(14, 20, 34, 0.92)";
  ctx.fillRect(bx, by, bw, bh);
  ctx.strokeStyle = "#90b6ff";
  ctx.strokeRect(bx, by, bw, bh);
  ctx.fillStyle = "#f2f7ff";
  ctx.font = "bold 10px monospace";
  if (twoLines) {
    let split = txt.lastIndexOf(" ", maxLine);
    if (split < 8) split = maxLine;
    ctx.fillText(txt.slice(0, split), bx + 4, by + 11);
    ctx.fillText(txt.slice(split).trim().slice(0, maxLine), bx + 4, by + 22);
  } else {
    ctx.fillText(txt.slice(0, maxLine), bx + 4, by + 11);
  }
}

// ════════════════════════════════════════════════════════════════════
//  WiFi Food – Sablina's Hunting Grounds  🍖☠️
// ════════════════════════════════════════════════════════════════════

const FOOD_SIMULATED_APS = [
  { ssid: "HomeNetwork_5G",    bssid: "A4:CF:12:8B:3D:E1", channel: 6,  encryption: "WPA2", rssi: -42, clients: 3 },
  { ssid: "TP-Link_0F4A",      bssid: "50:C7:BF:0F:4A:22", channel: 1,  encryption: "WPA2", rssi: -58, clients: 1 },
  { ssid: "NETGEAR-Guest",     bssid: "28:80:88:5A:12:CC", channel: 11, encryption: "OPEN",  rssi: -65, clients: 0 },
  { ssid: "Vecino_WiFi",       bssid: "D8:47:32:1C:AA:07", channel: 3,  encryption: "WPA2", rssi: -71, clients: 2 },
  { ssid: "CafeLibre",         bssid: "F0:9F:C2:33:BB:10", channel: 6,  encryption: "WPA",  rssi: -68, clients: 4 },
  { ssid: "",                   bssid: "1A:2B:3C:4D:5E:6F", channel: 9,  encryption: "WPA2", rssi: -77, clients: 0 },
  { ssid: "IoT_Devices",       bssid: "AC:84:C6:22:11:AB", channel: 1,  encryption: "WPA3", rssi: -80, clients: 5 },
  { ssid: "AndroidAP_Juan",    bssid: "02:00:00:44:55:66", channel: 6,  encryption: "WPA2", rssi: -53, clients: 1 },
  { ssid: "MOVISTAR_4B2F",     bssid: "E4:AB:89:4B:2F:C0", channel: 11, encryption: "WPA2", rssi: -62, clients: 3 },
  { ssid: "Printer_Office",    bssid: "00:1B:44:11:3A:B7", channel: 4,  encryption: "WEP",  rssi: -73, clients: 0 },
];

function addAuditEntry(type, ssid, detail) {
  const now = new Date();
  const ts = String(now.getHours()).padStart(2, "0") + ":" +
              String(now.getMinutes()).padStart(2, "0") + ":" +
              String(now.getSeconds()).padStart(2, "0");
  state.auditLog.unshift({ ts, type, ssid: (ssid || "???").slice(0, 14), detail: detail || "" });
  if (state.auditLog.length > 30) state.auditLog.pop();
}

function wifiFoodTick(nowMs) {
  const a = state.wifiFood;
  if (a.mode === "IDLE") return;

  const dt = nowMs - a.lastTickMs;
  if (dt < 200) return;
  a.lastTickMs = nowMs;

  // Channel hopping
  if (a.hopping) {
    if (nowMs - a.lastHopMs > 220) {
      a.channel = (a.channel % 13) + 1;
      a.lastHopMs = nowMs;
    }
  }

  // Simulate packet capture
  const pktBurst = Math.floor(Math.random() * 12) + 2;
  a.totalPackets += pktBurst;
  a.mgmtPackets += Math.floor(pktBurst * 0.4);
  a.dataPackets += Math.floor(pktBurst * 0.6);

  // Scanning: gradually discover APs
  if (a.mode === "SCAN" && a.aps.length < FOOD_SIMULATED_APS.length) {
    if (Math.random() < 0.3) {
      const nextAP = FOOD_SIMULATED_APS[a.aps.length];
      a.aps.push({
        ...nextAP,
        handshake: false,
        pmkid: false,
        beacons: Math.floor(Math.random() * 50) + 10,
      });
      addAuditEntry("SCAN", nextAP.ssid || "(hidden)", `CH:${nextAP.channel} ${nextAP.encryption}`);
    }
  }

  // Deauth: count sent frames
  if (a.mode === "DEAUTH") {
    const prevDeauths = a.deauthsSent;
    a.deauthsSent += 5;
    state.lifetime.deauths += 5;
    // Log every 30 deauths sent
    if (Math.floor(a.deauthsSent / 30) > Math.floor(prevDeauths / 30)) {
      const ap = a.aps[a.selectedAP];
      addAuditEntry("DAUTH", ap ? ap.ssid : "???", `x${a.deauthsSent} sent`);
    }
    // After some deauths, simulate a handshake appearing
    if (a.deauthsSent > 30 && a.handshakes.length === 0) {
      const ap = a.aps[a.selectedAP];
      if (ap && !ap.handshake && Math.random() < 0.15) {
        a.eapolPackets += 4;
        a.handshakes.push({
          apBssid: ap.bssid,
          clientMac: "C0:EE:" + Math.random().toString(16).slice(2,4).toUpperCase() + ":AA:BB:CC",
          ssid: ap.ssid,
          messages: "M1+M2+M3+M4",
          complete: true,
        });
        ap.handshake = true;
        sfxCoin();
        // 🍖 Sablina eats! Handshakes are a full meal
        state.hun = Math.min(100, state.hun + 15);
        state.fat = Math.min(100, state.fat + 8);
        state.exp = clamp(state.exp + 5, 0, 9999);
        state.lifetime.handshakes++;
        state.lifetime.coinsEarned += 5;
        state.bubble = `☠️ ${state.name} devoured a handshake from ${ap.ssid}!`;
        addAuditEntry("HS", ap.ssid, `DEAUTH ${ap.bssid.slice(0,8)}...`);
      }
    }
  }

  // Handshake capture (passive)
  if (a.mode === "HANDSHAKE") {
    a.eapolPackets += Math.random() < 0.1 ? 1 : 0;
    const ap = a.aps[a.selectedAP];
    if (ap && !ap.handshake && Math.random() < 0.02) {
      a.eapolPackets += 4;
      a.handshakes.push({
        apBssid: ap.bssid,
        clientMac: "B8:27:" + Math.random().toString(16).slice(2,4).toUpperCase() + ":11:22:33",
        ssid: ap.ssid,
        messages: "M1+M2",
        complete: true,
      });
      ap.handshake = true;
      sfxCoin();
      // 🍖 Passive capture = lighter snack
      state.hun = Math.min(100, state.hun + 10);
      state.fat = Math.min(100, state.fat + 5);
      state.exp = clamp(state.exp + 3, 0, 9999);
      state.lifetime.handshakes++;
      state.lifetime.coinsEarned += 3;
      state.bubble = `🍖 ${state.name} caught a handshake from ${ap.ssid}!`;
      addAuditEntry("HS", ap.ssid, `PASSIVE M1+M2`);
    }
  }

  // PMKID capture
  if (a.mode === "PMKID") {
    for (const ap of a.aps) {
      if (!ap.pmkid && ap.encryption === "WPA2" && Math.random() < 0.008) {
        const pmkidHex = Array.from({length: 16}, () =>
          Math.floor(Math.random()*256).toString(16).padStart(2,"0")).join("");
        a.pmkids.push({ apBssid: ap.bssid, ssid: ap.ssid, pmkid: pmkidHex });
        ap.pmkid = true;
        sfxCoin();
        // 🍬 PMKID = candy for Sablina
        state.hun = Math.min(100, state.hun + 8);
        state.fat = Math.min(100, state.fat + 3);
        state.exp = clamp(state.exp + 2, 0, 9999);
        state.lifetime.pmkidCaptures++;
        state.lifetime.coinsEarned += 2;
        state.bubble = `🍬 ${state.name} snatched a PMKID from ${ap.ssid || 'hidden'}!`;
        addAuditEntry("PMKID", ap.ssid || "(hidden)", pmkidHex.slice(0, 10) + "...");
      }
    }
  }

  // Probe Sniff: passively capture probe requests from nearby devices
  // (like Marauder's sniffbeacon / probe-request-sniff)
  if (a.mode === "PROBES") {
    if (!a.probes) a.probes = 0;
    if (!a.probeSnacks) a.probeSnacks = 0;
    const PROBE_DEVICES = [
      "iPhone_Juan","Galaxy_S22","Laptop_HP","Pixel_7","iPad_Casa",
      "MacBook_Pro","ThinkPad","Galaxy_Tab","Redmi_Note","OnePlus_11",
    ];
    // Simulate 1-3 probe bursts per tick
    const burst = Math.floor(Math.random() * 3) + 1;
    for (let i = 0; i < burst; i++) {
      if (Math.random() < 0.4) {
        a.probes++;
        a.totalPackets++;
        a.mgmtPackets++;
        const dev = PROBE_DEVICES[Math.floor(Math.random() * PROBE_DEVICES.length)];
        const mac = Array.from({length:6},()=>Math.floor(Math.random()*256).toString(16).padStart(2,"0")).join(":");
        addAuditEntry("PROBE", dev, mac.slice(0,8)+"...");
        // Every 5 probes = a small snack for Sablina (curiosity food)
        if (a.probes % 5 === 0) {
          a.probeSnacks++;
          state.hun = Math.min(100, state.hun + 3);
          state.exp = clamp(state.exp + 1, 0, 9999);
          state.lifetime.coinsEarned++;
          state.bubble = `📡 ${state.name} sniffed ${dev}'s probe request!`;
          sfxCoin();
        }
      }
    }
  }

  // Beacon Spam: spray fake SSIDs to confuse clients
  // (like Marauder's beacon-spam-random)
  if (a.mode === "BEACON") {
    if (!a.beaconsSent) a.beaconsSent = 0;
    if (!a.confused) a.confused = 0;
    const FAKE_SSIDS = [
      "FBI_Surveillance_Van","Pretty_Fly_for_WiFi","The_Internet","Skynet_Global",
      "HackerLair_5G","MomUseThis1","NSA_Mobile_Unit","NotYourWifi",
      "TotallyLegitAP","VPN_for_Sale","GoAway","Hidden_Network_OwO",
    ];
    a.beaconsSent += Math.floor(Math.random() * 8) + 3;
    a.totalPackets += Math.floor(Math.random() * 5) + 2;
    // Occasionally confuse a client (and feed Sablina happiness)
    if (Math.random() < 0.08) {
      a.confused++;
      const fakeSSID = FAKE_SSIDS[Math.floor(Math.random() * FAKE_SSIDS.length)];
      state.hun = Math.min(100, state.hun + 2);
      state.fat = Math.min(100, state.fat + 1);
      state.exp = clamp(state.exp + 1, 0, 9999);
      state.lifetime.coinsEarned++;
      state.bubble = `📢 ${state.name} confused a device with "${fakeSSID}"!`;
      addAuditEntry("BCSPAM", fakeSSID, `confused x${a.confused}`);
      sfxCoin();
    }
  }
}

function wifiFoodMenuItems(a) {
  const items = [
    { icon: "\uD83D\uDD0D", label: "Sniff",    cmd: "food_scan",      col: "#a7ffeb" },
    { icon: "\uD83D\uDCE1", label: "Probes",   cmd: "food_probes",    col: "#b3e5fc" },
    { icon: "\u2620",       label: "Shake",    cmd: "food_deauth",    col: "#ffcdd2", needsAP: true },
    { icon: "\uD83C\uDF56", label: "Catch HS", cmd: "food_handshake", col: "#e1bee7", needsAP: true },
    { icon: "\uD83C\uDF6C", label: "PMKID",    cmd: "food_pmkid",     col: "#ffe0b2", needsAP: true },
    { icon: "\uD83D\uDCE2", label: "Beacon",   cmd: "food_beacon",    col: "#fff9c4" },
    { icon: "\u23F9",       label: "Stop",     cmd: "food_stop",      col: "#cfd8dc" },
    { icon: "\u25C0",       label: "Prev AP",  cmd: "food_prev",      col: "#90a4ae" },
    { icon: "\u25B6",       label: "Next AP",  cmd: "food_next",      col: "#90a4ae" },
    { icon: "\uD83C\uDFE0", label: "Home",     cmd: "screen:home",    col: "#c8e6c9" },
  ];
  return items;
}

// ── Shared helpers for wifi food screen ─────────────────────────────────────
function _wfDrawBottomMenu(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  const menuItems = wifiFoodMenuItems(a);
  const menuIdx = a.menuIdx % menuItems.length;
  const visCount = 5;
  const startIdx = Math.max(0, Math.min(menuIdx - 2, menuItems.length - visCount));
  const menuY = SCREEN.h - 42;
  ctx.fillStyle = "rgba(6,10,16,0.96)";
  ctx.fillRect(x0, menuY, w, 34);
  ctx.strokeStyle = "#1a2a3a";
  ctx.strokeRect(x0, menuY, w, 34);
  const itemW = Math.floor(w / visCount);
  for (let vi = 0; vi < visCount; vi++) {
    const ii = startIdx + vi;
    if (ii >= menuItems.length) break;
    const m = menuItems[ii];
    const sel = ii === menuIdx;
    const mx = x0 + vi * itemW;
    if (sel) {
      ctx.fillStyle = "rgba(0,200,80,0.22)";
      ctx.fillRect(mx + 1, menuY + 1, itemW - 2, 32);
      ctx.strokeStyle = "#00e676";
      ctx.strokeRect(mx + 1, menuY + 1, itemW - 2, 32);
    }
    ctx.font = "10px monospace";
    ctx.fillStyle = sel ? "#fff" : "#506070";
    ctx.textAlign = "center";
    ctx.fillText(m.icon, mx + itemW / 2, menuY + 13);
    ctx.font = "6px monospace";
    ctx.fillStyle = sel ? m.col : "#405060";
    ctx.fillText(m.label, mx + itemW / 2, menuY + 25);
    ctx.textAlign = "left";
  }
  for (let di = 0; di < menuItems.length; di++) {
    ctx.fillStyle = di === menuIdx ? "#00e676" : "#1a2a3a";
    ctx.fillRect(x0 + 4 + di * 6, menuY + 31, 4, 2);
  }
  ctx.font = "6px monospace";
  ctx.fillStyle = "#1a4a2a";
  ctx.fillText("[A]=run  [B]=next", x0 + 2, SCREEN.h - 2);
}

function _wfBg(col1, col2) {
  // vertical gradient-like background via two bands
  const x0 = LEFT_X, w = LEFT_W, h = SCREEN.h;
  ctx.fillStyle = col1;
  ctx.fillRect(x0, 0, w, h);
  const grad = ctx.createLinearGradient(x0, 0, x0, h);
  grad.addColorStop(0, col1);
  grad.addColorStop(1, col2);
  ctx.fillStyle = grad;
  ctx.fillRect(x0, 0, w, h);
}

// ── IDLE screen ──────────────────────────────────────────────────────────────
function _wfDrawIdle(nowMs) {
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#040a0e", "#071218");
  ctx.font = "bold 11px monospace";
  ctx.fillStyle = "#1e3a4a";
  ctx.fillText("☠ WIFI FOOD", x0 + 4, 16);
  const tools = [
    ["🔍","Sniff","Scan APs on all channels","#a7ffeb"],
    ["📡","Probes","Catch probe requests","#b3e5fc"],
    ["☠","Shake","Deauth flood on target","#ffcdd2"],
    ["🍖","Catch HS","EAPOL handshake capture","#e1bee7"],
    ["🍬","PMKID","Clientless RSN extract","#ffe0b2"],
    ["📢","Beacon","Spam fake SSIDs","#fff9c4"],
  ];
  let ty = 32;
  const pulse = 0.6 + 0.4 * Math.sin(nowMs / 800);
  tools.forEach(([ic, name, desc, col], i) => {
    const a = (i % 2 === 0) ? pulse : 1 - pulse * 0.3;
    ctx.font = "bold 7px monospace";
    ctx.fillStyle = col;
    ctx.globalAlpha = a;
    ctx.fillText(`${ic} ${name}`, x0 + 4, ty);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#405060";
    ctx.globalAlpha = 1;
    ctx.fillText(desc, x0 + 62, ty);
    ty += 12;
  });
  ctx.globalAlpha = 1;
  const blink = Math.floor(nowMs / 600) % 2 === 0;
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = blink ? "#00e676" : "#1a4a2a";
  ctx.fillText("▶ press [A] to activate a tool", x0 + 10, ty + 8);
}

// ── SCAN screen,radar + AP table ──────────────────────────────────────────
function _wfDrawScan(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#020d08", "#041209");
  // Radar circle (top-right)
  const rx = x0 + w - 28, ry = 26, rr = 20;
  ctx.strokeStyle = "#0a2a18";
  ctx.lineWidth = 1;
  for (let r = rr; r >= 6; r -= 7) {
    ctx.beginPath(); ctx.arc(rx, ry, r, 0, Math.PI * 2); ctx.stroke();
  }
  ctx.beginPath(); ctx.moveTo(rx - rr, ry); ctx.lineTo(rx + rr, ry); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(rx, ry - rr); ctx.lineTo(rx, ry + rr); ctx.stroke();
  // Sweep arm
  const angle = ((nowMs / 900) % 1) * Math.PI * 2 - Math.PI / 2;
  ctx.strokeStyle = "#00e676";
  ctx.lineWidth = 1.5;
  ctx.globalAlpha = 0.8;
  ctx.beginPath(); ctx.moveTo(rx, ry);
  ctx.lineTo(rx + Math.cos(angle) * rr, ry + Math.sin(angle) * rr); ctx.stroke();
  ctx.globalAlpha = 1; ctx.lineWidth = 1;
  // Sweep trail (fade)
  for (let t = 1; t <= 6; t++) {
    const ta = angle - t * 0.18;
    ctx.strokeStyle = `rgba(0,230,118,${0.12 - t * 0.018})`;
    ctx.beginPath(); ctx.moveTo(rx, ry);
    ctx.lineTo(rx + Math.cos(ta) * rr, ry + Math.sin(ta) * rr); ctx.stroke();
  }
  // AP blips on radar
  a.aps.forEach((ap, i) => {
    const bAngle = ((ap.channel / 14) * Math.PI * 2) - Math.PI / 2 + i * 0.6;
    const bDist = rr * (0.3 + 0.6 * ((-ap.rssi - 40) / 50));
    const bx = rx + Math.cos(bAngle) * Math.min(bDist, rr - 3);
    const by = ry + Math.sin(bAngle) * Math.min(bDist, rr - 3);
    ctx.fillStyle = i === a.selectedAP ? "#ffeb3b" : "#00e676";
    ctx.fillRect(bx - 1, by - 1, 3, 3);
  });
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#00e676";
  ctx.fillText(`[SCAN] CH:${a.channel}  ${a.aps.length}/${FOOD_SIMULATED_APS.length} APs`, x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#2a5a3a";
  ctx.fillText(`PKT:${a.totalPackets}  hopping...`, x0 + 2, 20);
  // AP list
  let ty = 34;
  ctx.font = "bold 6px monospace";
  ctx.fillStyle = "#1a4a2a";
  ctx.fillText("  SSID           CH  ENC   RSSI", x0 + 2, ty); ty += 9;
  ctx.strokeStyle = "#1a3a2a";
  ctx.beginPath(); ctx.moveTo(x0 + 1, ty - 2); ctx.lineTo(x0 + w - 30, ty - 2); ctx.stroke();
  const maxRows = 7;
  for (let i = 0; i < Math.min(a.aps.length, maxRows); i++) {
    const ap = a.aps[i];
    const sel = i === a.selectedAP;
    const encCol = { WPA3:"#a7ffeb", WPA2:"#00e676", WPA:"#ffeb3b", WEP:"#ff7043", OPEN:"#ff5252" }[ap.encryption] || "#ccc";
    ctx.font = "7px monospace";
    ctx.fillStyle = sel ? "#ffeb3b" : "#c0c8d0";
    const ssid = (ap.ssid || "<hidden>").slice(0, 14).padEnd(14);
    ctx.fillText(`${sel?"▸":" "}${ssid} ${String(ap.channel).padStart(2)}`, x0 + 2, ty);
    ctx.fillStyle = encCol;
    ctx.fillText(ap.encryption.padEnd(5), x0 + 148, ty);
    // RSSI bar
    const barW = Math.round(((-ap.rssi - 40) / 55) * 30);
    ctx.fillStyle = "#1a3a1a";
    ctx.fillRect(x0 + 185, ty - 6, 30, 6);
    ctx.fillStyle = sel ? "#ffeb3b" : "#00c853";
    ctx.fillRect(x0 + 185, ty - 6, Math.max(1, barW), 6);
    ty += 9;
  }
  if (a.aps.length === 0) {
    const blink = Math.floor(nowMs / 500) % 2;
    ctx.font = "8px monospace";
    ctx.fillStyle = blink ? "#00e676" : "#1a3a2a";
    ctx.fillText("scanning...", x0 + w / 2 - 28, 85);
  }
}

// ── PROBES screen,probe request sniffer ────────────────────────────────────
function _wfDrawProbes(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#020810", "#040d1e");
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#40c4ff";
  ctx.fillText(`[PROBE SNIFF] CH:${a.channel}`, x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#1a3a5a";
  ctx.fillText(`caught: ${a.probes||0}  snacks: ${a.probeSnacks||0}  PKT:${a.totalPackets}`, x0 + 2, 20);
  // Antenna icon (center-ish)
  const ax = x0 + w - 24, ay = 22;
  ctx.strokeStyle = "#1a4a8a";
  ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(ax, ay + 28); ctx.lineTo(ax, ay + 8); ctx.stroke();
  ctx.lineWidth = 1;
  // Radio waves from antenna
  for (let r = 1; r <= 3; r++) {
    const wAlpha = (0.7 - r * 0.18) * (0.5 + 0.5 * Math.sin(nowMs / 300 - r * 0.9));
    ctx.strokeStyle = `rgba(64,196,255,${wAlpha})`;
    ctx.beginPath();
    ctx.arc(ax, ay + 8, r * 7, -Math.PI * 0.85, -Math.PI * 0.15);
    ctx.stroke();
  }
  ctx.lineWidth = 1;
  // Flying probe dots (animated, 8 pseudo-random paths)
  const PROBE_COLORS = ["#40c4ff","#82b1ff","#80d8ff","#e1f5fe","#b3e5fc"];
  for (let i = 0; i < 8; i++) {
    const phase = ((nowMs / (900 + i * 120) + i * 0.37) % 1);
    if (phase > 0.75) continue;
    const startX = x0 + 5 + (i * 28) % (w - 60);
    const endX = ax;
    const bx = startX + (endX - startX) * phase;
    const by = 40 + i * 8 + Math.sin(phase * Math.PI) * -12;
    const alpha = 1 - phase * 1.3;
    ctx.fillStyle = PROBE_COLORS[i % PROBE_COLORS.length];
    ctx.globalAlpha = Math.max(0, alpha);
    ctx.fillRect(bx, by, 3, 3);
    ctx.globalAlpha = 1;
  }
  // Recent probe log
  const recent = state.auditLog.filter(e => e.type === "PROBE").slice(0, 5);
  ctx.font = "bold 6px monospace";
  ctx.fillStyle = "#1a3a6a";
  ctx.fillText("── PROBE LOG ─────────────────────", x0 + 2, 36);
  let ty = 47;
  if (recent.length === 0) {
    ctx.font = "7px monospace";
    ctx.fillStyle = "#2a4a7a";
    ctx.fillText("listening for devices...", x0 + 8, ty + 14);
  } else {
    recent.forEach((e, i) => {
      const rowAlpha = 1 - i * 0.15;
      ctx.font = "7px monospace";
      ctx.fillStyle = `rgba(129,199,255,${rowAlpha})`;
      ctx.fillText(`${e.ts}  ${e.ssid.padEnd(14)}`, x0 + 4, ty);
      ctx.fillStyle = `rgba(96,112,144,${rowAlpha})`;
      ctx.fillText(e.detail || "", x0 + 4, ty + 8);
      ty += 17;
    });
  }
  // Hunger bar (snack meter)
  const snackPct = Math.min(1, (a.probeSnacks || 0) / 10);
  ctx.fillStyle = "#0a1a2a";
  ctx.fillRect(x0 + 2, 120, w - 28, 7);
  ctx.fillStyle = "#40c4ff";
  ctx.fillRect(x0 + 2, 120, Math.round((w - 28) * snackPct), 7);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#1a4a6a";
  ctx.fillText(`snack meter: ${a.probeSnacks||0}/10`, x0 + 2, 133);
}

// ── DEAUTH screen,attack mode ──────────────────────────────────────────────
function _wfDrawDeauth(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#0e0202", "#1a0404");
  const ap = a.aps[a.selectedAP];
  // Pulsing red overlay
  const pulse = 0.05 + 0.07 * Math.abs(Math.sin(nowMs / 180));
  ctx.fillStyle = `rgba(200,0,0,${pulse})`;
  ctx.fillRect(LEFT_X, 0, LEFT_W, SCREEN.h);
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#ff5252";
  ctx.fillText("☠ DEAUTH FLOOD", x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#7a2020";
  ctx.fillText(`TX: ${a.deauthsSent} frames  CH:${a.channel}`, x0 + 2, 20);
  // Target box
  const targetSsid = ap ? (ap.ssid || "<hidden>") : "NO TARGET";
  ctx.strokeStyle = "#ff1744";
  ctx.lineWidth = 1.5;
  ctx.strokeRect(x0 + 4, 26, w - 8, 28);
  // Crosshair corners
  const cX = x0 + 4, cY = 26, cW = w - 8, cH = 28;
  ctx.lineWidth = 2;
  [[cX, cY],[cX+cW,cY],[cX,cY+cH],[cX+cW,cY+cH]].forEach(([bx,by],i) => {
    const dx = i % 2 === 0 ? 6 : -6;
    const dy = i < 2 ? 6 : -6;
    ctx.strokeStyle = "#ff5252";
    ctx.beginPath(); ctx.moveTo(bx, by + dy); ctx.lineTo(bx, by); ctx.lineTo(bx + dx, by); ctx.stroke();
  });
  ctx.lineWidth = 1;
  ctx.font = "bold 9px monospace";
  ctx.fillStyle = "#ff1744";
  ctx.fillText(targetSsid.slice(0, 22), x0 + 8, 38);
  if (ap) {
    ctx.font = "6px monospace";
    ctx.fillStyle = "#7a2020";
    ctx.fillText(`${ap.bssid}  CH:${ap.channel}`, x0 + 8, 50);
  }
  // Lightning bolts (animated)
  const numBolts = 3;
  for (let b = 0; b < numBolts; b++) {
    const bPhase = ((nowMs / 300 + b * 0.45) % 1);
    const alpha = bPhase < 0.3 ? bPhase / 0.3 : (bPhase < 0.6 ? 1 : 1 - (bPhase - 0.6) / 0.4);
    ctx.strokeStyle = `rgba(255,100,0,${alpha * 0.9})`;
    ctx.lineWidth = 1.5;
    const bx = x0 + 20 + b * 70 + Math.sin(nowMs / 100 + b) * 8;
    ctx.beginPath();
    ctx.moveTo(bx, 60);
    ctx.lineTo(bx + 6, 72); ctx.lineTo(bx - 2, 72);
    ctx.lineTo(bx + 4, 90); ctx.stroke();
  }
  ctx.lineWidth = 1;
  // Deauth counter bar
  const maxDeauths = 200;
  const pct = Math.min(1, a.deauthsSent / maxDeauths);
  ctx.fillStyle = "#2a0808";
  ctx.fillRect(x0 + 2, 96, w - 4, 10);
  const barCol = pct < 0.4 ? "#ff7043" : pct < 0.7 ? "#ff1744" : "#d50000";
  ctx.fillStyle = barCol;
  ctx.fillRect(x0 + 2, 96, Math.round((w - 4) * pct), 10);
  ctx.font = "bold 7px monospace";
  ctx.fillStyle = "#ff8a80";
  ctx.fillText(`FRAMES SENT: ${a.deauthsSent}`, x0 + 4, 105);
  // Handshakes caught during deauth
  const caughtHS = a.handshakes.filter(h => h.complete);
  if (caughtHS.length > 0) {
    ctx.font = "bold 7px monospace";
    ctx.fillStyle = "#ce93d8";
    ctx.fillText(`🍖 HS CAUGHT: ${caughtHS.length}`, x0 + 4, 116);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#7a4a8a";
    ctx.fillText(caughtHS[0].ssid?.slice(0,20) || "", x0 + 4, 125);
  } else {
    ctx.font = "6px monospace";
    const blink = Math.floor(nowMs / 500) % 2;
    ctx.fillStyle = blink ? "#7a2020" : "#2a0808";
    ctx.fillText(a.deauthsSent < 30 ? "waiting for disconnect..." : "forcing reconnect...", x0 + 4, 116);
  }
}

// ── HANDSHAKE screen,EAPOL capture ────────────────────────────────────────
function _wfDrawHandshake(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#08020e", "#100416");
  const ap = a.aps[a.selectedAP];
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#ce93d8";
  ctx.fillText("🍖 EAPOL HANDSHAKE", x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#4a2a5a";
  ctx.fillText(`target: ${ap ? (ap.ssid||"<hidden>").slice(0,20) : "none"}  CH:${a.channel}`, x0 + 2, 20);
  // 4-way handshake ladder (M1→M2→M3→M4)
  const msgs = ["M1","M2","M3","M4"];
  const caught = a.eapolPackets;
  const capturedMsgs = Math.min(4, Math.floor(caught / 1.2));
  const ladderX = x0 + 10, ladderY = 30, ladderH = 60;
  const clientX = x0 + w - 30;
  // AP column
  ctx.font = "bold 7px monospace";
  ctx.fillStyle = "#6a3a8a";
  ctx.fillText("AP", ladderX + 4, ladderY - 4);
  ctx.strokeStyle = "#3a1a5a";
  ctx.beginPath(); ctx.moveTo(ladderX + 8, ladderY); ctx.lineTo(ladderX + 8, ladderY + ladderH); ctx.stroke();
  // Client column
  ctx.font = "bold 7px monospace";
  ctx.fillStyle = "#6a3a8a";
  ctx.fillText("CLI", clientX - 4, ladderY - 4);
  ctx.beginPath(); ctx.moveTo(clientX + 8, ladderY); ctx.lineTo(clientX + 8, ladderY + ladderH); ctx.stroke();
  // Message arrows
  msgs.forEach((msg, i) => {
    const my = ladderY + 6 + i * 14;
    const captured = i < capturedMsgs;
    const active = i === capturedMsgs && Math.floor(nowMs / 400) % 2 === 0;
    const fromX = i % 2 === 0 ? ladderX + 8 : clientX + 8;
    const toX   = i % 2 === 0 ? clientX + 8 : ladderX + 8;
    ctx.strokeStyle = captured ? "#ce93d8" : (active ? "#9c27b0" : "#2a1a3a");
    ctx.lineWidth = captured ? 1.5 : 1;
    ctx.beginPath(); ctx.moveTo(fromX, my); ctx.lineTo(toX, my); ctx.stroke();
    // Arrow head
    const dir = toX > fromX ? 1 : -1;
    ctx.beginPath(); ctx.moveTo(toX, my); ctx.lineTo(toX - dir * 5, my - 3); ctx.moveTo(toX, my); ctx.lineTo(toX - dir * 5, my + 3); ctx.stroke();
    ctx.lineWidth = 1;
    ctx.font = "bold 6px monospace";
    ctx.fillStyle = captured ? "#e1bee7" : (active ? "#9c27b0" : "#3a2a4a");
    const midX = (fromX + toX) / 2;
    ctx.textAlign = "center";
    ctx.fillText(msg, midX, my - 2);
    ctx.textAlign = "left";
  });
  // EAPOL packet meter
  ctx.fillStyle = "#1a0a2a";
  ctx.fillRect(x0 + 2, 98, w - 4, 8);
  ctx.fillStyle = "#9c27b0";
  ctx.fillRect(x0 + 2, 98, Math.min(w - 4, Math.round((a.eapolPackets / 20) * (w - 4))), 8);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#6a3a8a";
  ctx.fillText(`EAPOL pkts: ${a.eapolPackets}`, x0 + 4, 108);
  // Caught handshakes
  const caught_hs = a.handshakes.filter(h => h.complete);
  if (caught_hs.length > 0) {
    ctx.font = "bold 8px monospace";
    ctx.fillStyle = "#e040fb";
    ctx.fillText(`✓ ${caught_hs.length} HS CAPTURED!`, x0 + 4, 120);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#7a4a8a";
    caught_hs.slice(0, 2).forEach((h, i) => {
      ctx.fillText(`${h.ssid?.slice(0,14)||""}  ${h.messages}`, x0 + 4, 130 + i * 9);
    });
  } else {
    ctx.font = "6px monospace";
    const blink = Math.floor(nowMs / 600) % 2;
    ctx.fillStyle = blink ? "#4a2a6a" : "#2a1a4a";
    ctx.fillText("waiting for 4-way handshake...", x0 + 4, 120);
  }
}

// ── PMKID screen,clientless RSN extraction ─────────────────────────────────
function _wfDrawPmkid(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#080600", "#100e00");
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#ffb74d";
  ctx.fillText("🍬 PMKID EXTRACT", x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#5a3a00";
  ctx.fillText(`APs: ${a.aps.length}  PMKIDs: ${a.pmkids.length}  PKT:${a.totalPackets}`, x0 + 2, 20);
  // AP grid with lock icons
  let col = 0, row = 0;
  const cellW = 44, cellH = 24, gridX = x0 + 4, gridY = 28;
  a.aps.slice(0, 10).forEach((ap, i) => {
    const cx = gridX + col * cellW;
    const cy = gridY + row * cellH;
    const cracked = ap.pmkid;
    ctx.fillStyle = cracked ? "rgba(255,183,77,0.18)" : "rgba(30,20,0,0.8)";
    ctx.fillRect(cx, cy, cellW - 2, cellH - 2);
    ctx.strokeStyle = cracked ? "#ffb74d" : "#3a2a00";
    ctx.strokeRect(cx, cy, cellW - 2, cellH - 2);
    ctx.font = "7px monospace";
    ctx.fillStyle = cracked ? "#ffb74d" : "#6a5a30";
    ctx.fillText(cracked ? "🍬" : "🔒", cx + 2, cy + 10);
    ctx.font = "6px monospace";
    ctx.fillStyle = cracked ? "#ffe0b2" : "#4a3a18";
    ctx.fillText((ap.ssid || "?").slice(0, 6), cx + 14, cy + 10);
    ctx.fillStyle = cracked ? "#ffa000" : "#2a1a00";
    ctx.fillText(`CH${ap.channel}`, cx + 4, cy + 20);
    col++;
    if (col >= 5) { col = 0; row++; }
  });
  if (a.aps.length === 0) {
    ctx.font = "7px monospace";
    ctx.fillStyle = "#4a3a00";
    ctx.fillText("run Sniff first to find APs...", x0 + 4, 70);
  }
  // PMKID hex preview (last cracked)
  const lastPmkid = a.pmkids[a.pmkids.length - 1];
  if (lastPmkid) {
    ctx.fillStyle = "#1a1200";
    ctx.fillRect(x0 + 2, 98, w - 4, 22);
    ctx.strokeStyle = "#ffb74d";
    ctx.strokeRect(x0 + 2, 98, w - 4, 22);
    ctx.font = "bold 6px monospace";
    ctx.fillStyle = "#ffa000";
    ctx.fillText(`PMKID: ${lastPmkid.ssid?.slice(0,12)||"?"}`, x0 + 5, 107);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#7a5a00";
    ctx.fillText(lastPmkid.pmkid.slice(0, 30), x0 + 5, 117);
  } else {
    // Animated "assoc request" arrow
    const progX = (nowMs / 60) % (w - 10);
    ctx.fillStyle = "#3a2a00";
    ctx.fillRect(x0 + 2, 98, w - 4, 22);
    ctx.strokeStyle = "#5a3a00";
    ctx.strokeRect(x0 + 2, 98, w - 4, 22);
    ctx.fillStyle = "#ffb74d";
    ctx.fillRect(x0 + 4 + progX % (w - 20), 104, 8, 4);
    ctx.font = "6px monospace";
    ctx.fillStyle = "#4a3a00";
    ctx.fillText("sending ASSOC REQ →", x0 + 5, 114);
  }
  // Progress line
  if (a.pmkids.length > 0) {
    ctx.font = "bold 7px monospace";
    ctx.fillStyle = "#ff8f00";
    ctx.fillText(`🍬 x${a.pmkids.length} captured!`, x0 + 4, 128);
  }
}

// ── BEACON screen,fake SSID spam ───────────────────────────────────────────
function _wfDrawBeacon(nowMs) {
  const a = state.wifiFood;
  const x0 = LEFT_X + 1, w = LEFT_W - 2;
  _wfBg("#080800", "#121200");
  // Header
  ctx.font = "bold 8px monospace";
  ctx.fillStyle = "#fff176";
  ctx.fillText("📢 BEACON SPAM", x0 + 2, 10);
  ctx.font = "6px monospace";
  ctx.fillStyle = "#4a4a00";
  ctx.fillText(`sent: ${a.beaconsSent||0}  confused: ${a.confused||0}  PKT:${a.totalPackets}`, x0 + 2, 20);
  // Antenna (center bottom of content area)
  const antX = x0 + w / 2, antY = 105;
  ctx.strokeStyle = "#6a6a00";
  ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(antX, antY); ctx.lineTo(antX, antY - 28); ctx.stroke();
  ctx.lineWidth = 1;
  ctx.strokeStyle = "#4a4a00";
  ctx.beginPath(); ctx.moveTo(antX - 8, antY); ctx.lineTo(antX + 8, antY); ctx.stroke();
  // Ripple waves from antenna
  for (let r = 1; r <= 4; r++) {
    const phase = ((nowMs / 400) + r * 0.28) % 1;
    const radius = phase * 50;
    const alpha = (1 - phase) * 0.6;
    ctx.strokeStyle = `rgba(255,241,0,${alpha})`;
    ctx.beginPath();
    ctx.arc(antX, antY - 28, radius, Math.PI * 1.1, Math.PI * 1.9);
    ctx.stroke();
    ctx.beginPath();
    ctx.arc(antX, antY - 28, radius, -Math.PI * 0.9, -Math.PI * 0.1);
    ctx.stroke();
  }
  // Flying fake SSIDs (12 projectiles in arcs)
  const FAKE_SSIDS_DISPLAY = ["FBI_Van","PrettyFly","Skynet","HackerLair","NSA_Unit","NotUrWifi","TotallyLegit","VPN4Sale","GoAway","HiddenOwO","MomUseThis","TheInternet"];
  for (let i = 0; i < 8; i++) {
    const seed = i * 1.37 + 0.3;
    const phase = ((nowMs / (700 + i * 90) + seed) % 1);
    if (phase < 0.05) continue;
    const angle = -Math.PI * (0.2 + seed * 0.8);
    const dist = phase * 80;
    const fx = antX + Math.cos(angle) * dist;
    const fy = (antY - 28) + Math.sin(angle) * dist;
    const alpha = Math.max(0, 1 - phase * 1.3);
    ctx.globalAlpha = alpha;
    ctx.font = "6px monospace";
    ctx.fillStyle = "#fff176";
    ctx.fillText(FAKE_SSIDS_DISPLAY[i % FAKE_SSIDS_DISPLAY.length].slice(0, 10), fx - 18, fy);
    ctx.globalAlpha = 1;
  }
  // Confusion log (recent beacon spam events)
  const beaconLog = state.auditLog.filter(e => e.type === "BCSPAM").slice(0, 3);
  if (beaconLog.length > 0) {
    let ty = 28;
    beaconLog.forEach((e, i) => {
      ctx.font = "6px monospace";
      ctx.fillStyle = `rgba(255,241,118,${1 - i * 0.25})`;
      ctx.fillText(`↯ ${e.ssid.slice(0,18)}`, x0 + 4, ty);
      ty += 10;
    });
  } else {
    ctx.font = "7px monospace";
    const blink = Math.floor(nowMs / 400) % 2;
    ctx.fillStyle = blink ? "#5a5a00" : "#3a3a00";
    ctx.fillText("broadcasting...", x0 + 4, 40);
  }
  // Confused counter big
  if ((a.confused || 0) > 0) {
    ctx.font = "bold 10px monospace";
    ctx.fillStyle = "#ffee58";
    ctx.fillText(`${a.confused} confused!`, x0 + 4, 120);
  }
}

// ── Main dispatcher ──────────────────────────────────────────────────────────
function drawWifiFoodScreen(nowMs) {
  switch (state.wifiFood.mode) {
    case "SCAN":      _wfDrawScan(nowMs);      break;
    case "PROBES":    _wfDrawProbes(nowMs);    break;
    case "DEAUTH":    _wfDrawDeauth(nowMs);    break;
    case "HANDSHAKE": _wfDrawHandshake(nowMs); break;
    case "PMKID":     _wfDrawPmkid(nowMs);     break;
    case "BEACON":    _wfDrawBeacon(nowMs);    break;
    default:          _wfDrawIdle(nowMs);      break;
  }
  _wfDrawBottomMenu(nowMs);
}

function drawOverlayScreen() {
  if (state.screen === screens.HOME) return;
  if (state.scene === "sleep" || state.scene === "rest") return; // Sleep scene takes priority
  if (state.screen === screens.WIFI_FOOD) return; // Food screen draws itself fully

  if (isShowcaseScreen()) {
    const x = LEFT_X + 12;
    const y = SCREEN.h - 36;
    const w = LEFT_W - 24;
    const h = 26;
    ctx.fillStyle = "rgba(10, 12, 16, 0.72)";
    ctx.fillRect(x, y, w, h);
    ctx.strokeStyle = "#d0d0d0";
    ctx.strokeRect(x, y, w, h);
    ctx.fillStyle = "#ffdc8a";
    ctx.font = "8px monospace";
    const lbl = state.activity === "HUNT"
      ? `Scanning ${state.wifiStats.netCount} networks...`
      : state.activity === "DISCOVER"
      ? "New signal found!"
      : "Exploring the wild";
    ctx.fillText(lbl, x + 6, y + 10);
    ctx.fillStyle = "#b0c8ff";
    const wv = state.wifiStats;
    ctx.fillText(`Net:${wv.netCount} Str:${wv.strongCount} RSSI:${wv.avgRSSI}`, x + 6, y + 22);
    return;
  }

  const x = LEFT_X + 8;
  const y = 20;
  const w = LEFT_W - 16;
  const h = SCREEN.h - 28;

  // Use forest2 as animated background for text screens
  const f2Frames = Array.isArray(SPRITES.forest2?.frames) ? SPRITES.forest2.frames.length : 1;
  const f2Idx = Math.floor(performance.now() / 500) % Math.max(1, f2Frames);
  const f2Canvas = getFrameCanvas("forest2", f2Idx);
  if (f2Canvas) {
    ctx.drawImage(f2Canvas, x, y, w, h);
  }

  // Semi-transparent overlay for readability
  ctx.fillStyle = "rgba(6, 8, 14, 0.65)";
  ctx.fillRect(x, y, w, h);
  ctx.strokeStyle = "#5a6a9a";
  ctx.strokeRect(x, y, w, h);

  ctx.fillStyle = "#ffdc8a";
  ctx.font = "9px monospace";
  ctx.fillText(panelHeader(), x + 5, y + 10);

  ctx.fillStyle = "#f1f1f1";
  ctx.font = "8px monospace";

  if (state.screen === screens.PET_STATUS) {
    // ── SABLINA SOUL CARD ──────────────────────────────────────────
    // Archetype derived from traits
    const c = state.traits.curiosity, ac = state.traits.activity, st = state.traits.stress;
    let archetype, archCol;
    if (c > 70 && ac > 70 && st < 40)       { archetype = "GHOST HUNTER";     archCol = "#00e5ff"; }
    else if (st > 70 && ac > 70)            { archetype = "CHAOS AGENT";      archCol = "#ff5555"; }
    else if (c > 70 && ac < 30)             { archetype = "SILENT OBSERVER";  archCol = "#c97aff"; }
    else if (ac > 70 && st < 30)            { archetype = "PACKET HUNTER";    archCol = "#66e08b"; }
    else if (c < 30 && ac < 30)             { archetype = "SLEEPY LURKER";    archCol = "#607080"; }
    else if (c > 60 && ac > 60 && st > 60)  { archetype = "APEX PREDATOR";    archCol = "#ff9a57"; }
    else                                    { archetype = "NET WANDERER";     archCol = "#ffe06a"; }

    const moodFlavors = {
      HAPPY:   "Packets taste sweeter today \u2713",
      EXCITED: "WiFi smells like adventure!",
      HUNGRY:  "Need more handshakes...",
      SICK:    "Corrupted packets incoming",
      BORED:   "Nothing interesting on-air",
      CURIOUS: "Lurking in the spectrum...",
      CALM:    "Signal steady. All is well.",
    };
    const flavor = moodFlavors[state.mood] || "Monitoring the airwaves...";

    const lt = state.lifetime;
    const hackPts = lt.handshakes * 5 + lt.pmkidCaptures * 3 + lt.deauths + lt.wifiScans * 2 + lt.networksFound;
    const rankLabel = hackPts < 10 ? "NOOB" : hackPts < 50 ? "WARDRIVER" : hackPts < 200 ? "PKTSNIPER" : hackPts < 1000 ? "WIFI NINJA" : "ELITE HACKER";
    const rankCol   = hackPts < 10 ? "#888"  : hackPts < 50 ? "#f2b93b"   : hackPts < 200 ? "#57d2ff"   : hackPts < 1000 ? "#c97aff"    : "#ff5555";

    // ── Name / stage / rank row
    ctx.font = "bold 8px monospace";
    ctx.fillStyle = "#ffe06a";
    ctx.fillText(state.name.slice(0, 10), x + 5, y + 20);
    ctx.fillStyle = "#aaa";
    ctx.fillText(state.stage, x + 5, y + 30);
    ctx.fillStyle = "#666";
    ctx.fillText(`${state.ageDays}d ${state.ageHours}h`, x + 5, y + 40);
    ctx.fillStyle = rankCol;
    ctx.font = "bold 7px monospace";
    ctx.textAlign = "right";
    ctx.fillText(rankLabel, x + w - 5, y + 20);
    ctx.fillStyle = "#555";
    ctx.font = "7px monospace";
    ctx.fillText(`${hackPts}pts`, x + w - 5, y + 30);
    ctx.textAlign = "left";

    // ── Archetype
    ctx.fillStyle = archCol;
    ctx.font = "bold 8px monospace";
    ctx.fillText(`\u25B6 ${archetype}`, x + 5, y + 52);

    // ── Mood + flavor
    const moodColors = { HAPPY:"#66e08b", EXCITED:"#ffe06a", HUNGRY:"#f2b93b", SICK:"#ff5555", BORED:"#607080", CURIOUS:"#c97aff", CALM:"#57d2ff" };
    ctx.fillStyle = moodColors[state.mood] || "#aaa";
    ctx.font = "8px monospace";
    ctx.fillText(`\u25C6 ${state.mood}`, x + 5, y + 64);
    ctx.fillStyle = "#555";
    ctx.font = "7px monospace";
    ctx.fillText(`"${flavor}"`, x + 5, y + 74);

    // ── Trait bars (compact)
    function soulBar(label, val, yy, col) {
      const bw = w - 50;
      ctx.font = "7px monospace";
      ctx.fillStyle = "#444";
      ctx.fillText(label, x + 5, yy);
      ctx.fillStyle = "#111";
      ctx.fillRect(x + 30, yy - 6, bw, 6);
      ctx.fillStyle = col;
      ctx.fillRect(x + 30, yy - 6, Math.round(bw * Math.max(0, Math.min(100, val)) / 100), 6);
      ctx.fillStyle = "#555";
      ctx.fillText(Math.round(val), x + 30 + bw + 3, yy);
    }
    ctx.fillStyle = "#333";
    ctx.font = "6px monospace";
    ctx.fillText("\u2500\u2500 TRAITS", x + 5, y + 86);
    soulBar("CUR", state.traits.curiosity, y + 96,  "#c97aff");
    soulBar("ACT", state.traits.activity,  y + 105, "#ff9a57");
    soulBar("STR", state.traits.stress,    y + 114, "#ff5555");

    // ── Today's catches
    ctx.fillStyle = "#333";
    ctx.font = "6px monospace";
    ctx.fillText("\u2500\u2500 SESSION", x + 5, y + 126);
    ctx.font = "7px monospace";
    ctx.fillStyle = "#66e08b";
    ctx.fillText(`HS:${lt.handshakes}`, x + 5, y + 136);
    ctx.fillStyle = "#c97aff";
    ctx.fillText(`PK:${lt.pmkidCaptures}`, x + 42, y + 136);
    ctx.fillStyle = "#57d2ff";
    ctx.fillText(`NETS:${lt.networksFound}`, x + 78, y + 136);
    ctx.fillStyle = "#ffe06a";
    ctx.fillText(`\u25C8${state.exp} coins`, x + 130, y + 136);

    // ── Footer battery
    ctx.font = "7px monospace";
    const batCol = state.battery > 50 ? "#66e08b" : state.battery > 20 ? "#f2b93b" : "#ff5555";
    ctx.fillStyle = batCol;
    ctx.fillText(`Bat:${Math.round(state.battery)}%${state.charging ? " CHG" : ""}  WiFi:${state.wifi ? "ON" : "OFF"}  BLE:${state.ble ? "ON" : "OFF"}`, x + 5, y + h - 4);
  } else if (state.screen === screens.ENVIRONMENT) {
    const wv = state.wifiStats;
    ctx.fillText(`Net: ${wv.netCount}`, x + 5, y + 24);
    ctx.fillText(`Strong: ${wv.strongCount}`, x + 5, y + 34);
    ctx.fillText(`Hidden: ${wv.hiddenCount}`, x + 5, y + 44);
    ctx.fillText(`Open: ${wv.openCount}`, x + 5, y + 54);
    ctx.fillText(`WPA: ${wv.wpaCount}`, x + 5, y + 64);
    ctx.fillText(`RSSI: ${wv.avgRSSI}`, x + 5, y + 74);
    ctx.fillText(`Scan: ${wv.scanRunning ? "RUN" : "IDLE"}`, x + 5, y + 84);
    ctx.fillText(`Source: ${state.realWifi ? "REAL" : "SIM"}`, x + 5, y + 94);
    ctx.fillStyle = "#b0c8ff";
    ctx.fillText(`BLE Devices: ${state.bleStats.deviceCount}`, x + 5, y + 104);
    if (state.realWifi && state.realWifiNetworks.length > 0) {
      ctx.fillStyle = "#ffdc8a";
      ctx.fillText("Live Networks:", x + 5, y + 118);
      ctx.fillStyle = "#f1f1f1";
      const maxShow = Math.min(state.realWifiNetworks.length, 5);
      for (let ni = 0; ni < maxShow; ni++) {
        const net = state.realWifiNetworks[ni];
        const ssid = (net.ssid || "(hidden)").slice(0, 14);
        const sig = net.signal || 0;
        ctx.fillText(`${ssid} ${sig}%`, x + 5, y + 128 + ni * 10);
      }
    }
  } else if (state.screen === screens.SYSTEM) {
    const page = state.systemPage % 4;
    if (page === 0) {
      const upSec = Math.floor((performance.now() - state.engine.bootMs) / 1000);
      const hh = String(Math.floor(upSec / 3600)).padStart(2, "0");
      const mm = String(Math.floor((upSec % 3600) / 60)).padStart(2, "0");
      const ss = String(upSec % 60).padStart(2, "0");
      ctx.fillText("Firmware: 2.0", x + 5, y + 24);
      ctx.fillText("MCU: ESP32-S3", x + 5, y + 34);
      ctx.fillText(`Uptime: ${hh}:${mm}:${ss}`, x + 5, y + 44);
      ctx.fillText(`Battery: ${Math.round(state.battery)}%${state.charging ? " CHG" : ""}`, x + 5, y + 54);
      ctx.fillText(`WiFi: ${state.wifi ? "ON" : "OFF"}  BLE: ${state.ble ? "ON" : "OFF"}`, x + 5, y + 64);
      ctx.fillText(`BLE Dev: ${state.bleStats.deviceCount}`, x + 5, y + 74);
      ctx.fillText(`Peer: ${isPeerAlive() ? blePeer.name : "---"}`, x + 5, y + 84);
      ctx.fillText(`LLM: ${state.llm ? "ON" : "OFF"}`, x + 5, y + 94);
      ctx.fillText(`Alive: ${state.alive ? "YES" : "NO"}`, x + 5, y + 104);
    } else if (page === 1) {
      const bri = ["LOW", "MID", "HIGH"];
      ctx.fillText(`TFT: ${bri[state.controls.tftBrightnessIndex]}`, x + 5, y + 24);
      ctx.fillText(`LED: ${bri[state.controls.ledBrightnessIndex]}`, x + 5, y + 34);
      ctx.fillText(`Sound: ${state.controls.soundEnabled ? "ON" : "OFF"}`, x + 5, y + 44);
      ctx.fillText(`NeoPix: ${state.controls.neoPixelsEnabled ? "ON" : "OFF"}`, x + 5, y + 54);
      ctx.fillText(`Dark: ${state.darkMode ? "ON" : "OFF"}`, x + 5, y + 64);
    } else if (page === 2) {
      ctx.fillText(`Auto Sleep: ${state.controls.autoSleep ? "ON" : "OFF"}`, x + 5, y + 24);
      ctx.fillText(`Auto Save: ${state.controls.autoSaveMs / 1000}s`, x + 5, y + 34);
      ctx.fillText("Reset Pet: cmd/reset", x + 5, y + 44);
      ctx.fillText("Reset All: cmd/fullreset", x + 5, y + 54);
    } else {
      ctx.fillText(`Activity: ${state.activity}`, x + 5, y + 24);
      ctx.fillText(`Mood: ${state.mood}`, x + 5, y + 34);
      ctx.fillText(`Scene: ${state.scene}`, x + 5, y + 44);
      ctx.fillText(`WiFi Scan: ${state.wifiStats.scanRunning ? "RUN" : "IDLE"}`, x + 5, y + 54);
      ctx.fillText(`Hun/Fat/Cle`, x + 5, y + 64);
      ctx.fillText(`${state.hun}/${state.fat}/${state.cle}`, x + 5, y + 74);
    }
    ctx.fillStyle = "#aaa";
    ctx.fillText("BTN B: next page", x + 5, y + h - 10);
  } else if (state.screen === screens.SHOP) {
    ctx.fillText(`Coins: ${state.exp}`, x + 5, y + 24);
    const cols = 4;
    const rows = 3;
    const perPage = cols * rows;
    const page = Math.floor(state.shopCursor / perPage);
    const startIdx = page * perPage;
    const cellW = Math.floor((w - 10) / cols);
    const cellH = 30;
    for (let i = 0; i < perPage && startIdx + i < SHOP_FOODS.length; i++) {
      const fi = startIdx + i;
      const col = i % cols;
      const row = Math.floor(i / cols);
      const ix = x + 5 + col * cellW;
      const iy = y + 30 + row * cellH;
      const food = SHOP_FOODS[fi];
      // Highlight medicine item in red border if sick
      if (food.isMedicine && isSick()) {
        ctx.strokeStyle = "#ff5555";
        ctx.lineWidth = 2;
        ctx.strokeRect(ix, iy, cellW - 2, cellH - 2);
        ctx.lineWidth = 1;
      } else if (fi === state.shopCursor) {
        ctx.strokeStyle = "#ffe06a";
        ctx.lineWidth = 2;
        ctx.strokeRect(ix, iy, cellW - 2, cellH - 2);
        ctx.lineWidth = 1;
      }
      const sc = getSpriteCanvas(food.sprite);
      if (sc) ctx.drawImage(sc, ix + 2, iy + 1, cellH - 4, cellH - 4);
    }
    const sel = SHOP_FOODS[state.shopCursor];
    if (sel) {
      ctx.fillStyle = sel.isMedicine ? "#ff9944" : "#ffe06a";
      ctx.font = "8px monospace";
      ctx.fillText(`${sel.name}${sel.isMedicine && isSick() ? " [NEEDED!]" : ""}`, x + 5, y + h - 22);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(sel.isMedicine ? `Cost:${sel.cost} Cures sickness` : `Cost:${sel.cost} +${sel.hunger}hun`, x + 5, y + h - 12);
    }
  } else if (state.screen === screens.ACHIEVEMENTS) {
    const lt = state.lifetime;
    const badges = [
      { name: "First Bite",    desc: "Feed 1 time",         val: lt.foodEaten,     goal: 1 },
      { name: "Gourmet",       desc: "Feed 50 times",       val: lt.foodEaten,     goal: 50 },
      { name: "Feast Master",  desc: "Feed 200 times",      val: lt.foodEaten,     goal: 200 },
      { name: "Script Kiddie", desc: "Capture 1 handshake", val: lt.handshakes,    goal: 1 },
      { name: "Packet Hunter", desc: "Capture 25 HS",       val: lt.handshakes,    goal: 25 },
      { name: "PMKID Rookie",  desc: "Grab 1 PMKID",       val: lt.pmkidCaptures, goal: 1 },
      { name: "PMKID Lord",    desc: "Grab 50 PMKIDs",      val: lt.pmkidCaptures, goal: 50 },
      { name: "Deauth Storm",  desc: "Send 10 deauths",     val: lt.deauths,       goal: 10 },
      { name: "Net Scout",     desc: "Scan 20 times",       val: lt.wifiScans,     goal: 20 },
      { name: "Shopaholic",    desc: "Spend 100 coins",     val: lt.coinsSpent,    goal: 100 },
      { name: "Rich Pet",      desc: "Earn 500 coins",      val: lt.coinsEarned,   goal: 500 },
      { name: "Survivor",      desc: "Alive 7 days",        val: state.ageDays,    goal: 7 },
      { name: "Veteran",       desc: "Alive 30 days",       val: state.ageDays,    goal: 30 },
      { name: "Clean Freak",   desc: "Clean 30 times",      val: lt.cleans,        goal: 30 },
      { name: "Gamer",         desc: "Play 20 games",       val: lt.gamesPlayed,   goal: 20 },
      { name: "Champion",      desc: "Win 50 games",        val: lt.gamesWon,      goal: 50 },
      { name: "Net Whale",     desc: "Find 50+ networks",   val: lt.maxNetworks,   goal: 50 },
      { name: "Cuddly",        desc: "Pet 25 times",        val: lt.pets,          goal: 25 },
    ];
    const pg = state.achievePage % 4;
    const unlocked = badges.filter(b => b.val >= b.goal).length;
    if (pg < 3) {
      const perPage = 6;
      const start = pg * perPage;
      ctx.fillStyle = "#ffe06a";
      ctx.fillText(`${unlocked}/${badges.length} unlocked`, x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      for (let i = 0; i < perPage && start + i < badges.length; i++) {
        const b = badges[start + i];
        const done = b.val >= b.goal;
        const ly = y + 38 + i * 18;
        ctx.fillStyle = done ? "#66e08b" : "#555";
        ctx.fillText(done ? "\u2605" : "\u2606", x + 5, ly);
        ctx.fillStyle = done ? "#ffe06a" : "#888";
        ctx.fillText(b.name, x + 16, ly);
        ctx.fillStyle = done ? "#aaddaa" : "#666";
        ctx.fillText(done ? "DONE" : `${b.val}/${b.goal}`, x + 5, ly + 9);
        ctx.fillStyle = "#777";
        ctx.fillText(b.desc, x + 50, ly + 9);
      }
    } else {
      // pg 3, Hacker Rank card
      const lt = state.lifetime;
      const pts = lt.handshakes * 5 + lt.pmkidCaptures * 3 + lt.deauths + lt.wifiScans * 2 + lt.networksFound;
      const rank = pts < 10 ? "SCRIPT KIDDIE" : pts < 50 ? "WARDRIVER" : pts < 200 ? "PKTSNIPER" : pts < 1000 ? "WIFI NINJA" : "ELITE HACKER";
      const rankCol = pts < 10 ? "#888" : pts < 50 ? "#f2b93b" : pts < 200 ? "#57d2ff" : pts < 1000 ? "#c97aff" : "#ff5555";
      ctx.fillStyle = rankCol;
      ctx.font = "bold 8px monospace";
      ctx.fillText(`\u2620 ${rank}`, x + 5, y + 30);
      ctx.font = "8px monospace";
      ctx.fillStyle = "#ffe06a";
      ctx.fillText(`Hack Points: ${pts}`, x + 5, y + 46);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`Handshakes:  ${lt.handshakes}`, x + 5, y + 60);
      ctx.fillText(`PMKIDs:      ${lt.pmkidCaptures}`, x + 5, y + 72);
      ctx.fillText(`Deauths:     ${lt.deauths}`, x + 5, y + 84);
      ctx.fillText(`WiFi Scans:  ${lt.wifiScans}`, x + 5, y + 96);
      ctx.fillText(`Nets Found:  ${lt.networksFound}`, x + 5, y + 108);
      ctx.fillText(`Max Nets:    ${lt.maxNetworks}`, x + 5, y + 120);
      ctx.fillStyle = "#aaa";
      ctx.fillText(`Badges: ${unlocked}/${badges.length}`, x + 5, y + 134);
    }
    ctx.fillStyle = "#aaa";
    ctx.fillText(`Page ${pg + 1}/4  BTN B: next`, x + 5, y + h - 10);
  } else if (state.screen === screens.TOOLS) {
    const pg = state.toolsPage % 5;
    if (pg === 0) {
      // WiFi Audit
      const wf = state.wifiFood;
      const prey = wf.aps[wf.selectedAP];
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- WiFi Audit --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`Mode: ${wf.mode}`, x + 5, y + 38);
      ctx.fillText(`CH:${wf.channel}  PKT:${wf.totalPackets}  MGMT:${wf.mgmtPackets}`, x + 5, y + 48);
      ctx.fillStyle = "#ff9a57";
      ctx.fillText(`Prey: ${prey ? (prey.ssid || "(hidden)") : "none"}`, x + 5, y + 60);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`  CH:${prey ? prey.channel : "-"}  ENC:${prey ? prey.encryption : "-"}`, x + 5, y + 70);
      ctx.fillStyle = "#66e08b";
      ctx.fillText(`Handshakes: ${state.lifetime.handshakes}`, x + 5, y + 84);
      ctx.fillStyle = "#c97aff";
      ctx.fillText(`PMKIDs:     ${state.lifetime.pmkidCaptures}`, x + 5, y + 96);
      ctx.fillStyle = "#ff5555";
      ctx.fillText(`Deauths:    ${state.lifetime.deauths}`, x + 5, y + 108);
      ctx.fillStyle = "#57d2ff";
      const wv = state.wifiStats;
      ctx.fillText(`Nets:${wv.netCount} Str:${wv.strongCount} RSSI:${wv.avgRSSI}`, x + 5, y + 122);
    } else if (pg === 1) {
      // BLE Scan
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- BLE Scan --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`BLE: ${state.ble ? "ON" : "OFF"}  Devices: ${state.bleStats.deviceCount}`, x + 5, y + 38);
      ctx.fillText(`Peer: ${isPeerAlive() ? blePeer.name : "---"}`, x + 5, y + 50);
      ctx.fillText(`WiFi: ${state.wifi ? "ON" : "OFF"}  Source: ${state.realWifi ? "REAL" : "SIM"}`, x + 5, y + 62);
      ctx.fillText(`Scan: ${state.wifiStats.scanRunning ? "ACTIVE" : "IDLE"}`, x + 5, y + 74);
      // Animated BLE pulse dot
      const pulse = Math.sin(performance.now() / 400) > 0;
      ctx.fillStyle = state.ble ? (pulse ? "#57d2ff" : "#1a4a6a") : "#333";
      ctx.beginPath();
      ctx.arc(x + w - 15, y + 38, 6, 0, Math.PI * 2);
      ctx.fill();
      if (state.realWifi && state.realWifiNetworks.length > 0) {
        ctx.fillStyle = "#ffe06a";
        ctx.fillText("Live Networks:", x + 5, y + 90);
        ctx.fillStyle = "#f1f1f1";
        const maxShow = Math.min(state.realWifiNetworks.length, 5);
        for (let ni = 0; ni < maxShow; ni++) {
          const net = state.realWifiNetworks[ni];
          ctx.fillText(`${(net.ssid || "(hidden)").slice(0, 14)} ${net.signal || 0}%`, x + 5, y + 100 + ni * 10);
        }
      } else {
        ctx.fillStyle = "#555";
        ctx.fillText("Enable Real WiFi for live scan", x + 5, y + 90);
      }
    } else if (pg === 2) {
      // Signal Meter (visual animated)
      const rssi = state.wifiStats.avgRSSI;
      const strength = Math.max(0, Math.min(100, rssi + 100));
      const barW = Math.round((w - 20) * strength / 100);
      const barCol = strength > 60 ? "#66e08b" : strength > 30 ? "#f2b93b" : "#ff5555";
      ctx.fillStyle = "#222";
      ctx.fillRect(x + 5, y + 30, w - 20, 14);
      ctx.fillStyle = barCol;
      ctx.fillRect(x + 5, y + 30, barW, 14);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`RSSI: ${rssi} dBm  Strength: ${strength}%`, x + 5, y + 54);
      ctx.fillText(`Nets:${state.wifiStats.netCount}  Str:${state.wifiStats.strongCount}  Hid:${state.wifiStats.hiddenCount}`, x + 5, y + 66);
      ctx.fillText(`Open:${state.wifiStats.openCount}  WPA:${state.wifiStats.wpaCount}`, x + 5, y + 78);
      // Animated rising bars
      const t = performance.now();
      const numBars = 10;
      for (let bi = 0; bi < numBars; bi++) {
        const bx = x + 5 + bi * (Math.floor((w - 20) / numBars));
        const wave = Math.abs(Math.sin((t / 600) + bi * 0.7));
        const maxBH = 30;
        const bh = Math.round(wave * maxBH * (0.4 + (strength / 100) * 0.6)) + 2;
        const by = y + h - 26 - bh;
        const active = bi < Math.ceil(strength / (100 / numBars));
        ctx.fillStyle = active ? barCol : "#2a2a2a";
        ctx.fillRect(bx, by, Math.floor((w - 20) / numBars) - 2, bh);
      }
    } else if (pg === 3) {
      // Network List
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- AP List --", x + 5, y + 24);
      const aps = state.wifiFood.aps;
      if (aps.length === 0) {
        ctx.fillStyle = "#888";
        ctx.fillText("No APs found.", x + 5, y + 44);
        ctx.fillText("WiFi Hunt > Sniff to scan", x + 5, y + 56);
      } else {
        const maxShow = Math.min(aps.length, 7);
        for (let i = 0; i < maxShow; i++) {
          const ap = aps[i];
          const isSel = i === state.wifiFood.selectedAP;
          const sig = Math.max(0, Math.min(100, (ap.rssi || -80) + 100));
          const bars = Math.ceil(sig / 25);
          const barStr = "\u2593".repeat(bars) + "\u2591".repeat(4 - bars);
          const ly = y + 38 + i * 16;
          ctx.fillStyle = isSel ? "#ffe06a" : ap.handshake ? "#66e08b" : ap.pmkid ? "#c97aff" : "#f1f1f1";
          ctx.fillText(`${isSel ? ">" : " "}${(ap.ssid || "hidden").slice(0, 11).padEnd(11)} ${barStr}`, x + 4, ly);
          ctx.fillStyle = "#555";
          ctx.fillText(`CH:${ap.channel} ${ap.encryption}${ap.handshake ? " HS" : ""}${ap.pmkid ? " PK" : ""}`, x + 14, ly + 8);
        }
      }
      ctx.fillStyle = "#555";
      ctx.fillText(`${state.wifiFood.aps.length} AP(s) found`, x + 5, y + h - 22);
    } else {
      // Audit Log
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- Audit Log --", x + 5, y + 24);
      const log = state.auditLog;
      if (log.length === 0) {
        ctx.fillStyle = "#888";
        ctx.fillText("No events yet.", x + 5, y + 44);
        ctx.fillText("Start WiFi Hunt to capture.", x + 5, y + 56);
      } else {
        const typeColors = { HS: "#66e08b", PMKID: "#c97aff", DAUTH: "#ff5555", SCAN: "#57d2ff" };
        const maxShow = Math.min(log.length, 9);
        for (let i = 0; i < maxShow; i++) {
          const e = log[i];
          const ly = y + 36 + i * 13;
          ctx.fillStyle = typeColors[e.type] || "#f1f1f1";
          ctx.fillText(`[${e.ts}]`, x + 4, ly);
          ctx.fillStyle = typeColors[e.type] || "#f1f1f1";
          ctx.fillText(e.type, x + 54, ly);
          ctx.fillStyle = "#ccc";
          ctx.fillText(e.ssid.slice(0, 12), x + 82, ly);
          if (e.detail) {
            ctx.fillStyle = "#666";
            ctx.fillText(e.detail, x + 10, ly + 8);
          }
        }
      }
      ctx.fillStyle = "#555";
      ctx.fillText(`${log.length} total events`, x + 5, y + h - 22);
    }
    ctx.fillStyle = "#aaa";
    ctx.fillText(`Page ${pg + 1}/5  BTN B: next`, x + 5, y + h - 10);
  } else if (state.screen === screens.STATS) {
    const pg = state.statsPage % 5;
    if (pg === 0) {
      // Pet Vitals, visual bars (mirror PET_STATUS but in stats context)
      ctx.fillStyle = "#ffe06a";
      ctx.fillText(`${state.name.slice(0,10)}  ${state.stage}  ${state.mood}`, x + 5, y + 24);
      ctx.fillStyle = "#aaa";
      ctx.fillText(`Age: ${state.ageDays}d ${state.ageHours}h  Coins: ${state.exp}`, x + 5, y + 34);
      function statsBar(label, val, yy, col) {
        const bw = w - 52;
        ctx.font = "8px monospace";
        ctx.fillStyle = "#888";
        ctx.fillText(label, x + 5, yy);
        ctx.fillStyle = "#1a1a2e";
        ctx.fillRect(x + 34, yy - 7, bw, 8);
        ctx.fillStyle = col;
        ctx.fillRect(x + 34, yy - 7, Math.round(bw * Math.max(0, Math.min(100, val)) / 100), 8);
        ctx.fillStyle = "#f1f1f1";
        ctx.fillText(String(Math.round(val)), x + 34 + bw + 3, yy);
      }
      statsBar("HUN", state.hun, y + 50, state.hun < 30 ? "#ff5555" : "#f2b93b");
      statsBar("HAP", state.fat, y + 62, state.fat < 30 ? "#ff5555" : "#57d2ff");
      statsBar("HP ", state.cle, y + 74, state.cle < 30 ? "#ff5555" : "#66e08b");
      statsBar("CUR", state.traits.curiosity, y + 90, "#c97aff");
      statsBar("ACT", state.traits.activity,  y + 102, "#ff9a57");
      statsBar("STR", state.traits.stress,    y + 114, "#ff5555");
      ctx.fillStyle = "#aaa";
      ctx.fillText(`WiFi:${state.wifi?"ON":"OFF"}  BLE:${state.ble?"ON":"OFF"}  LLM:${state.llm?"ON":"OFF"}`, x + 5, y + 130);
    } else if (pg === 1) {
      // Time & Age
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- Time & Age --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      const now = new Date();
      const hh = String(now.getHours()).padStart(2, "0");
      const mm = String(now.getMinutes()).padStart(2, "0");
      ctx.fillText(`Clock:  ${hh}:${mm}`, x + 5, y + 38);
      ctx.fillText(`Age: ${state.ageDays}d ${state.ageHours}h ${state.ageMinutes}m`, x + 5, y + 48);
      ctx.fillStyle = "#8899bb";
      ctx.fillText(isNightTime() ? "Night time" : "Day time", x + 5, y + 60);
      const batCol = state.battery > 50 ? "#66e08b" : state.battery > 20 ? "#f2b93b" : "#ff5555";
      ctx.fillStyle = batCol;
      ctx.fillText(`Battery: ${Math.round(state.battery)}%${state.charging ? " CHG" : ""}`, x + 5, y + 74);
      ctx.fillStyle = "#f1f1f1";
      const upSec = Math.floor((performance.now() - state.engine.bootMs) / 1000);
      const uh = String(Math.floor(upSec / 3600)).padStart(2, "0");
      const um = String(Math.floor((upSec % 3600) / 60)).padStart(2, "0");
      const us = String(upSec % 60).padStart(2, "0");
      ctx.fillText(`Uptime: ${uh}:${um}:${us}`, x + 5, y + 88);
    } else if (pg === 2) {
      // Environment
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- Environment --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      const wv = state.wifiStats;
      ctx.fillText(`Networks: ${wv.netCount}`, x + 5, y + 38);
      ctx.fillText(`Strong:   ${wv.strongCount}`, x + 5, y + 48);
      ctx.fillText(`Hidden:   ${wv.hiddenCount}`, x + 5, y + 58);
      ctx.fillText(`Open:     ${wv.openCount}`, x + 5, y + 68);
      ctx.fillText(`WPA:      ${wv.wpaCount}`, x + 5, y + 78);
      ctx.fillText(`Avg RSSI: ${wv.avgRSSI}`, x + 5, y + 88);
      ctx.fillStyle = "#b0c8ff";
      ctx.fillText(`BLE Dev:  ${state.bleStats.deviceCount}`, x + 5, y + 102);
      ctx.fillText(`Source:   ${state.realWifi ? "REAL" : "SIM"}`, x + 5, y + 112);
    } else if (pg === 3) {
      // System
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- System --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText("Firmware: 2.0  MCU: ESP32-S3", x + 5, y + 38);
      const upSec2 = Math.floor((performance.now() - state.engine.bootMs) / 1000);
      const uh2 = String(Math.floor(upSec2 / 3600)).padStart(2, "0");
      const um2 = String(Math.floor((upSec2 % 3600) / 60)).padStart(2, "0");
      const us2 = String(upSec2 % 60).padStart(2, "0");
      ctx.fillText(`Uptime: ${uh2}:${um2}:${us2}`, x + 5, y + 48);
      const batCol2 = state.battery > 50 ? "#66e08b" : state.battery > 20 ? "#f2b93b" : "#ff5555";
      ctx.fillStyle = batCol2;
      ctx.fillText(`Battery: ${Math.round(state.battery)}%${state.charging ? " CHG" : ""}`, x + 5, y + 58);
      ctx.fillStyle = "#f1f1f1";
      ctx.fillText(`WiFi: ${state.wifi ? "ON" : "OFF"}  BLE: ${state.ble ? "ON" : "OFF"}  LLM: ${state.llm ? "ON" : "OFF"}`, x + 5, y + 68);
      ctx.fillText(`Alive: ${state.alive ? "YES" : "NO"}  Stage: ${state.stage}`, x + 5, y + 78);
      const bri = ["LOW", "MID", "HIGH"];
      ctx.fillText(`TFT: ${bri[state.controls.tftBrightnessIndex]}  LED: ${bri[state.controls.ledBrightnessIndex]}`, x + 5, y + 92);
      ctx.fillText(`Sound: ${state.controls.soundEnabled ? "ON" : "OFF"}  NeoPix: ${state.controls.neoPixelsEnabled ? "ON" : "OFF"}`, x + 5, y + 102);
      ctx.fillText(`Dark: ${state.darkMode ? "ON" : "OFF"}  AutoSave: ${state.controls.autoSaveMs / 1000}s`, x + 5, y + 112);
    } else {
      // pg 4, Lifetime
      ctx.fillStyle = "#ffe06a";
      ctx.fillText("-- Lifetime --", x + 5, y + 24);
      ctx.fillStyle = "#f1f1f1";
      const lt = state.lifetime;
      ctx.fillText(`Food Eaten:  ${lt.foodEaten}`, x + 5, y + 38);
      ctx.fillText(`Handshakes:  ${lt.handshakes}`, x + 5, y + 48);
      ctx.fillText(`PMKIDs:      ${lt.pmkidCaptures}`, x + 5, y + 58);
      ctx.fillText(`Deauths:     ${lt.deauths}`, x + 5, y + 68);
      ctx.fillText(`WiFi Scans:  ${lt.wifiScans}`, x + 5, y + 78);
      ctx.fillText(`Max Nets:    ${lt.maxNetworks}`, x + 5, y + 88);
      ctx.fillStyle = "#ffe06a";
      ctx.fillText(`Coins Earned: ${lt.coinsEarned}`, x + 5, y + 102);
      ctx.fillText(`Coins Spent:  ${lt.coinsSpent}`, x + 5, y + 112);
      ctx.fillStyle = "#b0c8ff";
      ctx.fillText(`Games: ${lt.gamesPlayed}  Won: ${lt.gamesWon}`, x + 5, y + 126);
      ctx.fillText(`Cleans: ${lt.cleans}  Sleeps: ${lt.sleeps}  Pets: ${lt.pets}`, x + 5, y + 138);
    }
    ctx.fillStyle = "#aaa";
    ctx.fillText(`Page ${pg + 1}/5  BTN B: next`, x + 5, y + h - 10);
  } else if (state.screen === screens.WIFI_FOOD) {
    // WiFi Food screen is drawn separately by drawWifiFoodScreen
  }
}

function drawTopBar() {
  ctx.fillStyle = "rgba(0, 0, 0, 0.85)";
  ctx.fillRect(0, 0, LEFT_W, 14);
  ctx.strokeStyle = "#3b4b7a";
  ctx.beginPath();
  ctx.moveTo(0, 14);
  ctx.lineTo(LEFT_W, 14);
  ctx.stroke();
  ctx.fillStyle = "#f2f2f2";
  ctx.font = "10px monospace";
  ctx.fillText(panelHeader(), 6, 10);
}

function drawSelectionIndicator() {
  if (!iconsVisible()) return;  // hide selector when icons are auto-hidden
  const items = twoButtonMenuItems();
  const selected = items[state.twoBtn.cursor];
  if (!selected || state.screen !== screens.HOME || typeof selected.gx !== "number") return;

  const sz = selected.gs || 32;
  const x = gameToScreenX(selected.gx);
  const y = gameToScreenY(selected.gy);
  const padX = Math.max(1, Math.round((2 * GAME_W) / GAME.w));
  const padY = Math.max(1, Math.round((2 * GAME_H) / GAME.h));
  ctx.strokeStyle = "#ffe06a";
  ctx.lineWidth = 2;
  ctx.strokeRect(x - padX, y - padY, gameToScreenW(sz) + padX * 2, gameToScreenH(sz) + padY * 2);
  ctx.lineWidth = 1;
}

function drawHomeBase(nowMs) {
  // Death screen
  if (!state.alive) {
    drawDeathScreen();
    return;
  }

  // Hatching screen
  if (!state.hatched) {
    drawHatchingScreen();
    return;
  }

  // Mini-game
  if (state.miniGame.active) {
    drawMiniGame();
    return;
  }

  const sceneCfg = currentSceneConfig();
  if (sceneCfg) {
    const isSleepScene = state.scene === "sleep" || state.scene === "rest";
    const sleepElapsed = isSleepScene ? (nowMs - (state.sceneStartMs || nowMs)) : 0;
    updateSceneFrame(nowMs, sceneCfg);
    drawCurrentScene(sceneCfg);
    // After 2s walking in the room, show the sleeping close-up
    if (isSleepScene && sleepElapsed > 2000) {
      const slC = getSpriteCanvas("sablinasleep");
      if (slC) ctx.drawImage(slC, GAME_X, GAME_Y, GAME_W, GAME_H);
    }
  } else {
    updatePetFrame(nowMs);
    ctx.fillStyle = "#000000";
    ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);
    const petSpr = getPetSpriteName();
    const petSprInfo = SPRITES[petSpr] || SPRITES.sablinagif || {};
    drawFrameGame(petSpr, state.petFrame,
      (GAME.w - (petSprInfo.w || GAME.w)) / 2,
      (GAME.h - (petSprInfo.h || GAME.h)) / 2);
  }

  // Night overlay
  if (isNightTime()) {
    ctx.fillStyle = "rgba(0, 5, 30, 0.35)";
    ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);
  }

  // Vibration visual shake
  if (state.vibrating) {
    const shakeX = Math.random() * 4 - 2;
    const shakeY = Math.random() * 4 - 2;
    ctx.save();
    ctx.translate(shakeX, shakeY);
  }

  if (iconsVisible(nowMs) && state.scene !== "sleep" && state.scene !== "rest") {
    const IS = 22;
    // Blink the top-left alert icon when sick (500ms blink)
    const sickBlink = isSick() && Math.floor(nowMs / 500) % 2 === 0;
    const topAlert = (isSick() && !sickBlink) ? "warn"
                    : (state.hun < 60 || state.fat < 60 || state.cle < 60) ? "warn" : "pest";
    drawSpriteGameSmall(topAlert, 2, 0, IS);
    drawSpriteGameSmall("weber", 34, 0, IS);
    drawSpriteGameSmall("baby", 66, 0, IS);
    drawSpriteGameSmall("freg", 98, 0, IS);

    drawSpriteGameSmall("shop", 2, 106, IS);
    drawSpriteGameSmall("door", 34, 106, IS);
    drawSpriteGameSmall("box", 66, 106, IS);
    drawSpriteGameSmall("computer", 98, 106, IS);

    drawSelectionIndicator();
  }

  if (state.vibrating) {
    ctx.restore();
  }

  drawBubbleOnLeft(nowMs);
}

function isNightTime() {
  const h = new Date().getHours();
  return h >= 20 || h < 7;
}

function drawForestShowcase(nowMs) {
  updateForestFrame(nowMs);
  drawFrameGame("forest1", state.forestFrame, 0, 24);
}

function drawScreenBase(nowMs) {
  if (state.screen === screens.HOME) return;

  ctx.fillStyle = "#0b101b";
  ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);

  ctx.strokeStyle = "#24324f";
  ctx.strokeRect(GAME_X, GAME_Y, GAME_W, GAME_H);

  if (state.screen === screens.FOREST) {
    drawForestShowcase(nowMs);
    return;
  }
}

function drawGameArea(nowMs) {
  const effectiveHidden = panelHidden || !iconsVisible(nowMs);
  applyLayoutVars(effectiveHidden);

  ctx.fillStyle = "#0d1220";
  ctx.fillRect(0, 0, SCREEN.w, SCREEN.h);

  ctx.fillStyle = "#11182a";
  ctx.fillRect(LEFT_X, 0, LEFT_W, SCREEN.h);
  if (!effectiveHidden) {
    ctx.fillStyle = "#11182a";
    ctx.fillRect(RIGHT_X, 0, RIGHT_W, SCREEN.h);
  }

  ctx.fillStyle = "#000000";
  ctx.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H);
  if (!effectiveHidden) {
    ctx.fillStyle = "#12172a";
    ctx.fillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  }

  ctx.strokeStyle = "#2e2e2e";
  ctx.strokeRect(GAME_X, GAME_Y, GAME_W, GAME_H);
  if (!effectiveHidden) {
    ctx.strokeStyle = "#2f3d5a";
    ctx.beginPath();
    ctx.moveTo(RIGHT_X, 0);
    ctx.lineTo(RIGHT_X, SCREEN.h);
    ctx.stroke();
    ctx.strokeStyle = "#3b4b7a";
    ctx.strokeRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H);
  }

  const isSleeping = state.scene === "sleep" || state.scene === "rest";
  if (state.screen === screens.WIFI_FOOD) {
    drawWifiFoodScreen(nowMs);
  } else if (state.screen === screens.HOME || isSleeping) {
    drawHomeBase(nowMs);
  } else {
    drawScreenBase(nowMs);
  }

  drawTopBar();
  if (!effectiveHidden) drawInfoPanel();
  drawOverlayScreen();

  // NeoPixels glow simulation
  if (state.controls.neoPixelsEnabled && state.alive && state.hatched) {
    const moodColors = {
      CALM: "rgba(60, 100, 200, 0.12)",
      HAPPY: "rgba(100, 200, 100, 0.15)",
      EXCITED: "rgba(255, 200, 50, 0.15)",
      CURIOUS: "rgba(150, 100, 255, 0.12)",
      BORED: "rgba(100, 100, 100, 0.08)",
      HUNGRY: "rgba(255, 150, 50, 0.12)",
      SICK: "rgba(200, 50, 50, 0.12)",
    };
    ctx.fillStyle = moodColors[state.mood] || moodColors.CALM;
    ctx.fillRect(0, SCREEN.h - 3, SCREEN.w, 3);
  }

  // Visual indicators, sound, vibration, BLE peer
  drawSoundIndicator(nowMs);
  drawVibrateIndicator(nowMs);
  drawBlePeerIndicator(nowMs);

  // Dark mode overlay
  if (state.darkMode) {
    ctx.fillStyle = "rgba(0, 0, 10, 0.25)";
    ctx.fillRect(0, 0, SCREEN.w, SCREEN.h);
  }
}

function drawHud() {
  refs.moodBadge.textContent = state.mood;
  refs.stageBadge.textContent = state.stage;
  refs.activityBadge.textContent = state.activity;
  refs.ageBadge.textContent = `${state.ageDays}d ${state.ageHours}h ${state.ageMinutes}m`;
}

/* ── Self-healing animation state repair ── */
const _VALID_SCENES    = new Set(["home","eat","clean","play","sleep","shake","pet","hunt","discover","rest","forest","walk","color","garden","baby","box"]);
const _VALID_MOODS     = new Set(["CALM","HAPPY","EXCITED","HUNGRY","SICK","BORED","CURIOUS"]);
const _VALID_ACTIVITIES= new Set(["IDLE","EAT","CLEAN","PLAY","SLEEP","SHAKE","PET","HUNT","DISCOVER","REST"]);
const _VALID_STAGES    = new Set(["EGG","BABY","CHILD","TEEN","ADULT","ELDER"]);

function _healAnimState() {
  if (!_VALID_SCENES.has(state.scene))      { console.warn("[heal] bad scene:", state.scene);    state.scene    = "home"; }
  if (!_VALID_MOODS.has(state.mood))        { console.warn("[heal] bad mood:", state.mood);      state.mood     = "CALM"; }
  if (!_VALID_ACTIVITIES.has(state.activity)){console.warn("[heal] bad activity:", state.activity); state.activity= "IDLE"; }
  if (!_VALID_STAGES.has(state.stage))      { console.warn("[heal] bad stage:", state.stage);    state.stage    = "BABY"; }
  if (!Object.values(screens).includes(state.screen)) { state.screen = screens.HOME; }
  // Reset animation timers so frames re-sync cleanly
  state.sceneFrame  = 0;
  state.sceneTickMs = 0;
  state.petFrame    = 0;
  state.petTickMs   = 0;
}

let _lastRenderMs = 0;
let _renderWatchdogTimer = null;

function _startRenderWatchdog() {
  if (_renderWatchdogTimer) clearInterval(_renderWatchdogTimer);
  _renderWatchdogTimer = setInterval(() => {
    const gap = performance.now() - _lastRenderMs;
    if (_lastRenderMs > 0 && gap > 2000) {
      console.warn("[Sablina] render loop stalled for", gap.toFixed(0), "ms,restarting");
      _lastRenderMs = performance.now();
      requestAnimationFrame(render);
    }
  }, 2000);
}

function render(nowMs) {
  _lastRenderMs = performance.now();
  try {
    const now = nowMs || _lastRenderMs;
    logicTick(now);
    if (state.miniGame.active) updateMiniGame(now);
    updateVibration(now);
    ctx.fillStyle = "#000";
    ctx.fillRect(0, 0, SCREEN.w, SCREEN.h);
    drawGameArea(now);
    drawHud();
  } catch (err) {
    console.warn("[Sablina] render error,self-healing:", err.message, err.stack?.split("\n")[1]);
    _healAnimState();
  }
  requestAnimationFrame(render);
}

function resetPet(full) {
  state.hun = 70;
  state.fat = 70;
  state.cle = 70;
  state.alive = true;
  state.engine.zeroStatMs = 0;
  if (full) {
    state.ageMinutes = 0;
    state.ageHours = 0;
    state.ageDays = 0;
    state.exp = 0;
    state.stage = "BABY";
    state.hatched = false;
    state.hatchFrame = 0;
    state.hatchTickMs = 0;
    state.battery = 100;
    state.traits = { curiosity: 70, activity: 60, stress: 40 };
  }
  state.activity = "IDLE";
  state.miniGame.active = false;
  setScene("home");
  syncInputs();
  persistState();
}

function applyCommand(cmd) {
  if (!state.alive && cmd !== "fullreset" && cmd !== "revive") return;
  if (state.miniGame.active && cmd !== "screen:home") {
    miniGameTap();
    return;
  }

  if (cmd === "feed") {
    state.hun = clamp(state.hun + 15, 0, 100);
    state.lifetime.foodEaten++;
    setScene("eat");
    state.targetRoom = "KITCHEN";
    sfxFeed();
  }
  if (cmd === "clean") {
    state.cle = clamp(state.cle + 12, 0, 100);
    state.lifetime.cleans++;
    setScene("clean");
    state.targetRoom = "BATHROOM";
    sfxClean();
  }
  if (cmd === "sleep") {
    state.fat = clamp(state.fat + 12, 0, 100);
    state.lifetime.sleeps++;
    state.activity = "REST";
    state.engine.activityUntilMs = performance.now() + 10000;
    setScene("sleep");
    state.targetRoom = "BEDROOM";
    sfxSleep();
  }
  if (cmd === "play") {
    state.screen = screens.HOME;
    state.activity = "IDLE";
    setScene("home");
    startMiniGame();
    sfxPlay();
    return;
  }
  if (cmd === "pet") {
    state.fat = clamp(state.fat + 8, 0, 100);
    state.traits.stress = clamp(state.traits.stress - 3, 0, 100);
    state.lifetime.pets++;
    setScene("pet");
    triggerVibration(100);
  }
  if (cmd === "shake") {
    state.fat = clamp(state.fat + 5, 0, 100);
    state.traits.activity = clamp(state.traits.activity + 2, 0, 100);
    state.exp = clamp(state.exp + 2, 0, 9999);
    setScene("shake");
    sfxPlay();
    triggerVibration(150);
  }

  if (cmd === "hunt") {
    state.activity = "HUNT";
    state.engine.activityUntilMs = performance.now() + 5000;
    setScene("hunt");
    state.targetRoom = "LAB";
  }
  if (cmd === "discover") {
    state.activity = "DISCOVER";
    state.engine.activityUntilMs = performance.now() + 5000;
    setScene("discover");
    state.targetRoom = "PLAYROOM";
  }
  if (cmd === "rest") {
    state.activity = "REST";
    state.engine.activityUntilMs = performance.now() + 7000;
    setScene("rest");
    state.targetRoom = "BEDROOM";
  }

  if (cmd === "reset") resetPet(false);
  if (cmd === "fullreset") { resetPet(true); state.alive = true; state.hatched = true; state.battery = 100; }
  if (cmd === "revive") {
    state.alive = true;
    state.hun = 50;
    state.fat = 50;
    state.cle = 50;
    state.battery = 80;
    state.engine.zeroStatMs = 0;
    state.engine.criticalStatMs = 0;
    state.engine.sickMs = 0;
    state.bubble = `${state.name} is back!`;
    sfxHatch();
    persistState();
  }
  if (cmd === "heal") {
    if (state.exp >= 10) {
      state.exp -= 10;
      state.lifetime.coinsSpent += 10;
      applyMedicine();
    } else {
      applyMedicine(); // Telegram heal is free (doctor's orders)
    }
  }

  // ── WiFi Food commands – Sablina hunts! ─────────────────────
  if (cmd === "food_scan") {
    state.wifiFood.mode = "SCAN";
    state.wifiFood.hopping = true;
    state.wifiFood.aps = [];
    state.wifiFood.clients = [];
    state.wifiFood.totalPackets = 0;
    state.wifiFood.mgmtPackets = 0;
    state.wifiFood.dataPackets = 0;
    state.wifiFood.eapolPackets = 0;
    state.wifiFood.deauthsSent = 0;
    state.wifiFood.handshakes = [];
    state.wifiFood.pmkids = [];
    state.wifiFood.probes = 0;
    state.wifiFood.probeSnacks = 0;
    state.wifiFood.beaconsSent = 0;
    state.wifiFood.confused = 0;
    state.screen = screens.WIFI_FOOD;
    state.bubble = `${state.name} is sniffing for food...`;
    sfxWarn();
  }
  if (cmd === "food_probes") {
    // Probe Request Sniff,like Marauder sniffbeacon/probe-request-sniff
    if (!state.wifiFood.probes) state.wifiFood.probes = 0;
    if (!state.wifiFood.probeSnacks) state.wifiFood.probeSnacks = 0;
    state.wifiFood.mode = "PROBES";
    state.wifiFood.hopping = true;
    state.screen = screens.WIFI_FOOD;
    state.bubble = `📡 ${state.name} is listening for hungry devices...`;
  }
  if (cmd === "food_deauth") {
    const ap = state.wifiFood.aps[state.wifiFood.selectedAP];
    if (ap) {
      state.wifiFood.mode = "DEAUTH";
      state.wifiFood.hopping = false;
      state.wifiFood.channel = ap.channel;
      state.screen = screens.WIFI_FOOD;
      state.bubble = `☠️ ${state.name} is shaking ${ap.ssid || 'hidden'} for food!`;
      sfxVibrate();
    } else {
      // No APs yet,auto-start scan with hint
      state.wifiFood.mode = "SCAN";
      state.wifiFood.hopping = true;
      state.wifiFood.aps = [];
      state.screen = screens.WIFI_FOOD;
      state.bubble = `⚠ No prey! ${state.name} starts sniffing first...`;
      sfxWarn();
    }
  }
  if (cmd === "food_handshake") {
    const ap = state.wifiFood.aps[state.wifiFood.selectedAP];
    if (ap) {
      state.wifiFood.mode = "HANDSHAKE";
      state.wifiFood.hopping = false;
      state.wifiFood.channel = ap.channel;
      state.screen = screens.WIFI_FOOD;
      state.bubble = `${state.name} is waiting for a meal near ${ap.ssid || 'hidden'}...`;
    } else {
      state.wifiFood.mode = "SCAN";
      state.wifiFood.hopping = true;
      state.wifiFood.aps = [];
      state.screen = screens.WIFI_FOOD;
      state.bubble = `⚠ No prey! ${state.name} starts sniffing first...`;
      sfxWarn();
    }
  }
  if (cmd === "food_pmkid") {
    const ap = state.wifiFood.aps[state.wifiFood.selectedAP];
    if (ap) {
      state.wifiFood.mode = "PMKID";
      state.wifiFood.hopping = true;
      state.screen = screens.WIFI_FOOD;
      state.bubble = `🍬 ${state.name} is hunting for PMKID candy from ${ap.ssid || 'hidden'}...`;
    } else {
      state.wifiFood.mode = "SCAN";
      state.wifiFood.hopping = true;
      state.wifiFood.aps = [];
      state.screen = screens.WIFI_FOOD;
      state.bubble = `⚠ No prey! ${state.name} starts sniffing first...`;
      sfxWarn();
    }
  }
  if (cmd === "food_beacon") {
    // Beacon Spam,like Marauder's beacon-spam-random
    if (!state.wifiFood.beaconsSent) state.wifiFood.beaconsSent = 0;
    if (!state.wifiFood.confused) state.wifiFood.confused = 0;
    state.wifiFood.mode = "BEACON";
    state.wifiFood.hopping = false;
    state.screen = screens.WIFI_FOOD;
    state.bubble = `📢 ${state.name} is flooding the air with fake SSIDs!`;
    sfxVibrate();
  }
  if (cmd === "food_stop") {
    state.wifiFood.mode = "IDLE";
    state.wifiFood.hopping = false;
    state.bubble = `${state.name} stopped hunting.`;
  }
  if (cmd === "food_next") {
    const a = state.wifiFood;
    if (a.aps.length > 0) a.selectedAP = (a.selectedAP + 1) % a.aps.length;
  }
  if (cmd === "food_prev") {
    const a = state.wifiFood;
    if (a.aps.length > 0) a.selectedAP = (a.selectedAP - 1 + a.aps.length) % a.aps.length;
  }

  if (cmd.startsWith("screen:")) {
    const next = cmd.slice(7);
    // Redirect absorbed screens to their new home in STATS
    if (next === screens.ENVIRONMENT) { state.screen = screens.STATS; state.statsPage = 2; state.llmScreenUntilMs = 0; return; }
    if (next === screens.SYSTEM)      { state.screen = screens.STATS; state.statsPage = 3; state.llmScreenUntilMs = 0; return; }
    if (Object.values(screens).includes(next)) {
      state.screen = next;
      state.llmScreenUntilMs = 0;
      if (next === screens.FOREST) resetShowcaseFrames();
    }
  }

  state.sceneFrame = 0;
  state.sceneTickMs = 0;
  syncInputs();
}

function persistState() {
  const a = state.wifiFood;
  const snap = {
    _v: 3,
    name: state.name,
    hun: state.hun,
    fat: state.fat,
    cle: state.cle,
    exp: state.exp,
    battery: state.battery,
    charging: state.charging,
    wifi: state.wifi,
    ble: state.ble,
    llm: state.llm,
    darkMode: state.darkMode,
    mood: state.mood,
    stage: state.stage,
    alive: state.alive,
    hatched: state.hatched,
    roomTheme: state.roomTheme,
    ageMinutes: state.ageMinutes,
    ageHours: state.ageHours,
    ageDays: state.ageDays,
    prevStage: state.prevStage,
    sickMs: state.engine.sickMs > 0 ? 1 : 0,  // persist sick flag (not raw ms)
    wifiStats: state.wifiStats,
    traits: state.traits,
    controls: state.controls,
    lifetime: state.lifetime,
    socialMemory: state.socialMemory,
    agent: {
      currentTool: state.agent.currentTool,
      lastThought: state.agent.lastThought,
      lastResult: state.agent.lastResult,
      cycleCount: state.agent.cycleCount,
    },
    // WiFi hunt aggregate stats (no large arrays)
    wifiFoodStats: {
      totalPackets: a.totalPackets,
      mgmtPackets: a.mgmtPackets,
      dataPackets: a.dataPackets,
      eapolPackets: a.eapolPackets,
      deauthsSent: a.deauthsSent,
      handshakesTotal: a.handshakes.filter(h => h.complete).length,
      pmkidsTotal: a.pmkids.length,
    },
    // Audit log,capped, strip heavy fields to keep size small
    auditLog: (state.auditLog || []).slice(0, 20).map(e => ({
      ts: e.ts, type: e.type, ssid: (e.ssid || "").slice(0, 14),
    })),
    lastSave: Date.now(),
  };

  // Size guard,if JSON exceeds 50 KB, drop audit log entirely
  let raw = JSON.stringify(snap);
  if (raw.length > 51200) {
    snap.auditLog = [];
    raw = JSON.stringify(snap);
  }
  try {
    localStorage.setItem(STORE_KEY, raw);
  } catch (e) {
    // localStorage full,try without auditLog
    snap.auditLog = [];
    try { localStorage.setItem(STORE_KEY, JSON.stringify(snap)); } catch (_) { /* truly full */ }
  }
}

function loadState() {
  const raw = localStorage.getItem(STORE_KEY);
  if (!raw) return;
  try {
    const s = JSON.parse(raw);
    if (typeof s !== "object" || s === null) return;
    Object.assign(state, {
      name: s.name ?? state.name,
      hun: clamp(Number(s.hun ?? state.hun), 0, 100),
      fat: clamp(Number(s.fat ?? state.fat), 0, 100),
      cle: clamp(Number(s.cle ?? state.cle), 0, 100),
      exp: clamp(Number(s.exp ?? state.exp), 0, 9999),
      battery: clamp(Number(s.battery ?? state.battery), 0, 100),
      charging: Boolean(s.charging ?? state.charging),
      wifi: Boolean(s.wifi ?? state.wifi),
      ble: Boolean(s.ble ?? state.ble),
      llm: true,
      darkMode: Boolean(s.darkMode ?? state.darkMode),
      mood: _VALID_MOODS.has(s.mood)  ? s.mood  : state.mood,
      stage: _VALID_STAGES.has(s.stage) ? s.stage : state.stage,
      alive: s.alive !== undefined ? Boolean(s.alive) : true,
      hatched: s.hatched !== undefined ? Boolean(s.hatched) : true,
      roomTheme: s.roomTheme || state.roomTheme,
      ageMinutes: clamp(Number(s.ageMinutes ?? state.ageMinutes), 0, 59),
      ageHours: clamp(Number(s.ageHours ?? state.ageHours), 0, 23),
      ageDays: clamp(Number(s.ageDays ?? state.ageDays), 0, 999),
      prevStage: _VALID_STAGES.has(s.prevStage) ? s.prevStage : state.prevStage,
    });
    // Restore sickness flag (use current time as placeholder so timer logic works)
    if (s.sickMs) state.engine.sickMs = performance.now();

    // Battery recovery: auto-charge if critically low on load
    if (state.battery < 5) { state.charging = true; }

    if (s.wifiStats) {
      Object.assign(state.wifiStats, s.wifiStats);
      state.wifiStats.netCount    = clamp(Number(state.wifiStats.netCount), 0, 20);
      state.wifiStats.strongCount = clamp(Number(state.wifiStats.strongCount), 0, 20);
      state.wifiStats.hiddenCount = clamp(Number(state.wifiStats.hiddenCount), 0, 20);
      state.wifiStats.openCount   = clamp(Number(state.wifiStats.openCount), 0, 20);
      state.wifiStats.wpaCount    = clamp(Number(state.wifiStats.wpaCount), 0, 20);
      state.wifiStats.avgRSSI     = clamp(Number(state.wifiStats.avgRSSI), -100, -30);
      state.wifiStats.scanRunning = false;
    }

    if (s.traits) {
      Object.assign(state.traits, s.traits);
      state.traits.curiosity = clamp(Number(state.traits.curiosity), 0, 100);
      state.traits.activity  = clamp(Number(state.traits.activity),  0, 100);
      state.traits.stress    = clamp(Number(state.traits.stress),    0, 100);
    }

    if (s.controls) {
      Object.assign(state.controls, s.controls);
      state.controls.tftBrightnessIndex = clamp(Number(state.controls.tftBrightnessIndex), 0, 2);
      state.controls.ledBrightnessIndex = clamp(Number(state.controls.ledBrightnessIndex), 0, 2);
      state.controls.autoSaveMs = [15000, 30000, 60000].includes(Number(state.controls.autoSaveMs))
        ? Number(state.controls.autoSaveMs)
        : 30000;
    }

    if (s.lifetime && typeof s.lifetime === "object") {
      Object.assign(state.lifetime, s.lifetime);
    }

    if (s.socialMemory && typeof s.socialMemory === "object") {
      state.socialMemory = Object.fromEntries(
        Object.entries(s.socialMemory).slice(0, 12).map(([id, memory]) => {
          const item = memory && typeof memory === "object" ? memory : {};
          return [String(id), {
            id: String(id),
            name: String(item.name || "Nearby Sablina"),
            encounters: clamp(Number(item.encounters || 0), 0, 9999),
            chats: clamp(Number(item.chats || 0), 0, 9999),
            affinity: clamp(Number(item.affinity || 0), 0, 100),
            giftsGiven: clamp(Number(item.giftsGiven || 0), 0, 9999),
            giftsReceived: clamp(Number(item.giftsReceived || 0), 0, 9999),
            lastGift: String(item.lastGift || ""),
            lastSeenMs: Number(item.lastSeenMs || 0),
          }];
        }),
      );
    }

    // Restore ReAct agent state
    if (s.agent && typeof s.agent === "object") {
      state.agent.currentTool = String(s.agent.currentTool || "none");
      state.agent.lastThought = String(s.agent.lastThought || "");
      state.agent.lastResult  = String(s.agent.lastResult || "");
      state.agent.cycleCount  = Math.max(0, Number(s.agent.cycleCount) || 0);
    }

    // Restore audit log (strip unknown fields, cap at 20)
    if (Array.isArray(s.auditLog)) {
      state.auditLog = s.auditLog.slice(0, 20).map(e => ({
        ts: String(e.ts || ""), type: String(e.type || ""), ssid: String(e.ssid || ""), detail: "",
      }));
    }

    // Restore WiFi hunt aggregate stats into lifetime if missing
    if (s.wifiFoodStats && typeof s.wifiFoodStats === "object") {
      const wfs = s.wifiFoodStats;
      if (typeof state.lifetime.handshakes === "undefined") {
        state.lifetime.handshakes    = clamp(Number(wfs.handshakesTotal || 0), 0, 9999);
        state.lifetime.pmkidCaptures = clamp(Number(wfs.pmkidsTotal    || 0), 0, 9999);
        state.lifetime.deauths       = clamp(Number(wfs.deauthsSent    || 0), 0, 9999);
      }
    }

    // Final sanity repair
    _healAnimState();
  } catch (_) {
    // Corrupted save,start fresh but keep it (don't delete)
    console.warn("[Sablina] corrupted save, starting fresh");
    _healAnimState();
  }
}

function syncInputs() {
  refs.petName.value = state.name;

  refs.hun.value = state.hun;
  refs.fat.value = state.fat;
  refs.cle.value = state.cle;
  refs.exp.value = state.exp;
  refs.hunVal.textContent = state.hun;
  refs.fatVal.textContent = state.fat;
  refs.cleVal.textContent = state.cle;
  refs.expVal.textContent = state.exp;

  refs.bubble.value = state.bubble;

  refs.wifi.checked = state.wifi;
  refs.ble.checked = state.ble;
  refs.llm.checked = true;
  refs.llm.disabled = true;
  state.llm = true;
  refs.darkMode.checked = state.darkMode;

  refs.netCount.value = state.wifiStats.netCount;
  refs.strongCount.value = state.wifiStats.strongCount;
  refs.hiddenCount.value = state.wifiStats.hiddenCount;
  refs.openCount.value = state.wifiStats.openCount;
  refs.wpaCount.value = state.wifiStats.wpaCount;
  refs.avgRssi.value = state.wifiStats.avgRSSI;

  refs.curiosity.value = Math.round(state.traits.curiosity);
  refs.activityTrait.value = Math.round(state.traits.activity);
  refs.stress.value = Math.round(state.traits.stress);

  refs.autoSleep.checked = state.controls.autoSleep;
  refs.autoSaveMs.value = String(state.controls.autoSaveMs);
  refs.soundEnabled.checked = state.controls.soundEnabled;
  refs.neoPixelsEnabled.checked = state.controls.neoPixelsEnabled;
  refs.tftBrightnessIndex.value = String(state.controls.tftBrightnessIndex);
  refs.ledBrightnessIndex.value = String(state.controls.ledBrightnessIndex);

  drawHud();
  syncTwoButtonLabel();
}

function bindNumberInput(ref, min, max, apply) {
  ref.addEventListener("input", () => {
    const value = clamp(Number(ref.value), min, max);
    apply(value);
  });
}

function hookEvents() {
  refs.petName.addEventListener("input", () => {
    state.name = refs.petName.value || "Sablina";
    persistState();
  });

  bindNumberInput(refs.hun, 0, 100, (v) => {
    state.hun = v;
    refs.hunVal.textContent = v;
    persistState();
  });
  bindNumberInput(refs.fat, 0, 100, (v) => {
    state.fat = v;
    refs.fatVal.textContent = v;
    persistState();
  });
  bindNumberInput(refs.cle, 0, 100, (v) => {
    state.cle = v;
    refs.cleVal.textContent = v;
    persistState();
  });
  bindNumberInput(refs.exp, 0, 999, (v) => {
    state.exp = v;
    refs.expVal.textContent = v;
    persistState();
  });

  refs.wifi.addEventListener("change", () => {
    state.wifi = refs.wifi.checked;
    persistState();
  });
  refs.ble.addEventListener("change", () => {
    state.ble = refs.ble.checked;
    persistState();
  });
  refs.llm.addEventListener("change", () => {
    state.llm = true;
    refs.llm.checked = true;
    persistState();
  });

  refs.darkMode.addEventListener("change", () => {
    state.darkMode = refs.darkMode.checked;
    resetShowcaseFrames();
    persistState();
  });

  refs.realWifi.addEventListener("change", () => {
    state.realWifi = refs.realWifi.checked;
    if (state.realWifi) pollRealWifi();
    persistState();
  });

  refs.charging.addEventListener("change", () => {
    state.charging = refs.charging.checked;
  });

  refs.bubble.addEventListener("input", () => {
    state.bubble = refs.bubble.value;
    state.targetRoom = inferRoomFromStateAndText(state.bubble);
  });

  bindNumberInput(refs.netCount, 0, 20, (v) => {
    state.wifiStats.netCount = v;
    persistState();
  });
  bindNumberInput(refs.strongCount, 0, 20, (v) => {
    state.wifiStats.strongCount = v;
    persistState();
  });
  bindNumberInput(refs.hiddenCount, 0, 20, (v) => {
    state.wifiStats.hiddenCount = v;
    persistState();
  });
  bindNumberInput(refs.openCount, 0, 20, (v) => {
    state.wifiStats.openCount = v;
    persistState();
  });
  bindNumberInput(refs.wpaCount, 0, 20, (v) => {
    state.wifiStats.wpaCount = v;
    persistState();
  });
  bindNumberInput(refs.avgRssi, -100, -30, (v) => {
    state.wifiStats.avgRSSI = v;
    persistState();
  });

  bindNumberInput(refs.curiosity, 0, 100, (v) => {
    state.traits.curiosity = v;
    persistState();
  });
  bindNumberInput(refs.activityTrait, 0, 100, (v) => {
    state.traits.activity = v;
    persistState();
  });
  bindNumberInput(refs.stress, 0, 100, (v) => {
    state.traits.stress = v;
    persistState();
  });

  refs.autoSleep.addEventListener("change", () => {
    state.controls.autoSleep = refs.autoSleep.checked;
    persistState();
  });

  refs.autoSaveMs.addEventListener("change", () => {
    state.controls.autoSaveMs = Number(refs.autoSaveMs.value);
    persistState();
  });

  refs.soundEnabled.addEventListener("change", () => {
    state.controls.soundEnabled = refs.soundEnabled.checked;
    persistState();
  });

  refs.neoPixelsEnabled.addEventListener("change", () => {
    state.controls.neoPixelsEnabled = refs.neoPixelsEnabled.checked;
    persistState();
  });

  refs.tftBrightnessIndex.addEventListener("change", () => {
    state.controls.tftBrightnessIndex = Number(refs.tftBrightnessIndex.value);
    persistState();
  });

  refs.ledBrightnessIndex.addEventListener("change", () => {
    state.controls.ledBrightnessIndex = Number(refs.ledBrightnessIndex.value);
    persistState();
  });

  bindShortLongPress(refs.btnA, twoButtonSelect, twoButtonHome);
  refs.btnB.addEventListener("click", () => twoButtonNext());

  // Sidebar toggle (web panel)
  refs.sidebarToggle.addEventListener("click", () => {
    const layout = document.querySelector(".layout");
    layout.classList.toggle("panel-closed");
    refs.sidebarToggle.textContent = layout.classList.contains("panel-closed") ? "▶" : "◀";
  });

  // Canvas info panel toggle (device screen right panel)
  document.getElementById("canvasPanelToggle").addEventListener("click", () => {
    setLayoutPanelHidden(!panelHidden);
    document.getElementById("canvasPanelToggle").textContent = panelHidden ? "☰ Panel" : "✕ Panel";
  });

  document.addEventListener("keydown", (ev) => {
    state.iconsLastInteractMs = performance.now();
    if (ev.key.toLowerCase() === "x") {
      twoButtonNext();
    }
    if (ev.key.toLowerCase() === "z") {
      if (ev.shiftKey) twoButtonHome();
      else twoButtonSelect();
    }
  });

  document.querySelectorAll("button[data-cmd]").forEach((btn) => {
    btn.addEventListener("click", () => {
      state.iconsLastInteractMs = performance.now();
      applyCommand(btn.dataset.cmd);
      persistState();
    });
  });
}

/* ── Real WiFi polling via wifi-scan-server.js ── */
let _wifiPollTimer = null;
function pollRealWifi() {
  if (_wifiPollTimer) return;              // already running
  _wifiPollTimer = setInterval(async () => {
    if (!state.realWifi) { clearInterval(_wifiPollTimer); _wifiPollTimer = null; return; }
    try {
      const r = await fetch("http://127.0.0.1:3210/scan");
      const j = await r.json();
      if (j.ok && Array.isArray(j.networks)) {
        state.realWifiNetworks = j.networks;
        // Also push summary into wifiStats for the HUD
        state.wifiStats.netCount = j.networks.length;
        const strong = j.networks.filter(n => n.signal >= 60).length;
        state.wifiStats.strongCount = strong;
        state.wifiStats.hiddenCount = j.networks.filter(n => !n.ssid).length;
        state.wifiStats.openCount = j.networks.filter(n => !n.security || n.security === '--').length;
        state.wifiStats.wpaCount = j.networks.filter(n => n.security && n.security.includes('WPA')).length;
        if (j.networks.length) {
          state.wifiStats.avgRSSI = Math.round(j.networks.reduce((s,n) => s + (n.signal || 0), 0) / j.networks.length) - 100;
        }
      }
    } catch (_) { /* server not running – silent */ }
  }, 15000);     // scan every 15 s
  // immediate first scan
  fetch("http://127.0.0.1:3210/scan").then(r => r.json()).then(j => {
    if (j.ok) { state.realWifiNetworks = j.networks; }
  }).catch(() => {});
}

loadState();
hookEvents();
syncInputs();
initBleChannel();
render();
_startRenderWatchdog();

// Save on tab close / background,never lose progress
window.addEventListener("beforeunload", () => persistState());
document.addEventListener("visibilitychange", () => { if (document.hidden) persistState(); });

// BLE status display, interactions are autonomous
setInterval(() => {
  const el = document.getElementById("blePeerStatus");
  const logEl = document.getElementById("bleLog");
  if (!el) return;
  if (!state.ble) {
    el.textContent = "BLE disabled";
    el.classList.remove("connected");
    if (logEl) logEl.textContent = "";
  } else if (isPeerAlive()) {
    el.textContent = `🔗 ${blePeer.name} | ${blePeer.mood} | ${blePeer.stage} | Bat:${blePeer.battery}%`;
    el.classList.add("connected");
    if (logEl && blePeerInteraction) logEl.textContent = blePeerInteraction;
  } else {
    el.textContent = "Scanning... Open another tab to connect";
    el.classList.remove("connected");
    if (logEl) logEl.textContent = "";
  }
}, 1000);

// WiFi Food status display
setInterval(() => {
  const el = document.getElementById("foodStatus");
  if (!el) return;
  const a = state.wifiFood;
  const hs = a.handshakes.filter(h => h.complete).length;
  let statusText = `Mode: ${a.mode} | CH:${a.channel} | PKT: ${a.totalPackets}`;
  if (a.mode === "PROBES")  statusText += ` | 📡 Probes: ${a.probes||0} | Snacks: ${a.probeSnacks||0}`;
  else if (a.mode === "BEACON") statusText += ` | 📢 Sent: ${a.beaconsSent||0} | Confused: ${a.confused||0}`;
  else statusText += ` | Prey: ${a.aps.length} | 🍖 HS: ${hs} | 🍬 PMKID: ${a.pmkids.length}`;
  el.textContent = statusText;
  const modeColorMap = { DEAUTH:"#f44336", HANDSHAKE:"#ce93d8", PMKID:"#ffb74d", PROBES:"#b3e5fc", BEACON:"#fff176" };
  const mc = modeColorMap[a.mode];
  if (mc) {
    el.style.borderColor = mc;
    el.style.color = mc;
  } else if (a.mode !== "IDLE") {
    el.style.borderColor = "#00e5ff";
    el.style.color = "#00e5ff";
  } else {
    el.style.borderColor = "#1a2a3a";
    el.style.color = "#607080";
  }
}, 500);

// Auto-start polling if realWifi was persisted as true
if (state.realWifi) { refs.realWifi.checked = true; pollRealWifi(); }

/* ═══════════════════════════════════════════════════════════
   ██  PROJECT EDITOR  ██
   ═══════════════════════════════════════════════════════════ */

const FLASH_MAX_MB = 16;
const FIRMWARE_OVERHEAD_KB = 250; // bootloader + partition + code

/* ── Scene mapping config (editable) ── */
const sceneMappingDefs = [
  { scene: "clean",    label: "Clean / Shower" },
  { scene: "play",     label: "Play / Game" },
  { scene: "sleep",    label: "Sleep" },
  { scene: "shake",    label: "Shake" },
  { scene: "pet",      label: "Pet" },
  { scene: "hunt",     label: "Hunt (WiFi Food)" },
  { scene: "discover", label: "Discover / Explore" },
  { scene: "rest",     label: "Rest" },
  { scene: "walk",     label: "Walk" },
  { scene: "color",    label: "Color Play" },
  { scene: "garden",   label: "Garden" },
  { scene: "baby",     label: "Baby" },
  { scene: "box",      label: "Box / Idle 2" },
];

// Editable scene→sprite map (mirrors currentSceneConfig)
const sceneMap = {
  clean:    "cleangif",
  play:     "gamegif",
  sleep:    "sleepgif",
  shake:    "gardengif",
  pet:      "colorgif",
  hunt:     "eatgif",
  discover: "gardengif",
  rest:     "sleepgif",
  walk:     "gamewalk",
  color:    "colorgif",
  garden:   "gardengif",
  baby:     "baby",
  box:      "box",
};

/* ── Size calculation ── */
function calcSpriteBinaryBytes(s) {
  if (!s) return 0;
  const px = s.w * s.h;
  const frameCount = Array.isArray(s.frames) ? s.frames.length : 1;
  return px * 2 * frameCount; // 2 bytes per pixel (RGB565)
}

function updateSizeBar() {
  const bar = document.getElementById("sizeBar");
  const label = document.getElementById("sizeLabel");
  const breakdown = document.getElementById("sizeBreakdown");
  if (!bar) return;

  let totalBytes = FIRMWARE_OVERHEAD_KB * 1024;
  const categories = {};

  for (const [name, s] of Object.entries(SPRITES)) {
    const bytes = calcSpriteBinaryBytes(s);
    totalBytes += bytes;

    let cat = "other";
    if (name.startsWith("f") && name.length <= 3) cat = "food";
    else if (name.startsWith("sablinaeat")) cat = "eat-anim";
    else if (name.startsWith("room")) cat = "rooms";
    else if (name.startsWith("forest")) cat = "forest";
    else if (["cleangif","eatgif","gamegif","gardengif","sleepgif","colorgif"].includes(name)) cat = "scenes";
    else if (["sablinagif","sablinasleep","gamewalk","baby","salida"].includes(name)) cat = "pet";
    else if (["warn","pest","weber","freg","shop","door","box","computer"].includes(name)) cat = "icons";
    else if (["picture0","picture1","pancho","sushi","watercooler"].includes(name)) cat = "items";

    categories[cat] = (categories[cat] || 0) + bytes;
  }

  const totalMB = totalBytes / 1024 / 1024;
  const pct = Math.min((totalMB / FLASH_MAX_MB) * 100, 100);

  bar.style.width = pct + "%";
  if (totalMB > FLASH_MAX_MB * 0.85) {
    bar.style.background = "linear-gradient(90deg, #f2b93b, #ff5555)";
  } else {
    bar.style.background = "linear-gradient(90deg, #66e08b, #f2b93b)";
  }
  label.textContent = `${totalMB.toFixed(2)} / ${FLASH_MAX_MB} MB (${pct.toFixed(1)}%)`;

  // Breakdown
  const catNames = {
    "food": "Food Sprites (f01-f22)",
    "eat-anim": "Eat Animation",
    "rooms": "Room Backgrounds",
    "forest": "Forest Scenes",
    "scenes": "Activity Scenes",
    "pet": "Pet Sprites",
    "icons": "UI Icons",
    "items": "Shop Items",
    "other": "Other",
  };
  const sortedCats = Object.entries(categories).sort((a, b) => b[1] - a[1]);
  let html = `<span><span class="sb-name">Firmware</span><span class="sb-size">${FIRMWARE_OVERHEAD_KB} KB</span></span>`;
  for (const [cat, bytes] of sortedCats) {
    const kb = (bytes / 1024).toFixed(1);
    const name = catNames[cat] || escHtml(cat);
    html += `<span><span class="sb-name">${name}</span><span class="sb-size">${kb} KB</span></span>`;
  }
  html += `<span><span class="sb-name"><b>TOTAL</b></span><span class="sb-size"><b>${totalMB.toFixed(2)} MB</b></span></span>`;
  breakdown.innerHTML = html;
}

/* ── Scene mapping editor ── */
function buildSceneMappings() {
  const container = document.getElementById("sceneMappings");
  if (!container) return;

  const animatedSprites = Object.entries(SPRITES)
    .filter(([, s]) => Array.isArray(s.frames) && s.frames.length > 1)
    .map(([n]) => n)
    .sort();

  let html = "";
  for (const def of sceneMappingDefs) {
    html += `<div class="scene-row">
      <label>${escHtml(def.label)}</label>
      <select data-scene="${escHtml(def.scene)}">`;
    for (const sp of animatedSprites) {
      const sel = sceneMap[def.scene] === sp ? " selected" : "";
      html += `<option value="${escHtml(sp)}"${sel}>${escHtml(sp)}</option>`;
    }
    html += `</select></div>`;
  }
  container.innerHTML = html;

  container.querySelectorAll("select").forEach(sel => {
    sel.addEventListener("change", () => {
      sceneMap[sel.dataset.scene] = sel.value;
    });
  });
}

// Activate editable scene map
window._sceneMapOverride = sceneMap;

/* ── Sprite gallery ── */
let previewSprite = null;
let previewFrame = 0;
let previewAnimTimer = null;

function buildSpriteGallery() {
  const gallery = document.getElementById("spriteGallery");
  const countEl = document.getElementById("spriteCount");
  if (!gallery) return;

  const names = Object.keys(SPRITES).sort();
  countEl.textContent = names.length;

  gallery.innerHTML = "";
  for (const name of names) {
    const s = SPRITES[name];
    const card = document.createElement("div");
    card.className = "sprite-card";
    card.dataset.name = name;

    const cvs = document.createElement("canvas");
    const hasFrames = Array.isArray(s.frames);
    cvs.width = s.w;
    cvs.height = s.h;
    const cctx = cvs.getContext("2d");

    // Draw first frame or static
    if (hasFrames && s.frames.length > 0) {
      const fc = getFrameCanvas(name, 0);
      if (fc) cctx.drawImage(fc, 0, 0);
    } else {
      const sc = getSpriteCanvas(name);
      if (sc) cctx.drawImage(sc, 0, 0);
    }

    const frameCount = hasFrames ? s.frames.length : 1;
    const bytes = calcSpriteBinaryBytes(s);
    const kb = (bytes / 1024).toFixed(1);

    card.innerHTML = "";
    card.appendChild(cvs);

    const nameDiv = document.createElement("div");
    nameDiv.className = "sc-name";
    nameDiv.textContent = name;
    card.appendChild(nameDiv);

    const infoDiv = document.createElement("div");
    infoDiv.className = "sc-info";
    infoDiv.textContent = `${s.w}×${s.h} · ${frameCount}f · ${kb}KB`;
    card.appendChild(infoDiv);

    card.addEventListener("click", () => openPreview(name));
    gallery.appendChild(card);
  }
}

function openPreview(name) {
  const s = SPRITES[name];
  if (!s) return;

  // Highlight card
  document.querySelectorAll(".sprite-card").forEach(c => c.classList.remove("active"));
  const card = document.querySelector(`.sprite-card[data-name="${name}"]`);
  if (card) card.classList.add("active");

  previewSprite = name;
  previewFrame = 0;
  if (previewAnimTimer) { clearInterval(previewAnimTimer); previewAnimTimer = null; }

  const panel = document.getElementById("spritePreview");
  const nameEl = document.getElementById("previewName");
  const infoEl = document.getElementById("previewInfo");
  panel.style.display = "";
  nameEl.textContent = name;

  const hasFrames = Array.isArray(s.frames);
  const frameCount = hasFrames ? s.frames.length : 1;
  const bytes = calcSpriteBinaryBytes(s);

  infoEl.innerHTML = `
    <b>Size:</b> ${s.w} × ${s.h} px &nbsp;|&nbsp;
    <b>Frames:</b> ${frameCount} &nbsp;|&nbsp;
    <b>Binary:</b> ${(bytes / 1024).toFixed(1)} KB &nbsp;|&nbsp;
    <b>Pixels/frame:</b> ${s.w * s.h}
  `;

  drawPreviewFrame();
}

function drawPreviewFrame() {
  const cvs = document.getElementById("previewCanvas");
  const pctx = cvs.getContext("2d");
  const s = SPRITES[previewSprite];
  if (!s) return;

  const hasFrames = Array.isArray(s.frames);
  const frameCount = hasFrames ? s.frames.length : 1;

  // Scale to fit 256px canvas
  const scale = Math.min(256 / s.w, 256 / s.h);
  const dw = Math.round(s.w * scale);
  const dh = Math.round(s.h * scale);
  const dx = Math.round((256 - dw) / 2);
  const dy = Math.round((256 - dh) / 2);

  pctx.fillStyle = "#0a0e1a";
  pctx.fillRect(0, 0, 256, 256);
  pctx.imageSmoothingEnabled = false;

  let src;
  if (hasFrames && s.frames.length > 0) {
    src = getFrameCanvas(previewSprite, previewFrame % frameCount);
  } else {
    src = getSpriteCanvas(previewSprite);
  }
  if (src) pctx.drawImage(src, dx, dy, dw, dh);

  const labelEl = document.getElementById("previewFrameLabel");
  labelEl.textContent = `Frame ${previewFrame + 1} / ${frameCount}`;
}

function initEditorEvents() {
  document.getElementById("previewPrev")?.addEventListener("click", () => {
    const s = SPRITES[previewSprite];
    if (!s) return;
    const fc = Array.isArray(s.frames) ? s.frames.length : 1;
    previewFrame = (previewFrame - 1 + fc) % fc;
    drawPreviewFrame();
  });

  document.getElementById("previewNext")?.addEventListener("click", () => {
    const s = SPRITES[previewSprite];
    if (!s) return;
    const fc = Array.isArray(s.frames) ? s.frames.length : 1;
    previewFrame = (previewFrame + 1) % fc;
    drawPreviewFrame();
  });

  document.getElementById("previewPlay")?.addEventListener("click", () => {
    if (previewAnimTimer) {
      clearInterval(previewAnimTimer);
      previewAnimTimer = null;
      document.getElementById("previewPlay").textContent = "▶ Play Animation";
      return;
    }
    document.getElementById("previewPlay").textContent = "⏸ Stop";
    previewAnimTimer = setInterval(() => {
      const s = SPRITES[previewSprite];
      if (!s) return;
      const fc = Array.isArray(s.frames) ? s.frames.length : 1;
      previewFrame = (previewFrame + 1) % fc;
      drawPreviewFrame();
    }, 160);
  });

  document.getElementById("spriteFilter")?.addEventListener("input", (e) => {
    const q = e.target.value.toLowerCase();
    document.querySelectorAll(".sprite-card").forEach(card => {
      card.style.display = card.dataset.name.includes(q) ? "" : "none";
    });
  });
}

/* ── Boot editor ── */
updateSizeBar();
buildSceneMappings();
buildSpriteGallery();
initEditorEvents();

/* ── Telegram Simulator Panel ── */
(function initTelegramSimulator() {
  const tgLog = document.getElementById("tgLog");
  const tgInput = document.getElementById("tgCustomMsg");
  const tgSendBtn = document.getElementById("tgSendBtn");
  if (!tgLog || !tgInput || !tgSendBtn) return;

  function tgLogMsg(text, color) {
    const line = document.createElement("div");
    line.style.color = color || "#7af";
    line.textContent = text;
    tgLog.appendChild(line);
    tgLog.scrollTop = tgLog.scrollHeight;
    // keep log short
    while (tgLog.childElementCount > 20) tgLog.removeChild(tgLog.firstChild);
  }

  function handleTgCommand(raw) {
    const cmd = (raw || "").trim().toLowerCase();
    if (!cmd) return;
    tgLogMsg(`→ ${cmd}`, "#aaa");

    const map = {
      "/feed": "feed", "feed": "feed",
      "/clean": "clean", "clean": "clean",
      "/sleep": "sleep", "sleep": "sleep",
      "/play": "play", "play": "play",
      "/pet": "pet", "pet": "pet",
      "/heal": "heal", "heal": "heal",
      "/revive": "revive", "revive": "revive",
      "/stats": "screen:stats",
      "/shop": "screen:shop",
      "/home": "screen:home",
    };

    const action = map[cmd];
    if (action) {
      applyCommand(action);
      tgLogMsg(`✓ ${state.bubble || action}`, "#66e08b");
    } else {
      tgLogMsg(`⚠ Comando desconocido. Prueba: /feed /clean /sleep /play /pet /heal /stats /shop /revive`, "#f2b93b");
    }
  }

  // Wire the send button
  tgSendBtn.addEventListener("click", () => {
    handleTgCommand(tgInput.value);
    tgInput.value = "";
  });
  tgInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") { handleTgCommand(tgInput.value); tgInput.value = ""; }
  });

  // Wire the quick-action Telegram buttons (the ones in the TG panel)
  // Note: the global button[data-cmd] listener already fires applyCommand().
  // We hook the SAME buttons here additionally to log the action in the TG log.
  tgLog.closest(".group")?.querySelectorAll("button[data-cmd]").forEach(btn => {
    btn.addEventListener("click", () => {
      tgLogMsg(`→ ${btn.dataset.cmd}`, "#aaa");
      setTimeout(() => tgLogMsg(`✓ ${state.bubble || btn.dataset.cmd}`, "#66e08b"), 50);
    });
  });

  tgLogMsg("📱 Telegram Simulator listo. Escribe /feed, /clean, etc.", "#7af");
})();
