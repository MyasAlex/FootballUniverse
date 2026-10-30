#include "Game.hpp"
#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include <set>

static int roleGroup(const std::string& pos){
    if(pos=="GK")return 0;
    if(pos=="DL"||pos=="DC"||pos=="DR")return 1;
    if(pos=="ST"||pos=="AML"||pos=="AMR")return 3;
    return 2;
}

int main(){
    fu::Game g;
    // Each of the 36 clubs has exactly 11 starters, incl. one goalkeeper.
    for(const auto& c:g.clubs()){
        const auto xi=g.starters(c.id);
        assert(xi.size()==11);
        assert(std::set<int>(xi.begin(),xi.end()).size()==xi.size());
        int goalkeepers=0;
        for(int id:xi){const auto* p=g.findPlayer(id);assert(p&&p->club==c.id);goalkeepers+=(p->position=="GK");}
        assert(goalkeepers==1);
    }
    auto own=g.ownPlayers();
    int in=-1, out=-1;
    for(int playerId:own){
        const auto* p=g.findPlayer(playerId);
        if(p->starter||p->injured)continue;
        for(int started:own){
            const auto* first=g.findPlayer(started);
            if(first->starter&&roleGroup(first->position)==roleGroup(p->position)){
                in=playerId;out=started;break;
            }
        }
        if(in>=0)break;
    }
    assert(in>=0&&out>=0);
    assert(g.swapStarter(in,out).find("Стартовый состав")!=std::string::npos);
    assert(g.findPlayer(in)->starter&&!g.findPlayer(out)->starter);
    assert(g.starters(g.teamId()).size()==11);
    assert(g.swapStarter(in,out).find("Первый игрок должен быть запасным")!=std::string::npos);
    assert(g.cycleTrainingFocus(out).find("командный план")==std::string::npos);
    assert(g.findPlayer(out)->trainingFocus==0);
    for(int i=0;i<10;++i)g.cycleTrainingFocus(out);
    assert(g.findPlayer(out)->trainingFocus==-1);
    g.changeFormation();
    assert(g.starters(g.teamId()).size()==11);
    assert(g.lineupText().find("ОСНОВА (11/11)")!=std::string::npos);
    // Interactive match and saved in-progress lineup.
    g.advance(30);
    assert(g.matchDue());
    assert(g.startLiveMatch());
    assert(g.live().homeXI.size()==11&&g.live().awayXI.size()==11);
    assert(g.stepLiveMatch(13));
    const auto& live=g.live();
    bool home=g.upcomingMatch()->home==g.teamId();
    auto onField=home?live.homeXI:live.awayXI;
    auto used=home?live.usedHome:live.usedAway;
    int bench=-1, leaving=-1;
    for(int id:own){
        const auto* p=g.findPlayer(id);
        if(p->injured||std::find(used.begin(),used.end(),id)!=used.end())continue;
        for(int active:onField){
            const auto* q=g.findPlayer(active);
            if(q&&roleGroup(q->position)==roleGroup(p->position)){
                leaving=active;bench=id;break;
            }
        }
        if(bench>=0)break;
    }
    assert(bench>=0&&leaving>=0);
    assert(g.substitute(leaving,bench).find("Замена выполнена")!=std::string::npos);
    assert(g.substitute(leaving,bench).find("Выберите доступного запасного")!=std::string::npos);
    assert(g.substitute(bench,bench).find("повторный выход")!=std::string::npos);
    assert(g.live().subsH+g.live().subsA==1);
    assert(g.save("match_save_v4.fus").find("Карьера сохранена")!=std::string::npos);
    fu::Game saved;
    assert(saved.load("match_save_v4.fus").find("загружена")!=std::string::npos);
    assert(saved.hasLiveMatch());
    assert(saved.live().subsH+saved.live().subsA==1);
    assert(std::find(saved.live().usedHome.begin(),saved.live().usedHome.end(),bench)!=saved.live().usedHome.end() ||
           std::find(saved.live().usedAway.begin(),saved.live().usedAway.end(),bench)!=saved.live().usedAway.end());
    while(saved.hasLiveMatch())saved.stepLiveMatch(15);
    assert(saved.findPlayer(leaving)->appearances==1);
    assert(saved.findPlayer(bench)->appearances==1);
    // Reserve players who never took the field must not be marked as appeared.
    int resting=-1;
    for(int id:own){
        const auto* p=saved.findPlayer(id);
        if(p&&!p->starter&&p->appearances==0){resting=id;break;}
    }
    assert(resting>=0);
    assert(saved.findPlayer(resting)->appearances==0);
    assert(saved.matchArchive().find("Сыгранных матчей пока нет") == std::string::npos);
    assert(saved.save("post_match_v4.fus").find("сохранена")!=std::string::npos);
    fu::Game again;
    assert(again.load("post_match_v4.fus").find("загружена")!=std::string::npos);
    assert(again.findPlayer(bench)->appearances==1);
    assert(!again.hasLiveMatch());
    std::filesystem::remove("match_save_v4.fus");
    std::filesystem::remove("post_match_v4.fus");
    std::cout<<"PASS: lineup, selection, training focus, substitutions, player appearances, FUSAVE_4\\n";
}
