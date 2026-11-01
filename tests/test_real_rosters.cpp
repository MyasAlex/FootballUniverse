#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <set>

int main(){
    fu::Game game;
    game.newCareer("Проверка базы",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1});
    const int oldCount=int(game.players().size());
    const auto firstId=game.ownPlayers().front();
    assert(game.realPlayerCount()==0);
    const char* path="test_wikidata_player_snapshot.csv";
    {
        std::ofstream file(path,std::ios::binary);
        file<<"club_id;wikidata_qid;name;position;birth_date\n"
            <<"0;Q10001;Sample Real Goalkeeper;GK;1997-02-04\n"
            <<"0;Q10002;Sample Real Defender;DC;2000-06-15\n"
            <<"0;Q10003;Sample Real Forward;ST;2003-08-07\n"
            <<"1;Q10004;Sample Other Club Player;MC;2001-04-09\n"
            <<"2;WC:342229;Sample World Cup Player;ST;1998-12-20\n"
            <<"1;Q10004;Duplicate Identical Person;MC;2001-04-09\n"
            <<"1;Qxyz;Invalid Identity;MC;2001-04-09\n"
            <<"2;Q10005;Invalid Birthday;GK;2050-01-01\n"
            <<"3;Q10006;Invalid Position;CMB;2002-11-20\n";
    }
    assert(game.importPlayers(path));
    assert(game.players().size()==size_t(oldCount));
    assert(game.realPlayerCount()==5);
    assert(game.realPlayerCount(0)==3);
    assert(game.realPlayerCount(1)==1);
    assert(game.realPlayerCount(2)==1);
    assert(game.ownPlayers().size()==24);
    assert(game.findPlayer(firstId)!=nullptr);
    const fu::Player* keeper=nullptr;
    for(const auto& p:game.players()){
        if(p.sourceQid=="Q10001"){keeper=&p;break;}
    }
    assert(keeper&&keeper->club==0&&keeper->position=="GK");
    assert(keeper->age>=27&&keeper->age<=30);
    assert(game.squadText().find("Из реальной базы: 3")!=std::string::npos);
    assert(!game.importPlayers(path));
    assert(game.realPlayerCount()==4);
    const std::string save="test_wikidata_save.fus";
    assert(game.save(save).find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load(save).find("загружена")!=std::string::npos);
    assert(restored.realPlayerCount()==5);
    assert(restored.findPlayer(keeper->id)->sourceQid=="Q10001");
    assert(std::any_of(restored.players().begin(),restored.players().end(),
        [](const fu::Player& p){return p.club==2&&p.sourceQid=="WC:342229";}));
    assert(restored.players().size()==game.players().size());
    assert(restored.ownPlayers().size()==24);
    restored.advance(1);
    assert(!restored.importPlayers(path)); // Protect in-progress careers.
    std::filesystem::remove(path);
    std::filesystem::remove(save);
    std::cout<<"PASS: real roster CSV, QID validation, stable slots, fallback, save/load\n";
}
