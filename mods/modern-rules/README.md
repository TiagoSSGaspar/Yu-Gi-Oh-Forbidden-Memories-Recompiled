# Modern rules

An optional code mod for SLUS_01411 that brings in rules from the later
games. It needs **Extra hand plays** (`mods/extra-hand-plays`), which it
lists under `requires`: enable both in the Mods window. The mod adds a card,
so applying or removing it takes a restart. It ships disabled.

Extra hand plays brings the hand back after each play: one monster a turn
by default, and any number of magic, trap and equip cards. These rules
come on top.

## Options

| Setting | Behavior |
|---|---|
| Playing a card is optional | START in the first hand of a turn plays no card and goes to the field, as in the later games. Without it, a full monster row with nothing that fuses would leave the player no legal play, since a monster can no longer be replaced. |
| Draw one card a turn | Each side opens with five cards, then draws one card a turn. The side that goes first starts with its five and draws nothing on the duel's first turn; the other side's first draw is its five and its turn's card. The hand holds up to ten cards. Off refills the hand to five each turn, as on the disc. |
| Hand limit at the end of a turn | 0–10, default 6. With one card drawn a turn, a side ending its turn with more cards discards down to this many: the player picks them, the CPU throws away its weakest. 0 turns the limit off. |
| Keep cards a fusion cannot use | When the player confirms two or more marked cards, or puts cards onto one of their monsters, the chain is checked first. A chain that would throw a card away is refused with the cancel sound. The player's marks stay as they were, to be changed. A monster on the field is never replaced by one that does not fuse with it or equip it, on either side: the CPU plays its cards elsewhere, or none. An equip used from the field must equip the monster the player puts it onto. |
| Treasure Trickster draws two | Using Treasure Trickster draws two cards, and the hand comes back up for one more play that does not count against the plays per turn. The CPU uses it too, as soon as it holds one. |

## Rules

- **Drawing.** The hand holds up to ten cards (Extra hand plays lays them
  out overlapped past five). A card drawn with no room waits, and is drawn
  into the next hand: the next bonus or extra hand, or the next turn, on
  top of that turn's card. The hand shows how many are waiting. Drawing
  with no cards left in the deck loses the duel, as at a turn's draw.
- **Hand limit.** At the end of a turn a side keeps at most six cards
  (the setting): passing turns without playing, the hand grows 5, 6, then
  7 during the turn and back to 6 at its end.
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
- **Treasure Trickster.** The mod's own card in Pot of Greed's part from
  the later games: a Magic card that draws two, played from the hand face
  up, or set and activated later. It is won in duels and dealt to
  opponents in place of Sparks, and sold on the Password screen for 300
  StarChips (password `55144522`). Its picture is the mod's own
  (`images/treasure_trickster.png`). Its identity in saves is still
  `modern-rules:pot-of-greed:1`, the card's first name, so copies saved
  under that name stay. Without the mod's code, for example with the
  setting off, it does nothing.

## How it works

Extra hand plays owns the turn: the draw, bringing the hand back up, and
dealing cards into the first empty slots. It provides a table,
`extra-hand-plays:api` (`mods/extra-hand-plays/extra_hand_plays.h`). This
mod uses it in three places:

- It sets the turn draws: 6 on a side's first draw (nothing drawn yet:
  the five and the turn's card), 1 after; and the hand limits, ten held and
  six kept.
- It answers Treasure Trickster's use, through the card-effect event, with a
  bonus hand that draws two.
- It wraps the CPU's hand decision (`AiScript_Run`). When the CPU holds a
  Treasure Trickster, its decision becomes "use it, face up". The
  CPU decides its play again in the bonus hand that follows. The manifest
  loads this mod after AI Hard Mode, so it also wraps that mod's planner.

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
draws, the chain check, the effect and the CPU's use. Drawing, queueing and
bonus hands are checked in Extra hand plays' test.
