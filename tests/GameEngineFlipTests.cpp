#include "GameEngine.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;
int testsRun = 0;

void check(bool condition, const std::string& message) {
    ++testsRun;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    } else {
        std::cout << "PASS: " << message << '\n';
    }
}

void makeGameReady(GameEngine& engine, GameState& state) {
    std::string error;
    for (int player = 0; player < state.getPlayerCount(); ++player) {
        Action action{};
        action.type = ActionType::CONFIRM_READY;
        if (!engine.performAction(player, action, error)) {
            std::cerr << "Could not ready test game: " << error << '\n';
            std::exit(2);
        }
    }
}

void setCard(GameState& state, int player, int position, const Card& card) {
    Player::replaceCardForAllObservers(
        state.getPlayers(), player, position, card
    );
    // Simulate the owner knowing the card they have seen/selected in this test.
    state.getPlayers()[player].observeCard(player, position, card);
}

Action ownFlip(int position) {
    Action action{};
    action.type = ActionType::FLIP_CARD;
    action.position = position;
    action.targetPlayer = -1;
    return action;
}

void testOwnSuccessfulFlipAndOnePerDiscard() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 1, 2, Card(Rank::TWO, Color::RED));
    setCard(state, 2, 0, Card(Rank::TWO, Color::BLACK));
    state.addToDiscardPile(Card(Rank::TWO, Color::BLACK), 0);

    const int before = state.getPlayers()[1].getCardCount();
    std::string error;

    check(engine.performAction(1, ownFlip(2), error),
          "correct own flip is accepted out of turn");
    check(!state.getPlayers()[1].hasCardAtPosition(2),
          "successful own flip leaves the grid position empty");
    check(state.getPlayers()[1].getCardCount() == before - 1,
          "successful own flip reduces the player's card count");
    check(state.hasSuccessfulFlipOnTopDiscard(),
          "successful flip is recorded on the current discard");
    check(state.getTopDiscard().getRank() == Rank::TWO &&
          state.getTopDiscard().getColor() == Color::RED,
          "the correctly flipped card is placed on the discard pile");
    check(state.getTopDiscardOwner() == 1,
          "the flipper owns the discard created by their flip");
    check(!engine.getEvents().empty() &&
          engine.getEvents().back().publicCard.has_value(),
          "a correct flip publishes the flipped card to everyone");

    error.clear();
    check(!engine.performAction(2, ownFlip(0), error),
          "a second successful flip on the same discard is rejected");
}

void testSuccessfulOpponentFlipTransfersCard() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 1, 3, Card(Rank::ACE, Color::RED));
    setCard(state, 2, 1, Card(Rank::THREE, Color::BLACK));
    state.addToDiscardPile(Card(Rank::THREE, Color::RED), 0);

    Action flip{};
    flip.type = ActionType::FLIP_CARD;
    flip.targetPlayer = 2;
    flip.targetPosition = 1;

    std::string error;
    const int flipperBefore = state.getPlayers()[1].getCardCount();
    const int targetBefore = state.getPlayers()[2].getCardCount();

    check(engine.performAction(1, flip, error),
          "correct opponent flip is accepted");
    check(!state.getPlayers()[2].hasCardAtPosition(1),
          "the correctly flipped opponent card leaves their grid");
    check(state.getTopDiscard().getRank() == Rank::THREE &&
          state.getTopDiscard().getColor() == Color::BLACK &&
          state.getTopDiscardOwner() == 1,
          "the correctly flipped opponent card goes to the discard pile");
    check(state.getPlayers()[1].getCardCount() == flipperBefore,
          "the flipper keeps their cards until choosing the gift");
    check(state.getPlayers()[2].getCardCount() == targetBefore - 1,
          "the target is one card down until the gift arrives");
    check(engine.hasPendingFlipGiftForPlayer(1),
          "the flipper is prompted to choose a gift card");

    Action gift{};
    gift.type = ActionType::GIVE_CARD_AFTER_FLIP;
    gift.position = 3;
    check(engine.performAction(1, gift, error),
          "flipper can give a grid card after the correct flip");
    check(!state.getPlayers()[1].hasCardAtPosition(3),
          "the gifted card leaves the flipper's grid");
    check(state.getPlayers()[1].getCardCount() == flipperBefore - 1,
          "flipper loses the chosen gift card");
    check(state.getPlayers()[2].getExtraCardCount() == 1,
          "the opponent receives the gift as an extra card");
    check(state.getPlayers()[2].getCardCount() == targetBefore,
          "the opponent ends up with the same number of cards as before");
}

