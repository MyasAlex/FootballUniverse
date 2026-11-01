#include "TacticalModel.hpp"
#include "Game.hpp"
#include <algorithm>

namespace fu {

TacticalEffect evaluateTactics(const Club& own,const Club& opponent,int averageFitness){
    TacticalEffect effect;
    // Styles are choices with different costs; their numeric IDs are not strength.
    switch(own.tactic){
        case 0: effect={-5,-3,-1,2};break;  // defensive block
        case 2: effect={2,3,1,-1};break;   // attacking football
        case 3: effect={4,1,0,-2};break;   // high press
        case 4: effect={-5,-1,2,-1};break; // counterattacks
        default: break;                   // balanced
    }

    const int pressing=std::clamp(own.pressing,0,2);
    const int tempo=std::clamp(own.tempo,0,2);
    const int width=std::clamp(own.width,0,2);
    const int opposingPress=std::clamp(opponent.pressing,0,2);
    effect.possession+=(pressing-1)*2+(1-tempo)*2;
    effect.shots+=(pressing==2?1:0)+(tempo-1)*3+(width-1);
    effect.accuracy+=(1-tempo)*4-(opposingPress==2?3:0);
    if(width==2)--effect.possession;

    // A compact defence closes space. Committing players forward opens it.
    if(opponent.tactic==0)effect.quality-=3;
    if(opponent.tactic==2)effect.quality+=2;
    if(opponent.tactic==3)++effect.quality;

    // Direct attacks can exploit space behind a press, but do not automatically
    // win possession or beat a settled low block.
    if(own.tactic==4&&(opponent.tactic==3||opposingPress==2)){
        effect.quality+=3;
        effect.shots+=2;
    }

    // A depleted team cannot sustain the same pressing advantage indefinitely.
    if(own.tactic==3||pressing==2){
        const int tiredness=std::clamp((70-std::clamp(averageFitness,0,100))/10,0,6);
        effect.possession-=tiredness*2;
        effect.shots-=tiredness/2;
        effect.accuracy-=tiredness;
    }

    effect.possession=std::clamp(effect.possession,-12,12);
    effect.shots=std::clamp(effect.shots,-8,8);
    effect.quality=std::clamp(effect.quality,-6,6);
    effect.accuracy=std::clamp(effect.accuracy,-10,10);
    return effect;
}

int matchFatigueCost(const Club& club,int stamina){
    const bool highPress=club.tactic==3||club.pressing>=2;
    int cost=1+(highPress?1:0)+(club.tempo>=2?1:0);
    if(std::clamp(stamina,0,100)>=80&&cost>1)--cost;
    return std::clamp(cost,1,3);
}

} // namespace fu
