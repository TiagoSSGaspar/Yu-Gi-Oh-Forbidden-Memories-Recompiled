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
| Show skip hint | While an extra hand is up for the player, a line says that START skips it, and another says when the turn's monsters are spent. |

## Rules

- The first play of a turn is required, as in the original game. In an
  extra hand the player may press **START** to skip the remaining plays
  and go to the field. START does nothing in the first hand, unless
  another mod makes playing a card optional (Modern rules does), and does
  nothing while cards are marked for a fusion.
- No cards are drawn between plays, unless another mod asks for some
  (Modern rules' Pot of Greed). The remaining cards are moved to the left
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

## Hands past five

The disc's hand has five slots. When another mod raises the limit
(`set_hand_limits`, below; Modern rules does), the hand holds up to ten:

- Past five cards, the hand is laid out overlapped across the width of
  five, later cards over earlier ones and the card under the cursor over
  all. The player walks, views, marks and plays every card as with five; at
  most five cards are marked, as a fusion chain takes.
- The CPU's hand script sees the cards past the fifth ahead of its upcoming
  deck, numbered the way it numbers deck cards it may swap into the hand.
  One it picks is swapped into the five by the game's own swap (or AI Hard
  Mode's), so it always plays from the five.
- At the end of a turn, a side holding more cards than the limit's `keep`
  discards down to it. For the player the hand comes up on its panel: UP
  marks a card to throw away, DOWN or circle takes the mark back, X or
  square throws the marked cards away once there are as many as asked for.
  The CPU throws away its weakest cards: monsters by their better stat,
  any other card counted as 1500. The game has no graveyard; a discarded
  card leaves the duel.
- Exodia is assembled from every slot.

## For other mods

The mod provides a table that other mods find as `extra-hand-plays:api`
(`extra_hand_plays.h`). A mod that uses it lists `extra-hand-plays` under
`requires`. [Modern rules](../modern-rules/README.md) uses it. The table
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
- `set_hand_limits` (version 4): how many cards a side may hold (5 to 10)
  and keep at the end of its turn (0 for no limit).
- `hand_slots`, `hand_card(slot)` and `hand_mark(slot)` (version 4): the
  hand of the side whose turn it is, slot by slot, past the fifth too: how
  many slots it has, the card in one and the order it was marked in.

When a turn draws fewer cards than the hand has room for, or a hand is
rebuilt with draws, this mod deals the cards itself. It follows the draw
resolution's own loop, but puts each card into the first empty slot
instead of the last ones, so the hand stays packed to the left. Cards
that do not fit wait. They are drawn into the next hand, and the player's
hand shows how many are waiting. Drawing with no cards left in the deck
loses the duel, as at a turn's draw.

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
- it holds the CPU's hand window (`Ai_GetHandSize`) to the cards in hand.

START sets the hand panel's slide-out, the same way the hand state does for
a magic card used from it. The panel releases its cursor and cards on the
way out, and the field starts when it is gone.

Past five cards (`big_hand.c`), cards six to ten keep their deck indices
beside the side's `hand[5]`, their slots on screen beside `D_800EA030`, and
borrow the other side's five hand records, which nothing reads during the
turn; they are cleared when the turn ends and when it begins. The five
slots are always filled first. The player's hand state is a copy of the
disc's player steps over every slot, used only past five cards, and the
five-slot helpers it calls (the cursor, the pick number, the zone cursor,
dimming) are taught the rest. The hand panel's own move lays the cards out
and takes them along. `Duel_SetupCardRecord` adds 40 to a position below 40
in a second-side record, so during the first side's turn a borrowed record
is given its card 40 short. The end-of-turn discard (`discard.c`) runs
before the turn switch (scene state 10).

The mod's state (plays and hands this turn, a rebuild or deal in progress,
bonus hands, each side's waiting cards, the cards past the fifth and their
slots) is registered with the host, so save states keep it. The slots hold
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
