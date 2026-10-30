#include "Game.hpp"
#include <iostream>
int main(){
    fu::Game g;
    std::cout<<"Football Universe C++ core / season "<<g.season()<<"\n"<<g.homeText()<<"\n";
    while(!g.matchDue())g.advance(1);
    std::cout<<g.matchText()<<"\n";
    g.startLiveMatch();while(g.hasLiveMatch())g.stepLiveMatch(15);
    std::cout<<g.matchArchive()<<"\n";
}
