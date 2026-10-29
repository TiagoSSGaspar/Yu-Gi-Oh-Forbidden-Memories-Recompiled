#ifndef EXTRA_HAND_PLAYS_HAND_INTERNAL_H
#define EXTRA_HAND_PLAYS_HAND_INTERNAL_H
/* What the mod's sources share: its state, kept in save states, and the hand
 * past five cards (big_hand.c). */
#include "types.h"
#include "game/card_constants.h"
#include "game/duel_hand.h"
#include "game/duel_selection_layout.h"
#include "pc/mods/modapi.h"
#include "extra_hand_plays.h"

/* Hand cards (0 up) are the side's hand, up to a whole deck; screen slots
 * (0 to 9) are what is drawn of it: screen slot i shows hand card S.view + i. */
#define EHP_HAND 40                         /* cards a hand may hold: a whole deck */
#define EHP_EXTRA (EHP_HAND - HAND_SIZE)    /* hand cards past the fifth, in S.xhand */
#define EHP_VIEW 10                         /* screen slots: the five and the other side's five hand records */
#define EHP_BORROWED (EHP_VIEW - HAND_SIZE) /* screen slots past the fifth, in S.xslot */
#define EHP_STATE_VERSION 6

/* No host pointers: kept in save states (register_state). The slots' objects
 * are guest pointers, which a save state restores with the guest memory. */
typedef struct {
    u8 plays;     /* counted hand states begun this turn */
    u8 monsters;  /* plays this turn that put a monster down from the hand */
    u8 hands;     /* every hand state begun this turn, bonus ones too */
    u8 pending;   /* a hand state ran and the field has not started since */
    u8 redraw;    /* the draw now running only rebuilds the hand */
    u8 skipping;  /* START: the hand panel is sliding away */
    u8 done;      /* skipped: no more hands this turn */
    u8 bonus;     /* bonus hands asked for and not yet begun */
    u8 in_bonus;  /* the hand being rebuilt is a bonus one: not counted */
    u8 bonus_draws; /* cards the next bonus hand draws */
    u8 custom;    /* the draw resolution running is this mod's */
    u8 to_draw;   /* cards it has still to draw */
    u8 queued[2]; /* each side's drawn cards that did not fit, still owed */
    u8 settle;    /* frames waited for a panel move before a hand comes back */
    u8 layout;    /* cards the hand panel is laid out for */
    u8 discard;   /* the end-of-turn discard's step (discard.c), 0 when none runs */
    u8 discard_left; /* cards still to mark for it */
    u8 focus;     /* the slot brought to the front, + 1; 0 for none */
    u8 main2;     /* phases (phases.c): 0 Main Phase 1 or the field; 1 Main Phase 2 asked for, the field closing; 2 in Main Phase 2 */
    u8 menu;      /* the phase menu: 0 closed, else 1 + the entry under its cursor */
    u8 to_end;    /* the hand slides away into the End Phase, not the field */
    u8 view;      /* the hand card on screen slot 0: the window's first (the player's only; 0 for the CPU) */
    u8 mark[EHP_HAND]; /* the player's marks by hand card: 0 none, else the order marked */
    s8 xhand[2][EHP_EXTRA]; /* each side's hand cards past the fifth: combined deck indices, packed, -1 empty */
    DuelHandSlot xslot[EHP_BORROWED]; /* screen slots past the fifth, as D_800EA030 for the five */
} EhpState;

extern EhpState ehp_state;
#define S ehp_state

extern const MemoriesModHost *ehp_host;

/* big_hand.c */
int Ehp_HandMax(void);              /* cards a hand may hold: 5, or what another mod set (up to 40) */
int Ehp_HandKeep(void);             /* cards kept at the end of a turn; 0 for no limit */
int Ehp_Count(int side);            /* cards in the side's hand, past the fifth too */
int Ehp_Extras(int side);           /* cards past the fifth */
s8 Ehp_Get(int side, int card);     /* the deck index of a hand card, -1 empty */
void Ehp_Set(int side, int card, s8 deck_index);
DuelHandSlot *Ehp_Slot(int slot);   /* a screen slot, for the side whose turn it is */
int Ehp_Record(int side, int slot); /* a screen slot's D_801A7AD8 record */
void Ehp_SetupRecord(int record, int deck_index);
int Ehp_LayoutX(int slot, int count); /* the slot's x on the panel, laid out for `count` cards */
void Ehp_Pack(int side);            /* cards past the fifth into empty slots of the five, in order */
void Ehp_SpawnExtras(int y);        /* objects for the cards past the fifth, as the draw spawns the five */
void Ehp_ClearBorrowed(int side);   /* the other side's hand records the side's extras used */
void Ehp_LayOut(DuelSelectionRecord *side); /* the hand panel's cards spread for S.layout */
void Ehp_Show(int view);            /* the window from hand card `view` onto the screen slots, on the panel */
int Ehp_Mark(int card);             /* the player's mark on a hand card: 0 none, else its order */
int Ehp_Cursor(void);               /* the hand card under the player's hand cursor */
void Ehp_Arrange(int slot);         /* a card drawn into a spread hand: its depth among the others */
void Ehp_RefreshExtras(void);       /* the CPU's cards past the fifth, after its swap changed them */
int Ehp_BigHand(void);              /* the hand on screen has more than five cards */
void Ehp_HandActions(DuelSelectionRecord *side); /* the player's hand state for more than five */
void Ehp_SetLimits(ExtraHandLimits fn);
int Ehp_Install(void);
int Ehp_CardId(int deck_index);

/* phases.c: the player's turn in phases, as in the GX games (set_phases) */
enum { PHASE_MAIN1 = 1, PHASE_BATTLE, PHASE_MAIN2, PHASE_END };
void Ehp_SetPhases(ExtraHandPhases fn);
int Ehp_Phases(void);               /* the player's turn runs in phases now */
int Ehp_PhaseItems(int field, int items[2]); /* the menu's entries, for the hand (0) or the field (1) */
int Ehp_PhaseMenu(int field);       /* 0 nothing, -1 the menu holds the buttons this frame, else the phase taken */
int Ehp_MainTwo(void);              /* the turn switch after the field closed for Main Phase 2: 1 when the hand comes back */

/* discard.c */
int Ehp_Discard(void);              /* the end-of-turn discard, before the turn switch: 1 while it runs */
int Ehp_DiscardInstall(void);

#endif
