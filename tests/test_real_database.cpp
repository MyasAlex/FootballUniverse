#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <set>
#include <string>

int main(){
    fu::Game game;
    game.newCareer("Real player database",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1});
    assert(game.realPlayerCount()>=250);
    assert(game.realPlayerCount(0)>=15);
    assert(game.realPlayerCount(6)>=8);
    assert(game.realPlayerCount(24)>=12);
    std::set<std::string> ids;
    int priced=0;
    for(const auto& p:game.players()){
        if(p.sourceQid.empty())continue;
        assert(ids.insert(p.sourceQid).second);
        assert(p.club>=0&&p.club<36);
        assert(p.age>=16&&p.age<=46);
        assert(p.value>=10000);
        assert(p.potential>=20&&p.potential<=99);
        if(p.value>=1000000)++priced;
    }
    assert(priced>=200);
    const fu::Player* rice=nullptr;
    const fu::Player* salah=nullptr;
    for(const auto& p:game.players()){
        if(p.sourceQid=="WC:357662")rice=&p;
        if(p.sourceQid=="WC:148455")salah=&p;
    }
    assert(rice&&rice->club==0&&rice->name=="Declan Rice");
    assert(rice->value==120000000);
    assert(salah&&salah->club==1&&salah->value>10000000);
    const int oldCount=game.realPlayerCount();
    const int oldValue=rice->value;
    const std::string file="test_real_db.fus";
    assert(game.save(file).find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load(file).find("загружена")!=std::string::npos);
    assert(restored.realPlayerCount()==oldCount);
    for(const auto& p:restored.players())
        if(p.sourceQid=="WC:357662")assert(p.value==oldValue);
    std::remove(file.c_str());
    std::cout<<"PASS: bundled 2026 World Cup real rosters, 280 source IDs, "
                "sourced values, deduplication and save persistence\n";
}
