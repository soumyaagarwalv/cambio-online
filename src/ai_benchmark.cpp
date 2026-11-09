#include "ai/AI.h"
#include "GameEngine.h"
#include "GameState.h"

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace {
struct Stats { int games=0; int wins=0; double avgScore=0; double avgTurns=0; };

bool act(GameEngine& engine, int player, const Action& a) {
    std::string error;
    if (!engine.performAction(player, a, error)) return false;
    return true;
}

Stats runMatch(bool monteCarlo, int games, int simulations) {
    Stats stats;
    for (int g=0; g<games; ++g) {
        GameState state({"AI-0", "AI-1"});
        GameEngine engine(state);
        int turns = 0;

        for (int p=0; p<2; ++p) {
            Action r{}; r.type=ActionType::CONFIRM_READY;
            if (!act(engine,p,r)) break;
        }

        cambio::ai::MonteCarloAI ai0(0, simulations, 1000u+g);
        cambio::ai::MonteCarloAI ai1(1, simulations, 2000u+g);
        std::mt19937 rng(7000u+g);
        bool failed=false;

        while (state.getPhase()!=GamePhase::FINISHED && turns < 200) {
            ++turns;
            const int p=state.getCurrentPlayerIndex();
            cambio::ai::MonteCarloAI& ai = p==0 ? ai0 : ai1;

            // A deterministic stopping rule creates a complete final round.
            if (state.getPhase()==GamePhase::PLAYING && turns >= 18) {
                Action c{}; c.type=ActionType::CALL_CAMBIO;
                if (!act(engine,p,c)) { failed=true; break; }
                continue;
            }

            auto source = monteCarlo ? ai.chooseDrawSource(state)
                                     : cambio::ai::Decision{};
            if (!monteCarlo) {
                source.action.type = (state.isDiscardPileEmpty() || (rng()%2))
                    ? ActionType::DRAW_FROM_DECK : ActionType::DRAW_FROM_DISCARD;
            }
            if (!act(engine,p,source.action)) { failed=true; break; }

            auto drawn = engine.getPendingDrawForPlayer(p);
            if (!drawn) { failed=true; break; }
            const bool fromDiscard = source.action.type==ActionType::DRAW_FROM_DISCARD;

            cambio::ai::Decision decision;
            if (monteCarlo) {
                decision = ai.choosePendingAction(state,*drawn,fromDiscard);
            } else {
                Action a{};
                if (fromDiscard) {
                    a.type=ActionType::REPLACE_CARD_WITH_DRAWN;
                    a.position=rng()%4;
                    while (!state.getPlayers()[p].hasCardAtPosition(a.position)) a.position=rng()%4;
                } else if (drawn->getPower()==CardPower::NONE && (rng()%2==0)) {
                    a.type=ActionType::DISCARD_DRAWN_CARD;
                } else {
                    a.type=ActionType::REPLACE_CARD_WITH_DRAWN;
                    a.position=rng()%4;
                    while (!state.getPlayers()[p].hasCardAtPosition(a.position)) a.position=rng()%4;
                }
                decision.action=a;
            }

            if (!act(engine,p,decision.action)) {
                // Fall back to a safe replacement if a strategic power is
                // unavailable in a late-game edge case.
                Action fallback{};
                fallback.type=ActionType::REPLACE_CARD_WITH_DRAWN;
                fallback.position=0;
                for (int pos=0;pos<4;++pos) if (state.getPlayers()[p].hasCardAtPosition(pos)) { fallback.position=pos; break; }
                if (!act(engine,p,fallback)) { failed=true; break; }
            }
        }

        if (failed || state.getPhase()!=GamePhase::FINISHED) continue;
        ++stats.games;
        const auto& winners=engine.getWinnerPlayers();
        for (int w: winners) if (w==0) ++stats.wins;
        stats.avgScore += state.getPlayers()[0].calculateScore();
        stats.avgTurns += turns;
    }
    if (stats.games) {
        stats.avgScore/=stats.games;
        stats.avgTurns/=stats.games;
    }
    return stats;
}
}

int main(int argc,char**argv){
    int games=100;
    int simulations=1000;
    if(argc>1) games=std::max(1,std::stoi(argv[1]));
    if(argc>2) simulations=std::max(1,std::stoi(argv[2]));
    const auto start=std::chrono::steady_clock::now();
    const auto mc=runMatch(true,games,simulations);
    const auto random=runMatch(false,games,simulations);
    const auto end=std::chrono::steady_clock::now();
    const double seconds=std::chrono::duration<double>(end-start).count();
    auto print=[&](const char* name,const Stats& s){
        std::cout << name << ": games="<<s.games
                  << " wins(AI-0)="<<s.wins
                  << " win_rate="<<(s.games?100.0*s.wins/s.games:0.0)<<"%"
                  << " avg_score="<<s.avgScore
                  << " avg_turns="<<s.avgTurns<<"\n";
    };
    std::cout << "Cambio AI benchmark\n";
    print("MonteCarlo",mc);
    print("Random",random);
    std::cout << "simulations_per_decision="<<simulations
              << " total_runtime="<<seconds<<"s\n";
}
