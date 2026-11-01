#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(){
    fu::Game g;
    g.newCareer("Проверка матча",0,fu::Role::Coach,fu::Difficulty::Realistic,{0,1});
    g.advance(30);
    assert(g.startLiveMatch());
    int passes=0,interceptions=0,shots=0,actorChecks=0;
    auto previous=g.pitchFrame();
    for(int minute=1;minute<=45;++minute){
        assert(g.stepLiveMatch(1));
        const auto& l=g.live();
        assert(l.minute==minute&&l.ballCarrierId>0);
        assert(l.attackPhase>=0&&l.attackPhase<=3);
        assert(l.lastAction>=0&&l.lastAction<=3);
        const auto frame=g.pitchFrame();
        assert(frame.minute==minute);
        auto exists=[&](int id){return std::any_of(frame.actors.begin(),frame.actors.end(),
             [id](const fu::PitchActor& p){return p.playerId==id;});};
        assert(exists(frame.carrierId));
        assert(frame.action==l.lastAction||l.previousCarrierId<0);
        for(const auto& player:frame.actors)
            for(const auto& old:previous.actors)if(player.playerId==old.playerId){
                assert(std::abs(player.x-old.x)<=12);
                assert(std::abs(player.y-old.y)<=12);
                ++actorChecks;
                break;
            }
        previous=frame;
    }
    for(const auto& event:g.live().events){
        passes+=event.text.find("передал мяч")!=std::string::npos;
        interceptions+=event.text.find("перехватил мяч")!=std::string::npos;
        shots+=event.text.find("пробил.")!=std::string::npos;
    }
    assert(passes>20&&interceptions>0&&shots>0&&actorChecks>500);
    assert(g.save("flow_v14.fus").find("сохранена")!=std::string::npos);
    fu::Game copy;
    assert(copy.load("flow_v14.fus").find("загружена")!=std::string::npos);
    assert(copy.live().ballCarrierId==g.live().ballCarrierId);
    assert(copy.live().previousCarrierId==g.live().previousCarrierId);
    assert(copy.live().attackPhase==g.live().attackPhase);
    assert(copy.live().possessionHome==g.live().possessionHome);
    assert(copy.pitchFrame().ballX==g.pitchFrame().ballX);
    for(int i=0;i<12;++i){
        assert(copy.stepLiveMatch(1)&&g.stepLiveMatch(1));
        assert(copy.live().ballCarrierId==g.live().ballCarrierId);
        assert(copy.live().possessionHome==g.live().possessionHome);
        assert(copy.live().goalsH==g.live().goalsH&&copy.live().goalsA==g.live().goalsA);
    }
    std::filesystem::remove("flow_v14.fus");
    std::cout<<"PASS: continuous minute possession, 2D, commentary, deterministic save\n";
}
