#include "Game.hpp"
#include "TacticalModel.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace {
const std::vector<int>& ownXI(const fu::Game& game){
    const auto& fixture=game.matches().at(game.live().id);
    return fixture.home==game.teamId()?game.live().homeXI:game.live().awayXI;
}
int minutesPlayed(const fu::Game& game,int playerId){
    for(const auto& stat:game.live().playerStats)
        if(stat.playerId==playerId)return stat.minutes;
    return 0;
}
void liveFatigueIntegration(){
    fu::Game initial;
    initial.advance(30);
    assert(initial.matchDue());
    assert(initial.startLiveMatch());
    initial.myClub().tactic=1;
    initial.myClub().pressing=1;
    initial.myClub().tempo=1;
    initial.myClub().width=1;
    int starterKeeper=-1,benchKeeper=-1;
    for(int id:initial.ownPlayers()){
        const auto* player=initial.findPlayer(id);
        if(player->position!="GK")continue;
        if(std::find(ownXI(initial).begin(),ownXI(initial).end(),id)!=ownXI(initial).end())
            starterKeeper=id;
        else if(!player->injured&&!player->suspension)benchKeeper=id;
    }
    assert(starterKeeper>0&&benchKeeper>0);
    const int starterFitness=initial.findPlayer(starterKeeper)->fitness;
    const int benchFitness=initial.findPlayer(benchKeeper)->fitness;

    // Copies begin with precisely the same world and RNG state. Tactics are
    // allowed to change subsequent events, while their physical cost is fixed.
    auto balancedGame=initial,intenseGame=initial;
    intenseGame.myClub().tactic=3;
    intenseGame.myClub().pressing=2;
    intenseGame.myClub().tempo=2;
    assert(balancedGame.stepLiveMatch(14));
    assert(intenseGame.stepLiveMatch(14));
    assert(balancedGame.findPlayer(starterKeeper)->fitness==starterFitness);
    assert(intenseGame.findPlayer(starterKeeper)->fitness==starterFitness);
    assert(balancedGame.stepLiveMatch(1));
    assert(intenseGame.stepLiveMatch(1));
    assert(minutesPlayed(balancedGame,starterKeeper)==15);
    assert(minutesPlayed(intenseGame,starterKeeper)==15);
    const int intenseCost=fu::matchFatigueCost(intenseGame.myClub(),
                                            intenseGame.findPlayer(starterKeeper)->skill[7]);
    assert(balancedGame.findPlayer(starterKeeper)->fitness==starterFitness-1);
    assert(intenseGame.findPlayer(starterKeeper)->fitness==starterFitness-intenseCost);
    assert(intenseCost>1);
    for(int id:initial.ownPlayers()){
        if(std::find(ownXI(initial).begin(),ownXI(initial).end(),id)==ownXI(initial).end()){
            assert(balancedGame.findPlayer(id)->fitness==initial.findPlayer(id)->fitness);
            assert(intenseGame.findPlayer(id)->fitness==initial.findPlayer(id)->fitness);
        }
    }

    // A substitute entering in minute 14 has played one minute at the global
    // quarter-hour, and should first pay a full interval's cost in minute 29.
    auto late=initial;
    assert(late.stepLiveMatch(14));
    assert(late.substitute(starterKeeper,benchKeeper).find("Замена")!=std::string::npos);
    assert(minutesPlayed(late,starterKeeper)==14);
    assert(minutesPlayed(late,benchKeeper)==0);
    const auto path=std::filesystem::temp_directory_path()/
        ("fu_tactical_fatigue_"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count())+".fus");
    assert(late.save(path.string()).find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load(path.string()).find("загружена")!=std::string::npos);
    std::filesystem::remove(path);
    assert(late.stepLiveMatch(1));
    assert(restored.stepLiveMatch(1));
    assert(late.findPlayer(benchKeeper)->fitness==benchFitness);
    assert(restored.findPlayer(benchKeeper)->fitness==benchFitness);
    assert(late.findPlayer(starterKeeper)->fitness==starterFitness);
    assert(late.stepLiveMatch(13));
    assert(restored.stepLiveMatch(13));
    assert(minutesPlayed(late,benchKeeper)==14);
    assert(late.findPlayer(benchKeeper)->fitness==benchFitness);
    assert(late.stepLiveMatch(1));
    assert(restored.stepLiveMatch(1));
    assert(minutesPlayed(late,benchKeeper)==15);
    assert(late.findPlayer(benchKeeper)->fitness==benchFitness-1);
    assert(restored.findPlayer(benchKeeper)->fitness==benchFitness-1);
    assert(late.findPlayer(starterKeeper)->fitness==starterFitness);
    assert(late.live().events.size()==restored.live().events.size());
    assert(late.live().passesH==restored.live().passesH);
    assert(late.live().passesA==restored.live().passesA);
    assert(late.live().goalsH==restored.live().goalsH);
    assert(late.live().goalsA==restored.live().goalsA);
    while(late.hasLiveMatch()){
        assert(late.stepLiveMatch(15));
        assert(restored.stepLiveMatch(15));
    }
    assert(!restored.hasLiveMatch());
    assert(late.matchArchive()==restored.matchArchive());
    for(const auto& player:late.players())
        assert(player.fitness==restored.findPlayer(player.id)->fitness);
}
} // namespace

