#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <tuple>

static void validate(const fu::PitchFrame& frame, const fu::Game& game){
    assert(frame.homeClub>=0&&frame.awayClub>=0);
    assert(frame.homeClub!=frame.awayClub);
    assert(frame.minute>=0&&frame.minute<=90);
    assert(frame.homeGoals>=0&&frame.awayGoals>=0);
    assert(frame.ballX>=0&&frame.ballX<=1000);
    assert(frame.ballY>=0&&frame.ballY<=700);
    assert(frame.ballFromX>=0&&frame.ballFromX<=1000);
    assert(frame.ballFromY>=0&&frame.ballFromY<=700);
    assert(frame.action>=0&&frame.action<=3);
    assert(frame.actors.size()>=14&&frame.actors.size()<=22);
    std::set<int> ids;
    int home=0,away=0;
    for(const auto& actor:frame.actors){
        assert(ids.insert(actor.playerId).second);
        assert(actor.team==0||actor.team==1);
        assert(actor.x>=0&&actor.x<=1000);
        assert(actor.y>=0&&actor.y<=700);
        const auto* player=game.findPlayer(actor.playerId);
        assert(player);
        if(actor.team==0){++home;assert(player->club==frame.homeClub);}
        else{++away;assert(player->club==frame.awayClub);}
    }
    assert(home>=7&&home<=11&&away>=7&&away<=11);
    assert(ids.count(frame.carrierId)==1);
    if(frame.receiverId>=0)assert(ids.count(frame.receiverId)==1);
}
static void equal(const fu::PitchFrame& a,const fu::PitchFrame& b){
    assert(std::tie(a.minute,a.homeClub,a.awayClub,a.homeGoals,a.awayGoals,
                    a.ballFromX,a.ballFromY,a.ballX,a.ballY,
                    a.carrierId,a.receiverId,a.action,a.highlight)==
           std::tie(b.minute,b.homeClub,b.awayClub,b.homeGoals,b.awayGoals,
                    b.ballFromX,b.ballFromY,b.ballX,b.ballY,
                    b.carrierId,b.receiverId,b.action,b.highlight));
    assert(a.actors.size()==b.actors.size());
    for(size_t i=0;i<a.actors.size();++i){
        const auto& x=a.actors[i],&y=b.actors[i];
        assert(std::tie(x.playerId,x.team,x.x,x.y)==
               std::tie(y.playerId,y.team,y.x,y.y));
    }
}
int main(){
    fu::Game game;
    game.newCareer("Видеодвижок",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1});
    game.advance(30);
    assert(game.matchDue());
    const auto preview=game.pitchFrame();
    assert(preview.minute==0);
    validate(preview,game);
    assert(game.startLiveMatch());
    const auto kickoff=game.pitchFrame();
    assert(kickoff.minute==0);
    validate(kickoff,game);
    assert(game.stepLiveMatch(1));
    const auto first=game.pitchFrame();
    validate(first,game);
    assert(first.minute==1);
    equal(first,game.pitchFrame()); // repeated rendering is stable and read-only
    assert(game.stepLiveMatch(14));
    const auto current=game.pitchFrame();
    validate(current,game);
    assert(current.minute==15);
    // A sending-off or early injury may legitimately reduce on-field actors.
    assert(current.actors.size()>=14&&current.actors.size()<=22);
    assert(game.save("v11_pitch_mid.fus").find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load("v11_pitch_mid.fus").find("загружена")!=std::string::npos);
    assert(restored.hasLiveMatch());
    equal(current,restored.pitchFrame()); // saved state recreates same graphical positions
    assert(game.stepLiveMatch(1));
    assert(restored.stepLiveMatch(1));
    equal(game.pitchFrame(),restored.pitchFrame()); // no visual RNG perturbs match simulation
    assert(game.live().shotsH==restored.live().shotsH);
    assert(game.live().shotsA==restored.live().shotsA);
    assert(game.live().goalsH==restored.live().goalsH);
    assert(game.live().goalsA==restored.live().goalsA);
    const int playedMatchId=game.live().id;
    while(game.hasLiveMatch())game.stepLiveMatch(1);
    assert(!game.hasLiveMatch());
    assert(game.matches().at(playedMatchId).played);
    const auto after=game.pitchFrame();
    validate(after,game);
    assert(after.minute==90);
    assert(after.homeGoals==game.matches().at(playedMatchId).homeGoals);
    assert(after.awayGoals==game.matches().at(playedMatchId).awayGoals);
    std::filesystem::remove("v11_pitch_mid.fus");
    std::cout<<"PASS v0.11: actor formation, ball paths, deterministic visual frames, "
                "save/resume playback and RNG isolation\n";
}
