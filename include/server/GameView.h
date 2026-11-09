#ifndef CAMBIO_SERVER_GAME_VIEW_H
#define CAMBIO_SERVER_GAME_VIEW_H

#include "GameState.h"
#include "GameEngine.h"
#include <optional>
#include <string>
#include <vector>

struct VisibleCard {
    int position = -1;
    bool occupied = false;
    bool known = false;
    std::string value;   // point value as text
    std::string label;   // A, 2-10, J, Q, K, JKR
    std::string color = "NONE";
};

struct PlayerView {
    int playerId = -1;
    std::string name;
    int cardCount = 0;
    int extraCardCount = 0;
    bool ready = false;
    std::vector<VisibleCard> cards;
    std::vector<std::string> extraCards; // viewer's own extras (all players' extras once finished)
    std::vector<std::string> extraLabels;
    std::vector<std::string> extraColors;
    int score = 0;       // only meaningful once the game is finished
};

// A flip is public: every player sees the flipped card (briefly, client-side).
// Only present in the broadcast immediately after the flip happened.
struct FlipView {
    bool success = false;
    int flipper = -1;
    int targetPlayer = -1;
    int position = -1;
    std::string value;
    std::string label;
    std::string color = "NONE";
};

struct GameView {
    int viewerId = -1;
    std::string phase;
    std::string turnStage;
    std::string prompt;
    int currentPlayer = -1;
    int cambioCaller = -1;
    std::vector<int> winnerPlayers;
    std::vector<PlayerView> players;
    std::optional<std::string> topDiscard;
    std::string topDiscardLabel;
    std::string topDiscardColor = "NONE";
    int topDiscardOwner = -1;
    std::optional<std::string> pendingDraw;
    std::string pendingDrawLabel;
    bool pendingDrawFromDiscard = false;
    std::string pendingDrawPower = "NONE";
    std::string pendingDrawColor = "NONE";
    bool pendingLook = false;                // King: waiting for swap decision
    int pendingLookCards[4] = {-1, -1, -1, -1};
    bool pendingFlipGift = false;
    int pendingFlipGiftTarget = -1;
    std::optional<FlipView> flip;
};

class GameViewBuilder {
public:
    static GameView build(const GameState& state,
                          const GameEngine& engine,
                          int viewerId);
};

std::string gameViewToJson(const GameView& view);

#endif
