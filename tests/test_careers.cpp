#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <set>
#include <string>

static void progressDay(fu::Game& g){
    if(g.matchDue())assert(g.instantMatch());
    g.advance(1);
}
int main(){
    fu::Game g;
    g.newCareer("Тренер",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1,5});
    assert(g.clubs().size()==72);
    assert(g.players().size()==72*24);
    assert(g.leagues().size()==12);
    assert(g.matches().size()==12*90+16);
    assert(g.cups().size()==7);
    assert(g.coaches().size()==g.clubs().size());
    assert(g.managerReputation()==55);
    assert(g.jobApplication().stage==-1);
    assert(g.coaches()[0].name==g.manager());
    assert(g.leagues()[0].tier==0&&g.leagues()[0].systemId==0);
    for(int sys=0;sys<6;++sys){
        assert(g.leagues()[sys].tier==0&&g.leagues()[sys].systemId==sys);
        assert(g.leagues()[sys+6].tier==1&&g.leagues()[sys+6].systemId==sys);
        assert(g.leagueClubs(sys).size()==6);
        assert(g.leagueClubs(sys+6).size()==6);
    }
    assert(g.standings(6).find("ПОВЫШЕНИЕ")!=std::string::npos);
    assert(g.standings(0).find("ВЫЛЕТ")!=std::string::npos);
    assert(g.divisionsText().find("ДВА УРОВНЯ")==std::string::npos); // text is intentionally natural case
    assert(g.divisionsText().find("два уровня")!=std::string::npos);
    assert(g.jobMarketText(36).find(g.clubs()[36].name)!=std::string::npos);
    assert(g.applyForJob(0).find("Выберите другой клуб")!=std::string::npos);
    const int target=36;
    assert(g.applyForJob(target).find("отправлена")!=std::string::npos);
    assert(g.jobApplication().stage==0);
    assert(g.applyForJob(37).find("предыдущую")!=std::string::npos);
    assert(g.save("v09_pending_job.fus").find("сохранена")!=std::string::npos);
    fu::Game waiting;
    assert(waiting.load("v09_pending_job.fus").find("загружена")!=std::string::npos);
    assert(waiting.jobApplication().stage==0);
    for(int i=0;i<2;++i)progressDay(waiting);
    assert(waiting.jobApplication().stage==1);
    assert(waiting.save("v09_offered_job.fus").find("сохранена")!=std::string::npos);
    fu::Game hired;
    assert(hired.load("v09_offered_job.fus").find("загружена")!=std::string::npos);
    assert(hired.jobApplication().stage==1);
    assert(hired.acceptJobOffer().find("Новая работа")!=std::string::npos);
    assert(hired.teamId()==target);
    assert(hired.coaches()[0].vacant);
    assert(hired.coaches()[target].name==hired.manager());
    assert(hired.jobApplication().stage==3);
    assert(hired.ownPlayers().size()==24);
    assert(hired.myClub().league==6);
    assert(hired.leagues()[6].active);
    assert(hired.save("v09_switched_job.fus").find("сохранена")!=std::string::npos);
    fu::Game switched;
    assert(switched.load("v09_switched_job.fus").find("загружена")!=std::string::npos);
    assert(switched.teamId()==target);
    assert(switched.coaches()[target].name==switched.manager());
    assert(switched.jobApplication().stage==3);
    assert(switched.applyForJob(35).find("отправлена")!=std::string::npos);
    assert(switched.withdrawApplication().find("отозвана")!=std::string::npos);
    assert(switched.jobApplication().stage==4);

    // Simulate an entire 2026 season, then verify relegation/promotion in all six systems.
    fu::Game season;
    season.newCareer("Переходы",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    int steps=0;
    while(season.season()==2026&&steps++<2000)progressDay(season);
    assert(steps<2000&&season.season()==2027);
    assert(season.leagueMovements().size()==6);
    assert(season.leagueHistory().size()==12);
    assert(season.trophyHistory().size()==7);
    assert(season.cups().size()==7);
    assert(season.archive().size()==12*90+37);
    assert(season.matches().size()==12*90+16);
    std::set<int> promoted,relegated;
    for(const auto& m:season.leagueMovements()){
        assert(m.season==2026);
        assert(m.systemId>=0&&m.systemId<6);
        assert(season.clubs()[m.promotedClub].league==m.systemId);
        assert(season.clubs()[m.relegatedClub].league==m.systemId+6);
        assert(promoted.insert(m.promotedClub).second);
        assert(relegated.insert(m.relegatedClub).second);
        assert(season.leagueClubs(m.systemId).size()==6);
        assert(season.leagueClubs(m.systemId+6).size()==6);
    }
    assert(season.seasonHistoryText().find("ПОВЫШЕНИЯ И ВЫЛЕТЫ")!=std::string::npos);
    assert(season.divisionsText().find("↑")!=std::string::npos);
    assert(season.save("v09_after_promotions.fus").find("сохранена")!=std::string::npos);
    fu::Game loaded;
    assert(loaded.load("v09_after_promotions.fus").find("загружена")!=std::string::npos);
    assert(loaded.leagueMovements().size()==season.leagueMovements().size());
    assert(loaded.leagueHistory().size()==12);
    assert(loaded.clubs().size()==72);
    assert(loaded.seasonHistoryText()==season.seasonHistoryText());
    assert(loaded.divisionsText()==season.divisionsText());
    assert(loaded.coaches().size()==72);
    std::cout<<"PASS 0.9: 12 leagues, fixtures, promotions, coach market, job offers, FUSAVE_10\n";
}
