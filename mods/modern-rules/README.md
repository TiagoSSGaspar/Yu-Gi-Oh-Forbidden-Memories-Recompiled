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
| The moth's evolution | On by default. As in Weevil's duels in the anime: Cocoon of Evolution put onto Petit Moth or Larvae Moth (the anime's name for it), or the two fused from the hand, make the cocoon, which comes down in defense (the Forest's field bonus raises it, as in the anime). At its owner's 5th turn after, it opens: Perfectly Ultimate Great Moth, face up in attack, with what equips gave the cocoon. Destroyed before, it dies with the larva, unless it has had 4 of those turns: then Great Moth comes out in its place, in attack, as when Yugi destroyed Weevil's cocoon. A line above the hand says how many turns the player's cocoons have left. A Cocoon played alone stays a Cocoon. The CPU's cocoons evolve too. |
| Equips stay on the field | On by default. As in the anime and the card game: an equip goes to the magic and trap zone behind the monster it equips (the nearest free one when that is taken; one used from the field keeps its own zone), face up, and stays there. Destroyed (Harpie's Feather Duster, a card put over it), it takes its own bonus off the monster; the monster leaving the field (a battle, Raigeki, a fusion onto it) destroys its equips, each with the game's explosion. With no free zone for it, an equip cannot be played: the player's confirm is refused, and the CPU plays elsewhere, or no card. An equip on the field is not used again. A magic, trap or ritual card needs a free zone of its own as well: none is put over a card already there (which would free that zone), and with the row full none is played, set or face up (the original game uses a magic card face up straight from the hand, with no zone); the CPU is held to the same. With the field cursor on an equip, a line says which monster it equips; on a monster, its equips. |
| The swamp brothers | On by default. As in Joey's duel with Rex Raptor: with Swamp Battleguard and Lava Battleguard on the same side, a monster attacking one of them meets both. The attacked one fights in attack position with its brother's ATK added (1800 + 1550 = 3350), even from defense, and a line says so during the battle; the weaker side loses its monster and the difference in life points. A battleguard that survives gets its own ATK and position back. Both sides. |
| Kuriboh and Multiply | On by default. As in Yugi's duel with Kaiba at the castle (episode 24): Blue-Eyes Ultimate Dragon attacks the wall of Kuribohs, a Kuriboh dies, another takes its place, and Yugi loses no life points. Multiply, a card of this mod's (password 40703222, won in duels and dealt to opponents in Legendary Sword's place), is an equip of Kuriboh's alone with no bonus, and stays on the field as every equip does; it is not destroyed with its Kuriboh. While it is on the field, with 3D Monsters on, a wall of five more Kuribohs floats in front of the side's zones, with no cards under them and taking no monster zone; the Kuriboh stays in defense and cannot leave it; an attack on it goes to the wall, where a clone dies and another takes its place, and costs no life points; and should the wall's Kuriboh leave the field, another is born in its zone, so the side always has one and takes no direct attack. Another Kuriboh of the side is a monster as any other. The side's other monsters are attacked as ever, as Yugi's Celtic Guardian was. Destroying Multiply breaks the wall. A second Multiply cannot be played on a side whose wall is up, by the player or the CPU. How the CPU plays it depends on its level (below). |
| Living Arrow | On by default. As in Yugi's duel with Kaiba at the castle: Polymerization and Living Arrow fuse Mammoth Graveyard into Blue-Eyes Ultimate Dragon, which rots from within. Mark Polymerization, Living Arrow and a Zombie (Mammoth Graveyard counts, as in the anime) in the hand and confirm: pick a face-up living monster of the opponent's with left and right (not a Zombie, a Machine or a Rock: only living flesh rots), cross to infect, circle to cancel. The three cards leave the hand and the hand comes back. At the end of each of your turns the infected loses the Zombie's ATK, down to 0, until it leaves the field. With 3D Monsters the Zombie flies into the target's chest and stays there, and the target darkens as it rots, flashing as it loses ATK. Polymerization (password 24094653) and Living Arrow (41180607), cards of this mod's, are won in duels and dealt to opponents; alone they cannot be played. The CPU plays the combo on the player's strongest living monster when it is stronger than all of its own. Needs Extra hand plays 0.9.0. |
| Mai's harpies | On by default. As in Mai's duels with Yugi and Joey: Elegant Egotist put onto Harpie Lady, or the two fused from the hand, makes Harpie Lady Sisters, the one harpy split into three (the game's own fusion; no equip is left on the field). Harpie's Pet Dragon gains 300 ATK for each harpy on its side, Harpie Lady counting one and Harpie Lady Sisters three; the bonus follows the field. Both sides. |
| Castle of Dark Illusions | On by default. As in PaniK's duel with Yugi: with Castle of Dark Illusions face up on the field, its owner's other monsters are in the dark. They are drawn dark (cards and 3D models), the player's attack on one is refused with a line saying why, and the CPU, at every level, aims elsewhere or nowhere; the castle itself can be attacked. Swords of Revealing Light used by the other side lights the field: that castle hides no one any more, until another castle comes. Both sides. |
| Mirror Force | On by default. Mirror Force (password 44095762), a trap of this mod's with the card game's art, is won in duels and dealt to opponents. Set on the field it springs on any attack of the other side's, as Widespread Ruin does: the attacker is destroyed and the attack stops; then every other monster of the attacker's in attack position is destroyed, one explosion at a time. Off, it is a Widespread Ruin. Both sides. |
| Change of Heart | On by default. Change of Heart (password 04031928), a magic card of this mod's with the card game's art, is won in duels and dealt to opponents. Used face up (from the hand or the field), it asks for a monster of the opponent's: left and right pick (the line names it), cross takes it. It comes to a free zone of the user's as it is (position, face, what equips and the harpies gave it; its equips stay where they are and go on equipping it) and may attack that turn. When the next turn starts it goes back to its owner, to the zone it left or another free one, or is destroyed when they have none. Refused with no monster to take or no free zone. The CPU takes the player's strongest monster when it is stronger than all of its own. Off, the card does nothing. |
| Monster Reborn | On by default. Monster Reborn (password 83764718), a magic card of this mod's with the card game's art, is won in duels and dealt to opponents. The disc keeps no graveyard, so the mod writes one: each monster destroyed in the duel, its owner's (a monster taken by Change of Heart is still its owner's). Used face up, it asks for one of either side's: left and right pick (the line names it, its ATK and DEF and whose it was), cross revives it in a free zone of the user's, face up in attack, with the ritual's light. Refused with an empty graveyard or no free zone. The CPU revives the strongest when it is stronger than all of its own. Off, the card does nothing. |
| Magical Hats | On by default. Magical Hats (password 81210420), a trap of this mod's with the card game's art, is won in duels and dealt to opponents. Set on the field it springs as Fake Trap does when the other side attacks: the attack stops. When a monster was attacked, up to three hats go up in its owner's free monster zones and the monster hides among them, face down in defense in one picked at random; the others hold decoys (Magical Hat, 0/0, made from the trap's own spent deck card, so a decoy turned over shows hats). The next attacks pick a hat by its zone, as Kaiba does: a decoy dies with no damage, the monster fights as itself. Decoys do not attack, change position or go to the graveyard, and leave when the hidden monster does. With no free zone, or on a direct attack, it only stops the attack. Off, it is a Fake Trap. |
| The CPU's level | With the anime's rules on. As in the anime, the duelists of little skill fall for every trick and the bosses turn them on the player. The level is the game's own: each opponent's will to use Harpie's Feather Duster in its AI table (25, 50, 75). Beginners attack the wall of Kuribohs for ever and do not see the swamp brothers fight together. The middle ones learn the wall after two attacks on it, see the brothers half of the time, and play the combos (Multiply onto Kuriboh, the brothers together, Living Arrow) when behind. Bosses (Heishin, Mai, Bakura, Pegasus, Isis, Kaiba, Labyrinth Mage, Heishin 2nd, Seto 3rd, Nitemare) learn the wall at the first attack and break it with Harpie's Feather Duster, always see the brothers, play every combo as soon as they can, and their decks are dealt the combos' cards often. Against the player's Castle of Dark Illusions hiding monsters, a boss plays Swords of Revealing Light, else Raigeki, else Dark Hole when it has no monster to lose; a middle one half of the time; a beginner never. Mirror Force teaches: once a boss has fallen into it (a middle one twice), facing a face-down card in the player's magic and trap row it attacks once a turn and puts its other monsters in defense first; forgotten in the next duel. With AI Hard Mode applied every opponent is a boss. |
| Traps spring on their own | Always, with the mod. As in the anime and the card game, a trap set on the field springs when its moment comes (an attack): the player's confirm on it, walking the field, is refused, where the original game used it as a magic card. A magic card set there is still used. The CPU never used its traps so. |
| Great Moth's poison dust | Always, with the mod. As in Yugi's duel with Weevil, where its scales wore Gaia the Dragon Champion down from 2600 to 1900: at the start of each of its owner's turns, the opponent's monsters lose 300 ATK, and keep the loss when it is gone. The card's text says so. |
| Draw one card a turn | Each side opens with five cards, then draws one card a turn. The side that goes first starts with its five and draws nothing on the duel's first turn; the other side's first draw is its five and its turn's card. The hand holds up to forty cards. Off refills the hand to five each turn, as on the disc. |
| Keep cards a fusion cannot use | When the player confirms two or more marked cards, or puts cards onto one of their monsters, the chain is checked first. A chain that would throw a card away is refused with the cancel sound. The player's marks stay as they were, to be changed. A monster on the field is never replaced by one that does not fuse with it or equip it, on either side: the CPU plays its cards elsewhere, or none. An equip used from the field must equip the monster the player puts it onto. |

## Rules

- **Drawing.** The hand holds up to forty cards, a whole deck (Extra hand
  plays lays them out overlapped past five, ten at a time on screen, in a
  window the cursor moves). A card drawn with no room waits, and is drawn
  into the next hand: the next bonus or extra hand, or the next turn, on
  top of that turn's card. The hand shows how many are waiting. Drawing
  with no cards left in the deck loses the duel, as at a turn's draw.
- **No hand limit.** The hand is not cut down at the end of a turn: passing
  turns without playing, it grows by one each turn, up to forty.
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
  the five and the turn's card), 1 after; and the hand limits, forty held and
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

The equips (`equips.c`) wrap the placement (`DuelScene_UpdateCardPlacement`):
each equip's step (5, then 4) is measured, what the monster's
`stat_modifier` and `defense_modifier` gained being its bonus, and a fusion
or a card thrown away after it forgets it, as the disc does. When the
monster lands in its zone (`func_8001B170`, which they wrap too) each equip
is remade in its zone (`func_80024D34`) face up and tied to the monster:
the monster's card and deck card, and the bonus, turned round when Reverse
Trap springs there. The ties are looked at while the hand or the field is
idle (`Equips_Hold`, where the moth's evolution is looked at): the
monster's record no longer holding it destroys the equip, one explosion at a
time (`DuelEffect_CreateRequest(0xB)`, `DuelCard_RemoveFromField`); the
equip's no longer holding it takes the bonus off. The CPU's view of the
field (`func_80028220`) shows its equips in use as used this turn.

The swamp brothers (`swamp.c`) wrap the battle (`DuelScene_UpdateBattle`).
The battle keeps the attacked monster's `stat_modifier` and flags as it
starts (`D_8009B170[1]`, `D_8009B178[1]`) and puts them back from those
before it works the battle out again for the models, so the brother's ATK
goes into the record and the kept values alike, and defense comes off both;
once the battle is over a battleguard still there gets its own back.

Multiply (`multiply.c`) has 3D Monsters draw the wall: it finds
`3d-monsters:api` (`mods/3d-monsters/monsters_3d.h`) and asks for five more
of the Kuriboh's model, which 3D Monsters loads as entries of their own and
builds, once its frame's packet buffer is full, in the other buffer of the
pair. Multiply makes a new Kuriboh from the
Kuriboh's deck card (`func_80024D34`) while the hand or the field is idle,
as the equips are looked at.

The CPU's level and its plays are in `cpu.c`; AI Hard Mode shares whether it
is applied as `ai-hard-mode:on`.

Living Arrow (`arrow.c`) takes the player's confirm in the idle hand when
the three marked cards are the combo, and the buttons while they aim; the
cards leave through Extra hand plays' `take_hand` (API 8). The CPU's combo
is played from its hand script, its decision emptied. The ATK goes at the
start of the other side's turn (the turn draws Extra hand plays asks for),
and an infected card is drawn green (`func_80015EF4`). With 3D Monsters
(`3d-monsters:api` version 2) the Zombie's model is an extra drawn with the
infected monster's, flying in and then in its chest, and the monster's
model takes a colour (`set_tint`) that darkens as it rots.

The moth's evolution, the equips, the swamp brothers, Multiply and Living
Arrow, the harpies, the castle, Mirror Force, Change of Heart, Monster Reborn and Magical Hats share the mod's one state buffer (`state.h`, version 15), saved in save
states.

## Build and checks

```sh
python3 tools/pc/build_mod.py mods/modern-rules --out tmp/pc/modern-rules
```

`tests/pc/modern_rules_test.c` (ctest `pc_modern_rules`) checks the turn
draws, the hand limits, the chain check and the equips' zones;
`tests/pc/equips_test.c` (ctest `pc_equips`) the equips on the field;
`tests/pc/swamp_test.c` (ctest `pc_swamp`) the swamp brothers;
`tests/pc/multiply_test.c` (ctest `pc_multiply`) Kuriboh and Multiply;
`tests/pc/arrow_test.c` (ctest `pc_arrow`) Living Arrow;
`tests/pc/harpies_test.c` (ctest `pc_harpies`) the harpies;
`tests/pc/castle_test.c` (ctest `pc_castle`) the castle;
`tests/pc/mirror_test.c` (ctest `pc_mirror`) Mirror Force;
`tests/pc/spells_test.c` (ctest `pc_spells`) Change of Heart and Monster Reborn;
`tests/pc/hats_test.c` (ctest `pc_hats`) Magical Hats. Drawing, queueing and bonus
hands are checked in Extra hand plays' test.
