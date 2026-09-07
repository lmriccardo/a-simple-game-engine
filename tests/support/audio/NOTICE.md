# tone.ogg

A 50ms, 8kHz mono 440Hz sine tone, synthetically generated for this repo
(not third-party content) to exercise `AudioClip::Load`'s real Ogg Vorbis
decode path -- unlike the hand-built WAV bytes used elsewhere in this suite,
Vorbis's codebook/packet framing isn't practical to hand-construct, the same
reason `tests/support/fonts/Ahem.ttf` exists for TrueType.

Regenerate with:

```python
import soundfile as sf, numpy as np
sr = 8000
t = np.linspace(0, 0.05, int(sr * 0.05), endpoint=False)
tone = (0.5 * np.sin(2 * np.pi * 440.0 * t)).astype(np.float32)
sf.write("tone.ogg", tone, sr, format="OGG", subtype="VORBIS")
```
