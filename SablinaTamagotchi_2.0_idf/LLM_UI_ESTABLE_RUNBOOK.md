# Runbook: Stable UI + Local LLM (ESP32-S3)

This document records exactly what was implemented to avoid repeating the cycle of breaking UI while enabling a local LLM.

## 1) Final Objective

Keep the stable Arduino-bridge UI firmware and enable local inference using the model stored in SPIFFS.

Expected on-device result:
- Normal UI behavior (same as stable mode).
- Local LLM loaded from SPIFFS.
- Runtime local generation visible in logs with tok/s.

## 2) Original Problem

There were two competing modes:
- Stable UI mode (Arduino bridge) with correct rendering.
- Internal IDF mode (native app_main.c) with working local LLM but different UX/UI behavior.

Switching via build flags caused either visual regressions or dependency conflicts.

## 3) Applied Strategy

Two layers were used:

1. Physical project separation for internal experiments:
- Separate internal project: SablinaTamagotchi_2.0_idf_internal
- Used to test pure internal LLM behavior without touching stable firmware.

2. Local LLM integration inside stable firmware:
- Added a C bridge (internal_llm_bridge) to the stable build.
- The offline personality engine (llm_personality.h) uses this bridge to generate local text when remote endpoint flow is not used.
- UI/main flow was not replaced.

## 4) Modified Files

### 4.1 Stable build system (add local bridge)
File: main/CMakeLists.txt

Key changes in stable branch (else):
- Added sources:
  - internal_llm_bridge.c
  - llm.c
- Kept arduino_entry.cpp + sablina_sketch.cpp to preserve stable UI behavior.

### 4.2 Local LLM bridge
New files:
- main/internal_llm_bridge.h
- main/internal_llm_bridge.c

Responsibilities:
- Initialize transformer/tokenizer/sampler once.
- Expose minimal API:
  - internal_llm_begin()
  - internal_llm_is_ready()
  - internal_llm_generate(...)

Asset path candidates (important):
- /spiffs/data/stories260K.bin
- /spiffs/stories260K.bin
- /data/stories260K.bin
- /stories260K.bin
- /spiffs/data/tok512.bin
- /spiffs/tok512.bin
- /data/tok512.bin
- /tok512.bin

Reason:
- In Arduino mode, SPIFFS commonly mounts at /spiffs.
- In pure IDF flow, assets may appear under /data.

### 4.3 Integration with existing personality engine
File: ../SablinaTamagotchi_2.0/llm_personality.h

Key changes:
- Conditional include of internal_llm_bridge.h via __has_include.
- In begin(): initialize local bridge (internal_llm_begin).
- In offline paths:
  - _offlineAutonomousThought(...)
  - _offlineReact(...)
  first try _tryInternalThought(...).
- If local bridge fails/not ready, fallback remains the previous offline phrase engine.

This preserves UI responsiveness and keeps the system resilient.

## 5) Build and Flash Flow Used

From SablinaTamagotchi_2.0_idf:

```bash
. ~/esp/esp-idf/export.sh > /dev/null 2>&1
idf.py build
idf.py -p /dev/ttyACM0 flash
```

## 6) Log Verification (Success Criteria)

Serial logs should contain lines equivalent to:

- INT_LLM_BRIDGE: loading checkpoint=/spiffs/stories260K.bin tokenizer=/spiffs/tok512.bin
- INT_LLM_BRIDGE: internal llm ready vocab=512 seq=256
- achieved tok/s: <number>
- INT_LLM_BRIDGE: generate done <number> tok/s

If these lines appear, stable UI is running and local LLM is active.

## 7) Known Errors and Fixes

### 7.1 "internal assets not found"
Cause:
- Bridge searched /data/* while the runtime mounted assets at /spiffs/*.

Fix:
- Added /spiffs/... and /spiffs/data/... candidates in internal_llm_bridge.c.

### 7.2 Internal build instability when mixing profiles
Cause:
- Native + Arduino sources mixed in one target without enough isolation.

Fix:
- Keep the internal project separate for aggressive experiments.
- Keep the stable project as the main UI path.

## 8) Operational Rule to Avoid Breaking UI Again

- Do not replace arduino_entry.cpp/sablina_sketch.cpp in stable profile.
- Integrate new features as non-intrusive bridges (like internal_llm_bridge).
- Any internal UI/RTOS experiments go only into SablinaTamagotchi_2.0_idf_internal.

## 9) Recommended Switching Commands

Stable mode (UI + integrated local LLM):

```bash
cd /home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf
. ~/esp/esp-idf/export.sh > /dev/null 2>&1
idf.py -p /dev/ttyACM0 flash
```

Pure internal mode (test sandbox):

```bash
cd /home/nexland/Mikuru_Tamagotchi_ESP32/SablinaTamagotchi_2.0_idf_internal
. ~/esp/esp-idf/export.sh > /dev/null 2>&1
idf.py -p /dev/ttyACM0 flash
```

## 10) Current State

A stable path is now documented to:
- preserve UI,
- keep local LLM active,
- and avoid regressions caused by profile mixing.
