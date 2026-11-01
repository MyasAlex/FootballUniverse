#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

int main(){
    fu::Game g;
    g.newCareer("Последовательность",0,fu::Role::Coach,fu::Difficulty::Easy,{0,1});
    g.advance(30);
    assert(g.startLiveMatch());
    int passes=0,interceptions=0,shots=0;
    for(int minute=1;minute<=45;++minute){
        assert(g.stepLiveMatch(1));
        const auto& live=g.live();
        assert(live.minute==minute);
        assert(live.ballCarrierId>0);
        assert(live.attackPhase>=0&&live.attackPhase<=3);
        assert(live.lastAction>=0&&live.lastAction<=3);
        const auto& frame=g.pitchFrame();
        assert(frame.minute==minute);
        const auto exists=[&](int id){
            return std::any_of(frame.actors.begin(),frame.actors.end(),
                [id](const fu::PitchActor& a){return a.playerId==id;});
        };
        assert(exists(frame.carrierId));
        if(live.lastAction==1&&exists(live.previousCarrierId)){
            assert(frame.ballFromX>=0&&frame.ballFromX<=1000);
            assert(frame.ballX>=0&&frame.ballX<=1000);
        }
        if(live.lastAction>=2&&exists(live.previousCarrierId))
            assert(frame.action==live.lastAction);
    }
    for(const auto& e:g.live().events){
        if(e.text.find("передал мяч")!=std::string::npos)++passes;
        if(e.text.find("перехватил мяч")!=std::string::npos)++interceptions;
        if(e.text.find("пробил.")!=std::string::npos)++shots;
    }
    assert(passes>10);
    assert(interceptions>0);
    assert(shots>0);
    const auto before=g.pitchFrame();
    assert(g.save("v14_continuity.fus").find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load("v14_continuity.fus").find("загружена")!=std::string::npos);
    assert(restored.live().ballCarrierId==g.live().ballCarrierId);
    assert(restored.live().previousCarrierId==g.live().previousCarrierId);
    assert(restored.live().attackPhase==g.live().attackPhase);
    assert(restored.live().possessionHome==g.live().possessionHome);
    assert(restored.pitchFrame().ballX==before.ballX);
    assert(restored.pitchFrame().ballY==before.ballY);
    for(int n=0;n<10;++n){
        assert(restored.stepLiveMatch(1)&&g.stepLiveMatch(1));
        assert(restored.live().ballCarrierId==g.live().ballCarrierId);
        assert(restored.live().goalsH==g.live().goalsH);
        assert(restored.live().goalsA==g.live().goalsA);
    }
    std::filesystem::remove("v14_continuity.fus");
    std::cout<<"PASS: connected possession, commentary, pitch and save/reload\n";
}
