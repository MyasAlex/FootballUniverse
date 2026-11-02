#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <tuple>

namespace {
struct TemporaryFiles {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        ("fu_preparation_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryFiles(){assert(std::filesystem::create_directory(root));}
    ~TemporaryFiles(){std::filesystem::remove_all(root);}
    std::string file(const std::string& name)const{return (root/name).string();}
};
std::string read(const std::string& path){
    std::ifstream file(path,std::ios::binary);
    assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
auto settings(const fu::Club& club){
    return std::make_tuple(club.formation,club.tactic,club.pressing,club.tempo,club.width);
}
fu::Match fixture(int id,int season,int day,int home,int away,int goalsH,int goalsA){
    fu::Match match;
    match.id=id;match.season=season;match.day=day;match.home=home;match.away=away;
    match.homeGoals=goalsH;match.awayGoals=goalsA;match.played=true;
    return match;
}
void queriesDoNotChangeSimulation(const TemporaryFiles& files){
    fu::Game observed;
    fu::Game reference=observed;
    const auto before=files.file("before.fus"),after=files.file("after.fus");
    assert(observed.save(before).find("сохранена")!=std::string::npos);
    const auto report=observed.opponentReport();
    assert(report.find(observed.clubs().at(observed.upcomingMatch()->away).name)!=std::string::npos);
    for(int i=0;i<5;++i){
        assert(observed.opponentReport()==report);
        assert(!observed.preparationText().empty());
        (void)observed.recommendedMatchPlan();
        (void)observed.clubForm(observed.teamId());
    }
    assert(observed.save(after).find("сохранена")!=std::string::npos);
    assert(read(before)==read(after)); // Includes full RNG state and all mutable fields.
    observed.advance(30);reference.advance(30);
    assert(observed.matchDue()&&reference.matchDue());
    assert(observed.instantMatch()&&reference.instantMatch());
    assert(observed.matchArchive()==reference.matchArchive());
    assert(observed.save(before).find("сохранена")!=std::string::npos);
    assert(reference.save(after).find("сохранена")!=std::string::npos);
    assert(read(before)==read(after)); // Refreshing reports must not change the next match.
}
void chronologyAndNoHistory(){
    fu::Game game;
    assert(game.clubForm(0)=="Нет сыгранных матчей");
    assert(game.clubForm(-1)=="Клуб не найден");
    auto& current=const_cast<std::vector<fu::Match>&>(game.matches());
    auto& archive=const_cast<std::vector<fu::Match>&>(game.archive());
    archive={fixture(1,2025,300,0,1,2,0),fixture(2,2025,330,1,0,1,1)};
    current={fixture(1,2026,10,3,0,2,0),fixture(0,2026,3,0,2,3,0),
        fixture(3,2026,24,0,5,1,0),fixture(2,2026,17,0,4,0,0)};
    // Same archive fixture must not be counted twice, nor may IDs from another
    // season collide. Input vectors are deliberately not in chronological order.
    archive.push_back(current[1]);
    assert(game.clubForm(0)=="Н В П Н В");
    assert(game.clubForm(3)=="В"); // Away/home perspective is relative to the club.
    current[2].played=false;
    assert(game.clubForm(0)=="В Н В П Н");
    current[3].penaltiesH=4;current[3].penaltiesA=5;
    assert(game.clubForm(4)=="Н"); // The UI explicitly reports 90-minute form.
    current.clear();archive.clear();
    assert(game.opponentReport().find("ещё не определён")!=std::string::npos);
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Balanced);
}
void readinessUsesActualPlayers(){
    fu::Game game;
    const int opponent=game.upcomingMatch()->away;
    auto& players=const_cast<std::vector<fu::Player>&>(game.players());
    const auto starters=game.starters(game.teamId());
    assert(starters.size()==11);
    for(auto& player:players)if(player.club==game.teamId()){
        player.contractYear=game.season()+3;
        player.fitness=60;
        if(player.id==starters[0]){player.injured=4;player.contractYear=game.season();}
        if(player.id==starters[1]){player.suspension=2;player.contractYear=game.season()+1;}
    }
    const int unavailable=game.starters(opponent).at(1);
    for(auto& player:players)if(player.id==unavailable){
        player.name="UNAVAILABLE_THREAT";player.skill.fill(100);player.injured=10;
    }
    auto& deals=const_cast<std::vector<fu::TransferDeal>&>(game.transferDeals());
    fu::TransferDeal pending,action;
    pending.stage=fu::DealStage::ClubReview;action.stage=fu::DealStage::AgentOffer;
    deals={pending,action};
    const auto text=game.preparationText();
    assert(text.find("травмы: 1; дисквалификации: 1")!=std::string::npos);
    assert(text.find("Доступная основа: 9/11; средняя готовность: 60/100")!=std::string::npos);
    assert(text.find("Усталых в основе (готовность ниже 70): 9")!=std::string::npos);
    assert(text.find("Контракты до лета 2027 или раньше: 2")!=std::string::npos);
    assert(text.find("Ждём ответ: 1; требуют решения: 1")!=std::string::npos);
    assert(text.find("UNAVAILABLE_THREAT")==std::string::npos);
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Counter);
    assert(game.starters(0)==starters); // Reports filter for analysis, never repick.
}
void presetsAndPermissions(const TemporaryFiles& files){
    fu::Game game;
    const auto originalXI=game.starters(game.teamId());
    game.myClub().formation=2;
    const std::array<std::array<int,4>,5> expected={{{1,1,1,1},{1,1,0,0},
        {3,2,2,2},{4,0,2,2},{0,0,0,0}}};
    for(int i=0;i<5;++i){
        const auto plan=static_cast<fu::TacticalPlan>(i);
        assert(game.applyMatchPlan(plan).find(fu::tacticalPlanName(plan))!=std::string::npos);
        assert(game.myClub().formation==2);
        assert(game.starters(game.teamId())==originalXI);
        assert(game.myClub().tactic==expected[i][0]);
        assert(game.myClub().pressing==expected[i][1]);
        assert(game.myClub().tempo==expected[i][2]);
        assert(game.myClub().width==expected[i][3]);
        assert(game.preparationText().find("НАШ ПЛАН: "+fu::tacticalPlanName(plan))!=std::string::npos);
    }
    const auto beforeInvalid=settings(game.myClub());
    assert(game.applyMatchPlan(static_cast<fu::TacticalPlan>(-1)).find("Неизвестный")!=std::string::npos);
    assert(game.applyMatchPlan(static_cast<fu::TacticalPlan>(5)).find("Неизвестный")!=std::string::npos);
    assert(settings(game.myClub())==beforeInvalid);
    const auto save=files.file("plan.fus");
    assert(game.save(save).find("сохранена")!=std::string::npos);
    assert(read(save).rfind("FUSAVE_16\n",0)==0);
    fu::Game loaded;
    assert(loaded.load(save).find("загружена")!=std::string::npos);
    assert(settings(loaded.myClub())==settings(game.myClub()));
    for(auto role:{fu::Role::Owner,fu::Role::President}){
        fu::Game restricted;
        restricted.newCareer("Руководитель",0,role,fu::Difficulty::Realistic,{0});
        const auto before=settings(restricted.myClub());
        assert(restricted.applyMatchPlan(fu::TacticalPlan::Gegenpress).find("Нет полномочий")!=std::string::npos);
        assert(settings(restricted.myClub())==before);
        restricted.newCareer("Свободно",0,role,fu::Difficulty::Free,{0});
        assert(restricted.applyMatchPlan(fu::TacticalPlan::Gegenpress).find("План принят")!=std::string::npos);
    }
}
void recommendationsReactToLineups(){
    fu::Game game;
    const int opponentId=game.upcomingMatch()->away;
    auto& opponent=const_cast<fu::Club&>(game.clubs().at(opponentId));
    auto& players=const_cast<std::vector<fu::Player>&>(game.players());
    for(auto& player:players)if(player.club==game.teamId()||player.club==opponentId){
        player.skill.fill(70);player.fitness=90;player.morale=70;
    }
    opponent.tactic=1;opponent.pressing=1;
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Balanced);
    opponent.tactic=0;
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Possession);
    opponent.pressing=2;
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Counter);
    opponent.pressing=1;opponent.tactic=1;
    for(auto& player:players){
        if(player.club==game.teamId())player.skill.fill(95);
        else if(player.club==opponentId)player.skill.fill(40);
    }
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::Gegenpress);
    game.myClub().width=0;
    assert(game.preparationText().find("НАШ ПЛАН: Свои установки")!=std::string::npos);
}
void livePlanPreservesMatch(const TemporaryFiles& files){
    fu::Game game;
    game.advance(30);
    assert(game.startLiveMatch());
    assert(game.stepLiveMatch(15));
    auto& live=const_cast<fu::LiveMatch&>(game.live());
    // A controlled late-match fixture makes preservation of a sending-off and
    // the substitution budget deterministic rather than dependent on random cards.
    live.minute=75;live.goalsH=2;live.goalsA=1;live.subsH=3;live.subsA=2;
    const int offender=live.homeXI.at(1);
    live.homeXI.erase(live.homeXI.begin()+1);
    auto card=std::find_if(live.playerStats.begin(),live.playerStats.end(),[&](const auto& stat){return stat.playerId==offender;});
    assert(card!=live.playerStats.end());card->reds=1;
    const fu::LiveMatch before=live;
    const auto originalXI=game.starters(game.teamId());
    assert(game.recommendedMatchPlan()==fu::TacticalPlan::ProtectLead);
    assert(game.applyMatchPlan(fu::TacticalPlan::ProtectLead).find("План принят")!=std::string::npos);
    assert(game.live().minute==before.minute);
    assert(game.live().goalsH==before.goalsH&&game.live().goalsA==before.goalsA);
    assert(game.live().homeXI==before.homeXI&&game.live().awayXI==before.awayXI);
    assert(game.live().usedHome==before.usedHome&&game.live().usedAway==before.usedAway);
    assert(game.live().subsH==before.subsH&&game.live().subsA==before.subsA);
    assert(game.live().shotsH==before.shotsH&&game.live().shotsA==before.shotsA);
    assert(game.live().events.size()==before.events.size());
    assert(game.starters(game.teamId())==originalXI);
    const auto save=files.file("live.fus");
    assert(game.save(save).find("сохранена")!=std::string::npos);
    fu::Game loaded;
    assert(loaded.load(save).find("загружена")!=std::string::npos);
    assert(loaded.live().homeXI==before.homeXI);
    assert(loaded.live().subsH==3&&loaded.live().subsA==2);
    assert(loaded.myClub().tactic==0&&loaded.myClub().pressing==0&&loaded.myClub().tempo==0&&loaded.myClub().width==0);
    const auto restoredCard=std::find_if(loaded.live().playerStats.begin(),loaded.live().playerStats.end(),[&](const auto& stat){return stat.playerId==offender;});
    assert(restoredCard!=loaded.live().playerStats.end()&&restoredCard->reds==1);
}
}

int main(){
    TemporaryFiles files;
    queriesDoNotChangeSimulation(files);
    chronologyAndNoHistory();
    readinessUsesActualPlayers();
    presetsAndPermissions(files);
    recommendationsReactToLineups();
    livePlanPreservesMatch(files);
    std::cout<<"PASS: match preparation, chronological form, real availability, tactical plans, permissions, live preservation, RNG purity and FUSAVE_12\n";
}
