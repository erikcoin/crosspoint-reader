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
