# Extra hand plays

An optional API 4 code mod for SLUS_01411. Enable **Extra hand plays** in the
Mods window. It ships disabled; with it off, or with **Hand plays per turn**
at 1, turns run as on the disc.

After a card is played from the hand, the hand comes back up for another
play, up to the configured number of plays per turn. A fusion, an equip or a
magic card used from the hand counts as one play, as it does in the original
game. The field phase (attacks, position changes, activating set cards)
starts after the last play.

## Options

| Setting | Behavior |
|---|---|
| Hand plays per turn | 1–5, default 2. 1 is the original game. |
| Applies to | Both sides, the player only, or the CPU only. |
| Show skip hint | While an extra hand is up for the player, a line says that START skips it. |

## Rules

- The first play of a turn is required, as in the original game. In an
  extra hand the player may press **START** to skip the remaining plays
  and go to the field. START does nothing in the first hand, and does
  nothing while cards are marked for a fusion.
- No cards are drawn between plays. The remaining cards are moved to the
  left of the hand, as the draw does at a turn start.
- The hand does not come back when it is empty, or once either side is out
  of life points.
- The Swords of Revealing Light countdown, the turn count, monster "draw"
  effects and the per-turn used marks happen once per turn, not once per
  hand.
- The CPU plays every extra hand it is given, through its own hand script
  (or AI Hard Mode's planner). In an extra hand it chooses only from the
  cards it holds. The original script reads the next deck cards from
  snapshot slot 16, which is only correct with five cards in hand, so
  upcoming deck cards are not offered in an extra hand. The first hand of
  each turn keeps the opponent's normal deck access.
- End-of-duel rank statistics count every play. For example, face-down
  plays and fusions add up as they would over separate turns.

## How it works

The game's turn is a chain of scene states: draw (2), draw resolution (3),
hand (4), the play itself (6, 7, 8), then the field (5). Only the draw leads
to the hand. The mod hooks the field state. On the first frame after a play,
when another play is allowed, it sends the scene back to the draw. That
draw:

- runs the game's own first step, which compacts the hand, spawns its cards
  and slides the hand panel in;
- undoes the turn-level work (Swords countdown, turn count, used marks);
- skips the draw resolution and goes straight to the hand state.

The hand state then runs unchanged for the player or the CPU.

The retail hand state assumes five cards, so the mod covers a short hand
while one is up:

- it colours only the occupied slots on the idle step's first frame;
- it replaces `func_8001B8B8` (dimming) with a copy that skips empty slots;
- it masks a cursor step right past the last card;
- it holds the CPU's hand window (`Ai_GetHandSize`) to the cards in hand.

START sets the hand panel's slide-out, the same way the hand state does for
a magic card used from it. The panel releases its cursor and cards on the
way out, and the field starts when it is gone.

The mod's state (plays this turn, a rebuild in progress) is five bytes. It
is registered with the host, so save states keep it.

## Build and checks

```sh
python3 tools/pc/build_mod.py mods/extra-hand-plays --out tmp/pc/extra-hand-plays
```

`tests/pc/extra_hand_plays_test.c` (ctest `pc_extra_hand_plays`) drives the
hooks through whole turns. The game's scene states are replaced by
stand-ins, so the test checks the hand-overs between states and the state
the mod preserves, not the game's drawing.
