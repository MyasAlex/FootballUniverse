#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

static fu::Game career(fu::Role role,fu::Difficulty diff=fu::Difficulty::Realistic){
    fu::Game g;g.newCareer("Проверка должности",0,role,diff,{0,1});return g;
}
int main(){
    {
        auto g=career(fu::Role::Coach);
        assert(g.canCoach()&&g.canTransfer()&&!g.canInvest()&&!g.canCommercial());
        assert(g.roleOverview().find("ГЛАВНЫЙ ТРЕНЕР")!=std::string::npos);
        const int tactic=g.myClub().tactic;
        g.changeTactic();assert(g.myClub().tactic!=tactic);
        assert(g.spendMoney("stadium").find("владелец")!=std::string::npos);
        assert(g.negotiateSponsor().find("президент")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::Director);
        assert(!g.canCoach()&&g.canTransfer()&&!g.canInvest()&&!g.canCommercial());
        const int tactic=g.myClub().tactic;
        g.changeTactic();assert(g.myClub().tactic==tactic);
        assert(g.autoPickLineup().find("Нет полномочий")!=std::string::npos);
        assert(g.applyMatchPlan(fu::TacticalPlan::Gegenpress).find("Нет полномочий")!=std::string::npos);
        assert(g.teamTalk(0).find("Нет полномочий")!=std::string::npos);
        const auto name=g.ownPlayers().front();
        assert(g.talk(name,"Молодец!").find("главный тренер")!=std::string::npos);
        assert(g.scoutPlayer(g.transferCandidates().front()).find("Скаут отправлен")!=std::string::npos);
        assert(g.save("director_role_v14.fus").find("сохранена")!=std::string::npos);
        fu::Game loaded;assert(loaded.load("director_role_v14.fus").find("загружена")!=std::string::npos);
        assert(!loaded.canCoach()&&loaded.canTransfer());
        std::filesystem::remove("director_role_v14.fus");
    }
    {
        auto g=career(fu::Role::Owner);
        assert(!g.canCoach()&&!g.canTransfer()&&g.canInvest()&&!g.canCommercial());
        assert(g.spendMoney("training").find("Инвестиция одобрена")!=std::string::npos);
        assert(g.adjustTickets(5).find("президент")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::President);
        assert(!g.canCoach()&&!g.canTransfer()&&!g.canInvest()&&g.canCommercial());
        assert(g.spendMoney("training").find("владелец")!=std::string::npos);
        assert(g.negotiateSponsor().find("Спонсорские переговоры проводит президент") ==std::string::npos);
        assert(g.adjustTickets(5).find("президент") ==std::string::npos);
        assert(g.speakToFans("Спасибо за поддержку").find("Обращения к болельщикам") ==std::string::npos);
    }
    {
        auto g=career(fu::Role::Sandbox);
        assert(g.canCoach()&&g.canTransfer()&&g.canInvest()&&g.canCommercial());
        assert(g.roleOverview().find("ПЕСОЧНИЦА")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::Owner,fu::Difficulty::Free);
        assert(g.canCoach()&&g.canTransfer()&&g.canInvest()&&g.canCommercial());
    }
    std::cout<<"PASS: coaching, sporting direction, ownership, presidency and sandbox\n";
}
