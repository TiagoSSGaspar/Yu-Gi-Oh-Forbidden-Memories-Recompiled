# Dialogue voices

Status, 2026-10-12: **paused.** The engine side works and is tested; the
first voice pack was made and played through, but it is not published, and
the work has not been proposed upstream.

## What is in the tree

- The mod field, `"audio": {"voice": {...}}`, and how a page's key is found:
  [modding.md, Voices](modding.md#voices).
- The runtime: [`src/pc/text/voice.c`](../src/pc/text/voice.c). It tracks
  the page each text box shows, starts the page's voice at its first letter,
  and stops it when the page is confirmed, the box closes or Square is held.
- Playback: `AudioReplace_VoicePage` in
  [`src/pc/audio/replace.c`](../src/pc/audio/replace.c), on a channel of its
  own on the sound effects' bus. A file is only checked when the mod is
  applied and decoded when its page shows.
- Three hooks in game C, all inside `#ifdef MEMORIES_PC`:
  `TextBox_BuildStep` (a text starts; a letter is drawn),
  `Text_CompletePageAdvance` (a page is confirmed) and `TextBox_Destroy`
  (the box closes). Removing the `MEMORIES_PC` blocks with
  `unifdef -UMEMORIES_PC` leaves those three files byte-identical to before,
  so the PS1 build compiles the same source. A full `make match` was not run.
- `"voice"` is an overlap key in `src/pc/mods/overlap.c` and the FM Editor's
  `overlaps.py`; the unit test is `test_voices` in
  `tests/pc/audio_replace_test.c`.

Checked on 2026-10-12: macOS smoke 13 MATCH and 2 SKIP (the two cases that
need test hooks the ARM64 build lacks); Linux and Windows build,
`check_layouts` finds no structure that differs, CTest 134/134; played on
macOS from a new game and from a save.

## The first pack: voices-en

Not in the repository: it is generated audio, and the scripts that made it
are not either. How it was made, so it can be made again:

1. **Script.** The US disc's dialogue, listed with `tools/pc/text_listing.py`,
   keeping strings from `0x500` up (the campaign). A text's pages follow its
   `{jump}` and `{call}`, and each page is keyed by where it starts, as the
   mod field expects.
2. **Speakers.** All 1163 campaign pages were labelled by who speaks. The
   F6 portrait is not enough: a page without one keeps the last face shown,
   so labels went by what the page says, with the portraits as a check.
   Portraits identified from the bank in
   [dialog-portrait-bank.md](dialog-portrait-bank.md): `0x41`/`0x48` Simon
   Muran, `0x42`/`0x4B` Jono, `0x43` Teana, `0x44` Seto, `0x46` Heishin,
   `0x49`/`0x4A` the Prince, `0x4C` Shadi, `0x4D` Sadin, `0x4E`..`0x52` the
   five High Mages, `0x55` Bakura, `0x56` Pegasus, `0x57` Isis, `0x58`
   Kaiba, `0x59` DarkNite.
3. **Voices.** [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M) v1.0
   (Apache-2.0), run offline through `kokoro-onnx`. Each character has one
   Kokoro voice, or two blended half and half, and a speed. No real person's
   voice is cloned.
4. **Cuts.** Laughs and grunts ("Hahaha", "Heheh", "Ho ho", "Hmph",
   "Hmmm") come out spelled, so they are removed and the words around them
   kept; a page of nothing else has no voice.

Version 0.4: 1137 voiced pages, about 46 minutes, 24 MB of Ogg Vorbis.

| Character | Kokoro voices | Speed | Pages |
|---|---|---:|---:|
| Jono | `am_fenrir` | 1.05 | 173 |
| Seto | `am_michael` | 0.95 | 125 |
| Simon Muran | `bm_george` | 1.0 | 104 |
| Teana | `af_heart` | 1.0 | 93 |
| Villager Man | `am_liam` | 1.0 | 75 |
| Prince | `am_puck` | 1.0 | 74 |
| Mage | `am_liam` + `bm_daniel` | 0.95 | 70 |
| Heishin | `bm_lewis` | 0.9 | 68 |
| Sadin | `am_onyx` + `bm_daniel` | 0.9 | 53 |
| Child | `af_sky` | 1.1 | 43 |
| Old Man (the card trader) | `bm_daniel` | 0.85 | 37 |
| DarkNite | `am_onyx` + `bm_lewis` | 0.82 | 35 |
| Shadi | `am_echo` | 0.9 | 29 |
| Kaiba | `am_onyx` | 0.95 | 12 |
| High Mage Anubisius | `bm_lewis` + `am_echo` | 0.88 | 12 |
| Bakura | `bm_daniel` | 0.95 | 11 |
| Guard | `am_adam` | 1.0 | 11 |
| High Mage Secmeton | `bm_george` + `am_onyx` | 0.92 | 10 |
| High Mage Atenza | `am_eric` | 0.95 | 10 |
| High Mage Martis | `am_michael` + `bm_daniel` | 0.95 | 10 |
| High Mage Kepura | `am_santa` | 1.0 | 10 |
| Mage Soldier | `am_eric` + `am_adam` | 1.0 | 9 |
| Guardian Sebek | `am_adam` | 0.9 | 9 |
| Guardian Neku | `am_adam` + `bm_lewis` | 0.9 | 7 |
| Pegasus | `bm_fable` | 1.0 | 6 |
| Isis | `bf_emma` | 0.95 | 6 |
| Rex Raptor | `am_liam` | 1.1 | 6 |
| Weevil | `bm_daniel` | 1.15 | 6 |
| Mai Valentine | `af_bella` | 1.0 | 6 |
| Bandit Keith | `am_fenrir` + `am_onyx` | 0.95 | 6 |
| Narrator | `af_kore` | 1.0 | 6 |
| Villager Woman | `af_sarah` | 1.0 | 5 |

British voices (`b*`) speak with the `en-gb` lexicon, the others `en-us`.

Two speakers share the old man's lines and are easy to mix up: Sadin, the
guardian of the Valley of the Kings, owns strings `0x0575`..`0x0578` and
`0x058A` and calls the hero "my prince"; the card trader, in the village and
later underground, owns strings between `0x0512` and `0x0532` and `0x05DD`/`0x05DE` and says
"boy". `0x05D8` and `0x05F9` say neither and were left with the trader.

## Findings along the way

- `MEMORIES_INPUT` takes the pad's raw bits (Square `0x8000`, Cross
  `0x4000`, Start `0x0008`); the game's `PAD_BUTTON_*` constants are
  byte-swapped from those (Square is `0x80` there, and raw `0x0080` is Left).
- Only the disc's own text has a place in the dialogue bank, so translated
  text, `"text"` mods and Game > Language's European text are silent.

## Tried and set aside

- **Chatterbox:** its output carries an imperceptible watermark.
- **Orpheus 3B:** much more expressive than Kokoro (it takes `<laugh>` and
  `<sigh>` tags), but it has few voices, too few for one per character, and
  no official Portuguese model.
- **The anime's voices:** cloning them would need the actors' consent and the
  rights to their recordings, so the pack uses designed voices only.

## Open

- Propose the engine side upstream.
- Decide where the pack is published (a release asset, not the repository),
  and whether its generator belongs under `tools/` so others can rebuild it.
- Orpheus, or another expressive model, only for the most emotional lines,
  if a voice close enough to each character's Kokoro voice can be found.
- Voices for translated text would need keys other than disc offsets.
