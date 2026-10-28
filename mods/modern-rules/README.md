# Duelist Kingdom rules

Its id stays `modern-rules`; it was named Modern rules before 0.5.0.

An optional code mod for SLUS_01411 that brings in the rules of the anime's
Duelist Kingdom, as far as they fit the game: life points stay at 8000. It
needs **Extra hand plays** (`mods/extra-hand-plays`), which it lists under
`requires`: enable both in the Mods window. Applying or removing it takes a
restart.

The Battle City's rules come later, as a mod of their own: the hand limit
of six at the end of a turn and Pot of Greed (this mod's Treasure Trickster
until 0.5.0, which moves there). Copies of Treasure Trickster stay in the
saves (`notes/more-cards.md`) under `modern-rules:pot-of-greed:1`.

Extra hand plays brings the hand back after each play: one monster a turn
by default, and any number of magic, trap and equip cards. These rules
come on top.

## Options

| Setting | Behavior |
|---|---|
| Playing a card is optional | START in the first hand of a turn plays no card and goes to the field, as in the anime. Without it, a full monster row with nothing that fuses would leave the player no legal play, since a monster can no longer be replaced. |
| Turn phases | On by default. START opens a menu of the next phases instead of ending the hand or the turn: from the hand (Main Phase 1) the Battle Phase (the field) or the End Phase; from the field Main Phase 2, to play from the hand again after attacking, or the End Phase. UP/DOWN pick, X takes, circle closes. The turn's monster still counts once across both Main Phases. The CPU plays as before. See Extra hand plays' README. |
| Draw one card a turn | Each side opens with five cards, then draws one card a turn. The side that goes first starts with its five and draws nothing on the duel's first turn; the other side's first draw is its five and its turn's card. The hand holds up to ten cards. Off refills the hand to five each turn, as on the disc. |
| Keep cards a fusion cannot use | When the player confirms two or more marked cards, or puts cards onto one of their monsters, the chain is checked first. A chain that would throw a card away is refused with the cancel sound. The player's marks stay as they were, to be changed. A monster on the field is never replaced by one that does not fuse with it or equip it, on either side: the CPU plays its cards elsewhere, or none. An equip used from the field must equip the monster the player puts it onto. |

## Rules

- **Drawing.** The hand holds up to ten cards (Extra hand plays lays them
  out overlapped past five). A card drawn with no room waits, and is drawn
  into the next hand: the next bonus or extra hand, or the next turn, on
  top of that turn's card. The hand shows how many are waiting. Drawing
  with no cards left in the deck loses the duel, as at a turn's draw.
- **No hand limit.** The hand is not cut down at the end of a turn: passing
  turns without playing, it grows by one each turn, up to ten.
- **Fusion chains.** Cards are played in the order they were marked, as in
  the original game. Each next card must either fuse with the card the
  chain has made so far, or one of the two must equip the other. Chains of
  several fusions work: A + B make C, then C + D make E. Cards put onto one
  of the player's monsters start the chain from that monster. In the
  original game a card that does not fuse with it takes its place and the
  monster is thrown away, even with all five zones full; that is refused.
  An empty zone, and the spell and trap row, are not checked. An equip set
  in the spell and trap row and used later goes onto a monster the player
  picks; the game throws it away when it does not equip that monster (or
  fuse with it), so that pick is refused too.
- **The CPU** is held to the same rule. When its choice would throw a card
  away, its cards go to an empty zone, or onto a monster they fuse with or
  equip; a chain that breaks is played as far as it keeps every card. With
  no such zone it plays no card, when playing a card is optional (with that
  setting off, its own choice stands).

## How it works

Extra hand plays owns the turn: the draw, bringing the hand back up, and
dealing cards into the first empty slots. It provides a table,
`extra-hand-plays:api` (`mods/extra-hand-plays/extra_hand_plays.h`). This
mod uses it in two places:

- It sets the turn draws: 6 on a side's first draw (nothing drawn yet:
  the five and the turn's card), 1 after; and the hand limits, ten held and
  none kept at the end of a turn.
- It wraps the CPU's hand decision (`AiScript_Run`) for the fusion check
  below. The manifest loads this mod after AI Hard Mode, so it also wraps
  that mod's planner.

The fusion check wraps the hand state (`DuelScene_UpdateHandActions`) and
the field state's equip target (`DuelScene_UpdateFieldActions`, step 6) for
the player, and the CPU's hand decision (`AiScript_Run`, the picks and zone
in `D_800EAE88`) for the CPU, which passes through Extra hand plays'
`pass_hand` when it has no play left. It
plays every step the way placement does (`Fusion_Step` in
`src/pc/cards/fusion.c`), through `Duel_CheckFusion` and `Duel_CheckEquip`,
so fusion and equip tables and mods are respected.

## Build and checks

```sh
python3 tools/pc/build_mod.py mods/modern-rules --out tmp/pc/modern-rules
```

`tests/pc/modern_rules_test.c` (ctest `pc_modern_rules`) checks the turn
draws, the hand limits and the chain check. Drawing, queueing and bonus
hands are checked in Extra hand plays' test.
