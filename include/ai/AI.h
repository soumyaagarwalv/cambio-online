#ifndef CAMBIO_AI_AI_H
#define CAMBIO_AI_AI_H

#include "Action.h"
#include "Card.h"
#include "GameState.h"

#include <random>
#include <string>
#include <vector>

namespace cambio::ai {

struct Decision {
    Action action{};
    double estimatedScore = 0.0;
    double winProbability = 0.0;
    int simulations = 0;
    std::string explanation;
};

class MonteCarloAI {
public:
    MonteCarloAI(int playerId, int simulations = 5000,
                 unsigned seed = std::random_device{}());

    Decision chooseDrawSource(const GameState& state) const;
    Decision choosePendingAction(const GameState& state,
                                  const Card& drawn,
                                  bool fromDiscard) const;

private:
    int playerId_;
    int simulations_;
    unsigned seed_;

    std::vector<Card> unseenCards(const GameState& state) const;
    std::vector<Action> candidateActions(const GameState& state,
                                          const Card& drawn,
                                          bool fromDiscard) const;
    double sampleActionScore(const GameState& state,
                             const Action& action,
                             const Card& drawn,
                             const std::vector<Card>& unseen,
                             std::mt19937& rng) const;
};

std::string actionName(ActionType type);
std::string cardRankName(Rank rank);

} // namespace cambio::ai

#endif
