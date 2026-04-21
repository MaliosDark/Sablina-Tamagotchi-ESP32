# BLE Peer Validation

Use this checklist with two ESP32 devices after flashing the updated firmware.

## Goal

Verify that nearby Sablinas:

1. Detect each other over BLE advertising.
2. Exchange real short peer messages device-to-device.
3. Render the two-speaker popup without freezing or dropping input.

## Setup

1. Flash the same Arduino firmware to two boards.
2. Set both units near each other, ideally within 1 to 2 meters.
3. Boot both devices and wait at least 20 seconds.

## Expected Detection

1. The sidebar should switch from `Peer scan` to a peer name like `Sablina-XXXX`.
2. RSSI should improve when both devices are moved closer together.
3. If one device is powered off, the peer should disappear within about 16 seconds.

## Expected Message Exchange

1. On first detection, one unit should advertise a short offer line.
2. The other unit should answer with a short reply line.
3. Both units should eventually show a dual-speaker popup with different local and peer lines.
4. After the popup closes, another exchange should happen only after the configured chat interval.

## Behavior Checks

1. Lower hunger on one device and confirm that the outgoing line changes toward snack or food language.
2. Lower rest and confirm that the outgoing line changes toward a slow walk or rest tone.
3. Lower clean and confirm that the outgoing line changes toward cleaning or bath language.
4. Bring the devices very close and confirm that closeness lines can appear.

## Stability Checks

1. Leave both devices running together for 10 minutes.
2. Confirm that the UI keeps updating and buttons still respond.
3. Confirm that BLE app connectivity still works if your mobile app connects while peer exchange is active.

## IDF Check

If you are testing the IDF branch too:

1. Flash the IDF build to both boards.
2. Confirm that a peer text popup appears when the other unit emits a room or thought status.
3. Confirm that the peer name remains visible only while the other device is still advertising nearby.