#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>

int main(){
    fu::Game g;
    g.newCareer("Тест менеджера",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    assert(g.myClub().captainId>0 && g.myClub().viceCaptainId>0);
    assert(g.myClub().captainId!=g.myClub().viceCaptainId);
    int nextCaptain=-1;
    for(int id:g.ownPlayers()){
        if(id!=g.myClub().captainId && id!=g.myClub().viceCaptainId){nextCaptain=id;break;}
    }
    assert(nextCaptain>0);
    int prevMorale=g.findPlayer(nextCaptain)->morale;
    assert(g.setCaptain(nextCaptain).find("назначен капитаном")!=std::string::npos);
    assert(g.myClub().captainId==nextCaptain);
    assert(g.findPlayer(nextCaptain)->morale>=prevMorale);
    assert(g.setCaptain(g.myClub().viceCaptainId).find("разными")!=std::string::npos);
    int target=g.transferCandidates().front();
    auto started=g.scoutPlayer(target);
    assert(started.find("Скаут отправлен")!=std::string::npos);
    assert(g.toggleShortlist(target).find("короткий список")!=std::string::npos);
    assert(g.scoutReport(target)&&g.scoutReport(target)->shortlisted);
    assert(!g.transferCandidates(true).empty());
    assert(g.transferCandidates(true).front()==target);
    assert(g.scoutReport(target)->days==0);
    auto meeting=g.teamTalk(0);
    assert(meeting.find("Мотивационная встреча")!=std::string::npos);
    assert(g.teamTalk(0).find("через")!=std::string::npos);
    for(int i=0;i<8;++i){
        if(g.matchDue())assert(g.instantMatch());
        else g.advance(1);
    }
    assert(g.scoutReport(target)->days==5);
    assert(g.scoutReport(target)->ability>0);
    assert(g.scoutText(target).find("Наблюдение завершено")!=std::string::npos);
    assert(g.teamTalk(1).find("Похвала команде")!=std::string::npos);
    assert(g.save("features_save.fus").find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load("features_save.fus").find("загружена")!=std::string::npos);
    assert(restored.myClub().captainId==nextCaptain);
    assert(restored.scoutReport(target));
    assert(restored.scoutReport(target)->days==5);
    assert(restored.scoutReport(target)->shortlisted);
    assert(restored.transferCandidates(true).front()==target);
    assert(restored.teamTalk(0).find("через")!=std::string::npos);
    std::cout<<"PASS: captaincy, meetings, cooldown, scouting, shortlist, v5 save/load\n";
}