int main(){
    fu::Club balanced;
    const auto neutral=fu::evaluateTactics(balanced,balanced,90);
    assert(neutral.possession==0&&neutral.shots==0&&
           neutral.quality==0&&neutral.accuracy==0);

    auto control=balanced;
    control.tempo=0;
    const auto measured=fu::evaluateTactics(control,balanced,90);
    assert(measured.possession>neutral.possession);
    assert(measured.accuracy>neutral.accuracy);
    assert(measured.shots<neutral.shots);

    auto attack=balanced;
    attack.tactic=2;attack.tempo=2;attack.width=2;
    const auto aggressive=fu::evaluateTactics(attack,balanced,90);
    assert(aggressive.shots>neutral.shots);
    assert(aggressive.accuracy<neutral.accuracy);
    // An attacking side also exposes better chances to its opponent.
    assert(fu::evaluateTactics(balanced,attack,90).quality>neutral.quality);

    auto press=balanced;
    press.tactic=3;press.pressing=2;press.tempo=2;
    auto counter=balanced;
    counter.tactic=4;
    const auto direct=fu::evaluateTactics(counter,press,90);
    const auto settled=fu::evaluateTactics(counter,balanced,90);
    assert(direct.quality>settled.quality&&direct.shots>settled.shots);
    // Counterattacks have the highest style ID, but concede possession.
    assert(settled.possession<neutral.possession);

    auto block=balanced;
    block.tactic=0;block.pressing=0;block.tempo=0;
    assert(fu::evaluateTactics(counter,block,90).quality<settled.quality);
    const auto freshPress=fu::evaluateTactics(press,balanced,90);
    const auto tiredPress=fu::evaluateTactics(press,balanced,30);
    assert(freshPress.possession>tiredPress.possession);
    assert(freshPress.accuracy>tiredPress.accuracy);
    assert(freshPress.shots>=tiredPress.shots);

    assert(fu::matchFatigueCost(balanced,60)==1);
    assert(fu::matchFatigueCost(press,60)==3);
    assert(fu::matchFatigueCost(press,90)<fu::matchFatigueCost(press,60));
    assert(fu::matchFatigueCost(balanced,100)==1);

    // Every supported combination remains modest; malformed fitness values
    // cannot turn a tactical modifier into an unbounded score bonus.
    for(int style=0;style<5;++style)for(int opposingStyle=0;opposingStyle<5;++opposingStyle)
    for(int pressing=0;pressing<3;++pressing)for(int tempo=0;tempo<3;++tempo)
    for(int width=0;width<3;++width)for(int fitness:{-100,0,30,70,100,200}){
        auto own=balanced,opponent=balanced;
        own.tactic=style;own.pressing=pressing;own.tempo=tempo;own.width=width;
        opponent.tactic=opposingStyle;
        const auto result=fu::evaluateTactics(own,opponent,fitness);
        assert(result.possession>=-12&&result.possession<=12);
        assert(result.shots>=-8&&result.shots<=8);
        assert(result.quality>=-6&&result.quality<=6);
        assert(result.accuracy>=-10&&result.accuracy<=10);
        assert(fu::matchFatigueCost(own,fitness)>=1&&fu::matchFatigueCost(own,fitness)<=3);
    }
    liveFatigueIntegration();
    std::cout<<"PASS: tactical tradeoffs, counterattack matchups, fatigue boundaries, substitutes and save/load\n";
}
