# Home Assistant remote

Settings → System → **Home Assistant** opens a list of your Home Assistant
entities. Tap a row to toggle that entity; the row then shows the state Home
Assistant reports (On, Off, Playing, ...). Leaving the screen drops Wi-Fi and
restarts to Home, like the other network screens.

## Configuration

Put `homeassistant.json` in the **root of the SD card**. The first time you open
the screen without one, the firmware creates it with a template; add your token
and open the screen again.

```json
{
  "url": "http://homeassistant.local:8123",
  "token": "PASTE_LONG_LIVED_ACCESS_TOKEN_HERE",
  "entities": [
    { "name": "Bibliotheeklamp", "entity_id": "light.bibliotheeklamp" },
    { "name": "Keukentafellamp", "entity_id": "light.keukentafellamp" },
    { "name": "Soundbar", "entity_id": "media_player.jbl_bar_9_1_3818" },
    { "name": "Staande lampen", "entity_id": "light.staandelampen" }
  ]
}
```

- `url`: origin of Home Assistant, `http://` or `https://`, no path. A
  `.local` name is looked up over mDNS; if that fails, use the IP address.
- `token`: Home Assistant → your profile → Security → **Long-lived access
  tokens** → Create token. Keep this file private; the token is stored in plain
  text.
- `entities`: up to 8 rows, in display order. `name` is optional and defaults to
  the entity id.

## How it talks to Home Assistant

- On open: `GET /api/states/<entity_id>` for each row, to show the current state.
- On tap: `POST /api/services/<domain>/toggle` with `{"entity_id": "..."}`, where
  `<domain>` is the part of the entity id before the dot (`light`,
  `media_player`, `switch`, ...). Any entity whose domain has a `toggle` service
  works.
- All requests send `Authorization: Bearer <token>`.

## Code

- `src/activities/network/HomeAssistantActivity.*`: the screen (a `CatalogActivity`, so Wi-Fi
  connection, error/retry and teardown match the OPDS browser).
- `src/network/HomeAssistantClient.*`: the REST calls and mDNS lookup.
- `src/util/HomeAssistantUtil.h`: pure helpers, covered by `test/home_assistant_util`.

## Sleep screen

Before deep sleep the screen shows a white page with an icon and a text under
it. Change the text in Settings → Display → **Sleep Screen Text** (default
"Press a button to wake me up").

To use another icon, for example one from [Pictogrammers MDI](https://pictogrammers.com/library/mdi/)
(download it as a PNG, black on transparent), save it on the SD card as
`/sleep-icon.png` (or `/sleep-icon.bmp`). It is scaled down to fit 128 px.
Without that file the moon icon is used.

## Waking from deep sleep

On the X4 Pro the device sleeps in deep sleep and any hardware button (Power,
Up, Down) wakes it. Touch does not wake it, because the touch controller is
powered off in sleep to save battery.

## Bluetooth link (no Wi-Fi)

Settings → System → **Home Assistant Link** chooses how the screen talks to
Home Assistant: **Wi-Fi** (above) or **Bluetooth**. Bluetooth saves battery
because nothing connects: a tap broadcasts a short BLE message and Home
Assistant acts on it.

- The Bluetooth radio is off until you tap a row. A tap starts it, sends for
  about 3 seconds, then shuts it down again.
- Only the touch screen sends messages. The hardware buttons never send
  anything.
- It is **send-only**. The rows show "Sent", not the real On/Off state, and the
  url and token are not used. Only the first 4 `entities` are used, in order:
  row 1 is Button 1 in Home Assistant, row 2 is Button 2, and so on.
- The message is a BTHome v2 button event (unencrypted), so anyone in radio range
  who knows the format could send the same event. Keep that in mind before
  using it for anything sensitive.
- Needs a firmware built with Bluetooth: the `x4pro` env has it
  (`-DCROSSPOINT_HA_BLE=1` plus the `NimBLE-Arduino` library). For another
  ESP32-S3 env, add the same flag and `h2zero/NimBLE-Arduino@^2.3.8` to its
  `lib_deps`. Builds without it show an error and need the Wi-Fi link.

### Home Assistant setup

1. Home Assistant needs a Bluetooth adapter, or an
   [ESPHome Bluetooth proxy](https://esphome.io/components/bluetooth_proxy.html)
   near the reader.
2. Open the screen on the reader and tap a row once. Home Assistant should
   offer a new **BTHome** device named "CrossPoint": accept it (no bind key).
3. The device gets four event entities, Button 1 to Button 4. Create one
   automation per button, for example:

```yaml
automation:
  - alias: Reader button 1 - bibliotheeklamp
    triggers:
      - trigger: state
        entity_id: event.crosspoint_button_1
    conditions:
      - condition: template
        value_template: "{{ trigger.to_state.state not in ['unavailable', 'unknown'] }}"
    actions:
      - action: light.toggle
        target:
          entity_id: light.bibliotheeklamp
```

   Use `media_player.toggle` with `media_player.jbl_bar_9_1_3818` for the
   soundbar. The exact entity ids depend on how Home Assistant names the device.

### Code

- `src/network/BleButtonBeacon.*`: the BLE sender (NimBLE, advertising only).
- `src/util/BthomeUtil.h`: builds the message bytes; covered by `test/bthome_util`.

## USB drive (SD card in Explorer)

By default the USB port is a serial (COM) port. Settings → System →
**USB Drive** switches it to a USB drive: connect the cable and the SD card
opens as a drive in Explorer. Eject the drive or unplug the cable to go back.
This is the way to copy `homeassistant.json` or a sleep icon to the card.
