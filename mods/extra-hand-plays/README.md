# Extra hand plays

An optional API 4 code mod for SLUS_01411. Enable **Extra hand plays** in the
Mods window. It ships disabled; with it off, turns run as on the disc.

After a card is played from the hand, the hand comes back up for another
play. As in the anime, a turn may put down one monster (the setting allows
up to five), and any number of magic, trap and equip cards. A fusion counts
as one monster, and so does a monster put onto one already on the field.
The field phase (attacks, position changes, activating set cards) starts
after the last play.

## Options

| Setting | Behavior |
|---|---|
| Monsters per turn | 1–5, default 1. Magic, trap and equip cards are not counted. |
| Applies to | Both sides, the player only, or the CPU only. The other side plays one card a turn, as on the disc. |
| Show skip hint | While an extra hand is up for the player, a line says that START skips it, and another says when the turn's monsters are spent. The lines sit centred under the strip at the top, which View > Fusion helper takes. |

## Rules

- The first play of a turn is required, as in the original game. In an
  extra hand the player may press **START** to skip the remaining plays
  and go to the field. START does nothing in the first hand, unless
  another mod makes playing a card optional (Duelist Kingdom rules does), and does
  nothing while cards are marked for a fusion.
- No cards are drawn between plays, unless another mod asks for some
  (`bonus_hand`, below: a card that draws). The remaining cards are moved to the left
  of the hand, as the draw does at a turn start.
- Once the turn's monsters are down, the hand comes back while it holds a
  card that is not a monster. Confirming a monster then, alone or marked
  with other cards, is refused with the cancel sound. A CPU decision that
  plays a monster then ends its hands for the turn.
- The hand does not come back when it is empty, or once either side is out
  of life points.
- A turn that starts with a full hand (the last one ended with START,
  playing nothing) draws nothing.
- Nothing carries over from one duel to the next: plays, bonus hands and
  waiting cards start again at zero.
- The Swords of Revealing Light countdown, the turn count, monster "draw"
  effects and the per-turn used marks happen once per turn, not once per
  hand.
