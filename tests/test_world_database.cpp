#include "WorldDatabase.hpp"
#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
int main(){
    const std::filesystem::path p="test_world_database.csv";
    {
        std::ofstream out(p);
        out<<"source_id;wikidata_qid;name;position;birth_date;nationality\n";
        out<<"reep_p000001;Q100;Alice Striker;Forward;2003-09-20;Norway\n";
        out<<"reep_p000002;Q101;Bob Keeper;Goalkeeper;1997-02-18;England\n";
        out<<"reep_p000003;;Carla Player;Midfielder;;Brazil\n";
        out<<"reep_p000002;Q101;Duplicate;Forward;2001-01-01;England\n";
        out<<"invalid;Q102;Not A Player;Forward;2000-01-01;France\n";
        for(int i=4;i<10004;++i)
            out<<"reep_p0000"<<i<<";;Regular Player "<<i<<";Defender;2004-05-04;Norway\n";
    }
    fu::WorldDatabase database;
    assert(database.load(p.string()));
    assert(database.size()==10003);
    assert(database.at(0)&&database.at(0)->name=="Alice Striker");
    assert(database.at(1)&&database.at(1)->age==29);
    assert(database.at(2)&&database.at(2)->age==0);
    auto match=database.search({"alice", "forward", "norway"});
    assert(match.total==1&&match.indices.size()==1);
    assert(database.search({"", "FORWARD", ""}).total==1);
    auto under25=database.search({"", "", "", 25});
    assert(under25.total==10001); // Alice (23) + 10000 generated, unknown DOB excluded
    auto paged=database.search({"regular", "defender", "norway", 0, 900, 30});
    assert(paged.total==10000&&paged.indices.size()==30&&paged.indices[0]==903);
    auto failed=database.load("not_found.csv");
    assert(!failed&&database.size()==10003); // failed reload preserves working dataset
    fu::Game normal;
    normal.newCareer("Test",0,fu::Role::Coach,fu::Difficulty::Realistic,{0,1});
    assert(normal.addWorldPlayerToSandbox(*database.at(0)).find("только в режиме")!=std::string::npos);
    fu::Game sandbox;
    sandbox.newCareer("Test",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    const auto originalSquad=sandbox.ownPlayers().size();
    assert(sandbox.addWorldPlayerToSandbox(*database.at(0)).find("добавлен")!=std::string::npos);
    assert(sandbox.ownPlayers().size()==originalSquad+1);
    assert(sandbox.realPlayerCount(0)>=1);
    assert(sandbox.addWorldPlayerToSandbox(*database.at(0)).find("уже присутствует")!=std::string::npos);
    assert(sandbox.addWorldPlayerToSandbox(*database.at(2)).find("нет подтверждаемого")!=std::string::npos);
    const auto save=std::filesystem::path("sandbox_world_player.fus");
    assert(sandbox.save(save.string()).find("сохранена")!=std::string::npos);
    fu::Game reloaded;
    assert(reloaded.load(save.string()).find("загружена")!=std::string::npos);
    assert(reloaded.ownPlayers().size()==originalSquad+1);
    std::filesystem::remove(save);
    std::filesystem::remove(p);
    std::cout<<"PASS: global index validation, 10k records, search filters, pages, fallback\n";
}
