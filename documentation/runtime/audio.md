# Audio

Everything audio goes through `main/audio/audio_service.*`: capture, wake word, encode/decode queues, and playback.

## Uplink (mic → server)

- Capture at 16 kHz mono. While a hold or wake session is open, frames ship to the server as raw PCM binary websocket frames.
- **Wake word runs on the raw mic channel** inside `AfeAudioEngine::Feed()`. On this board the AFE-integrated WakeNet never detects — verified across a long bisection — so WakeNet gets the microphone directly. Consequence: the raw path bypasses AEC, so barge-in over the device's own speech is untested.

## Downlink (server → speaker)

Two payload kinds share the decode queue, distinguished by `AudioStreamPacket.pcm`:

| Kind | Source | Path |
|------|--------|------|
| `pcm = true` | Apollo TTS | Copied straight to the playback queue (headerless little-endian PCM) |
| `pcm = false` | Local sound effects | Ogg demuxer → Opus decoder |

Both resample to the codec's output rate (24 kHz on this board) when needed.

## Design notes

- The `pcm` flag exists because a build flag can't make this call: the queue carries both kinds at once. Before it existed, Opus effect packets were played as raw PCM — loud radio static at a volume unrelated to the file.
- `tts_start` advertises the run's byte total; the device closes the run when the bytes arrive, or on `tts_aborted` when they never will.

## Navigation

Prev: [Protocol](protocol.md) · Next: [Face](face.md)
