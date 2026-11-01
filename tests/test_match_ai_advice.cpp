#include "Game.hpp"
#include "TacticalModel.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>

int main(){
    using namespace fu;
    Club neutral;
    neutral.tactic=1;neutral.pressing=1;neutral.tempo=1;neutral.width=1;
    auto steady=adaptiveAITactics(neutral,59,-2,90);
    assert(steady.tactic==neutral.tactic&&steady.pressing==neutral.pressing);
    auto chase=adaptiveAITactics(neutral,60,-1,90);
    assert(chase.tactic==2&&chase.pressing==2&&chase.tempo==2);
    auto tired=adaptiveAITactics(neutral,60,-1,59);
    assert(tired.tactic==4&&tired.pressing==0&&tired.tempo==2);
    auto defend=adaptiveAITactics(neutral,75,2,80);
    assert(defend.tactic==0&&defend.pressing==0&&defend.tempo==0);
    assert(neutral.tactic==1&&neutral.pressing==1); // AI orders are temporary

    assert(adviseLiveTactics({71,1,2,7,22,155,40,85}).plan==TacticalPlan::ProtectLead);
    assert(adviseLiveTactics({65,-1,2,7,22,155,40,88}).plan==TacticalPlan::Gegenpress);
    assert(adviseLiveTactics({65,-1,2,7,22,155,40,56}).plan==TacticalPlan::Counter);
    assert(adviseLiveTactics({39,0,0,2,0,25,61,88}).plan==TacticalPlan::Possession);
    assert(adviseLiveTactics({39,0,0,2,0,25,30,88}).plan==TacticalPlan::Counter);
    assert(adviseLiveTactics({15,0,0,0,0,0,50,90}).plan==TacticalPlan::Balanced);

    Game game;
    game.newCareer("Совет штаба",0,Role::Coach,Difficulty::Realistic,{0,1});
    game.advance(30);
    assert(game.startLiveMatch());
    assert(game.stepLiveMatch(25));
    const auto minute=game.live().minute;
    const auto lineups=game.live().homeXI;
    const auto beforeEvents=game.live().events.size();
    const auto advice=game.liveMatchAdviceText();
    assert(advice.find("ПОМОЩНИК ТРЕНЕРА")!=std::string::npos);
    assert(advice.find("Рекомендация")!=std::string::npos);
    const auto first=game.applyLiveMatchAdvice();
    assert(first.find("План принят")!=std::string::npos);
    assert(game.live().minute==minute&&game.live().homeXI==lineups);
    assert(game.live().events.size()>=beforeEvents);

    const auto savePath=std::filesystem::temp_directory_path()/"fu_0142_advice_save.fus";
    assert(game.save(savePath.string()).find("сохранена")!=std::string::npos);
    Game restored;
    assert(restored.load(savePath.string()).find("загружена")!=std::string::npos);
    std::filesystem::remove(savePath);
    assert(restored.liveMatchAdviceText()==game.liveMatchAdviceText());
    // Calling the adviser must not consume randomness or alter the saved match.
    const auto again=restored.liveMatchAdviceText();
    assert(again==game.liveMatchAdviceText());
    while(game.hasLiveMatch()){
        assert(game.stepLiveMatch(15));
        assert(restored.stepLiveMatch(15));
    }
    assert(game.latestMatchReport()==restored.latestMatchReport());
    assert(game.latestMatchReport().find("АНАЛИЗ ИГРЫ")!=std::string::npos);
    assert(game.latestMatchReport().find("Точность паса")!=std::string::npos);
    assert(game.latestMatchReport().find("xG/удар")!=std::string::npos);

    Game director;
    director.newCareer("Директор",0,Role::Director,Difficulty::Realistic,{0,1});
    director.advance(30);
    assert(director.startLiveMatch());
    const int oldTactic=director.myClub().tactic;
    assert(director.applyLiveMatchAdvice().find("Нет полномочий")!=std::string::npos);
    assert(director.myClub().tactic==oldTactic);
    std::cout<<"PASS: AI coaches adapt, assistant applies plan, safe saves and match reports\n";
}
