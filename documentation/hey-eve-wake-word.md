# Hey Eve wake-word training

The committed `main/assets/wakewords/hey_eve.tflite` is trained locally for
the phrase "hey eve", pronounced “jei iv”. Detection is on-device; no ambient
audio is sent to a server before a wake or display hold.

## Reproduce

- Trainer: TaterTotterson/microWakeWord-Trainer-AppleSilicon at
  `60abc9a2f92ea1f048e50684d7909b11c154435e`.
- Base engine: TaterTotterson/micro-wake-word at
  `97a4053537e749ba7eb4d32711adcbe1d9f9a653`.
- Host: Apple Silicon, Python 3.11, TensorFlow macOS 2.16.2, TensorFlow Metal
  1.2.0.
- Workspace: `/Users/fernando/Documents/ChatGPT/Eve/training/hey-eve`.
- Command: `MWW_ARTIFACT_SLUG=hey_eve ./train_microwakeword_macos.sh "hey eve"`
  `5000 100 --language en --english-accent mixed --tts-mode modern`
  `--tts-voice-count 64`.

Before the command, pre-clone the base engine at the revision above into
`data/micro-wake-word` and set its `origin` to `.`. The trainer then sees the
existing checkout and its best-effort update remains pinned locally.

Training data, caches, generated speech, environments, and checkpoints are not
versioned. The initial model uses no personal recordings. Add consented
`personal_samples/*.wav` only for a later retraining caused by repeated physical
test misses.
