#include "ai/AI.h"
#include "GameEngine.h"
#include <cassert>
#include <iostream>

int main() {
    GameState state({"AI", "Human"});
    GameEngine engine(state);
    Action ready{}; ready.type=ActionType::CONFIRM_READY;
    std::string error;
    assert(engine.performAction(0, ready, error));
    assert(engine.performAction(1, ready, error));

    cambio::ai::MonteCarloAI ai(0, 100, 42);
    auto source=ai.chooseDrawSource(state);
    assert(source.action.type==ActionType::DRAW_FROM_DECK || source.action.type==ActionType::DRAW_FROM_DISCARD);
    assert(engine.performAction(0, source.action, error));
    auto drawn=engine.getPendingDrawForPlayer(0);
    assert(drawn.has_value());
    auto decision=ai.choosePendingAction(state,*drawn,source.action.type==ActionType::DRAW_FROM_DISCARD);
    assert(decision.action.type!=ActionType::CONFIRM_READY);
    std::cout << "AI runtime tests passed.\n";
}
