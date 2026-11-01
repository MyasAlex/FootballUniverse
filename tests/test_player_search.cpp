#include "Game.hpp"
#include "PlayerSearch.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
std::string read(const std::filesystem::path& path){
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
void abilityWeights(){
    fu::Player player;
    player.position="GK";player.skill[9]=100;
    assert(fu::playerSearchAbility(player)==60);
    player.position="DC";
    assert(fu::playerSearchAbility(player)==0);
    player.skill[3]=100;
    assert(fu::playerSearchAbility(player)==40);
    player.position="ST";player.skill[1]=100;
    assert(fu::playerSearchAbility(player)==40);
    player.position="MC";player.skill[2]=100;
    assert(fu::playerSearchAbility(player)==30);
    player.skill.fill(100);
    assert(fu::playerSearchAbility(player)==100);
    player.fitness=0;player.morale=0;
    assert(fu::playerSearchAbility(player)==100);
    player.skill.fill(-100);
    assert(fu::playerSearchAbility(player)==0);
    player.skill.fill(200);
    assert(fu::playerSearchAbility(player)==100);
}
} // namespace

int main(){
    abilityWeights();
    fu::Game game;
    fu::PlayerSearchFilter filter;
    const auto world=fu::searchPlayers(game,filter);
    const auto expected=std::count_if(game.players().begin(),game.players().end(),
                                     [&game](const auto& player){return player.club!=game.teamId();});
    assert(world.size()==static_cast<size_t>(expected));
    assert(world.size()>100);
    for(int id:world)assert(game.findPlayer(id)->club!=game.teamId());
    filter.position="ST";
    const auto strikers=fu::searchPlayers(game,filter);
    assert(strikers.size()>100);
    for(int id:strikers)assert(game.findPlayer(id)->position=="ST");
    filter.position.clear();
    filter.activeLeaguesOnly=true;
    const auto active=fu::searchPlayers(game,filter);
    assert(!active.empty()&&active.size()<world.size());
    for(int id:active){
        const auto* club=game.findClub(game.findPlayer(id)->club);
        assert(game.leagues()[club->league].active);
    }

    // Controlled names and skill/budget boundaries inside an otherwise real
    // seeded world keep the assertions independent of generated identities.
    auto& players=const_cast<std::vector<fu::Player>&>(game.players());
    std::vector<fu::Player*> sample;
    for(auto& player:players)if(player.club==1&&sample.size()<3)sample.push_back(&player);
    assert(sample.size()==3);
    auto& club=const_cast<fu::Club&>(*game.findClub(1));
    club.name="ПоискТЕСТ ÉQUIPE";
    sample[0]->name="ТЕСТИГРОК АрТЁМ ÉLIAS";
    sample[1]->name="ТЕСТИГРОК Alex Smith";
    sample[2]->name="ТЕСТИГРОК Борис";
    for(auto* player:sample){
        player->age=18;player->position="ST";player->value=1000000;
        player->wage=10000;player->potential=85;player->skill.fill(70);
        player->sourceQid.clear();
    }
    sample[0]->sourceQid="Q100001";
    const auto id0=sample[0]->id,id1=sample[1]->id,id2=sample[2]->id;
    filter={};filter.name="аРтЁм éLiAs";
    assert(fu::searchPlayers(game,filter)==std::vector<int>{id0});
    filter.name="alex SMITH";
    assert(fu::searchPlayers(game,filter)==std::vector<int>{id1});
    filter.name="поисктест équipe";
    assert(fu::searchPlayers(game,filter).size()==24); // Match the club, not only a player.
    filter.name="Тестигрок ";
    filter.position="st";filter.minAge=18;filter.maxAge=18;
    filter.maxValue=1000000;filter.maxWage=10000;
    filter.minAbility=70;filter.minPotential=85;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id0,id1,id2}));
    filter.maxValue=999999;
    assert(fu::searchPlayers(game,filter).empty());
    filter.maxValue=1000000;filter.maxWage=9999;
    assert(fu::searchPlayers(game,filter).empty());
    filter.maxWage=10000;filter.minAbility=71;
    assert(fu::searchPlayers(game,filter).empty());
    filter.minAbility=70;filter.minPotential=86;
    assert(fu::searchPlayers(game,filter).empty());
    filter.minPotential=85;filter.position="GK";
    assert(fu::searchPlayers(game,filter).empty());
    filter.position="ALL";
    assert(fu::searchPlayers(game,filter).size()==3);
    filter.realOnly=true;
    assert(fu::searchPlayers(game,filter)==std::vector<int>{id0});
    game.toggleShortlist(id0);
    game.toggleShortlist(id1);
    filter.shortlistOnly=true;
    assert(fu::searchPlayers(game,filter)==std::vector<int>{id0});
    filter.realOnly=false;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id0,id1}));
    filter.activeLeaguesOnly=true;
    assert(fu::searchPlayers(game,filter).size()==2);
    game.toggleLeague(game.findClub(1)->league); // Own league cannot be disabled.
    assert(fu::searchPlayers(game,filter).size()==2);

    // Every sort has a visible order, deterministic ID ties, and no shortlist
    // promotion that would silently contradict the requested numeric order.
    filter={};filter.name="тестигрок";filter.position="ST";
    filter.minAge=18;filter.maxAge=18;filter.maxValue=1000000;filter.maxWage=10000;
    filter.minAbility=70;filter.minPotential=85;
    const std::vector<int> tied={id0,id1,id2};
    for(auto sort:{fu::PlayerSort::ValueAscending,fu::PlayerSort::AbilityDescending,
                   fu::PlayerSort::PotentialDescending,fu::PlayerSort::AgeAscending,
                   fu::PlayerSort::WageAscending}){
        filter.sort=sort;
        assert(fu::searchPlayers(game,filter)==tied);
    }
    sample[0]->value=900000;sample[1]->value=500000;sample[2]->value=700000;
    filter.sort=fu::PlayerSort::ValueAscending;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id1,id2,id0}));
    sample[0]->skill.fill(71);sample[1]->skill.fill(75);sample[2]->skill.fill(79);
    filter.sort=fu::PlayerSort::AbilityDescending;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id2,id1,id0}));
    sample[0]->potential=91;sample[1]->potential=88;sample[2]->potential=89;
    filter.sort=fu::PlayerSort::PotentialDescending;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id0,id2,id1}));
    sample[0]->age=20;sample[1]->age=18;sample[2]->age=19;
    filter.maxAge=20;filter.sort=fu::PlayerSort::AgeAscending;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id1,id2,id0}));
    sample[0]->wage=9000;sample[1]->wage=7000;sample[2]->wage=8000;
    filter.sort=fu::PlayerSort::WageAscending;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id1,id2,id0}));
    filter.sort=fu::PlayerSort::Name;
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id1,id0,id2}));
    sample[0]->name=sample[1]->name="Identical NAME";
    filter.name="identical name";
    assert(fu::searchPlayers(game,filter)==std::vector<int>({id0,id1}));

    auto invalid=filter;
    invalid.minAge=21;invalid.maxAge=20;assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.minAbility=101;assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.minPotential=-1;assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.maxWage=-1;assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.maxValue=-1;assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.position="CMB";assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.name=std::string("\xc0\xaf",2);assert(fu::searchPlayers(game,invalid).empty());
    invalid=filter;invalid.sort=static_cast<fu::PlayerSort>(99);assert(fu::searchPlayers(game,invalid).empty());

    // Saves serialize the RNG as well as world state: byte equality verifies
    // repeated filtering and sorting consume no randomness and change nothing.
    const auto stem=std::filesystem::temp_directory_path()/
        ("fu_search_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto before=stem.string()+"_before.fus",after=stem.string()+"_after.fus";
    assert(game.save(before).find("сохранена")!=std::string::npos);
    for(int n=0;n<5;++n){fu::searchPlayers(game,filter);fu::searchPlayers(game,{});}
    assert(game.save(after).find("сохранена")!=std::string::npos);
    assert(read(before)==read(after));
    std::filesystem::remove(before);std::filesystem::remove(after);
    std::cout<<"PASS: player search, Unicode names, combined limits, world scope, stable sorts and pure queries\n";
}
