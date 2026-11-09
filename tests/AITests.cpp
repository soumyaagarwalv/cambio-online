#include "ai/AI.h"

#include <cassert>
#include <iostream>

int main() {
    GameState state({"AI", "Opponent"});
    cambio::ai::MonteCarloAI ai(0, 250, 12345);

    auto drawDecision = ai.chooseDrawSource(state);
    assert(drawDecision.action.type == ActionType::DRAW_FROM_DECK ||
           drawDecision.action.type == ActionType::DRAW_FROM_DISCARD);

    std::string error;
    Action ready{};
    ready.type = ActionType::CONFIRM_READY;
    assert(!state.getPlayers().empty());

    std::cout << "AI tests passed\n";
    return 0;
}
