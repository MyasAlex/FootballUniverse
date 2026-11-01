#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
static void nextDay(fu::Game& g){
    if(g.matchDue())assert(g.instantMatch());
    g.advance(1);
}
static void passDays(fu::Game& g,int count){for(int i=0;i<count;++i)nextDay(g);}
int main(){
    fu::Game g;
    g.newCareer("Проверка академии",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    assert(g.staff().size()>=g.clubs().size()*4);
    assert(g.staffQuality(g.teamId(),0)>0);
    assert(!g.youthProspects().empty());
    const int baseStaff=g.staffQuality(g.teamId(),0);
    const int initialBudget=g.myClub().budget;
    assert(g.hireStaff(0,100).find("Нанят")!=std::string::npos);
    assert(g.staffQuality(g.teamId(),0)==100);
    assert(g.myClub().budget<initialBudget);
    const int newStaffId=g.staff().back().id;
    assert(g.staff().back().role==0);
    assert(g.fireStaff(newStaffId).find("покинул штаб")!=std::string::npos);
    assert(g.staffQuality(g.teamId(),0)==baseStaff);
    assert(g.fireStaff(newStaffId).find("не найден")!=std::string::npos);
    assert(g.hireStaff(1,90).find("Нанят")!=std::string::npos);
    assert(g.staffQuality(g.teamId(),1)==90);
    const int newCoachId=g.staff().back().id;
    assert(g.hireStaff(0,100).find("Нанят")!=std::string::npos);
    assert(g.staffQuality(g.teamId(),0)==100);

    const int prospectId=g.youthProspects().front().id;
    assert(g.signYouthProspect(prospectId).find("Из академии приглашён")!=std::string::npos);
    int youngId=g.players().back().id;
    assert(g.findPlayer(youngId)->academyGraduate);
    assert(g.findPlayer(youngId)->age<=23);
    assert(std::none_of(g.youthProspects().begin(),g.youthProspects().end(),
        [prospectId](const fu::YouthProspect& p){return p.id==prospectId;}));
    int mentorId=-1;
    for(int id:g.ownPlayers()){
        if(g.findPlayer(id)->age>=25 && id!=youngId){mentorId=id;break;}
    }
    assert(mentorId>0);
    assert(g.assignMentor(youngId,mentorId).find("наставником")!=std::string::npos);
    assert(g.findPlayer(youngId)->mentorId==mentorId);
    assert(g.improveRelationship(youngId,mentorId,0).find("отношения")!=std::string::npos);
    assert(g.improveRelationship(youngId,mentorId,0).find("через")!=std::string::npos);
    assert(!g.relationships().empty());
    int initialAffinity=g.relationships().front().affinity;
    assert(initialAffinity>=25);
    int focus=-1;
    for(int i=0;i<10;++i){
        g.cycleTrainingFocus(youngId);
        const auto* p=g.findPlayer(youngId);
        if(p->trainingFocus>=0 && p->skill[p->trainingFocus]+6<p->potential){
            focus=p->trainingFocus;break;
        }
    }
    assert(focus>=0);
    const int originalSkill=g.findPlayer(youngId)->skill[focus];
    // Injury recovery and fixture congestion can delay growth; allow enough
    // in-game training days for this deterministic progression requirement.
    const int trainingStart=g.totalDays();
    for(int attempts=0;attempts<800&&g.totalDays()<trainingStart+230&&
        g.findPlayer(youngId)->developedSkills<1;++attempts)
        nextDay(g);
    if(g.findPlayer(youngId)->developedSkills<1){
        const auto* p=g.findPlayer(youngId);
        std::cerr<<"DIAGNOSTIC: training time "<<g.totalDays()-trainingStart
                 <<" days, skill "<<p->skill[focus]<<" baseline "<<originalSkill
                 <<" potential "<<p->potential<<" focus "<<p->trainingFocus
                 <<" XP "<<p->developmentXP<<" injuries "<<p->injured
                 <<" club "<<p->club<<" our club "<<g.teamId()<<"\n";
    }
    assert(g.findPlayer(youngId)->skill[focus]>=originalSkill+1);
    assert(g.findPlayer(youngId)->developedSkills>=1);
    assert(g.relationships().front().affinity==initialAffinity);
    assert(g.improveRelationship(youngId,mentorId,1).find("Примирение")!=std::string::npos);
    assert(g.relationships().front().affinity>initialAffinity);
    const int relationAfter=g.relationships().front().affinity;
    assert(g.discoverYouth().find("новый список")!=std::string::npos);
    assert(g.discoverYouth().find("через")!=std::string::npos);
    const int pending=g.youthProspects().front().id;
    assert(g.save("v07_progress.fus").find("сохранена")!=std::string::npos);
    fu::Game saved;
    assert(saved.load("v07_progress.fus").find("загружена")!=std::string::npos);
    assert(saved.staffQuality(saved.teamId(),0)==100);
    assert(saved.staffQuality(saved.teamId(),1)==90);
    assert(saved.findPlayer(youngId)->mentorId==mentorId);
    assert(saved.findPlayer(youngId)->developedSkills==g.findPlayer(youngId)->developedSkills);
    assert(saved.findPlayer(youngId)->developmentXP==g.findPlayer(youngId)->developmentXP);
    assert(saved.findPlayer(youngId)->academyGraduate);
    assert(saved.relationships().front().affinity==relationAfter);
    assert(saved.youthProspects().front().id==pending);
    assert(saved.discoverYouth().find("через")!=std::string::npos);
    passDays(saved,30);
    assert(saved.discoverYouth().find("новый список")!=std::string::npos);

    std::cout<<"PASS: coaching staff, medical hires, academy signings, training progression, "
                "mentorship, chemistry, cooldown and v8 persistence\n";
}