void testOpponentFlipCanTransferExtraCard() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 2, 1, Card(Rank::THREE, Color::BLACK));
    state.getPlayers()[1].addExtraCard(Card(Rank::QUEEN, Color::RED));
    state.addToDiscardPile(Card(Rank::THREE, Color::RED), 0);

    Action flip{};
    flip.type = ActionType::FLIP_CARD;
    flip.targetPlayer = 2;
    flip.targetPosition = 1;

    std::string error;
    const int flipperBefore = state.getPlayers()[1].getCardCount();

    check(engine.performAction(1, flip, error),
          "successful opponent flip enters gift selection");

    Action gift{};
    gift.type = ActionType::GIVE_CARD_AFTER_FLIP;
    gift.extraCardIndex = 0;
    check(engine.performAction(1, gift, error),
          "successful opponent flip can transfer an extra card");
    check(state.getPlayers()[1].getExtraCardCount() == 0,
          "transferred extra card leaves the flipper's hand");
    check(state.getPlayers()[2].getExtraCardCount() == 1,
          "transferred extra card enters the opponent's hand");
    check(state.getPlayers()[1].getCardCount() == flipperBefore - 1,
          "flipper's total card count decreases after giving an extra card");
    check(!engine.hasPendingFlipGiftForPlayer(1),
          "gift selection is cleared after transfer");
}

void testWrongOpponentFlipDoesNotRequireGiftChoice() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 2, 1, Card(Rank::THREE, Color::BLACK));
    state.addToDiscardPile(Card(Rank::TWO, Color::RED), 0);

    Action action{};
    action.type = ActionType::FLIP_CARD;
    action.targetPlayer = 2;
    action.targetPosition = 1;
    // No gift card is selected: this should be fine for a wrong flip.

    std::string error;
    const int before = state.getPlayers()[1].getCardCount();

    check(engine.performAction(1, action, error),
          "wrong opponent flip is processed without selecting a gift card");
    check(state.getPlayers()[1].getCardCount() == before + 1,
          "wrong opponent flip adds the penalty card");
}


void testOpponentFlipExchangeDoesNotEliminateTargetAtSixCards() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 2, 1, Card(Rank::THREE, Color::BLACK));
    state.getPlayers()[2].addExtraCard(Card(Rank::FOUR, Color::RED));
    state.getPlayers()[2].addExtraCard(Card(Rank::FIVE, Color::BLACK));
    state.addToDiscardPile(Card(Rank::THREE, Color::RED), 0);

    Action flip{};
    flip.type = ActionType::FLIP_CARD;
    flip.targetPlayer = 2;
    flip.targetPosition = 1;

    std::string error;
    check(state.getPlayers()[2].getCardCount() == 6,
          "opponent-flip exchange test starts at six cards");
    check(engine.performAction(1, flip, error),
          "successful opponent flip is processed when target has six cards");

    Action gift{};
    gift.type = ActionType::GIVE_CARD_AFTER_FLIP;
    gift.position = 3;
    check(engine.performAction(1, gift, error),
          "flipper gives a card after the successful flip");
    check(state.getPlayers()[2].getCardCount() == 6,
          "the matched card is replaced by the gift, so the count stays six");
    check(state.getPhase() == GamePhase::PLAYING,
          "the exchange does not eliminate the target");
}

void testWrongFlipAddsPenaltyCard() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 1, 0, Card(Rank::ACE, Color::RED));
    state.addToDiscardPile(Card(Rank::TWO, Color::BLACK), 0);

    std::string error;
    const int before = state.getPlayers()[1].getCardCount();
    const int deckBefore = state.getDeck().size();

    check(engine.performAction(1, ownFlip(0), error),
          "wrong flip is processed as a penalty rather than rejected");
    check(state.getPlayers()[1].getCardCount() == before + 1,
          "wrong flip adds exactly one penalty card");
    check(state.getDeck().size() == deckBefore - 1,
          "wrong flip draws the penalty from the deck");
    check(state.getPhase() == GamePhase::PLAYING,
          "wrong flip does not end the game when the player has six or fewer cards");
    check(state.getPlayers()[1].hasCardAtPosition(0),
          "the wrongly flipped card stays in the grid");
    check(state.getTopDiscard().getRank() == Rank::TWO &&
          state.getTopDiscardOwner() == 0,
          "a wrong flip leaves the discard pile untouched");
    check(!engine.getEvents().empty() &&
          engine.getEvents().back().type == GameEventType::FLIP_FAILED &&
          engine.getEvents().back().publicCard.has_value(),
          "a wrong flip also publishes the flipped card to everyone");
}

