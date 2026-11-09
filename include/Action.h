
#ifndef ACTION_H
#define ACTION_H

enum class ActionType {
    // Starting the game
    CONFIRM_READY,

    // Drawing
    DRAW_FROM_DECK,
    DRAW_FROM_DISCARD,

    // Decisions after drawing
    REPLACE_CARD_WITH_DRAWN,
    DISCARD_DRAWN_CARD,
    USE_POWER_AND_DISCARD_DRAWN,

    // Special powers
    PEEK_OWN_CARD,
    PEEK_OPPONENT_CARD,
    BLIND_SWAP,
    LOOK_AND_OPTIONALLY_SWAP,   // one-shot look + decide (used by the AI)
    LOOK_TWO_CARDS,             // King, step 1: look at two cards
    DECIDE_SWAP,                // King, step 2: swap them or leave them (uses shouldSwap)

    // Other gameplay actions
    FLIP_CARD,
    GIVE_CARD_AFTER_FLIP,
    CALL_CAMBIO
};

struct Action {
    ActionType type;

    // Card position belonging to the acting player.
    int position = -1;

    // Target player and card position for interactions.
    int targetPlayer = -1;
    int targetPosition = -1;

    // Second card location, for swaps and two-card powers.
    int secondTargetPlayer = -1;
    int secondTargetPosition = -1;

    // For a successful opponent flip, optionally give an extra card
    // instead of a card from one of the four fixed grid positions.
    // -1 means use `position` from the grid.
    int extraCardIndex = -1;

    // Used by the look-and-optionally-swap power.
    bool shouldSwap = false;
};

#endif
