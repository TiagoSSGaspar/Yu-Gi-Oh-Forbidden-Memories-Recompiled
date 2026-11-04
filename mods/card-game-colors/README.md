# Card game colors

A data-only mod that draws each card's frame in the color the card has in the
card game and the anime. The disc draws every monster gold and a ritual spell
blue; it also holds a purple and an orange frame it never uses
([Frame color](../../notes/more-cards.md#frame-color)). This mod uses them:

| Frame | Cards | How many |
|---|---|---|
| Orange | effect monsters, flip effect monsters among them | 101 |
| Purple | fusion monsters | 49 |
| Blue (`Ritual`) | ritual monsters | 16 |
| Green (`Magic`) | ritual spells | 24 |

Normal monsters stay gold, magic and equip cards green, traps pink. Each
`cards` entry replaces one card by its number with nothing but a `frame`, so
the mod works with any mod that changes a card's other keys.

## Where the colors come from

Each card's type in the card game, looked up by the card's password (684
cards) or, when the disc's password is not the card game's, by its name (21).
Three monsters are not cards of the card game and take the color of what they
are in the game:

- Meteor B. Dragon (713): the card game's Meteor Black Dragon, a fusion.
- Psycho-Puppet (715) and Mask of Shine & Dark (720): the monsters of Puppet
  Ritual and Construct of Mask, so ritual monsters.

A monster that the game summons with a ritual but the card game prints as a
normal, effect or fusion monster keeps the card game's color: Yamadron,
Millennium Shield, Sengenjin, Tri-horned Dragon, Serpent Night Dragon and
Cosmo Queen stay gold, Gate Guardian is orange, Blue-eyes Ultimate Dragon
purple.

Six cards that are traps in the card game work as magic or equip cards on the
disc (Dragon Capture Jar, Spellbinding Circle, Kunai with Chain, Metalmorph,
Crush Card, Shadow Spell). They keep the green frame, which goes with the
MAGIC or EQUIP word printed on it and with how they are played.

The Build Deck and Trade screens' 16x16 card icon has no purple version on the
disc, so a fusion keeps the monster icon there.
