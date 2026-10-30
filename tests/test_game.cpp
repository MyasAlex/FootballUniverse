#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>
int main(){
    fu::Game g;
    assert(g.clubs().size()==36);
    assert(g.leagues().size()==6);
    assert(g.players().size()==36*24);
    assert(g.matches().size()==6*90);
    for(int lid=0;lid<6;++lid){
        int count=0;int gamesForTeam=0;
        int team=lid*6;
        for(const auto& m:g.matches())if(g.clubs()[m.home].league==lid){
            ++count;
            if(m.home==team||m.away==team)++gamesForTeam;
            assert(m.season==2026);
        }
        assert(count==90&&gamesForTeam==30);
    }
    assert(g.myClub().league==0);
    assert(g.leagues()[0].active);
    assert(g.leagues()[1].active);
    assert(!g.leagues()[2].active);
    assert(!g.matchDue());
    assert(g.dateString()=="2026-07-01");
    auto firstId=g.ownPlayers()[0];
    auto p=g.findPlayer(firstId);
    assert(p!=nullptr);
    int trust=p->trust;
    auto answer=g.talk(firstId,"Спасибо за поддержку и помощь семье!");
    assert(!answer.empty());assert(g.findPlayer(firstId)->trust>trust);
    g.advance(30);
    assert(g.matchDue());
    assert(g.startLiveMatch());
    while(g.hasLiveMatch())g.stepLiveMatch(15);
    assert(!g.matchDue());
    assert(g.myClub().wins+g.myClub().draws+g.myClub().losses==1);
    assert(g.matchArchive().find("Сыгранных матчей пока нет")==std::string::npos);
    auto saved=g.save("test_save.fus");assert(saved.find("Карьера сохранена")!=std::string::npos);
    fu::Game restored;
    auto load=restored.load("test_save.fus");
    assert(load.find("загружена")!=std::string::npos);
    assert(restored.day()==g.day());
    assert(restored.players().size()==g.players().size());
    assert(restored.matchArchive()==g.matchArchive());
    // Contract negotiation and infrastructure decisions with sandbox permissions.
    restored.newCareer("Тест",6,fu::Role::Sandbox,fu::Difficulty::Free,{1,3});
    assert(restored.myClub().name=="Реал Мадрид");
    assert(restored.leagues()[1].active && restored.leagues()[3].active);
    assert(restored.spendMoney("stadium").find("Инвестиция одобрена")!=std::string::npos);
    assert(restored.recruitYouth().find("академии приглашён")!=std::string::npos);
    int season=restored.season();
    for(int d=0;d<450;d++){
       if(restored.matchDue())restored.instantMatch();
       else restored.advance(1);
    }
    assert(restored.season()>season);
    assert(!restored.archive().empty());
    auto futureSaved=restored.save("history_save.fus");
    assert(futureSaved.find("сохранена")!=std::string::npos);
    fu::Game afterHistory;
    assert(afterHistory.load("history_save.fus").find("загружена")!=std::string::npos);
    assert(afterHistory.archive().size()==restored.archive().size());
    assert(afterHistory.season()==restored.season());
    assert(afterHistory.importPlayers("../data/players.csv"));
    std::cout<<"PASS: clubs, leagues, players, conversations, match, save/load, sandbox, youth, seasons\n";
}
