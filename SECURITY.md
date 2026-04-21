# 🔒 Security Policy

This document describes the security policy for the **Sablina / Sablina Tamagotchi ESP32** project, including how to report vulnerabilities, what is in scope, and how disclosures are handled.

---

## Supported Versions

| Branch / Version | Supported |
|---|---|
| `SablinaTamagotchi_2.0` (current) | ✅ Yes |

Only `SablinaTamagotchi_2.0` is maintained. Security fixes are applied to this branch only.

---

## Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.** Public issues expose the vulnerability to all users before a fix is available.

Instead:

1. Navigate to the **Security** tab of this repository
2. Click **"Report a vulnerability"** (GitHub Security Advisories, private disclosure)
3. Fill in the advisory form with as much detail as possible (see template below)

If GitHub's private advisory system is unavailable, contact the maintainer directly via the email listed in their GitHub profile with the subject line `[Sablina Security] <brief description>`.

**Expected response time:** within 7 days for acknowledgement, within 30 days for a resolution or mitigation plan.

---

## Vulnerability Report Template

```
Summary
-------
A short description of the vulnerability (1–3 sentences).

Affected component
------------------
File(s), function(s), or module(s) affected.

Reproduction steps
------------------
1. ...
2. ...
3. ...

Impact
------
What could an attacker achieve by exploiting this?

Suggested fix (optional)
------------------------
Any ideas for remediation.

Environment
-----------
- Firmware version / commit hash
- Hardware (ESP32-S3 DevKit / custom board / simulator)
- OS / toolchain version
```

---

## Scope

### In Scope

The following are considered valid security vulnerabilities for this project:

| Area | Examples |
|---|---|
| **Firmware, credential handling** | Hardcoded secrets leaking into build artefacts, NVS keys readable over JTAG/UART without authentication |
| **Firmware, network stack** | Buffer overflows in HTTP/HTTPS client code, lack of TLS certificate validation where it matters, SSRF-like behaviour in LLM endpoint handling |
| **Firmware, Telegram bot** | Unauthorised command execution (missing `chat_id` validation allows a third party to control the device), injection via crafted Telegram messages |
| **Firmware, WiFi audit module** | Unintended triggering of offensive features without user action, deauth/handshake capture running when `FEATURE_WIFI_AUDIT 0` is set |
| **Simulator (web)** | XSS in the browser simulator that could execute arbitrary JS in a user's browser, path traversal in the Python HTTP server |
| **Build system / supply chain** | Malicious code in vendored libraries, compromised dependency |

### Out of Scope

The following are **not** considered vulnerabilities for this project:

- Features that are **intentionally offensive by design** (deauth, handshake capture, PMKID), these are documented and require explicit user enablement
- Physical access attacks against a device you own (JTAG, UART flashing), physical security is the operator's responsibility
- Denial-of-service against the Telegram API or the LLM backend, those are third-party services
- Vulnerabilities in Arduino/ESP-IDF core, Espressif SDK, or third-party libraries (report those upstream)
- Social engineering
- Issues only reproducible on unreleased / modified forks

---

## Secrets & Credentials

### `secrets.h`, Never Committed

The file `SablinaTamagotchi_2.0/secrets.h` contains API keys and bot tokens. It is listed in `.gitignore` and **must never be committed** to any branch or fork.

```
SablinaTamagotchi_2.0/secrets.h   ← gitignored
secrets.h                          ← gitignored (any location)
```

If you discover that credentials have been accidentally committed to a public fork:

1. **Rotate the credentials immediately** (revoke the Telegram bot token via @BotFather, revoke the API key in your LLM provider dashboard)
2. Use `git filter-repo` or BFG Repo Cleaner to purge the commit history
3. Force-push the cleaned history

> GitHub also has an automated secret scanning service that will alert you if a known API key pattern is pushed to a public repository.

### NVS Storage

Credentials stored in ESP32 NVS (Non-Volatile Storage) are written in plaintext to flash. NVS is not encrypted by default. To enable NVS encryption:

```
# ESP-IDF, generate NVS encryption keys
idf.py menuconfig → Security features → Enable NVS encryption
```

For the Arduino variant, consider storing only a hash or truncated token in NVS and requiring the full token to be set over BLE/UART on first boot.

---

## WiFi Audit Module, Security Hardening Notes

The WiFi audit module (`wifi_audit.h`) is a **purposely offensive component**. The following hardening measures are built in:

| Measure | Implementation |
|---|---|
| **Compile-time disable** | `#define FEATURE_WIFI_AUDIT 0` removes all audit code at compile time |
| **Manual activation required** | No audit action runs automatically; all attacks require explicit user input (button press or Telegram command) |
| **Single-target scope** | Deauth and handshake capture are scoped to a single BSSID selected by the user, not broadcast-based |
| **Stop command** | All active audit operations can be halted instantly via the Stop button or `/stop` Telegram command |

If you believe any of these measures can be bypassed, that is a valid in-scope vulnerability.

---

## Disclosure Policy

This project follows a **coordinated disclosure** model:

1. Reporter submits a private advisory
2. Maintainer acknowledges within **7 days**
3. Maintainer investigates and produces a fix or mitigation within **30 days** (or agrees a longer timeline with the reporter for complex issues)
4. Fix is released; reporter is credited in the release notes (unless they request anonymity)
5. After the fix is publicly available, the reporter may publish their own write-up

We ask reporters to **not publicly disclose** until a fix is available, or until 90 days have passed from the initial report (whichever comes first), following the industry-standard 90-day responsible disclosure window.

---

## Legal Safe Harbour

Good-faith security research on this project is welcomed. The maintainers will not pursue legal action against researchers who:

- Report vulnerabilities through private channels as described above
- Do not access, modify, or exfiltrate data belonging to third parties in the course of their research
- Do not use the vulnerability to disrupt services or harm users
- Comply with all applicable laws in their jurisdiction

This safe harbour does not extend to misuse of the WiFi audit module against third-party networks, see [DISCLAIMER.md](DISCLAIMER.md).

---

## Attribution

Vulnerability reporters who consent to being named will be credited in the release notes and in a **Hall of Fame** section that will be added to this document as reports are resolved.

| Reporter | Vulnerability | CVE / Advisory | Date |
|---|---|---|---|
| *(none yet)* |, |, |, |

---

*Last updated: April 2026*
