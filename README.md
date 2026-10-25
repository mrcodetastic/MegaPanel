# MegaPanel

MegaPanel is the clean ESP32-Arduino LED matrix application built from the
reference projects in this repository. It uses GFX_Lite / GFX_Layer for the
framebuffer, a HAL for the physical panel, and exactly three drawable layers:

- **Background** — ambient patterns and atmosphere.
- **Foreground** — clocks, media and other main content.
- **System** — notifications, status and overlays.

The full architecture and porting plan is in
[`plans/panel-architecture.md`](plans/panel-architecture.md).

## Status

Phase 1 (framework), Phase 2 (content), Phase 3 (animated clocks, weather and
playlist) and Phase 4 (asset data, ticker integration and Canvas scenes) are
implemented and build clean.

### Phase 1 — framework

- Root PlatformIO ESP32-S3 project with OTA partitions and LittleFS.
- Vendored GFX_Lite under `lib/GFX_Lite`.
- Three-layer `PixelCanvas` compositor with layer alpha and transparency modes.
- Static output registry and pre-instantiated outputs.
- `start()` / `stop()` per-output resource ownership.
- HUB75 DMA reference backend and `NullPanel` fallback.
- Standard clock output and WiFi status overlay.
- PIR and LDR placeholder services with display brightness policy.
- ArduinoJson web API: status, config, scene, outputs, preview, display,
  sensors and reboot.
- Initial responsive web UI in `data/www`.
- WiFiManager provisioning with passwordless `MegaPanel-Setup` fallback.

### Phase 2 — content

Twenty-one outputs are registered and selectable from the UI:

| Category | IDs |
|---|---|
| Clock (foreground) | `clock.standard`, `clock.morph`, `clock.large`, `clock.words`, `clock.worldmap.cw`, `clock.mario.cw`, `clock.pacman.cw` |
| Background pattern | `pattern.starfield`, `pattern.plasma`, `pattern.rain`, `pattern.matrix`, `pattern.fire`, `pattern.wave`, `pattern.spiro`, `pattern.mandala`, `pattern.fireworks` |
| Media (foreground) | `media.ticker`, `media.pca`, `media.image` |
| System (overlay) | `sys.notify`, `sys.status` |

Phase 2 also adds:

- **Morph clock.** A generic glyph engine reduces any font to its lit points,
  resamples them by index and interpolates between them. Two glyph sources
  ship: a `GFXfont` adapter and a packed 1-bit-per-pixel source used by the
  `oled_morphing_clock` digit fonts.
- **Resolution-independent clocks.** `ClockLayout` picks five-across, two-row,
  single-row or a custom grid, so clocks adapt to 64x64, 78x78 and 128x64.
- **PCA animation player.** `media.pca` plays the `animatedpixelclock` PCA1
  format (4bpp, 12-byte header, per-frame delays) from `/anim/<name>.pca` and
  scales a legacy 128x64 animation to the active panel.
- **BMP image output.** `media.image` draws 24-bit BMP files from `/images`
  with stretch or letterbox fitting.
- **Notifications.** `NotificationService` drives the `sys.notify` marquee
  banner on the system layer. `/api/notify` accepts text, colour, icon,
  position and duration.
- **Schema-driven UI.** `/api/outputs` publishes each output's JSON config
  schema; the web UI renders it as live forms and saves through `/api/config`.

### Phase 3 — animated clocks, weather and playlist

Seventeen more outputs are registered:

| Category | IDs |
|---|---|
| Animated clock (foreground) | `clock.mario`, `clock.space`, `clock.pong`, `clock.pacman`, `clock.snake`, `clock.tetris`, `clock.asteroids`, `clock.dino`, `clock.matrix`, `clock.weather`, `clock.world`, `clock.bomberman`, `clock.tron` |
| Animated background | `ambient.stars`, `ambient.invaders`, `ambient.pacman_chase`, `ambient.custom_pca` |

Phase 3 also adds:

- **`AnimatedClock` base.** Owns the hour/minute values, which digit changed,
  the colon blink, the digit geometry derived from the real panel size, and the
  per-output POD state reserved in `start()` and released in `stop()`. Each
  clock keeps only its own scene state; there is no global clock state.
- **Weather.** `WeatherService` fetches current conditions and today's summary
  from Open-Meteo in a dedicated FreeRTOS task and publishes an immutable
  snapshot under a mutex, so the render loop never waits on the network. The
  snapshot is marked stale after three hours or when the last fetch failed, and
  `clock.weather` shows that state instead of pretending the numbers are fresh.
  Icons are drawn procedurally at 8, 16, 24 or 32 px, with animated rain, snow,
  fog, wind and lightning details.
- **World clock.** `clock.world` draws the clockwise2 world map as a dotted
  field and evaluates a POSIX TZ string per city (M-form DST rules only) without
  touching the process timezone, so each city keeps its own DST.
- **Playlist and transitions.** `PlaylistConfig` holds up to eight
  foreground/background/dwell entries. The playlist starts after a configurable
  idle period, advances on its own dwell timer, wraps, and either cuts or fades
  the foreground in over 450 ms. A manual scene change or PIR motion restarts
  the idle timer.
- **Web.** `GET /api/weather` reports the snapshot and fetch state,
  `POST /api/weather` requests an immediate refresh, `GET /api/scene` now
  includes the playlist and the current index, and the UI has weather and
  playlist cards.

### Phase 4 — assets, data, news and Canvas

Three more outputs and the data pipelines behind them:

- **`DataService`.** Up to four configurable HTTP/JSON sources. Each source has
  a URL, an optional label, dotted field paths for the name, price, change and
  change percent, and a refresh interval. Requests run in a dedicated FreeRTOS
  task with a 6 s timeout and a 32 KB response cap; results are published as
  immutable snapshots under a mutex. A failed fetch keeps the last good numbers
  and marks them stale; retries back off from 1 to 15 minutes.
- **`media.asset`.** The reusable asset display: name across the top, the price
  large in the middle, and the movement below in green, red or grey with an
  arrow. It works for crypto, equities, FX and anything else the mapping can
  describe. Price formatting adapts to the magnitude and drops thousands
  separators or the currency prefix rather than shrinking the digits on a
  64 px panel.
- **Ticker integration.** `media.ticker` gains `source` and `template`; the
  template expands `{name}`, `{price}`, `{change}` and `{percent}` from the
  selected source, so a scrolling line can show one asset or a mixed phrase.
  Setting `news` on the ticker scrolls the joined NewsService headlines
  horizontally instead, as an alternative to the vertical `media.news`.
- **`media.canvas`.** Frames of positioned primitives loaded from
  `/canvas/<name>.json` (`rows`, `text`, `rect`, `circle`), with per-frame
  durations and looping. This is the documented subset of the clockwise2
  Canvas idea; a demo scene ships in `data/canvas/demo.json`.
- **News feed and `media.news`.** `NewsService` polls one JSON endpoint in a
  worker task. The response can be an array of strings or an array of objects
  with a title field (`articles` + `title`, for example), matching the shape
  used by the RetroPanel ticker API. `media.news` word-wraps the headlines to
  the panel width and scrolls them upward smoothly, with an optional header row
  and bullet markers.
- **Web.** `GET /api/data` and `GET /api/news` report their snapshots,
  staleness and errors, `POST /api/data` and `POST /api/news` request an
  immediate refresh, and the UI has data-source and news cards.

PC metrics is descoped entirely. Lua remains optional and is not built: its
interpreter is not vendored in this repository, and the default board has no
PSRAM. See `plans/panel-architecture.md` for the phase map and compatibility
matrix.

Audio input, audio-driven visualisers, 3D (`fx3d`) and PC metrics are out of
scope for this revision.

## Build and flash

The PlatformIO Core used by the VS Code extension is usually at:

```text
%USERPROFILE%\.platformio\penv\Scripts\pio.exe
```

Build:

