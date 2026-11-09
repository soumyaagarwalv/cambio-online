#include "ai/AI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <limits>
#include <sstream>

namespace cambio::ai {
namespace {
std::vector<Card> fullDeck() {
    std::vector<Card> cards;
    const std::array<Rank, 13> ranks = {
        Rank::ACE, Rank::TWO, Rank::THREE, Rank::FOUR, Rank::FIVE,
        Rank::SIX, Rank::SEVEN, Rank::EIGHT, Rank::NINE, Rank::TEN,
        Rank::JACK, Rank::QUEEN, Rank::KING
    };
    for (Rank r : ranks) {
        cards.emplace_back(r, Color::RED);
        cards.emplace_back(r, Color::BLACK);
    }
    cards.emplace_back(Rank::JOKER, Color::NONE);
    cards.emplace_back(Rank::JOKER, Color::NONE);
    return cards;
}

bool sameCard(const Card& a, const Card& b) {
    return a.getRank() == b.getRank() && a.getColor() == b.getColor();
}

int expectedPoints(const std::vector<Card>& cards) {
    if (cards.empty()) return 0;
    double total = 0.0;
    for (const auto& c : cards) total += c.getPoints();
    return static_cast<int>(std::lround(total / cards.size()));
}
}

std::string cardRankName(Rank rank) {
    switch (rank) {
        case Rank::ACE: return "A";
        case Rank::TWO: return "2";
        case Rank::THREE: return "3";
        case Rank::FOUR: return "4";
        case Rank::FIVE: return "5";
        case Rank::SIX: return "6";
        case Rank::SEVEN: return "7";
        case Rank::EIGHT: return "8";
        case Rank::NINE: return "9";
        case Rank::TEN: return "10";
        case Rank::JACK: return "J";
        case Rank::QUEEN: return "Q";
        case Rank::KING: return "K";
        case Rank::JOKER: return "Joker";
    }
    return "?";
}

std::string actionName(ActionType type) {
    switch (type) {
        case ActionType::CONFIRM_READY: return "CONFIRM_READY";
        case ActionType::DRAW_FROM_DECK: return "DRAW_FROM_DECK";
        case ActionType::DRAW_FROM_DISCARD: return "DRAW_FROM_DISCARD";
        case ActionType::REPLACE_CARD_WITH_DRAWN: return "REPLACE_CARD_WITH_DRAWN";
        case ActionType::DISCARD_DRAWN_CARD: return "DISCARD_DRAWN_CARD";
        case ActionType::USE_POWER_AND_DISCARD_DRAWN: return "USE_POWER_AND_DISCARD_DRAWN";
        case ActionType::PEEK_OWN_CARD: return "PEEK_OWN_CARD";
        case ActionType::PEEK_OPPONENT_CARD: return "PEEK_OPPONENT_CARD";
        case ActionType::BLIND_SWAP: return "BLIND_SWAP";
        case ActionType::LOOK_AND_OPTIONALLY_SWAP: return "LOOK_AND_OPTIONALLY_SWAP";
        case ActionType::LOOK_TWO_CARDS: return "LOOK_TWO_CARDS";
        case ActionType::DECIDE_SWAP: return "DECIDE_SWAP";
        case ActionType::FLIP_CARD: return "FLIP_CARD";
        case ActionType::GIVE_CARD_AFTER_FLIP: return "GIVE_CARD_AFTER_FLIP";
        case ActionType::CALL_CAMBIO: return "CALL_CAMBIO";
    }
    return "UNKNOWN";
}

MonteCarloAI::MonteCarloAI(int playerId, int simulations, unsigned seed)
    : playerId_(playerId), simulations_(std::max(1, simulations)), seed_(seed) {}

std::vector<Card> MonteCarloAI::unseenCards(const GameState& state) const {
    std::vector<Card> remaining = fullDeck();
    const auto& observer = state.getPlayers().at(playerId_);

    for (int target = 0; target < state.getPlayerCount(); ++target) {
        for (int pos = 0; pos < 4; ++pos) {
            auto known = observer.getKnownCard(target, pos);
            if (!known) continue;
            auto it = std::find_if(remaining.begin(), remaining.end(),
                [&](const Card& c) { return sameCard(c, *known); });
            if (it != remaining.end()) remaining.erase(it);
        }
    }

    if (!state.isDiscardPileEmpty()) {
        const Card top = state.getTopDiscard();
        auto it = std::find_if(remaining.begin(), remaining.end(),
            [&](const Card& c) { return sameCard(c, top); });
        if (it != remaining.end()) remaining.erase(it);
    }

    return remaining;
}

std::vector<Action> MonteCarloAI::candidateActions(const GameState& state,
                                                    const Card& drawn,
                                                    bool fromDiscard) const {
    std::vector<Action> actions;
    const auto& players = state.getPlayers();
    const auto& me = players.at(playerId_);

    for (int pos = 0; pos < 4; ++pos) {
        if (!me.hasCardAtPosition(pos)) continue;
        Action a{};
        a.type = ActionType::REPLACE_CARD_WITH_DRAWN;
        a.position = pos;
        actions.push_back(a);
    }

    if (!fromDiscard && drawn.getPower() == CardPower::NONE) {
        Action a{};
        a.type = ActionType::DISCARD_DRAWN_CARD;
        actions.push_back(a);
    }

    if (fromDiscard) return actions;

    if (drawn.getPower() == CardPower::PEEK_OWN) {
        for (int pos = 0; pos < 4; ++pos) {
            if (!me.hasCardAtPosition(pos)) continue;
            Action a{}; a.type = ActionType::PEEK_OWN_CARD; a.position = pos;
            actions.push_back(a);
        }
    } else if (drawn.getPower() == CardPower::PEEK_OPPONENT) {
        for (int p = 0; p < state.getPlayerCount(); ++p) {
            if (p == playerId_) continue;
            for (int pos = 0; pos < 4; ++pos) {
                if (!players[p].hasCardAtPosition(pos)) continue;
                Action a{}; a.type = ActionType::PEEK_OPPONENT_CARD;
                a.targetPlayer = p; a.targetPosition = pos;
                actions.push_back(a);
            }
        }
    } else if (drawn.getPower() == CardPower::BLIND_SWAP ||
               drawn.getPower() == CardPower::LOOK_AND_SWAP) {
        std::vector<std::pair<int,int>> occupied;
        for (int p = 0; p < state.getPlayerCount(); ++p) {
            for (int pos = 0; pos < 4; ++pos) {
                if (players[p].hasCardAtPosition(pos)) occupied.emplace_back(p, pos);
            }
        }
        for (std::size_t i = 0; i < occupied.size(); ++i) {
            for (std::size_t j = i + 1; j < occupied.size(); ++j) {
                Action a{};
                a.type = drawn.getPower() == CardPower::BLIND_SWAP
                    ? ActionType::BLIND_SWAP
                    : ActionType::LOOK_AND_OPTIONALLY_SWAP;
                a.targetPlayer = occupied[i].first;
                a.targetPosition = occupied[i].second;
                a.secondTargetPlayer = occupied[j].first;
                a.secondTargetPosition = occupied[j].second;
                a.shouldSwap = true;
                actions.push_back(a);
            }
        }
    }

    return actions;
}

double MonteCarloAI::sampleActionScore(const GameState& state,
                                        const Action& action,
                                        const Card& drawn,
                                        const std::vector<Card>& unseen,
                                        std::mt19937& rng) const {
    const auto& me = state.getPlayers().at(playerId_);
    std::vector<int> points;
    std::vector<int> unknownPositions;

    for (int pos = 0; pos < 4; ++pos) {
        if (!me.hasCardAtPosition(pos)) continue;
        auto known = me.getKnownCard(playerId_, pos);
        if (known) points.push_back(known->getPoints());
        else {
            points.push_back(0);
            unknownPositions.push_back(static_cast<int>(points.size() - 1));
        }
    }

    std::vector<Card> pool = unseen;
    std::shuffle(pool.begin(), pool.end(), rng);
    for (std::size_t i = 0; i < unknownPositions.size() && i < pool.size(); ++i)
        points[unknownPositions[i]] = pool[i].getPoints();

    if (action.type == ActionType::REPLACE_CARD_WITH_DRAWN && action.position >= 0) {
        int index = 0;
        for (int pos = 0; pos < action.position; ++pos)
            if (me.hasCardAtPosition(pos)) ++index;
        if (index < static_cast<int>(points.size())) points[index] = drawn.getPoints();
    }

    double ownScore = std::accumulate(points.begin(), points.end(), 0.0);
    double opponentExpected = 0.0;
    int opponents = 0;
    const int unknownMean = expectedPoints(unseen);
    for (int p = 0; p < state.getPlayerCount(); ++p) {
        if (p == playerId_) continue;
        double score = 0.0;
        int count = 0;
        for (int pos = 0; pos < 4; ++pos) {
            if (!state.getPlayers()[p].hasCardAtPosition(pos)) continue;
            auto known = me.getKnownCard(p, pos);
            score += known ? known->getPoints() : unknownMean;
            ++count;
        }
        if (count) {
            opponentExpected += score;
            ++opponents;
        }
    }
    if (opponents) opponentExpected /= opponents;

    // Lower own score and a larger gap to opponents are better.
    return ownScore - 0.15 * opponentExpected;
}

Decision MonteCarloAI::choosePendingAction(const GameState& state,
                                           const Card& drawn,
                                           bool fromDiscard) const {
    const auto candidates = candidateActions(state, drawn, fromDiscard);
    if (candidates.empty()) return {};

    const auto unseen = unseenCards(state);
    std::mt19937 rng(seed_);
    double bestScore = std::numeric_limits<double>::infinity();
    Action best = candidates.front();
    double bestWin = 0.0;

    for (const Action& action : candidates) {
        double total = 0.0;
        int wins = 0;
        for (int i = 0; i < simulations_; ++i) {
            const double score = sampleActionScore(state, action, drawn, unseen, rng);
            total += score;
            if (score <= 0.0) ++wins;
        }
        const double mean = total / simulations_;
        if (mean < bestScore) {
            bestScore = mean;
            best = action;
            bestWin = static_cast<double>(wins) / simulations_;
        }
    }

    Decision d;
    d.action = best;
    d.estimatedScore = bestScore;
    d.winProbability = bestWin;
    d.simulations = simulations_;
    d.explanation = "Selected " + actionName(best.type) +
                    " using hidden-state Monte Carlo sampling.";
    return d;
}

Decision MonteCarloAI::chooseDrawSource(const GameState& state) const {
    Decision d;
    d.simulations = simulations_;

    if (state.isDiscardPileEmpty()) {
        d.action.type = ActionType::DRAW_FROM_DECK;
        d.explanation = "Discard pile is empty.";
        return d;
    }

    const auto unseen = unseenCards(state);
    const auto& me = state.getPlayers().at(playerId_);
    const int unknownMean = expectedPoints(unseen);

    int replaceReference = unknownMean;
    for (int pos = 0; pos < 4; ++pos) {
        if (!me.hasCardAtPosition(pos)) continue;
        auto known = me.getKnownCard(playerId_, pos);
        replaceReference = std::max(replaceReference,
                                    known ? known->getPoints() : unknownMean);
    }

    std::mt19937 rng(seed_);
    double deckGain = 0.0;
    for (int i = 0; i < simulations_; ++i) {
        const int sample = unseen.empty() ? unknownMean
            : unseen[static_cast<std::size_t>(rng() % unseen.size())].getPoints();
        deckGain += sample - replaceReference;
    }
    deckGain /= simulations_;

    const int discardGain = state.getTopDiscard().getPoints() - replaceReference;
    if (discardGain < deckGain) {
        d.action.type = ActionType::DRAW_FROM_DISCARD;
        d.estimatedScore = discardGain;
        d.explanation = "Public discard has lower expected replacement cost.";
    } else {
        d.action.type = ActionType::DRAW_FROM_DECK;
        d.estimatedScore = deckGain;
        d.explanation = "Monte Carlo sampling favors the deck.";
    }
    return d;
}

} // namespace cambio::ai