- The CPU plays every extra hand it is given, through its own hand script
  (or AI Hard Mode's planner). With fewer than five cards in hand it
  chooses only from the cards it holds. The original script reads the next
  deck cards from snapshot slot 16, which is only correct with five cards
  in hand, so upcoming deck cards are not offered then. A full hand keeps
  the opponent's normal deck access.
- End-of-duel rank statistics count every play. For example, face-down
  plays and fusions add up as they would over separate turns.

## Phases

When another mod asks for them (`set_phases`, below; Duelist Kingdom rules does),
the player's turn runs in phases, as in the GX games:

| Phase | Where | START opens a menu with |
|---|---|---|
| Main Phase 1 | the hand | Attack (the Battle Phase), End turn (the End Phase) |
| Battle Phase | the field (attacks, positions, set cards) | Play more cards (Main Phase 2), End turn |
| Main Phase 2 | the hand again | End turn |
| End Phase | the discard down to the hand limit, then the other side's turn | |

- The menu is a panel in the middle of the screen: the phase now on top,
  what the next phases do under it. On the duel's first turn, when the
  disc lets no monster attack, the Battle Phase's entry is Go to the field. UP and DOWN move its bar, X takes the phase
  under it, circle or START close the menu. While it is open no button
  reaches the game. The phase's name also shows under the strip at the
  top, where the hints are.
- A Main Phase lasts until the player leaves it through the menu: after a
  play the hand comes back while it holds a card, even when only monsters
  are left once the turn's monster is down (confirming one is refused).
  Only an empty hand moves on at once: to the field from Main Phase 1, to
  the end of the turn from Main Phase 2 (there is no second Battle Phase).
- Main Phase 2 is in the field's menu while the hand holds a card.
- The turn's monster plays count across both Main Phases.
- The field closes for Main Phase 2 as it does at the end of a turn; at
  the turn switch the hand comes back through the draw instead, with no
  card drawn.
- The CPU plays as before.

## Hands past five

The disc's hand has five slots. When another mod raises the limit
(`set_hand_limits`, below; Duelist Kingdom rules does), the hand holds up to
forty, a whole deck:

- Past five cards, the hand is laid out overlapped across the width of
  five, later cards over earlier ones and the card under the cursor over
  all. The player walks, views, marks and plays every card as with five; at
  most five cards are marked, as a fusion chain takes.
- No more than ten cards are on screen. Past ten, the screen is a window
  of ten over the hand: the cursor stepping past either end moves it one
  card, and a blinking red arrow beside the hand points to the cards off
  each end, with how many there are under it. A marked card keeps its mark and its number
  off screen, and goes down with the others when the play is confirmed. A
  card drawn past the window moves it on, so the card is seen coming in.
  Only the player's window moves: the CPU's cards past the tenth are not
  drawn.
- The CPU's hand script sees the cards past the fifth ahead of its upcoming
  deck, numbered the way it numbers deck cards it may swap into the hand.
  One it picks is swapped into the five by the game's own swap (or AI Hard
  Mode's), so it always plays from the five.
- At the end of a turn, a side holding more cards than the limit's `keep`
  discards down to it. For the player the hand comes up on its panel: UP
  marks a card to throw away, DOWN or circle takes the mark back, X or
  square throws the marked cards away once there are as many as asked for.
  Past ten cards the window moves as in the hand, and past the fifth mark
  a card is only raised, with no number. The CPU throws away its weakest
  cards: monsters by their better stat, any other card counted as 1500. The game has no graveyard; a discarded
  card leaves the duel.
- Exodia is assembled from the whole hand.
- The port's View > Fusion helper reads the ten cards on screen, so it
  finds fusions with the cards past the fifth too and numbers them where
  each card is left showing. The mod hands it the slots through the port's
  `FusionHelper_SetHand`, looked up by name: on a port without it the mod
  loads all the same, and the helper sees the five.

## For other mods

The mod provides a table that other mods find as `extra-hand-plays:api`
(`extra_hand_plays.h`). A mod that uses it lists `extra-hand-plays` under
`requires`. [Duelist Kingdom rules](../modern-rules/README.md) uses it. The table
offers:

- `set_turn_draws`: how many cards a turn draws, instead of the disc's
  refill to five.
- `bonus_hand(draws)`: after the card being played resolves, the hand comes
  up again with `draws` cards drawn into it. A bonus hand does not count
  against the plays per turn.
- `queued(side)`: how many drawn cards are still waiting for room.
- `set_optional_play` (version 2): while the function it is given says so,
  START also ends the first hand of a turn, so playing a card is optional.
- `pass_hand` (version 3): the side whose turn it is plays no card from the
  hand now up, as START does for the player. For the CPU it is called from
  inside its hand script, with the decision emptied.
- `set_hand_limits` (version 4): how many cards a side may hold (5 to 40;
  10 before version 6) and keep at the end of its turn (0 for no limit).
- `hand_slots`, `hand_card(card)` and `hand_mark(card)` (version 4): the
  hand of the side whose turn it is, card by card, past the fifth too: how
  many there are room for, the card at one and the order it was marked in.
  From version 6 they count the whole hand, off screen too; before, the
  slots on screen.
- `set_phases` (version 5): while the function it is given says so, the
  player's turn runs in phases (above).
- `hand_cursor` (version 6): the hand card under the player's hand cursor.
  Past ten cards the cursor's place on screen is not the card's.

When a turn draws fewer cards than the hand has room for, or a hand is
rebuilt with draws, this mod deals the cards itself. It follows the draw
resolution's own loop, but puts each card into the first empty slot
instead of the last ones, so the hand stays packed to the left. Cards
that do not fit wait. They are drawn into the next hand. When the player's
draw finds the hand full, a notice in the middle of the screen says so for
three seconds ("Hand full", the cards drawn and how many wait), and then
the hint line under the strip above the field keeps the count. Drawing
with no cards left in the deck loses the duel, as at a turn's draw.

## How it works

The game's turn is a chain of scene states: draw (2), draw resolution (3),
hand (4), the play itself (6, 7, 8), then the field (5). Only the draw leads
to the hand. The mod hooks the field state. On the first frame after a play,
when another play is allowed, it sends the scene back to the draw. That
draw:

- runs the game's own first step, which compacts the hand, spawns its cards
  and slides the hand panel in;
- undoes the turn-level work (Swords countdown, turn count, used marks);
- skips the draw resolution and goes straight to the hand state, or deals
  the cards asked for first (above).

The hand state then runs unchanged for the player or the CPU.

The retail hand state assumes five cards, so the mod covers any hand
with fewer, extra or not:

- it colours only the occupied slots on the idle step's first frame;
- it replaces `func_8001B8B8` (dimming) with a copy that skips empty slots;
- it masks a cursor step right past the last card;
- it holds the CPU's hand window (`Ai_GetHandSize`) to the cards in hand,
  and widens it by the cards past the fifth. It loads after AI Hard Mode
  (`after` in `mod.json`), so its window is the one asked first and AI
  Hard Mode's deck access is widened as well.

START sets the hand panel's slide-out, the same way the hand state does for
a magic card used from it. The panel releases its cursor and cards on the
way out, and the field starts when it is gone.

Past five cards (`big_hand.c`), the hand and what is drawn of it are kept
apart. The hand cards past the fifth keep their deck indices beside the
side's `hand[5]`, up to 35 of them. The screen has ten slots: the five
beside `D_800EA030`, and five more that borrow the other side's five hand
records, which nothing reads during the turn; they are cleared when the
turn ends and when it begins. Screen slot `i` shows hand card `view + i`;
moving the window sets each slot's record up again for its new card and
makes its object anew on the panel. Marks are kept by hand card. A marked
card off the window that goes down in a play takes the place of an
unmarked card on screen, which the panel takes away with it. The five
slots are always filled first. The player's hand state is a copy of the
disc's player steps over every slot, used only past five cards, and the
five-slot helpers it calls (the cursor, the pick number, the zone cursor,
dimming) are taught the rest. The hand panel's own move lays the cards out
and takes them along. `Duel_SetupCardRecord` adds 40 to a position below 40
in a second-side record, so during the first side's turn a borrowed record
is given its card 40 short. The end-of-turn discard (`discard.c`) runs
before the turn switch (scene state 10).

The mod's state (plays and hands this turn, a rebuild or deal in progress,
bonus hands, each side's waiting cards, the cards past the fifth, the
window, the marks and the slots) is registered with the host, so save states keep it. The slots hold
guest pointers to their card objects, which a save state restores with the
guest memory.

## Build and checks

```sh
python3 tools/pc/build_mod.py mods/extra-hand-plays --out tmp/pc/extra-hand-plays
```

`tests/pc/extra_hand_plays_test.c` (ctest `pc_extra_hand_plays`) drives the
hooks through whole turns. The game's scene states are replaced by
stand-ins, so the test checks the hand-overs between states and the state
the mod preserves, not the game's drawing.
