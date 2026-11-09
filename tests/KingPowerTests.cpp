#include "GameEngine.h"
#include "server/GameView.h"
#include <cassert>
#include <iostream>

// Deals fresh games until player 0's first deck draw is a King, then exercises
// the two-step power: LOOK_TWO_CARDS (reveals both cards) -> DECIDE_SWAP.
static bool tryGame(bool swap) {
    GameState state({"A", "B"});
    GameEngine engine(state);
    std::string err;
    engine.performAction(0, Action{ActionType::CONFIRM_READY}, err);
    engine.performAction(1, Action{ActionType::CONFIRM_READY}, err);
    Action draw{ActionType::DRAW_FROM_DECK};
    engine.performAction(0, draw, err);
    if (engine.getPendingDrawPower() != CardPower::LOOK_AND_SWAP) return false;

    const Card mine = state.getPlayers()[0].getCard(2);
    const Card theirs = state.getPlayers()[1].getCard(3);

    Action look{ActionType::LOOK_TWO_CARDS};
    look.targetPlayer = 0; look.targetPosition = 2;
    look.secondTargetPlayer = 1; look.secondTargetPosition = 3;
    assert(engine.performAction(0, look, err));

    // Turn has not ended; the viewer is in the decision stage and knows both cards.
    assert(state.getCurrentPlayerIndex() == 0);
    assert(engine.hasPendingLookForPlayer(0));
    const GameView v = GameViewBuilder::build(state, engine, 0);
    assert(v.turnStage == "LOOK_DECISION");
    assert(v.players[0].cards[2].known && v.players[1].cards[3].known);
    assert(!GameViewBuilder::build(state, engine, 1).players[0].cards[2].known);

    // Nothing else is allowed until the decision is made.
    Action replace{ActionType::REPLACE_CARD_WITH_DRAWN}; replace.position = 0;
    assert(!engine.performAction(0, replace, err));

    Action decide{ActionType::DECIDE_SWAP};
    decide.shouldSwap = swap;
    assert(engine.performAction(0, decide, err));
    assert(state.getCurrentPlayerIndex() == 1);
    assert(!engine.hasPendingLookForPlayer(0));

    const Card nowMine = state.getPlayers()[0].getCard(2);
    const Card nowTheirs = state.getPlayers()[1].getCard(3);
    if (swap) {
        assert(nowMine.getRank() == theirs.getRank() && nowTheirs.getRank() == mine.getRank());
    } else {
        assert(nowMine.getRank() == mine.getRank() && nowTheirs.getRank() == theirs.getRank());
    }
    // A decision with nothing pending is rejected.
    assert(!engine.performAction(1, decide, err));
    return true;
}

int main() {
    for (bool swap : {true, false}) {
        bool ok = false;
        for (int i = 0; i < 2000 && !ok; ++i) ok = tryGame(swap);
        assert(ok);
    }
    std::cout << "King power tests passed.\n";
    return 0;
}
