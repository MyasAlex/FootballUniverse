#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

static fu::Game career(fu::Role role,fu::Difficulty difficulty=fu::Difficulty::Realistic){
    fu::Game g;
    g.newCareer("Проверка роли",0,role,difficulty,{0,1});
    return g;
}
int main(){
    {
        auto g=career(fu::Role::Coach);
        assert(g.canCoach()&&g.canTransfer()&&!g.canInvest()&&!g.canCommercial());
        assert(g.roleOverview().find("ГЛАВНЫЙ ТРЕНЕР")!=std::string::npos);
        const int before=g.myClub().tactic;
        g.changeTactic();
        assert(g.myClub().tactic!=before);
        assert(g.spendMoney("stadium").find("владелец")!=std::string::npos);
        assert(g.negotiateSponsor().find("президент")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::Director);
        assert(!g.canCoach()&&g.canTransfer()&&!g.canInvest()&&!g.canCommercial());
        const int before=g.myClub().tactic;
        g.changeTactic();
        assert(g.myClub().tactic==before);
        assert(g.status().find("Должность")!=std::string::npos);
        assert(g.autoPickLineup().find("Нет полномочий")!=std::string::npos);
        assert(g.teamTalk(0).find("Нет полномочий")!=std::string::npos);
        int id=g.ownPlayers().front();
        assert(g.talk(id,"Молодец!").find("Только главный тренер")!=std::string::npos);
        assert(g.scoutPlayer(g.transferCandidates().front()).find("Скаут отправлен")!=std::string::npos);
        assert(g.save("roles_director.fus").find("сохранена")!=std::string::npos);
        fu::Game loaded;
        assert(loaded.load("roles_director.fus").find("загружена")!=std::string::npos);
        assert(loaded.role()==fu::Role::Director);
        assert(!loaded.canCoach()&&loaded.canTransfer());
        std::filesystem::remove("roles_director.fus");
    }
    {
        auto g=career(fu::Role::Owner,fu::Difficulty::Free);
        assert(!g.canCoach()&&!g.canTransfer()&&g.canInvest()&&!g.canCommercial());
        assert(g.spendMoney("training").find("Инвестиция одобрена")!=std::string::npos);
        assert(g.negotiateSponsor().find("президент")!=std::string::npos);
        assert(g.adjustTickets(5).find("президент")!=std::string::npos);
        assert(g.autoPickLineup().find("Нет полномочий")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::President);
        assert(!g.canCoach()&&!g.canTransfer()&&!g.canInvest()&&g.canCommercial());
        assert(g.spendMoney("training").find("владелец")!=std::string::npos);
        const std::string response=g.speakToFans("Спасибо за поддержку!");
        assert(response.find("Официальные обращения") ==std::string::npos);
        const std::string sponsor=g.negotiateSponsor();
        assert(sponsor.find("занимается президент") ==std::string::npos);
        const std::string ticket=g.adjustTickets(5);
        assert(ticket.find("президент клуба")==std::string::npos);
        assert(g.autoPickLineup().find("Нет полномочий")!=std::string::npos);
    }
    {
        auto g=career(fu::Role::Sandbox);
        assert(g.canCoach()&&g.canTransfer()&&g.canInvest()&&g.canCommercial());
        assert(g.roleOverview().find("ВСЕ ПОЛНОМОЧИЯ")!=std::string::npos);
        assert(g.autoPickLineup().find("Автоматически")!=std::string::npos);
        assert(g.spendMoney("training").find("Инвестиция одобрена")!=std::string::npos);
    }
    std::cout<<"PASS distinct coaching, sports direction, ownership, presidency and sandbox powers\n";
}
