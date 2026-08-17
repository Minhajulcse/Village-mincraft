# Audio credits

Every clip in this directory is derived from **public-domain** recordings hosted on
Wikimedia Commons. No attribution is legally required for public-domain works; the
sources are recorded here so the provenance stays checkable.

| Clip in `assets/` | Source file on Wikimedia Commons | Licence | Credited artist |
| --- | --- | --- | --- |
| `village_ambient.wav` | [Gentle breeze and birds singing.ogg](https://commons.wikimedia.org/wiki/File:Gentle_breeze_and_birds_singing.ogg) + [Birdsong mild sunny day.ogg](https://commons.wikimedia.org/wiki/File:Birdsong_mild_sunny_day.ogg) | Public domain | ezwa; stephan |
| `rooster.wav` | [Medium rooster crowing.ogg](https://commons.wikimedia.org/wiki/File:Medium_rooster_crowing.ogg) | Public domain | — |
| `chickens.wav` | [Chickens demanding food.ogg](https://commons.wikimedia.org/wiki/File:Chickens_demanding_food.ogg) | Public domain | alys |
| `hens.wav` | [Hens leaving coop.ogg](https://commons.wikimedia.org/wiki/File:Hens_leaving_coop.ogg) | Public domain | — |

## What was done to the originals

All processing was done with `ffmpeg`; the output format is 16-bit PCM WAV at 22050 Hz
(mono for the one-shots, stereo for the bed) so `paplay` / `aplay` / `afplay` / `PlaySound`
can all play it without a decoder.

* **`village_ambient.wav`** — the breeze recording mixed at full level with the birdsong at
  0.55 (`amix=inputs=2:duration=shortest:normalize=0`), trimmed to 32 s, then peak-normalised
  to −12 dBFS. To make it loop without a click, the first 4 s were crossfaded over the last
  4 s and concatenated with the 4–28 s middle, giving a seamless 28 s bed.
* **one-shots** — leading silence stripped
  (`silenceremove=start_periods=1:start_threshold=-45dB`), downmixed to mono, trimmed to
  4.5–5 s, given a 0.3 s fade-out, and peak-normalised to −9 dBFS so they sit above the bed.

Clips considered and rejected because they are CC BY-SA rather than public domain:
`Rooster crowing.ogg`, `Young rooster crowing.ogg`, `Single Cow Moo.ogg`, `Mudchute cow 1.ogg`.
