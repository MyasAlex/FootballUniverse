#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

static void nextDay(fu::Game& g){
    if(g.matchDue())assert(g.instantMatch());
    else g.advance(1);
}
static void playUntil(fu::Game& g,int target){
    int guard=0;
    while(g.season()==2026&&g.day()<target&&guard++<1500)nextDay(g);
    assert(guard<1500);
}
int main(){
    fu::Game g;
    g.newCareer("Тест кубков",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    assert(g.cups().size()==7);
    assert(g.matches().size()==556);
    assert(g.cups()[6].leagueId==-1);
    assert(g.cups()[0].leagueId==0);
    assert(g.cupsText().find("Международный кубок")!=std::string::npos);
    int leagueCount=0,nationalPrelim=0,internationalQuarters=0;
    std::vector<int> gamesByClub(36,0);
    for(const auto& m:g.matches()){
        assert(m.id>=0);
        assert(m.id<int(g.matches().size()));
        assert(m.season==2026);
        if(m.competition==-1){
            ++leagueCount;
            assert(m.day%10==3);
            ++gamesByClub[m.home];++gamesByClub[m.away];
        }else if(m.competition==6){
            ++internationalQuarters;
            assert(m.round==0&&m.day==51);
        }else{
            ++nationalPrelim;
            assert(m.round==0&&m.day==21);
        }
    }
    assert(leagueCount==540&&nationalPrelim==12&&internationalQuarters==4);
    for(int games:gamesByClub)assert(games==30);
    assert(g.calendarText().find("Чемпионат")!=std::string::npos);
    // The initial 1st league match should still be the next scheduled game.
    assert(g.upcomingMatch()&&g.upcomingMatch()->day==3);
    playUntil(g,20);
    assert(g.day()==20);
    std::vector<int> points;
    for(const auto& c:g.clubs())points.push_back(c.points);
    // Day 21 is cup-only: all 12 domestic preliminary games complete in background.
    assert(!g.matchDue());
    g.advance(1);
    assert(g.day()==21);
    for(const auto& c:g.clubs())assert(c.points==points[c.id]);
    assert(g.cups().size()==7);
    for(int i=0;i<6;++i){
        assert(g.cups()[i].round==1);
        assert(g.cups()[i].champion==-1);
    }
    int prelimPlayed=0;
    for(const auto& m:g.matches())if(m.competition>=0&&m.competition<6&&m.round==0){
        assert(m.played);
        assert(g.knockoutWinner(m)==m.home||g.knockoutWinner(m)==m.away);
        if(m.homeGoals==m.awayGoals){
            assert(m.penaltiesH>=0&&m.penaltiesA>=0&&m.penaltiesH!=m.penaltiesA);
        }
        ++prelimPlayed;
    }
    assert(prelimPlayed==12);
    // Preserve a tournament draw, future fixtures and existing league results.
    assert(g.save("v08_cup_mid.fus").find("сохранена")!=std::string::npos);
    fu::Game resumed;
    assert(resumed.load("v08_cup_mid.fus").find("загружена")!=std::string::npos);
    assert(resumed.matches().size()==g.matches().size());
    assert(resumed.cupsText()==g.cupsText());
    assert(resumed.cups()[0].round==1);
    assert(resumed.calendarText()==g.calendarText());

    int guard=0;
    while(resumed.season()==2026&&guard++<1800)nextDay(resumed);
    assert(guard<1800);
    assert(resumed.season()==2027);
    assert(resumed.trophyHistory().size()==7);
    assert(resumed.leagueHistory().size()==6);
    assert(resumed.archive().size()==540+6*5+7);
    std::set<int> trophyIds;
    for(const auto& t:resumed.trophyHistory()){
        assert(t.season==2026);
        assert(t.winner>=0&&t.runnerUp>=0&&t.winner!=t.runnerUp);
        assert(trophyIds.insert(t.cupId).second);
    }
    for(int cupId=0;cupId<7;++cupId)assert(trophyIds.count(cupId)==1);
    int cupMatches=0;
    for(const auto& m:resumed.archive())if(m.competition>=0){
        ++cupMatches;
        assert(m.played);
        assert(resumed.knockoutWinner(m)>=0);
        if(m.homeGoals==m.awayGoals)
            assert(m.penaltiesH>=0&&m.penaltiesA>=0&&m.penaltiesH!=m.penaltiesA);
    }
    assert(cupMatches==37);
    // New international quarterfinalists include each of the six champions.
    std::set<int> internationalEntrants;
    for(const auto& m:resumed.matches())if(m.competition==6&&m.round==0){
        internationalEntrants.insert(m.home);internationalEntrants.insert(m.away);
    }
    assert(internationalEntrants.size()==8);
    for(const auto& h:resumed.leagueHistory())
        assert(internationalEntrants.count(h.winner)==1);
    assert(resumed.seasonHistoryText().find("ИСТОРИЯ ПОБЕДИТЕЛЕЙ")!=std::string::npos);
    assert(resumed.scorersText().find("БОМБАРДИРЫ")!=std::string::npos);
    assert(resumed.save("v08_history.fus").find("сохранена")!=std::string::npos);
    fu::Game history;
    assert(history.load("v08_history.fus").find("загружена")!=std::string::npos);
    assert(history.trophyHistory().size()==resumed.trophyHistory().size());
    assert(history.leagueHistory().size()==resumed.leagueHistory().size());
    assert(history.archive().size()==resumed.archive().size());
    assert(history.cupsText()==resumed.cupsText());
    assert(history.seasonHistoryText()==resumed.seasonHistoryText());
    std::cout<<"PASS: 6 domestic cups, international cup, penalty winners, league standings, calendar, history, v9 save/load\n";
}
