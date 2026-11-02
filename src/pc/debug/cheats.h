#ifndef MEMORIES_PC_DEBUG_CHEATS_H
#define MEMORIES_PC_DEBUG_CHEATS_H
/* Development helpers (the MEMORIES_DEBUG_* variables, Debug > Jump to, the
 * control channel) and functions code mods are built against; the window's
 * Game > Cheats menu that used them was removed. They write the game's own
 * save workspace, so a save made afterwards keeps the result. The ones that
 * write it return 0, and do nothing, until a game is started or loaded
 * (Cheats_SaveLoaded). */

/* Nonzero once a game is started or loaded (the workspace holds a deck). */
int Cheats_SaveLoaded(void);

/* Put `count` copies of every card in the chest (the trunk), capped at
 * the game's own limit: exactly `count` of each (MEMORIES_DEBUG_CHEST), or
 * with Cheats_TopUpAllCards (the menu's rows) at least `count`, a card held
 * more times keeping its count, the cards mods added included. Takes effect
 * at once; open BUILD DECK to see it. They also return 0, and do nothing,
 * while Build Deck's chest is on screen (Cheats_ChestOnScreen): it writes
 * its own copy back as it closes. (Cheats_GiveAllCards kept its one
 * argument from release v0.1.2: code mods are built against it.) */
int Cheats_GiveAllCards(int count);
int Cheats_TopUpAllCards(int count);
/* Nonzero while Build Deck, or its screen before a duel, is open: both keep
 * a copy of the chest and deck and write it back over the save as they
 * close, so a change made to the save meanwhile would not last. */
int Cheats_ChestOnScreen(void);
/* Unlock every CPU opponent in the live save. Reopen Free Duel to refresh
 * its portraits and selection grid; save normally to keep the unlocks. */
int Cheats_UnlockAllFreeDuelists(void);
/* Set the StarChips balance, capped at the game's own 999999 (or a mod's
 * "limits", pc/cards/tables.h). */
int Cheats_SetStarchips(unsigned value);
/* What the removed Starting LP, Free spending and Show CPU's hand rows
 * changed, asked by the game code: now always the console's 8000 (at which
 * a mod's "limits" decide the start, Duel_InitSideStates), no free
 * spending (overlays/password/shop.c, the pack shop), and each side's own
 * card_view_mode (Cheats_CardViewMode, declared beside it in
 * game/duel_side_state.h). */
int Cheats_StartingLifePoints(void);
int Cheats_FreeSpending(void);
/* Once a frame: MEMORIES_DEBUG_CHEST=N gives N of every card, and
 * MEMORIES_DEBUG_DECK="723-762" (ids and ranges, repeated to forty) sets the
 * deck, the first time a save is live in the workspace. */
void Cheats_Frame(void);
/* MEMORIES_DEBUG_DECK's deck now (ids and ranges, repeated to forty), for
 * Debug > Jump to and the control channel's `jump`: the number of ids
 * taken, 0 when none was a card (the deck is left alone). */
int Cheats_SetDeck(const char *list);
/* The ids `list` names (what Cheats_SetDeck would take), writing nothing. */
int Cheats_DeckCount(const char *list);
#endif
