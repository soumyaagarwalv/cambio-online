#include "GameEngine.h"
#include <cassert>
#include <iostream>

int main() {
    GameState state({"A","B","C"});
    GameEngine engine(state);
    std::string error;
    Action ready{}; ready.type=ActionType::CONFIRM_READY;
    assert(engine.performAction(0,ready,error));
    assert(engine.performAction(1,ready,error));
    assert(engine.performAction(2,ready,error));
    assert(state.getPhase()==GamePhase::PLAYING);

    Action cambio{}; cambio.type=ActionType::CALL_CAMBIO;
    const int caller=state.getCurrentPlayerIndex();
    assert(engine.performAction(caller,cambio,error));
    assert(state.getPhase()==GamePhase::FINAL_ROUND);

    // Two non-callers get exactly one final turn each.
    for(int i=0;i<2;++i){
        int p=state.getCurrentPlayerIndex();
        Action draw{}; draw.type=ActionType::DRAW_FROM_DECK;
        assert(engine.performAction(p,draw,error));
        Action replace{}; replace.type=ActionType::REPLACE_CARD_WITH_DRAWN; replace.position=0;
        if(!state.getPlayers()[p].hasCardAtPosition(0)) replace.position=1;
        assert(engine.performAction(p,replace,error));
    }
    assert(state.getPhase()==GamePhase::FINISHED);
    assert(!engine.getWinnerPlayers().empty());
    std::cout<<"Engine rules tests passed.\n";
}