void testWrongFlipEliminatesAboveSixCards() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 1, 0, Card(Rank::ACE, Color::RED));
    state.getPlayers()[1].addExtraCard(Card(Rank::FOUR, Color::RED));
    state.getPlayers()[1].addExtraCard(Card(Rank::FIVE, Color::RED));
    state.addToDiscardPile(Card(Rank::TWO, Color::BLACK), 0);

    std::string error;
    check(state.getPlayers()[1].getCardCount() == 6,
          "penalty test starts with six cards");

    check(engine.performAction(1, ownFlip(0), error),
          "wrong flip is processed at the six-card boundary");
    check(state.getPlayers()[1].getCardCount() == 7,
          "penalty pushes the player above six cards");
    check(state.getPhase() == GamePhase::FINISHED,
          "player exceeding six cards ends the game");
    check(engine.getPenaltyLoser() == 1,
          "engine records the player eliminated by the penalty");
}

void testCannotFlipOwnDiscard() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 1, 0, Card(Rank::TWO, Color::RED));
    state.addToDiscardPile(Card(Rank::TWO, Color::BLACK), 1);

    std::string error;
    check(!engine.performAction(1, ownFlip(0), error),
          "player cannot flip the discard they made");
    check(error.find("just made") != std::string::npos,
          "own-discard rejection explains the restriction");
}

void testFlipAllowedInTheMiddleOfAnotherPlayersTurn() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 2, 1, Card(Rank::NINE, Color::RED));
    state.addToDiscardPile(Card(Rank::NINE, Color::BLACK), 0);

    std::string error;
    const int current = state.getCurrentPlayerIndex();
    Action draw{};
    draw.type = ActionType::DRAW_FROM_DECK;
    check(engine.performAction(current, draw, error),
          "current player draws and is now mid-turn");

    // Every other player can flip while that draw is still unresolved.
    int flipper = (current == 2) ? 1 : 2;
    if (flipper == 2) {
        check(engine.performAction(2, ownFlip(1), error),
              "another player flips correctly during someone else's turn");
    } else {
        setCard(state, 1, 1, Card(Rank::NINE, Color::RED));
        check(engine.performAction(1, ownFlip(1), error),
              "another player flips correctly during someone else's turn");
    }
    check(engine.hasPendingDrawForPlayer(current),
          "the current player's drawn card is still pending after the flip");

    Action replace{};
    replace.type = ActionType::REPLACE_CARD_WITH_DRAWN;
    replace.position = 0;
    check(engine.performAction(current, replace, error),
          "the current player can still finish their turn");
}

void testDiscardOwnerAndFlipStatusAreRestored() {
    GameState state({"A", "B", "C"});

    state.addToDiscardPile(Card(Rank::FOUR, Color::RED), 1);
    state.markSuccessfulFlipOnTopDiscard();
    state.addToDiscardPile(Card(Rank::FIVE, Color::BLACK), 2);

    check(state.getTopDiscardOwner() == 2,
          "top discard remembers its own owner");

    const Card drawn = state.takeTopDiscard();
    check(drawn.getRank() == Rank::FIVE,
          "drawing from discard returns the top card");

    check(state.getTopDiscardOwner() == 1,
          "removing the top discard restores the previous discard owner");
    check(state.hasSuccessfulFlipOnTopDiscard(),
          "previous discard retains its successful-flip status");
}


void setPlayerAllCards(GameState& state, int player, Rank rank, Color color) {
    for (int position = 0; position < 4; ++position) {
        setCard(state, player, position, Card(rank, color));
    }
}

void takeFinalTurnByReplacingFromDiscard(
    GameEngine& engine,
    GameState& state,
    int playerIndex,
    int position = 0
) {
    std::string error;
    Action draw{};
    draw.type = ActionType::DRAW_FROM_DISCARD;
    check(engine.performAction(playerIndex, draw, error),
          "final-round player can draw from the discard pile");

    Action replace{};
    replace.type = ActionType::REPLACE_CARD_WITH_DRAWN;
    replace.position = position;
    check(engine.performAction(playerIndex, replace, error),
          "final-round player can complete their final turn");
}

void testCambioMustBeCalledBeforeDrawing() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    std::string error;
    Action draw{};
    draw.type = ActionType::DRAW_FROM_DECK;
    check(engine.performAction(0, draw, error),
          "current player can draw before attempting the rule test");

    Action call{};
    call.type = ActionType::CALL_CAMBIO;
    check(!engine.performAction(0, call, error),
          "Cambio cannot be called after drawing");
    check(state.getPhase() == GamePhase::PLAYING,
          "invalid Cambio call does not start the final round");
}

