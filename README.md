# apollo-firmware

Custom firmware for **Apollo**, a personal desk agent living on a
[Waveshare ESP32-S3-Touch-LCD-1.85C](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85C)
(V2, round 360×360 touch display). Forked from
[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32); the server side lives at
[galfrevn/apollo](https://github.com/galfrevn/apollo) (Cloudflare Workers + Agents SDK).

The firmware adapts to Apollo's protocol, not the other way around: the server
speaks a small JSON-over-websocket dialect (see `main/protocols/apollo_protocol.cc`)
with raw PCM downlink for speech and raw PCM uplink for transcription.

## What's custom here

- **Apollo protocol** (`main/protocols/apollo_protocol.*`): hold/wake/gesture
  events up, `ui_state` / TTS / confirmations down. TTS streams as headerless
  PCM and skips the Opus decoder (`AudioStreamPacket.pcm`).
- **Animated face** on the emote engine, with Apollo's emotions mapped onto the
  emote vocabulary, plus an **accent ring** around the round display colored by
  the active speech mode (`main/display/emote_display.cc`).
- **Hold-to-talk** from the touchscreen, swipe to cycle speech modes, tap to
  answer confirmations. Idle screen sleep with wake on any user activity.
- **Wake word on the raw mic**: on this board the AFE-integrated WakeNet never
  fires, so WakeNet runs directly on the microphone channel inside
  `AfeAudioEngine::Feed()`.
- **UI sound effects** with random pitch variants (`main/assets/common/`,
  `main/assets/sound_variants.h`) so repeated effects never sound identical.

## Building

Requires ESP-IDF v6.0.2. The build script needs the manufacturer prefix, and it
prints `[ERROR]` but still exits 0 on failure — check the output, not the exit
code:

```sh
python scripts/build.py waveshare/esp32-s3-touch-lcd-1.85c
```

## Flashing

```sh
cd build
python -m esptool --chip esp32s3 -b 460800 --before default-reset \
  --after hard-reset write-flash "@flash_args"
```

**Never toggle DTR/RTS manually on the serial port**: driving DTR high while
releasing RTS pulls IO0 low and lands the chip in ROM download mode, silent to
both serial and esptool until physically replugged. Reset with esptool. Note
that merely opening the port resets the chip (USB-Serial-JTAG behavior), so any
serial capture starts with a fresh boot.

## Sound effects

Effects are Ogg/Opus, mono 16 kHz, 48 kbps, 60 ms frames, peak-normalized to
-20 dBFS. Convert new ones from mp3 with:

```sh
./scripts/convert_apollo_sounds.sh <mp3-dir>
```

The frequent effects (`mode_switch`, `listen_start`, `listen_end`,
`speech_done`) get pitch-shifted `_v2`/`_v3` siblings picked at random per
play. After adding a new `.ogg`, regenerate the language header (the CMake rule
does not depend on the sound files):

```sh
python3 scripts/gen_lang.py --language es-ES --output main/assets/lang_config.h
touch main/CMakeLists.txt   # re-run the configure-time asset glob
```

## Server configuration

Connection defaults are baked at build time (`CONFIG_APOLLO_URL`,
`CONFIG_APOLLO_TOKEN`, `CONFIG_APOLLO_DEVICE_ID` in `sdkconfig`) and can be
overridden per device via NVS namespace `apollo` without a rebuild.

## Upstream

Architecture notes for the codebase are in [AGENTS.md](AGENTS.md). Boards other
than the 1.85C are kept as-is from upstream to ease merging; only the Waveshare
1.85C path is maintained and tested here. Upstream reference docs live in
[docs/](docs/).
