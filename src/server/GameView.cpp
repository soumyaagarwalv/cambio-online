#include "server/GameView.h"

#include <sstream>
#include <stdexcept>

namespace {
std::string escapeJson(const std::string& s) {
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c;
        }
    }
    return out.str();
}

std::string phaseName(GamePhase phase) {
    switch (phase) {
        case GamePhase::MEMORIZATION: return "MEMORIZATION";
        case GamePhase::PLAYING: return "PLAYING";
        case GamePhase::FINAL_ROUND: return "FINAL_ROUND";
        case GamePhase::FINISHED: return "FINISHED";
    }
    return "UNKNOWN";
}


std::string colorName(Color color) {
    switch (color) {
        case Color::RED: return "RED";
        case Color::BLACK: return "BLACK";
        case Color::NONE: return "NONE";
    }
    return "NONE";
}

std::string powerName(CardPower power) {
    switch (power) {
        case CardPower::NONE: return "NONE";
        case CardPower::PEEK_OWN: return "PEEK_OWN";
        case CardPower::PEEK_OPPONENT: return "PEEK_OPPONENT";
        case CardPower::BLIND_SWAP: return "BLIND_SWAP";
        case CardPower::LOOK_AND_SWAP: return "LOOK_AND_SWAP";
    }
    return "NONE";
}
}

GameView GameViewBuilder::build(const GameState& state,
                                const GameEngine& engine,
                                int viewerId) {
    if (viewerId < 0 || viewerId >= state.getPlayerCount())
        throw std::invalid_argument("Invalid viewer id");

    GameView view;
    view.viewerId = viewerId;
    view.phase = phaseName(state.getPhase());
    view.currentPlayer = state.getCurrentPlayerIndex();
    view.cambioCaller = engine.getCambioCaller();
    view.winnerPlayers = engine.getWinnerPlayers();

    if (state.getPhase() == GamePhase::MEMORIZATION) {
        view.turnStage = "MEMORIZATION";
        view.prompt = engine.isReady(viewerId)
            ? "You are ready. Wait for everyone else to memorize and confirm."
            : "Memorize your two face-up cards, then press I'm Ready.";
    } else if (state.getPhase() == GamePhase::FINISHED) {
        view.turnStage = "FINISHED";
        view.prompt = "Game finished.";
    } else if (engine.hasPendingFlipGiftForPlayer(viewerId)) {
        view.turnStage = "FLIP_GIFT";
        view.prompt = "Correct flip! Choose one card to give the opponent.";
    } else if (engine.hasPendingLookForPlayer(viewerId)) {
        view.turnStage = "LOOK_DECISION";
        view.prompt = "Swap the two cards you looked at, or leave them.";
        view.pendingLook = true;
        for (int i = 0; i < 4; ++i) view.pendingLookCards[i] = engine.getPendingLookCards()[i];
    } else if (engine.hasPendingDrawForPlayer(viewerId)) {
        view.turnStage = "AFTER_DRAW";
        view.prompt = "You drew a card. Now choose what to do with it.";
    } else if (state.getCurrentPlayerIndex() == viewerId) {
        view.turnStage = "DRAW";
        view.prompt = state.getPhase() == GamePhase::FINAL_ROUND
            ? "Your final turn: draw a card, then resolve it."
            : "Your turn: draw a card from the deck or discard pile. You may also call Cambio before drawing.";
    } else {
        view.turnStage = "WAITING";
        const auto& players = state.getPlayers();
        view.prompt = "Waiting for " + players[state.getCurrentPlayerIndex()].getName() + " to play.";
    }

    const auto& players = state.getPlayers();
    for (int p = 0; p < state.getPlayerCount(); ++p) {
        const Player& player = players[p];
        PlayerView pv;
        pv.playerId = p;
        pv.name = player.getName();
        pv.cardCount = player.getCardCount();
        pv.extraCardCount = player.getExtraCardCount();
        pv.ready = engine.isReady(p);
        const bool finished = state.getPhase() == GamePhase::FINISHED;
        if (finished) pv.score = player.calculateScore();

        for (int pos = 0; pos < 4; ++pos) {
            VisibleCard card;
            card.position = pos;
            card.occupied = player.hasCardAtPosition(pos);
            if (card.occupied) {
                // At the end of the game every hand is revealed.
                auto known = finished
                    ? std::optional<Card>(player.getCard(pos))
                    : players[viewerId].getKnownCard(p, pos);
                if (known) {
                    card.known = true;
                    card.value = known->toString();
                    card.label = known->getLabel();
                    card.color = colorName(known->getColor());
                } else {
                    card.value = "HIDDEN";
                }
            } else {
                card.value = "EMPTY";
            }
            pv.cards.push_back(std::move(card));
        }

        if (p == viewerId || finished) {
            for (int i = 0; i < player.getExtraCardCount(); ++i) {
                const Card extra = player.getExtraCard(i);
                pv.extraCards.push_back(extra.toString());
                pv.extraLabels.push_back(extra.getLabel());
                pv.extraColors.push_back(colorName(extra.getColor()));
            }
        }

        view.players.push_back(std::move(pv));
    }

    if (!state.isDiscardPileEmpty()) {
        view.topDiscard = state.getTopDiscard().toString();
        view.topDiscardLabel = state.getTopDiscard().getLabel();
        view.topDiscardColor = colorName(state.getTopDiscard().getColor());
        view.topDiscardOwner = state.getTopDiscardOwner();
    }
    view.pendingFlipGift = engine.hasPendingFlipGiftForPlayer(viewerId);
    view.pendingFlipGiftTarget = engine.getPendingFlipGiftTarget();

    if (auto pending = engine.getPendingDrawForPlayer(viewerId); pending) {
        view.pendingDraw = pending->toString();
        view.pendingDrawLabel = pending->getLabel();
        view.pendingDrawColor = colorName(pending->getColor());
        view.pendingDrawFromDiscard = engine.isPendingDrawFromDiscard();
        view.pendingDrawPower = powerName(engine.getPendingDrawPower());
    }

    for (const GameEvent& ev : engine.getEvents()) {
        const bool isFlip = ev.type == GameEventType::FLIP_SUCCEEDED ||
                            ev.type == GameEventType::FLIP_FAILED;
        if (!isFlip || !ev.publicCard) continue;
        FlipView f;
        f.success = ev.type == GameEventType::FLIP_SUCCEEDED;
        f.flipper = ev.player;
        f.targetPlayer = ev.targetPlayer;
        f.position = ev.targetPosition;
        f.value = ev.publicCard->toString();
        f.label = ev.publicCard->getLabel();
        f.color = colorName(ev.publicCard->getColor());
        view.flip = f;
    }

    return view;
}

