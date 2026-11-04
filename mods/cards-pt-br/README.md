# Cards in Portuguese

A data-only mod that puts the cards in Brazilian Portuguese when the system's
language is Portuguese: their names, their texts, their types (Dragão, Mago,
Besta-Guerreira...), the guardian stars (Sol, Lua, Marte...) and the card
view's GUARDIAN STAR headings. Dialogue and menus stay in English. In any
other language the mod changes nothing; turned off, the cards stay English
in every language.

It works through its `lang/pt-BR.json`, whose `text` file is read only when
the player's language is Portuguese (mod API 23, [A mod in the player's
language](../../notes/modding.md#a-mod-in-the-players-language)).

## The table

`cards.json` holds, for each of the 722 cards:

| Key | What it is |
|---|---|
| `name` | Konami's official Portuguese name, or the game's English one where the card game never had one (`name_source`: `official` or `english`) |
| `official` | Konami's official Portuguese text from the card game, when there is one |
| `text` | our text: what the card says or does in this game, written the way Konami writes |
| `show` | which of the two the game shows |

A normal monster shows Konami's text, its story, whenever there is one and it
fits the card's box. An effect monster, a magic card or a trap shows ours: the
official one describes the card game's effect, which this game does not have.
When a card gets its card game effect, its `show` becomes `official`.

`text/cards.txt` is written from the table by `tools/pc/cards_pt_br.py`; edit
the table and run it again (`--check` says whether the file is up to date).
The script refuses a text that does not fit the box (8 lines of 20 letters,
broken at spaces), a letter the game cannot draw, and a missing card.

## Names

Only Konami's names. A card has its official Portuguese name (from the card
game's Portuguese cards, checked against Konami's own card database); a card
that never had one keeps the game's English name (`name_source`: `official` or
`english`). No name is made up or shortened. Where a box is narrower than a name, the
port draws its letters closer, in the box's width (down to 6 pixels a letter):
the duel's bar holds 32 letters that way, the card lists 21, and the Library's
line, which this mod limits to 36 cells (the game left it without a limit, as
no English name needed one), 48. Only a name longer still is cut.

The cards the Duelist Kingdom rules add have their Portuguese names and texts
in that mod's own `lang/pt-BR.json`.
