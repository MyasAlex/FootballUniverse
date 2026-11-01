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

TacticalRecommendation adviseLiveTactics(const MatchSituation& situation){
    const int minute=std::clamp(situation.minute,0,90);
    const int fitness=std::clamp(situation.averageFitness,0,100);
    const int possession=std::clamp(situation.possessionPercent,0,100);
    if(minute>=70&&situation.goalDifference>0)
        return {TacticalPlan::ProtectLead,
            "Мы ведём в концовке: снизить темп, закрыть зоны и сохранить преимущество."};
    if(minute>=55&&situation.goalDifference<0){
        if(fitness>=76)
            return {TacticalPlan::Gegenpress,
                "Мы уступаем: свежая команда может усилить прессинг и увеличить число атак."};
        return {TacticalPlan::Counter,
            "Мы уступаем, но команда устала: быстрые контратаки без изнуряющего прессинга."};
    }
    if(minute>=35&&situation.shotsFor<=1&&situation.xgFor<=20){
        if(possession<42)return {TacticalPlan::Counter,
            "Мало владения и моментов: отойдём ниже и попробуем выходить в быстрые атаки."};
        return {TacticalPlan::Possession,
            "Мяч у нас, но моментов мало: улучшить точность и движение через центр."};
    }
    if(minute>=35&&situation.xgAgainst>situation.xgFor+100&&fitness<75)
        return {TacticalPlan::Counter,
            "Соперник создаёт опаснее, а запас сил ограничен: снизить риск при обороне."};
    if(minute>=35&&possession<38&&fitness>=82)
        return {TacticalPlan::Gegenpress,
            "Соперник контролирует мяч: свежие игроки могут навязать более высокий прессинг."};
    return {TacticalPlan::Balanced,
        "По текущим показателям резкая перестройка не нужна: сохраняем баланс."};
}

Club adaptiveAITactics(const Club& base,int minute,int goalDifference,int averageFitness){
    Club temporary=base;
    if(minute>=75&&goalDifference>0){
        temporary.tactic=0;temporary.pressing=0;temporary.tempo=0;temporary.width=0;
    }else if(minute>=60&&goalDifference<0){
        if(averageFitness>=70){
            temporary.tactic=2;temporary.pressing=2;temporary.tempo=2;temporary.width=2;
        }else{
            temporary.tactic=4;temporary.pressing=0;temporary.tempo=2;temporary.width=2;
        }
    }
    return temporary;
}

int matchFatigueCost(const Club& club,int stamina){
    const bool highPress=club.tactic==3||club.pressing>=2;
    int cost=1+(highPress?1:0)+(club.tempo>=2?1:0);
    if(std::clamp(stamina,0,100)>=80&&cost>1)--cost;
    return std::clamp(cost,1,3);
}

} // namespace fu