std::string gameViewToJson(const GameView& view) {
    std::ostringstream out;
    out << "{\"type\":\"state\""
        << ",\"viewerId\":" << view.viewerId
        << ",\"phase\":\"" << escapeJson(view.phase) << "\""
        << ",\"turnStage\":\"" << escapeJson(view.turnStage) << "\""
        << ",\"prompt\":\"" << escapeJson(view.prompt) << "\""
        << ",\"currentPlayer\":" << view.currentPlayer
        << ",\"cambioCaller\":" << view.cambioCaller
        << ",\"players\":[";

    for (std::size_t i = 0; i < view.players.size(); ++i) {
        if (i) out << ',';
        const auto& p = view.players[i];
        out << "{\"playerId\":" << p.playerId
            << ",\"name\":\"" << escapeJson(p.name) << "\""
            << ",\"cardCount\":" << p.cardCount
            << ",\"extraCardCount\":" << p.extraCardCount
            << ",\"ready\":" << (p.ready ? "true" : "false")
            << ",\"score\":" << p.score
            << ",\"cards\":[";
        for (std::size_t j = 0; j < p.cards.size(); ++j) {
            if (j) out << ',';
            const auto& c = p.cards[j];
            out << "{\"position\":" << c.position
                << ",\"occupied\":" << (c.occupied ? "true" : "false")
                << ",\"known\":" << (c.known ? "true" : "false")
                << ",\"value\":\"" << escapeJson(c.value) << "\""
                << ",\"label\":\"" << escapeJson(c.label) << "\""
                << ",\"color\":\"" << escapeJson(c.color) << "\"}";
        }
        out << "]";
        out << ",\"extraCards\": [";
        for (std::size_t k = 0; k < p.extraCards.size(); ++k) {
            if (k) out << ',';
            out << "\"" << escapeJson(p.extraCards[k]) << "\"";
        }
        out << "],\"extraLabels\":[";
        for (std::size_t k = 0; k < p.extraLabels.size(); ++k) {
            if (k) out << ',';
            out << "\"" << escapeJson(p.extraLabels[k]) << "\"";
        }
        out << "],\"extraColors\":[";
        for (std::size_t k = 0; k < p.extraColors.size(); ++k) {
            if (k) out << ',';
            out << "\"" << escapeJson(p.extraColors[k]) << "\"";
        }
        out << "]}";
    }
    out << ']';

    if (view.topDiscard) {
        out << ",\"topDiscard\":\"" << escapeJson(*view.topDiscard) << "\""
            << ",\"topDiscardLabel\":\"" << escapeJson(view.topDiscardLabel) << "\""
            << ",\"topDiscardColor\":\"" << escapeJson(view.topDiscardColor) << "\""
            << ",\"topDiscardOwner\":" << view.topDiscardOwner;
    } else {
        out << ",\"topDiscard\":null,\"topDiscardOwner\":-1";
    }

    if (view.pendingDraw) {
        out << ",\"pendingDraw\":\"" << escapeJson(*view.pendingDraw) << "\""
            << ",\"pendingDrawLabel\":\"" << escapeJson(view.pendingDrawLabel) << "\"";
    } else {
        out << ",\"pendingDraw\":null";
    }

    out << ",\"pendingDrawFromDiscard\":" << (view.pendingDrawFromDiscard ? "true" : "false")
        << ",\"pendingDrawPower\":\"" << escapeJson(view.pendingDrawPower) << "\""
        << ",\"pendingDrawColor\":\"" << escapeJson(view.pendingDrawColor) << "\""
        << ",\"pendingLook\":" << (view.pendingLook ? "true" : "false")
        << ",\"pendingLookCards\":[" << view.pendingLookCards[0] << "," << view.pendingLookCards[1]
        << "," << view.pendingLookCards[2] << "," << view.pendingLookCards[3] << "]"
        << ",\"pendingFlipGift\":" << (view.pendingFlipGift ? "true" : "false")
        << ",\"pendingFlipGiftTarget\":" << view.pendingFlipGiftTarget
        << ",\"winners\":[";

    for (std::size_t i = 0; i < view.winnerPlayers.size(); ++i) {
        if (i) out << ',';
        out << view.winnerPlayers[i];
    }
    out << ']';

    if (view.flip) {
        const auto& f = *view.flip;
        out << ",\"flip\":{\"success\":" << (f.success ? "true" : "false")
            << ",\"flipper\":" << f.flipper
            << ",\"targetPlayer\":" << f.targetPlayer
            << ",\"position\":" << f.position
            << ",\"value\":\"" << escapeJson(f.value) << "\""
            << ",\"label\":\"" << escapeJson(f.label) << "\""
            << ",\"color\":\"" << escapeJson(f.color) << "\"}";
    } else {
        out << ",\"flip\":null";
    }
    out << '}';
    return out.str();
}
