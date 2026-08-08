# Touch

The round screen is the only input surface. The board's touch task (`main/boards/waveshare/esp32-s3-touch-lcd-1.85c/`) turns raw touches into gestures.

## Gestures

| Gesture | Effect |
|---------|--------|
| Hold (≥450 ms, finger still) | Push-to-talk: records until the finger lifts |
| Tap | Sent to the server; a pending confirmation captures it as "accept" |
| Double tap | Sent to the server (mute used to live here; removed as a trap) |
| Swipe left/right | Cycle speech mode; the switch sound cues locally, the ring color follows from the server echo |

## Design notes

- The hold threshold sits just past the tap window (400 ms), so a press only becomes push-to-talk once it is too long to still be a tap; the finger has to stay put or it would hijack a slow swipe.
- Any sign of the user — touch, wake word, a turn starting — calls `Application::NoteUserActivity()`, which wakes the screen and restarts the inactivity countdown. After 60 s idle the backlight goes dark and the emote engine stops decoding frames (that is the part that costs CPU).
- A confirmation request arms next-gesture capture in the protocol: tap accepts, anything else declines, so nobody waits out the 30 s expiry.

## Navigation

Prev: [Face](face.md) · Next: [Sounds](sounds.md)
