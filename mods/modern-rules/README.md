# Modern rules

An optional code mod for SLUS_01411 that brings in rules from the later
games. It needs **Extra hand plays** (`mods/extra-hand-plays`), which it
lists under `requires`: enable both in the Mods window. The mod adds a card,
so applying or removing it takes a restart. It ships disabled.

With Extra hand plays at one play a turn, a turn is the original game's,
plus these rules. With more plays, both apply.

## Options

| Setting | Behavior |
|---|---|
| Playing a card is optional | START in the first hand of a turn plays no card and goes to the field, as in the later games. Without it, a full monster row with nothing that fuses would leave the player no legal play, since a monster can no longer be replaced. |
| Draw one card a turn | Each side opens with five cards, then draws one card a turn. The opening five are the first turn's draw, so the side that goes first draws nothing more that turn. Off refills the hand to five each turn, as on the disc. |
| Keep cards a fusion cannot use | When the player confirms two or more marked cards, or puts cards onto one of their monsters, the chain is checked first. A chain that would throw a card away is refused with the cancel sound. The player's marks stay as they were, to be changed. A monster on the field is never replaced by one that does not fuse with it or equip it. |
| Pot of Greed copies for testing | 0–3, default 3. Keeps that many Pot of Greed in the player's trunk and deck together, topping the trunk up about once a second while a save is loaded (not while Build Deck or a duel's deck screen is open). 0 turns it off. Meant only for trying the card out, and to be removed after. |
| Pot of Greed draws two | Using Pot of Greed draws two cards, and the hand comes back up for one more play that does not count against the plays per turn. The CPU uses it too, as soon as it holds one. |

## Rules

- **Drawing.** The hand still has the game's five slots. A card drawn
  with no room waits, and is drawn into the next hand: the next bonus or
  extra hand, or the next turn, on top of that turn's card. The hand shows
  how many are waiting. Drawing with no cards left in the deck loses the
  duel, as at a turn's draw.
- **Fusion chains.** Cards are played in the order they were marked, as in
  the original game. Each next card must either fuse with the card the
  chain has made so far, or one of the two must equip the other. Chains of
  several fusions work: A + B make C, then C + D make E. Cards put onto one
  of the player's monsters start the chain from that monster. In the
  original game a card that does not fuse with it takes its place and the
  monster is thrown away, even with all five zones full; that is refused.
  An empty zone, and the spell and trap row, are not checked. The CPU picks
  its own plays and is not checked.
- **Pot of Greed.** A Magic card, played from the hand face up, or set and
  activated later. It is won in duels and dealt to opponents in place of
  Sparks, and sold on the Password screen for 300 StarChips (password
  `55144522`). Its picture is this mod's own (`images/pot_of_greed.png`).
  Without the mod's code, for example with the setting off, it does nothing.

## How it works

Extra hand plays owns the turn: the draw, bringing the hand back up, and
dealing cards into the first empty slots. It provides a table,
`extra-hand-plays:api` (`mods/extra-hand-plays/extra_hand_plays.h`). This
mod uses it in three places:

- It sets the turn draws: 5 on a side's first turn, 1 after.
- It answers Pot of Greed's use, through the card-effect event, with a
  bonus hand that draws two.
- It wraps the CPU's hand decision (`AiScript_Run`). When the CPU holds a
  Pot of Greed, its decision becomes "use the Pot of Greed, face up". The
  CPU decides its play again in the bonus hand that follows. The manifest
  loads this mod after AI Hard Mode, so it also wraps that mod's planner.

The fusion check wraps the hand state (`DuelScene_UpdateHandActions`). It
plays every step the way placement does (`Fusion_Step` in
`src/pc/cards/fusion.c`), through `Duel_CheckFusion` and `Duel_CheckEquip`,
so fusion and equip tables and mods are respected.

## Build and checks

```sh
python3 tools/pc/build_mod.py mods/modern-rules --out tmp/pc/modern-rules
```

`tests/pc/modern_rules_test.c` (ctest `pc_modern_rules`) checks the turn
draws, the chain check, the effect and the CPU's use. Drawing, queueing and
bonus hands are checked in Extra hand plays' test.
