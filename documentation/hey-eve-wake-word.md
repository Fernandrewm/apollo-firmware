# Hey Eve wake-word training

The committed `main/assets/wakewords/hey_eve.tflite` is trained locally for
the phrase "hey eve", pronounced “jei iv”. Detection is on-device; no ambient
audio is sent to a server before a wake or display hold.

## Reproduce

- Trainer: TaterTotterson/microWakeWord-Trainer-AppleSilicon at
  `60abc9a2f92ea1f048e50684d7909b11c154435e`.
- Base engine: FutureProofHomes/microWakeWord at
  `95f16d5951eb97eb8a4047b1042ca6e15b854dda`.
- Host: Apple Silicon, Python 3.11, TensorFlow macOS 2.16.2, TensorFlow Metal
  1.2.0.
- Workspace: `/Users/fernando/Documents/ChatGPT/Eve/training/hey-eve`.
- Command: `MWW_ARTIFACT_SLUG=hey_eve ./train_microwakeword_macos.sh "hey eve"`
  `5000 100 --language en --english-accent mixed --tts-mode modern`
  `--tts-voice-count 64`.

Training data, caches, generated speech, environments, and checkpoints are not
versioned. The initial model uses no personal recordings. Add consented
`personal_samples/*.wav` only for a later retraining caused by repeated physical
test misses.