```powershell
"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run
```

Upload firmware:

```powershell
"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run -t upload
```

Upload the web UI and data files:

```powershell
"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" run -t uploadfs
```

Open the serial monitor:

```powershell
"%USERPROFILE%\.platformio\penv\Scripts\pio.exe" device monitor
```

The default environment is `esp32s3` with 8 MB flash. The stock
`esp32-s3-devkitc-1` board definition reports **no PSRAM**; the build still
works for 64x64. For 128x64 or media-heavy builds, select a board definition
with PSRAM or add a custom board JSON, then set
`board_build.arduino.memory_type` to `qio_opi` or `qio_qspi` to match the
module. Use `partitions_16mb.csv` for a 16 MB board.

## First boot

1. Flash firmware and filesystem.
2. Connect to the `MegaPanel-Setup` WiFi access point if the panel has no saved
   credentials.
3. Configure WiFi through the captive portal.
4. Open the panel IP shown on serial, or `http://megapanel.local/` if mDNS is
   available.

The default logical panel is 64x64. Configure the panel size, panel geometry,
rotation, driver and brightness from the Display section or the JSON config.

## Panel HAL

`src/hal/IMatrixPanel.hpp` is the boundary between the application and the
hardware. The included `Hub75DmaPanel` is a working reference backend for
`ESP32-HUB75-MatrixPanel-I2S-DMA`.

A backend owns:

- the logical framebuffer (`CRGB* framebuffer()`);
- panel mapping, chaining, rotation and driver setup;
- `show()` to push or flip the frame;
- brightness and scan timing.

Outputs never include a panel library.

## Output model

Every output is a compiled-in, pre-instantiated class implementing
`src/core/IOutput.hpp`:

```cpp
bool configure(const ScreenSpec& screen, JsonObjectConst config);
bool start(OutputContext& ctx);
void stop();
void render(PixelCanvas& target, const FrameContext& frame);
```

`start()` reserves the buffers that output needs. `stop()` releases them. The
`SceneManager` never destroys or reconstructs the output object.

To add an output:

1. Derive from `IOutput`.
2. Provide `metaStatic()`, `schemaStatic()`, `configure`, `start`, `stop` and
   `render`.
3. Add a static instance and one descriptor entry in
   `src/core/OutputRegistry.cpp`.
4. Use `Layout` / `ClockLayout` instead of hard-coded resolutions.

## Web API

| Method | Path | Purpose |
|---|---|---|
| GET | `/api/status` | Panel, WiFi, scene, sensors and diagnostics |
| GET | `/api/config` | Full configuration JSON |
| POST | `/api/config` | Apply and persist configuration |
| GET | `/api/outputs` | Output catalog and config schemas |
| GET | `/api/scene` | Current scene |
| POST | `/api/scene` | Change background, foreground and alpha |
| GET | `/api/preview` | Raw RGB888 framebuffer with `X-Width` and `X-Height` |
| POST | `/api/display` | Display on/off and brightness |
| POST | `/api/notify` | Show a notification banner (`text`, `icon`, `color`, `position`, `durationMs`) |
| GET | `/api/weather` | Weather snapshot, staleness and fetch state |
| POST | `/api/weather` | Request an immediate weather refresh |
| GET | `/api/data` | Every data source with price, movement, staleness and error |
| POST | `/api/data` | Request an immediate refresh of all sources |
| GET | `/api/news` | Headlines, staleness and fetch state |
| POST | `/api/news` | Request an immediate news refresh |
| GET | `/api/sensors` | PIR and LDR live values |
| POST | `/api/reboot` | Reboot the panel |

The UI polls `/api/status` and `/api/preview`; the render loop is never blocked
by a web request.

## Repository notes

- `clockwise2/`, `oled_morphing_clock/`, `reference/` and `RetroPanel_esp32/`
  are read-only reference material.
- Only `src/` and `lib/` are compiled by PlatformIO.
- Keep GFX_Lite's licence and attribution files when updating `lib/GFX_Lite`.
