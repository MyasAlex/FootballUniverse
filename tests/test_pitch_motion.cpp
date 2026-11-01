#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>

int main(){
    fu::Game g;
    g.newCareer("Плавность поля",0,fu::Role::Coach,fu::Difficulty::Realistic,{0,1});
    g.advance(30);
    assert(g.matchDue());
    assert(g.startLiveMatch());
    auto previous=g.pitchFrame();
    int checked=0;
    for(int tick=1;tick<=65;++tick){
        assert(g.stepLiveMatch(1));
        if(!g.hasLiveMatch())break;
        const auto next=g.pitchFrame();
        assert(next.minute==tick);
        for(const auto& player:next.actors){
            for(const auto& prior:previous.actors)if(prior.playerId==player.playerId){
                const int dx=std::abs(player.x-prior.x);
                const int dy=std::abs(player.y-prior.y);
                // Positions should move gradually rather than jump after modulo wraps.
                assert(dx<=12&&dy<=12);
                ++checked;
                break;
            }
        }
        assert(next.ballX>=0&&next.ballX<=1000);
        assert(next.ballY>=0&&next.ballY<=700);
        previous=next;
    }
    assert(checked>300);
    std::cout<<"PASS: continuous tactical player motion; stable actor lanes after dismissal\n";
}
