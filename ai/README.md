# Cambio AI

The AI is deliberately separated from the authoritative engine.

## Information boundary

`MonteCarloAI` only uses the observer's `Player::knowledge` table plus public
state such as the top discard. It does not read unknown opponent card
identities from the true state.

## Monte Carlo layer

For a decision, the AI samples plausible identities for cards it has not seen,
then estimates the resulting score and relative opponent pressure. Candidate
actions are compared by their sampled expected score.

This is a first practical Monte Carlo policy. It is designed so a later version
can replace the evaluator with deeper multi-turn rollouts without changing the
server or game engine.
