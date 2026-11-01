#pragma once

namespace fu {

struct Club;
enum class TacticalPlan : int;

// Live recommendations depend only on visible match information. No RNG or saves.
struct MatchSituation {
    int minute=0, goalDifference=0, shotsFor=0, shotsAgainst=0;
    int xgFor=0, xgAgainst=0, possessionPercent=50, averageFitness=90;
};
struct TacticalRecommendation {
    TacticalPlan plan;
    const char* reason;
};
TacticalRecommendation adviseLiveTactics(const MatchSituation& situation);

// AI manager reactions are temporary match instructions: never mutate the club.
Club adaptiveAITactics(const Club& base,int minute,int goalDifference,int averageFitness);


// Relative modifiers to the minute simulation, not predictions of a match result.
// Each side's possession modifier is compared with its opponent's modifier.
struct TacticalEffect {
    int possession=0, shots=0, quality=0, accuracy=0;
};

// Includes style, pressing, tempo, width and the opposing tactical choices.
// Reading a plan has no effect on the world or its random number generator.
TacticalEffect evaluateTactics(const Club& own,const Club& opponent,int averageFitness);

// Fitness points spent per fifteen minutes by a player currently on the pitch.
int matchFatigueCost(const Club& club,int stamina);

} // namespace fu
