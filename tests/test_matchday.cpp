#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_set>

int main(){
    fu::Game game;
    game.newCareer("Тест матча",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1});
    assert(game.myClub().pressing==1 && game.myClub().tempo==1 &&
           game.myClub().width==1);
    game.changePressing();game.changeTempo();game.changeWidth();
    assert(game.myClub().pressing==2&&game.myClub().tempo==2&&game.myClub().width==2);
    assert(game.tacticsText().find("Прессинг: Высокий прессинг")!=std::string::npos);
    assert(game.tacticsText().find("Темп: Быстрый")!=std::string::npos);
    assert(game.tacticsText().find("Ширина: Широко")!=std::string::npos);
    game.advance(30);
    assert(game.matchDue());
    const int id=game.upcomingMatch()->id;
    assert(game.startLiveMatch());
    assert(game.stepLiveMatch(25));
    assert(game.hasLiveMatch());
    assert(game.live().minute==25);
    assert(game.live().playerStats.size()==22);
    assert(game.live().passesH+game.live().passesA>=25);
    assert(game.live().completedH<=game.live().passesH);
    assert(game.live().completedA<=game.live().passesA);
    assert(game.live().possessionTicksH<=25);
    assert(game.matchText().find("Владение:")!=std::string::npos);
    assert(game.save("v10_midmatch.fus").find("сохранена")!=std::string::npos);
    fu::Game loaded;
    assert(loaded.load("v10_midmatch.fus").find("загружена")!=std::string::npos);
    assert(loaded.hasLiveMatch());
    assert(loaded.myClub().pressing==2);
    assert(loaded.myClub().tempo==2);
    assert(loaded.myClub().width==2);
    assert(loaded.live().minute==25);
    assert(loaded.live().passesH==game.live().passesH);
    assert(loaded.live().possessionTicksH==game.live().possessionTicksH);
    assert(loaded.live().playerStats.size()==22);
    for(size_t i=0;i<loaded.live().playerStats.size();++i){
        const auto& old=game.live().playerStats[i];
        const auto& now=loaded.live().playerStats[i];
        assert(old.playerId==now.playerId);
        assert(old.minutes==now.minutes);
        assert(old.passes==now.passes);
        assert(old.completedPasses==now.completedPasses);
    }
    // Force a red card in a test fixture to validate match-day suspensions.
    const int offender=loaded.live().homeXI.at(1);
    auto& testLive=const_cast<fu::LiveMatch&>(loaded.live());
    auto find=std::find_if(testLive.playerStats.begin(),testLive.playerStats.end(),
         [offender](const auto& stat){return stat.playerId==offender;});
    assert(find!=testLive.playerStats.end());
    find->reds=1;
    while(loaded.hasLiveMatch())assert(loaded.stepLiveMatch(15));
    const auto& match=loaded.matches().at(id);
    assert(match.played);
    assert(match.playerStats.size()>=22);
    assert(match.passesH+match.passesA>0);
    assert(match.completedH<=match.passesH);
    assert(match.completedA<=match.passesA);
    assert(match.onTargetH<=match.shotsH);
    assert(match.onTargetA<=match.shotsA);
    assert(match.possessionH>=0&&match.possessionH<=100);
    assert(loaded.findPlayer(offender)->suspension==2);
    assert(!loaded.findPlayer(offender)->starter);
    int totalsH=0,totalsA=0;
    const auto& home=loaded.clubs().at(match.home);
    const auto& away=loaded.clubs().at(match.away);
    for(const auto& stat:match.playerStats){
        assert(stat.playerId>0);
        assert(stat.minutes>=0&&stat.minutes<=90);
        assert(stat.rating10>=35&&stat.rating10<=100);
        assert(stat.onTarget<=stat.shots);
        assert(stat.completedPasses<=stat.passes);
        const auto* p=loaded.findPlayer(stat.playerId);
        assert(p);
        if(p->club==home.id)totalsH+=stat.passes;
        if(p->club==away.id)totalsA+=stat.passes;
    }
    assert(totalsH==match.passesH);
    assert(totalsA==match.passesA);
    assert(loaded.latestMatchReport().find("ПОСЛЕМАТЧЕВЫЙ РАЗБОР")!=std::string::npos);
    assert(loaded.latestMatchReport().find("ИГРОК МАТЧА")!=std::string::npos);
    assert(loaded.latestMatchReport().find("ОЦЕНКИ ИГРОКОВ")!=std::string::npos);
    assert(loaded.matchReport(id)==loaded.latestMatchReport());
    assert(loaded.save("v10_postmatch.fus").find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load("v10_postmatch.fus").find("загружена")!=std::string::npos);
    assert(!restored.hasLiveMatch());
    assert(restored.matches().at(id).playerStats.size()>=22);
    assert(restored.latestMatchReport()==loaded.latestMatchReport());
    assert(restored.findPlayer(offender)->suspension==2);
    assert(restored.myClub().pressing==2);
    assert(restored.myClub().tempo==2);
    assert(restored.myClub().width==2);
    std::filesystem::remove("v10_midmatch.fus");
    std::filesystem::remove("v10_postmatch.fus");
    std::cout<<"PASS: detailed match stats, tactics, player ratings, cards, suspensions, FUSAVE_11\n";
}
