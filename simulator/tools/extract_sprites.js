const fs = require('fs');
const path = require('path');

const repo = '/home/nexland/Sablina_Tamagotchi_ESP32/SablinaTamagotchi_1.0';
const out = '/home/nexland/Sablina_Tamagotchi_ESP32/simulator/sprites.js';

const defs = [
  { type: 'single', name: 'warn', file: 'warn.h', w: 28, h: 28 },
  { type: 'single', name: 'pest', file: 'pest.h', w: 28, h: 28 },
  { type: 'single', name: 'weber', file: 'weber.h', w: 29, h: 29 },
  { type: 'single', name: 'baby', file: 'baby.h', w: 29, h: 29 },
  { type: 'single', name: 'freg', file: 'freg.h', w: 29, h: 29 },
  { type: 'single', name: 'shop', file: 'shop.h', w: 29, h: 29 },
  { type: 'single', name: 'door', file: 'door.h', w: 29, h: 29 },
  { type: 'single', name: 'box', file: 'box.h', w: 29, h: 29 },
  { type: 'single', name: 'computer', file: 'computer.h', w: 29, h: 29 },

  // Food icons used in the original food grid/animations.
  ...Array.from({ length: 22 }, (_, i) => ({
    type: 'single',
    name: `f${String(i + 1).padStart(2, '0')}`,
    file: `f${String(i + 1).padStart(2, '0')}.h`,
    w: 29,
    h: 29,
  })),

  // Extra icons from original project.
  { type: 'single', name: 'pancho', file: 'pancho.h', w: 29, h: 29 },
  { type: 'single', name: 'sushi', file: 'sushi.h', w: 29, h: 29 },
  { type: 'single', name: 'watercooler', file: 'watercooler.h', w: 29, h: 29 },
  { type: 'single', name: 'salida', file: 'salida.h', w: 29, h: 29 },

  // Gallery / collection pictures.
  { type: 'single', name: 'picture0', file: 'picture0.h', w: 126, h: 94 },
  { type: 'single', name: 'picture1', file: 'picture1.h', w: 126, h: 88 },

  // Full-screen/screen-window single images.
  { type: 'single', name: 'sablinaeat', file: 'sablinaeat.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat2', file: 'sablinaeat2.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat3', file: 'sablinaeat3.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat4', file: 'sablinaeat4.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat5', file: 'sablinaeat5.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat6', file: 'sablinaeat6.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat7', file: 'sablinaeat7.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat8', file: 'sablinaeat8.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat9', file: 'sablinaeat9.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat10', file: 'sablinaeat10.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinaeat11', file: 'sablinaeat11.h', w: 128, h: 88 },
  { type: 'single', name: 'sablinasleep', file: 'sablinasleep.h', w: 128, h: 128 },
  { type: 'single', name: 'roomwhite', file: 'roomwhite.h', w: 128, h: 104 },
  { type: 'single', name: 'roomgray', file: 'roomgray.h', w: 128, h: 104 },
  { type: 'single', name: 'roomblue', file: 'roomblue.h', w: 128, h: 104 },
  { type: 'single', name: 'roomgreen', file: 'roomgreen.h', w: 128, h: 104 },
  { type: 'single', name: 'roomred', file: 'roomred.h', w: 128, h: 104 },
  { type: 'single', name: 'roomblack', file: 'roomblack.h', w: 128, h: 104 },

  // Main home animation backgrounds used by maingif().
  { type: 'frames', name: 'forest1', file: 'forest1.h', w: 128, h: 60, framePixels: 7680, frameCount: 75 },
  { type: 'frames', name: 'forest2', file: 'forest2.h', w: 128, h: 58, framePixels: 7424, frameCount: 20 },

  // Main character animation (original project art).
  { type: 'frames', name: 'sablinagif', file: 'sablinagif.h', w: 100, h: 100, framePixels: 10000 },

  // Original mini-scenes in firmware (all drawn at 128x104 window).
  { type: 'frames', name: 'cleangif', file: 'cleangif.h', w: 128, h: 104, framePixels: 13312 },
  { type: 'frames', name: 'eatgif', file: 'eatgif.h', w: 128, h: 104, framePixels: 13312 },
  { type: 'frames', name: 'gamegif', file: 'gamegif.h', w: 128, h: 104, framePixels: 13312 },
  { type: 'frames', name: 'gardengif', file: 'gardengif.h', w: 128, h: 104, framePixels: 13312 },
  { type: 'frames', name: 'sleepgif', file: 'sleepgif.h', w: 128, h: 104, framePixels: 13312 },
  { type: 'frames', name: 'colorgif', file: 'colorgif.h', w: 126, h: 83, framePixels: 10458 },
  { type: 'frames', name: 'gamewalk', file: 'gamewalk.h', w: 23, h: 35, framePixels: 805, frameCount: 4 },
];

function parseArray(headerText, symbol) {
  const re = new RegExp(`const\\s+unsigned\\s+short\\s+(?:PROGMEM\\s+)?${symbol}[^=]*?(?:PROGMEM\\s*)?=\\s*\\{([\\s\\S]*?)\\};`);
  const m = headerText.match(re);
  if (!m) throw new Error(`Could not find array for ${symbol}`);
  const bodyNoComments = m[1].replace(/\/\/.*$/gm, '');
  const vals = bodyNoComments.match(/0x[0-9a-fA-F]+/g) || [];
  return vals.map((v) => parseInt(v, 16));
}

function normalizeLength(data, expected) {
  const out = data.slice(0, expected);
  while (out.length < expected) out.push(0x0000);
  return out;
}

const sprites = {};
for (const d of defs) {
  const txt = fs.readFileSync(path.join(repo, d.file), 'utf8');
  const data = parseArray(txt, d.name);

  if (d.type === 'frames') {
    const frameCount = d.frameCount || Math.floor(data.length / d.framePixels);
    const frames = [];
    for (let i = 0; i < frameCount; i += 1) {
      const start = i * d.framePixels;
      const frameData = normalizeLength(data.slice(start, start + d.framePixels), d.framePixels);
      frames.push(frameData);
    }

    sprites[d.name] = {
      w: d.w,
      h: d.h,
      framePixels: d.framePixels,
      frameCount,
      frames,
    };
    continue;
  }

  const expected = d.w * d.h;
  sprites[d.name] = { w: d.w, h: d.h, data: normalizeLength(data, expected) };
}

const content =
  '// Auto-generated from original project headers (.h RGB565).\n' +
  '// Do not edit manually; run: node simulator/tools/extract_sprites.js\n\n' +
  `window.ORIGINAL_SPRITES = ${JSON.stringify(sprites)};\n`;

fs.writeFileSync(out, content, 'utf8');
console.log(`Generated ${out}`);
