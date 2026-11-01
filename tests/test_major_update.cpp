#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

int main(){
    using namespace fu;
    Game g;
    g.newCareer("Большое обновление",0,Role::Coach,Difficulty::Realistic,{0,1});
    assert(g.analyticsText().find("АНАЛИТИЧЕСКИЙ ЦЕНТР")!=std::string::npos);
    assert(g.analyticsText().find("ДОСЬЕ НА СОПЕРНИКА")!=std::string::npos);
    assert(g.boardReportText().find("Сезонная цель")!=std::string::npos);
    const auto starters=g.starters(0);
    assert(g.changeDefensiveLine().find("изменена")!=std::string::npos);
    assert(g.changeAttackingRisk().find("Риск атаки")!=std::string::npos);
    assert(g.changeSetPieces().find("стандартами")!=std::string::npos);
    assert(g.myClub().defensiveLine==2);
    assert(g.myClub().attackingRisk==2);
    assert(g.myClub().setPieces==2);
    assert(g.starters(0)==starters);
    const int balance=g.myClub().budget;
    const auto request=g.requestBoardInvestment();
    assert(request.find("Совет")!=std::string::npos);
    const int after=g.myClub().budget;
    assert(after>=balance);
    assert(g.requestBoardInvestment().find("уже рассмотрена")!=std::string::npos);
    assert(g.myClub().budget==after);
    assert(g.boardReportText().find("уже рассмотрено")!=std::string::npos);

    const auto save=(std::filesystem::temp_directory_path()/"fu_0150_major.fus").string();
    assert(g.save(save).find("сохранена")!=std::string::npos);
    Game loaded;
    assert(loaded.load(save).find("загружена")!=std::string::npos);
    assert(loaded.myClub().defensiveLine==2);
    assert(loaded.myClub().attackingRisk==2);
    assert(loaded.myClub().setPieces==2);
    assert(loaded.requestBoardInvestment().find("уже рассмотрена")!=std::string::npos);
    assert(loaded.analyticsText()==g.analyticsText());

    Game owner;
    owner.newCareer("Владелец",0,Role::Owner,Difficulty::Realistic,{0});
    assert(owner.changeDefensiveLine().find("Нет полномочий")!=std::string::npos);
    assert(owner.changeAttackingRisk().find("Нет полномочий")!=std::string::npos);
    assert(owner.changeSetPieces().find("Нет полномочий")!=std::string::npos);
    assert(owner.requestBoardInvestment().find("подаёт тренер")!=std::string::npos);

    loaded.advance(30);
    assert(loaded.startLiveMatch());
    const Match& match=loaded.matches().at(loaded.live().id);
    const bool opponentHome=match.away==loaded.teamId();
    const auto& opponentXI=opponentHome?loaded.live().homeXI:loaded.live().awayXI;
    assert(opponentXI.size()==11);
    // Force genuinely poor fitness, so the rival manager must react.
    auto& world=const_cast<std::vector<Player>&>(loaded.players());
    for(int id:opponentXI)for(auto& p:world)if(p.id==id&&p.position!="GK")
        p.fitness=30;
    for(int i=0;i<55;++i)assert(loaded.stepLiveMatch(1));
    const auto mid=(std::filesystem::temp_directory_path()/"fu_0150_major_live.fus").string();
    assert(loaded.save(mid).find("сохранена")!=std::string::npos);
    Game resumed;
    assert(resumed.load(mid).find("загружена")!=std::string::npos);
    assert(loaded.live().momentumH==resumed.live().momentumH);
    for(int i=0;i<25;++i){
        assert(loaded.stepLiveMatch(1));
        assert(resumed.stepLiveMatch(1));
        assert(loaded.live().momentumH==resumed.live().momentumH);
        assert(loaded.live().goalsH==resumed.live().goalsH);
        assert(loaded.live().goalsA==resumed.live().goalsA);
        assert(loaded.live().ballCarrierId==resumed.live().ballCarrierId);
    }
    const auto& l=loaded.live();
    const int rivalSubs=opponentHome?l.subsH:l.subsA;
    assert(rivalSubs>=1&&rivalSubs<=5);
    assert((opponentHome?l.usedHome.size():l.usedAway.size())==size_t(11+rivalSubs));
    int aiEvents=0;
    for(const auto& e:l.events)if(e.text.find("автоматическая замена")!=std::string::npos)++aiEvents;
    assert(aiEvents>=1);
    int totalH=0,totalA=0;
    for(const auto& stat:l.playerStats){
        const auto* p=loaded.findPlayer(stat.playerId);
        assert(p);
        if(p->club==match.home)totalH+=stat.passes;
        if(p->club==match.away)totalA+=stat.passes;
    }
    assert(totalH==l.passesH&&totalA==l.passesA);
    std::filesystem::remove(save);
    std::filesystem::remove(mid);
    std::cout<<"PASS: strategy sliders, board decisions, restrictions, AI bench, momentum and v15 saves\n";
}
