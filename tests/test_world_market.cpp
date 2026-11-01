#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

namespace {
struct TemporaryFiles {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("fu_world_market_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryFiles(){assert(std::filesystem::create_directory(root));}
    ~TemporaryFiles(){std::filesystem::remove_all(root);}
    std::string file(const std::string& name)const{return (root/name).string();}
};
std::string read(const std::string& path){
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int positionGroup(const fu::Player& player){
    if(player.position=="GK")return 0;
    if(player.position=="DL"||player.position=="DC"||player.position=="DR")return 1;
    if(player.position=="ST"||player.position=="AML"||player.position=="AMR")return 3;
    return 2;
}
fu::Player& defender(fu::Game& game,int club){
    auto& players=const_cast<std::vector<fu::Player>&>(game.players());
    const auto it=std::find_if(players.begin(),players.end(),[=](const auto& p){return p.club==club&&p.position=="DC";});
    assert(it!=players.end());return *it;
}
fu::Game configuredWorld(){
    fu::Game game;
    // Keep unrelated match income and human match decisions out of a transfer
    // accounting fixture. The real public day progression still runs dailyTick.
    for(auto& match:const_cast<std::vector<fu::Match>&>(game.matches())){
        match.played=true;
        // Synthetic settled league draws are valid saves. Leaving a played cup
        // draw without penalties would rightly fail load validation; attaching a
        // winner instead would generate unrelated future cup income/fixtures.
        match.competition=-1;
    }
    for(auto& club:const_cast<std::vector<fu::Club>&>(game.clubs())){
        club.budget=0;club.prestige=60;club.captainId=-1;club.viceCaptainId=-1;club.fans=60;
    }
    std::vector<int> clubSlots(game.clubs().size());
    for(auto& player:const_cast<std::vector<fu::Player>&>(game.players())){
        player.skill.fill(35);player.wage=1000;player.value=100000;
        player.fitness=100;player.morale=70;player.age=24;
        const int slot=clubSlots[player.club]++;
        player.position=slot<2?"GK":slot<10?"DC":slot<18?"MC":"ST";
    }
    for(auto& person:const_cast<std::vector<fu::StaffMember>&>(game.staff()))person.weeklyWage=1000;
    auto& clubs=const_cast<std::vector<fu::Club>&>(game.clubs());
    clubs[0].budget=50000000;clubs[1].budget=50000000;
    auto& target=defender(game,2);
    target.name="Market Candidate";target.skill.fill(80);target.starter=false;
    return game;
}
int64_t cash(const fu::Game& game){
    int64_t total=0;for(const auto& club:game.clubs())total+=club.budget;return total;
}
void assertSensibleSquads(const fu::Game& game){
    const std::array<int,4> minimum={2,5,5,3};
    for(const auto& club:game.clubs()){
        std::array<int,4> counts{};int size=0;
        for(const auto& player:game.players())if(player.club==club.id){++counts[positionGroup(player)];++size;}
        assert(size>=20&&size<=28);
        for(size_t i=0;i<counts.size();++i)assert(counts[i]>=minimum[i]);
    }
}
void completedTransferAndProtection(){
    auto game=configuredWorld();
    const int target=defender(game,2).id;
    auto& loan=defender(game,3);loan.skill.fill(99);loan.parentClub=9;loan.loanReturnDay=100;
    auto& injured=defender(game,4);injured.skill.fill(99);injured.injured=100;
    auto& suspended=defender(game,5);suspended.skill.fill(99);suspended.suspension=100;
    auto& protectedPlayer=defender(game,6);protectedPlayer.skill.fill(99);
    fu::TransferDeal deal;deal.playerId=protectedPlayer.id;deal.sellerClub=6;
    deal.stage=fu::DealStage::ClubCounter;deal.expiryDay=100;
    deal.exchangePlayerId=game.ownPlayers().front();
    const_cast<std::vector<fu::TransferDeal>&>(game.transferDeals()).push_back(deal);
    const auto own=game.ownPlayers();
    auto& sellOn=defender(game,2);sellOn.sellOnClub=8;sellOn.sellOnPct=25;
    const int64_t before=cash(game);
    game.advance(6);
    assert(game.day()==6&&game.transferHistory().empty());
    game.advance(1);
    assert(game.day()==7);
    assert(game.transferHistory().size()==1);
    const auto transfer=game.transferHistory().front();
    assert(transfer.playerId==target&&transfer.fromClub==2&&transfer.toClub==1);
    assert(transfer.fee==100000&&transfer.kind==0&&transfer.totalDay==7);
    assert(transfer.playerName=="Market Candidate");
    assert(game.findPlayer(target)->club==1);
    assert(game.findPlayer(target)->contractYear==game.season()+3);
    assert(game.findPlayer(target)->sellOnClub==-1&&game.findPlayer(target)->sellOnPct==0);
    assert(game.clubs()[1].budget==50000000-transfer.fee);
    assert(game.clubs()[2].budget==75000&&game.clubs()[8].budget==25000);
    assert(cash(game)==before);
    assert(game.findPlayer(loan.id)->club==3);
    assert(game.findPlayer(injured.id)->club==4);
    assert(game.findPlayer(suspended.id)->club==5);
    assert(game.findPlayer(protectedPlayer.id)->club==6);
    assert(game.ownPlayers()==own);
    assert(game.transferDeal(protectedPlayer.id)->stage==fu::DealStage::ClubCounter);
    assertSensibleSquads(game);
    assert(game.worldMarketText().find("Market Candidate")!=std::string::npos);
    assert(game.worldMarketText(target).find("ИСТОРИЯ ИГРОКА: Market Candidate")!=std::string::npos);
    // The source name is retained in the ledger if a player is later renamed.
    const_cast<fu::Player*>(game.findPlayer(target))->name="Renamed";
    assert(game.worldMarketText(target).find("Market Candidate")!=std::string::npos);
    const_cast<std::vector<fu::Club>&>(game.clubs())[10].budget=50000000;
    game.advance(7);
    assert(game.transferHistory().size()==1); // A funded third club cannot flip the player a week later.
}
void budgetAndSquadGuards(){
    {
        auto game=configuredWorld();
        const_cast<std::vector<fu::Club>&>(game.clubs())[1].budget=100000;
        game.advance(7);assert(game.transferHistory().empty()); // Transfer fee alone is insufficient.
    }
    {
        auto game=configuredWorld();
        fu::DeferredPayment future;
        future.playerId=game.ownPlayers().front();future.buyerClub=1;future.sellerClub=2;
        future.amount=50000000;future.dueDay=100;
        const_cast<std::vector<fu::DeferredPayment>&>(game.deferredPayments()).push_back(future);
        game.advance(7);assert(game.transferHistory().empty());
    }
    {
        auto game=configuredWorld();
        for(auto& player:const_cast<std::vector<fu::Player>&>(game.players()))
            if(player.club==1)player.wage=20000000;
        game.advance(7);assert(game.transferHistory().empty()); // Reserve arithmetic exceeds 32-bit range.
    }
    {
        auto game=configuredWorld();
        auto& target=defender(game,2);
        const_cast<std::vector<fu::Club>&>(game.clubs())[2].captainId=target.id;
        game.advance(7);assert(game.transferHistory().empty());
    }
    {
        auto game=configuredWorld();
        // Move surplus defenders away so selling this target would break coverage.
        auto& players=const_cast<std::vector<fu::Player>&>(game.players());
        const int keep=defender(game,2).id;
        int defenders=0;
        for(auto& p:players)if(p.club==2&&positionGroup(p)==1){
            ++defenders;if(defenders>5&&p.id!=keep)p.club=10;
        }
        game.advance(7);assert(game.transferHistory().empty());
    }
}
void weeklyCapAndWindow(){
    auto game=configuredWorld();
    auto& clubs=const_cast<std::vector<fu::Club>&>(game.clubs());
    clubs[3].budget=50000000;clubs[5].budget=50000000;
    defender(game,4).skill.fill(80);defender(game,4).starter=false;
    defender(game,6).skill.fill(80);defender(game,6).starter=false;
    game.advance(7);
    assert(game.transferHistory().size()==2);
    assertSensibleSquads(game);
    game.advance(6);assert(game.transferHistory().size()==2);
    // Freeze every player's ownership, then cross the quiet part of the season.
    game.advance(49);
    assert(game.day()==62);
    const size_t before=game.transferHistory().size();
    const auto message=game.worldMarketText();
    assert(message.find("Трансферное окно закрыто")!=std::string::npos);
    defender(game,7).skill.fill(99);defender(game,7).starter=false;
    game.advance(21);
    assert(game.day()==83&&game.transferHistory().size()==before);
}
void reportsAndSaveResume(const TemporaryFiles& files){
    auto game=configuredWorld();
    const auto before=files.file("before.fus"),after=files.file("after.fus");
    assert(game.save(before).find("сохранена")!=std::string::npos);
    assert(game.worldMarketText().find("переходов пока нет")!=std::string::npos);
    assert(game.worldMarketText(999999).find("переходов пока нет")!=std::string::npos);
    assert(game.save(after).find("сохранена")!=std::string::npos);
    assert(read(before)==read(after));
    game.advance(6);
    assert(game.save(before).find("сохранена")!=std::string::npos);
    fu::Game loaded;
    assert(loaded.load(before).find("загружена")!=std::string::npos);
    game.advance(1);loaded.advance(1);
    assert(game.transferHistory().size()==1&&loaded.transferHistory().size()==1);
    assert(game.worldMarketText()==loaded.worldMarketText());
    for(size_t i=0;i<game.clubs().size();++i)assert(game.clubs()[i].budget==loaded.clubs()[i].budget);
    for(size_t i=0;i<game.players().size();++i)assert(game.players()[i].club==loaded.players()[i].club);
    assert(game.save(after).find("сохранена")!=std::string::npos);
    fu::Game resumed;
    assert(resumed.load(after).find("загружена")!=std::string::npos);
    assert(resumed.worldMarketText()==game.worldMarketText());
    assert(resumed.transferHistory().front().playerName=="Market Candidate");
}
}

int main(){
    TemporaryFiles files;
    completedTransferAndProtection();
    budgetAndSquadGuards();
    weeklyCapAndWindow();
    reportsAndSaveResume(files);
    std::cout<<"PASS: AI transfers, real ownership/cash/sell-on, protected squads and negotiations, windows, weekly cap, history and deterministic save/resume\n";
}