void testCambioStartsFinalRoundAndProtectsCallerFromFlips() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setCard(state, 0, 0, Card(Rank::ACE, Color::RED));
    state.addToDiscardPile(Card(Rank::ACE, Color::BLACK), 2);

    std::string error;
    Action call{};
    call.type = ActionType::CALL_CAMBIO;
    check(engine.performAction(0, call, error),
          "Cambio can be called before drawing on the player's turn");
    check(state.getPhase() == GamePhase::FINAL_ROUND,
          "calling Cambio starts the final round");
    check(engine.getCambioCaller() == 0,
          "engine records the Cambio caller");
    check(state.getCurrentPlayerIndex() == 1,
          "the next player starts the final round");

    Action flip{};
    flip.type = ActionType::FLIP_CARD;
    flip.targetPlayer = 0;
    flip.targetPosition = 0;
    flip.position = 1;
    check(!engine.performAction(1, flip, error),
          "opponents cannot flip the Cambio caller's cards");
    check(error.find("protected") != std::string::npos,
          "caller-card protection explains the rejected flip");
}

void testFinalRoundEndsAfterEveryNonCallerTakesOneTurn() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setPlayerAllCards(state, 0, Rank::ACE, Color::RED);
    setPlayerAllCards(state, 1, Rank::TEN, Color::RED);
    setPlayerAllCards(state, 2, Rank::TEN, Color::BLACK);
    state.addToDiscardPile(Card(Rank::TEN, Color::RED), -1);

    std::string error;
    Action call{};
    call.type = ActionType::CALL_CAMBIO;
    check(engine.performAction(0, call, error),
          "caller starts final round");

    takeFinalTurnByReplacingFromDiscard(engine, state, 1);
    check(state.getPhase() == GamePhase::FINAL_ROUND,
          "game remains in final round after the first opponent's turn");
    check(state.getCurrentPlayerIndex() == 2,
          "turn advances to the remaining opponent");

    takeFinalTurnByReplacingFromDiscard(engine, state, 2);
    check(state.getPhase() == GamePhase::FINISHED,
          "game finishes after every non-caller takes one final turn");
    check(engine.getWinnerPlayers().size() == 1 &&
          engine.getWinnerPlayers()[0] == 0,
          "caller wins when their score is strictly lower than everyone else's");
}

void testCambioCallerLosesOnTieAndNonCallerWins() {
    GameState state({"A", "B", "C"});
    GameEngine engine(state);
    makeGameReady(engine, state);

    setPlayerAllCards(state, 0, Rank::ACE, Color::RED);
    setPlayerAllCards(state, 1, Rank::ACE, Color::BLACK);
    setPlayerAllCards(state, 2, Rank::ACE, Color::RED);
    state.addToDiscardPile(Card(Rank::ACE, Color::BLACK), -1);

    std::string error;
    Action call{};
    call.type = ActionType::CALL_CAMBIO;
    check(engine.performAction(0, call, error),
          "tied-score test starts final round");

    takeFinalTurnByReplacingFromDiscard(engine, state, 1);
    takeFinalTurnByReplacingFromDiscard(engine, state, 2);

    check(state.getPhase() == GamePhase::FINISHED,
          "tied-score final round finishes");
    check(engine.getWinnerPlayers().size() == 2,
          "all non-callers tied for the lowest score are recorded as winners");
    check(engine.getWinnerPlayers()[0] != 0 &&
          engine.getWinnerPlayers()[1] != 0,
          "Cambio caller is excluded from winners when tied for lowest score");
}

} // namespace

int main() {
    testOwnSuccessfulFlipAndOnePerDiscard();
    testSuccessfulOpponentFlipTransfersCard();
    testOpponentFlipCanTransferExtraCard();
    testWrongOpponentFlipDoesNotRequireGiftChoice();
    testOpponentFlipExchangeDoesNotEliminateTargetAtSixCards();
    testWrongFlipAddsPenaltyCard();
    testWrongFlipEliminatesAboveSixCards();
    testCannotFlipOwnDiscard();
    testFlipAllowedInTheMiddleOfAnotherPlayersTurn();
    testDiscardOwnerAndFlipStatusAreRestored();
    testCambioMustBeCalledBeforeDrawing();
    testCambioStartsFinalRoundAndProtectsCallerFromFlips();
    testFinalRoundEndsAfterEveryNonCallerTakesOneTurn();
    testCambioCallerLosesOnTieAndNonCallerWins();

    std::cout << "\n" << (testsRun - failures) << "/" << testsRun
              << " checks passed.\n";

    return failures == 0 ? 0 : 1;
}
