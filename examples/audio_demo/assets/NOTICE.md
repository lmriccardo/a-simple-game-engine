# blip.ogg / ambient.ogg

Both synthetically generated for this demo (not third-party content), the
same way as `tests/support/audio/tone.ogg` -- see that file's NOTICE.md.

- `blip.ogg`: a bright ascending three-note arpeggio (C5-E5-G5), ~0.45s.
- `ambient.ogg`: a soft, low two-sine hum with a short fade in/out at each
  end so looping it doesn't click.

Regenerate with:

```python
import soundfile as sf
import numpy as np

sr = 22050

def note(freq, dur, sr):
    t = np.linspace(0, dur, int(sr * dur), endpoint=False)
    env = np.clip(np.minimum(t / 0.01, (dur - t) / 0.05), 0.0, 1.0)
    return 0.4 * np.sin(2 * np.pi * freq * t) * env

blip = np.concatenate([note(f, 0.15, sr) for f in (523.25, 659.25, 783.99)]).astype(np.float32)
sf.write("blip.ogg", blip, sr, format="OGG", subtype="VORBIS")

dur = 2.0
t = np.linspace(0, dur, int(sr * dur), endpoint=False)
hum = 0.15 * np.sin(2 * np.pi * 110.0 * t) + 0.1 * np.sin(2 * np.pi * 110.7 * t)
fade_len = int(sr * 0.05)
fade = np.ones_like(hum)
fade[:fade_len] = np.linspace(0, 1, fade_len)
fade[-fade_len:] = np.linspace(1, 0, fade_len)
sf.write("ambient.ogg", (hum * fade).astype(np.float32), sr, format="OGG", subtype="VORBIS")
```
