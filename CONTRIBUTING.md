# 🛠️ Contributing to Sablina Tamagotchi ESP32

Thank you for your interest in contributing! This document explains how to get involved, what we value in contributions, and what to expect from the process.

---

## Before You Start

- Read the [Code of Conduct](CODE_OF_CONDUCT.md) — all contributors are expected to follow it
- Check the [Security Policy](SECURITY.md) before reporting anything security-related — **do not open public issues for vulnerabilities**
- Review the [Disclaimer](DISCLAIMER.md) — contributions to the WiFi audit module must comply with applicable laws in your jurisdiction

---

## Ways to Contribute

| Type | How |
|---|---|
| 🐛 Bug reports | Open a GitHub Issue using the Bug Report template |
| 💡 Feature requests | Open a GitHub Issue using the Feature Request template |
| 🔧 Code fixes & features | Fork → branch → PR (see below) |
| 📖 Documentation | Edit Markdown files, improve README sections |
| 🎨 Sprites & art | Add sprite arrays (`.h` files) following the existing format |
| 🔒 Security issues | Use [GitHub Security Advisories](SECURITY.md) — **not** public issues |

---

## Development Setup

### Firmware (`SablinaTamagotchi_2.0/`)

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software) with the ESP32 board package (`espressif/arduino-esp32` ≥ 3.x)
2. Install required libraries via Library Manager:
   - `ArduinoJson` ≥ 7
   - `TFT_eSPI`
   - `NimBLE-Arduino`
3. Copy `SablinaTamagotchi_2.0/secrets.h.example` → `SablinaTamagotchi_2.0/secrets.h` and fill in your credentials — **never commit this file**
4. Select board: **ESP32S3 Dev Module** (or your specific board)
5. Build and upload

### Simulator (`simulator/`)

```bash
# Serve the simulator (Python 3)
cd /path/to/repo
python3 -m http.server 8080

# Optionally: start the WiFi scan server (requires nmcli)
node simulator/wifi-scan-server.js
```

Open `http://localhost:8080/simulator/` in your browser.

---

## Pull Request Guidelines

1. **Fork** the repository and create a branch from `main`:
   ```bash
   git checkout -b fix/short-description
   ```
2. Keep changes **focused** — one logical change per PR; avoid mixing unrelated fixes
3. **Test your changes:**
   - Firmware: compile cleanly with Arduino IDE (zero warnings preferred)
   - Simulator: verify the simulator loads and the changed feature works in-browser
4. If you are adding or changing firmware behaviour, update the relevant `*.md` docs
5. Ensure `secrets.h` is **not** included in your commits (it is gitignored)
6. Open a PR against `main` and fill in the PR template

### Commit style

Use concise imperative messages:
```
fix: correct chat_id validation in telegram_bot.h
feat: add PMKID capture to wifi_audit module
docs: update README architecture diagram
```

---

## Sprite / Asset Contributions

Sprites are C header files containing raw RGB565 bitmap arrays. To add a new sprite:

1. Convert your image to RGB565 format (use `tools/` scripts or an online converter)
2. Save as `SablinaTamagotchi_2.0/mysprite.h` following the existing naming and array format
3. Register the sprite in `SablinaTamagotchi_2.0/config.h` or the relevant include list

Do not include sprites sourced from copyrighted material without explicit permission from the rights holder.

---

## What We Will Not Accept

- Code that introduces hardcoded credentials or API keys
- Changes that remove safety guards from the WiFi audit module (e.g., removing the compile-time `FEATURE_WIFI_AUDIT` gate)
- AI-generated code submitted without review or testing
- Changes that break compilation on the target board (ESP32-S3)

---

## Questions?

Open a [GitHub Discussion](../../discussions) for general questions, ideas, or requests that don't fit the issue templates.
