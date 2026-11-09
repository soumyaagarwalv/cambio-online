
#include <iostream>
#include <string>
#include <vector>

#include "GameEngine.h"

int main() {
    std::vector<std::string> names = {
        "Player 0",
        "Player 1",
        "Player 2",
        "Player 3"
    };

    GameState game(names);
    GameEngine engine(game);

    std::string error;

    // TEST 1: Game should wait for everyone to be ready.
    if (game.getPhase() != GamePhase::MEMORIZATION) {
        std::cerr << "FAIL: Incorrect initial phase.\n";
        return 1;
    }

    std::cout << "PASS: Game starts in MEMORIZATION.\n";

    // TEST 2: Confirm each player.
    for (int p = 0; p < game.getPlayerCount(); p++) {
        Action readyAction;
        readyAction.type = ActionType::CONFIRM_READY;

        if (!engine.performAction(p, readyAction, error)) {
            std::cerr << "FAIL: Ready action failed: "
                      << error << '\n';
            return 1;
        }

        // The game must not start until the final confirmation.
        if (p < game.getPlayerCount() - 1 &&
            game.getPhase() != GamePhase::MEMORIZATION) {
            std::cerr << "FAIL: Game started too early.\n";
            return 1;
        }
    }

    if (game.getPhase() != GamePhase::PLAYING) {
        std::cerr << "FAIL: Game did not start after everyone was ready.\n";
        return 1;
    }

    std::cout << "PASS: Game starts after all players confirm.\n";

    // TEST 3: It is Player 0's turn.
    if (game.getCurrentPlayerIndex() != 0) {
        std::cerr << "FAIL: Unexpected starting player.\n";
        return 1;
    }

    // Player 1 must not be allowed to draw out of turn.
    Action drawAction;
    drawAction.type = ActionType::DRAW_FROM_DECK;

    if (engine.performAction(1, drawAction, error)) {
        std::cerr << "FAIL: Out-of-turn draw was allowed.\n";
        return 1;
    }

    std::cout << "PASS: Out-of-turn actions are rejected.\n";

    // TEST 4: Player 0 draws from the deck.
    if (!engine.performAction(0, drawAction, error)) {
        std::cerr << "FAIL: Draw failed: " << error << '\n';
        return 1;
    }

    auto ownDraw = engine.getPendingDrawForPlayer(0);
    auto otherDraw = engine.getPendingDrawForPlayer(1);

    if (!ownDraw.has_value()) {
        std::cerr << "FAIL: Drawing player cannot see their card.\n";
        return 1;
    }

    if (otherDraw.has_value()) {
        std::cerr << "FAIL: Another player can see the drawn card.\n";
        return 1;
    }

    std::cout << "PASS: Only the drawing player can see the drawn card.\n";
    std::cout << "Player 0 drew: " << ownDraw->toString() << '\n';

    // TEST 5: Replace position 2 with the drawn card.
    Card oldCard = game.getPlayers()[0].getCard(2);

    Action replaceAction;
    replaceAction.type = ActionType::REPLACE_CARD_WITH_DRAWN;
    replaceAction.position = 2;

    if (!engine.performAction(0, replaceAction, error)) {
        std::cerr << "FAIL: Replacement failed: " << error << '\n';
        return 1;
    }

    if (game.getPlayers()[0].getCard(2).toString()
        != ownDraw->toString()) {
        std::cerr << "FAIL: New card was not placed correctly.\n";
        return 1;
    }

    if (game.getTopDiscard().toString() != oldCard.toString()) {
        std::cerr << "FAIL: Replaced card was not discarded.\n";
        return 1;
    }

    if (game.getPlayers()[0].getKnownCard(0, 2)->toString()
        != ownDraw->toString()) {
        std::cerr << "FAIL: Player did not remember the replacement.\n";
        return 1;
    }

    if (engine.getPendingDrawForPlayer(0).has_value()) {
        std::cerr << "FAIL: Draw was not cleared after replacement.\n";
        return 1;
    }

    if (game.getCurrentPlayerIndex() != 1) {
        std::cerr << "FAIL: Turn did not advance.\n";
        return 1;
    }

    std::cout << "PASS: Replacement, knowledge, discard, and turn advance work.\n";

    std::cout << "\nAll current engine tests passed!\n";
    return 0;
}
