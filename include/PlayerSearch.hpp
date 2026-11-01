#pragma once

#include <string>
#include <vector>

namespace fu {

class Game;
struct Player;

enum class PlayerSort {
    ValueAscending, AbilityDescending, PotentialDescending,
    AgeAscending, WageAscending, Name
};

struct PlayerSearchFilter {
    std::string name;
    std::string position;
    int minAge=15,maxAge=50,maxValue=500000000,maxWage=20000000;
    int minAbility=0,minPotential=0;
    bool shortlistOnly=false,activeLeaguesOnly=false,realOnly=false;
    PlayerSort sort=PlayerSort::ValueAscending;
};

// Base ability for the player's position, without temporary fitness or morale.
int playerSearchAbility(const Player& player);

// Search the whole world, excluding the user's club. Names also match club
// names; ASCII, Russian and common accented Latin case pairs are supported.
// There is no hidden result limit. Equal sort keys are ordered by player ID.
std::vector<int> searchPlayers(const Game& game,const PlayerSearchFilter& filter);

} // namespace fu
