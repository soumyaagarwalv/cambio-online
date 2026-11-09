#include "server/GameView.h"
#include "server/Protocol.h"

#include <cassert>
#include <iostream>

int main() {
    GameState state({"Alice", "Bob"});
    GameEngine engine(state);

    const GameView view = GameViewBuilder::build(state, engine, 0);
    assert(view.viewerId == 0);
    assert(view.phase == "MEMORIZATION");
    assert(view.players.size() == 2);

    // Initial rules: each player knows only positions 0 and 1 of their own hand.
    assert(view.players[0].cards[0].known);
    assert(view.players[0].cards[1].known);
    assert(!view.players[0].cards[2].known);
    assert(!view.players[0].cards[3].known);

    // Other player's cards must never be leaked just because this is the server view.
    assert(!view.players[1].cards[0].known);
    assert(!view.players[1].cards[1].known);

    // Once every player confirms readiness, the initial face-up cards are
    // turned face-down again. The identities are no longer exposed by the
    // server-side knowledge model; later peek powers can reveal them again.
    std::string error;
    assert(engine.performAction(0, Action{ActionType::CONFIRM_READY}, error));
    assert(engine.performAction(1, Action{ActionType::CONFIRM_READY}, error));

    const GameView playingView = GameViewBuilder::build(state, engine, 0);
    assert(playingView.phase == "PLAYING");
    for (const auto& card : playingView.players[0].cards) {
        if (card.occupied) assert(!card.known);
    }

    const std::string json = gameViewToJson(view);
    assert(json.find("HIDDEN") != std::string::npos);
    assert(json.find("MEMORIZATION") != std::string::npos);

    // Card faces carry a letter label (top-left) plus the point value (bottom-right).
    assert(json.find("\"label\"") != std::string::npos);
    assert(Card(Rank::QUEEN, Color::RED).getLabel() == "Q");
    assert(Card(Rank::QUEEN, Color::RED).toString() == "12");
    assert(Card(Rank::KING, Color::RED).toString() == "-1");
    assert(Card(Rank::KING, Color::BLACK).toString() == "13");
    assert(Card(Rank::JOKER, Color::NONE).toString() == "0");
    assert(Card(Rank::ACE, Color::BLACK).getLabel() == "A");

    const auto command = parseClientCommand(
        R"({"type":"action","action":"FLIP_CARD","position":1,"targetPlayer":2,"targetPosition":3})",
        error
    );
    assert(command.has_value());
    assert(command->type == ClientCommand::Type::ACTION);
    assert(command->action.type == ActionType::FLIP_CARD);
    assert(command->action.position == 1);
    assert(command->action.targetPlayer == 2);
    assert(command->action.targetPosition == 3);

    std::cout << "Server view/protocol tests passed.\n";
    return 0;
}
