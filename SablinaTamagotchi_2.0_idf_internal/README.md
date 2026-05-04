# SablinaTamagotchi 2.0 IDF Variant

ESP-IDF firmware variant that uses the real DaveBben local model inference path (`stories260K.bin` + `tok512.bin`) and wires output events to:

- Floating popup event hook
- Beep event hook
- Vibration pulse queue
- BLE notify bridge API

## Included assets

- `data/stories260K.bin`
- `data/tok512.bin`

## Build

```bash
cd SablinaTamagotchi_2.0_idf
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Runtime behavior

- The `llm_task` runs local generation periodically.
- Generated token stream is captured through `generate_with_callbacks(...)`.
- Final generated text is sent to:
  - popup (`sablina_ui_show_floating_popup`)
  - beep queue (`sablina_audio_beep_once`)
  - vibration queue (`process_sound_haptic`)
  - BLE bridge (`ble_bridge_notify_text`)

## BLE status

- `main/ble_bridge.c` is currently a functional bridge stub API (logs + integration points).
- Replace it with NimBLE/Bludroid GATT implementation while keeping same API:
  - `ble_bridge_init()`
  - `ble_bridge_notify_text(const char*)`

## UI/Sound hooks

`main/app_main.c` exposes weak hooks for board-specific drivers:

- `sablina_ui_show_floating_popup(const char*, uint32_t)`
- `sablina_audio_beep_once(void)`

Override these in another source file to bind TFT popup rendering and amplifier tone output.
