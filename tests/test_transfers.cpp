#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void nextDay(fu::Game& game){
    if(game.matchDue())assert(game.instantMatch());
    else game.advance(1);
}
int main(){
    fu::Game g;
    g.newCareer("Алексей",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
    const int id=g.transferCandidates().front();
    const fu::Player* player=g.findPlayer(id);
    assert(player && player->club!=g.teamId());
    const int oldClub=player->club;
    const int originalBudget=g.myClub().budget;
    const int cheapOffer=player->value*85/100;
    assert(g.transferBid(id,cheapOffer).find("отправлено")!=std::string::npos);
    assert(g.transferDeal(id)->stage==fu::DealStage::ClubReview);
    assert(g.findPlayer(id)->club==oldClub);
    assert(g.transferBid(id,cheapOffer).find("уже идут")!=std::string::npos);
    assert(g.save("transfer_pending.fus").find("сохранена")!=std::string::npos);
    fu::Game restored;
    assert(restored.load("transfer_pending.fus").find("загружена")!=std::string::npos);
    assert(restored.transferDeal(id)->stage==fu::DealStage::ClubReview);
    nextDay(restored);
    assert(restored.transferDeal(id)->stage==fu::DealStage::ClubCounter);
    assert(restored.myClub().budget==originalBudget);
    assert(restored.acceptNegotiationTerms(id).find("отправлено")!=std::string::npos);
    nextDay(restored);
    assert(restored.transferDeal(id)->stage==fu::DealStage::AgentOffer);
    const int requested=restored.transferDeal(id)->askingWage;
    assert(requested>=500);
    assert(restored.proposePlayerContract(id,requested*70/100,4).find("отправлены")!=std::string::npos);
    nextDay(restored);
    assert(restored.transferDeal(id)->stage==fu::DealStage::AgentCounter);
    assert(restored.findPlayer(id)->club==oldClub);
    assert(restored.save("transfer_counter.fus").find("сохранена")!=std::string::npos);
    fu::Game same;
    assert(same.load("transfer_counter.fus").find("загружена")!=std::string::npos);
    assert(same.transferDeal(id)->stage==fu::DealStage::AgentCounter);
    assert(same.acceptNegotiationTerms(id).find("отправлены")!=std::string::npos);
    nextDay(same);
    assert(same.transferDeal(id)->stage==fu::DealStage::Completed);
    assert(same.findPlayer(id)->club==same.teamId());
    assert(same.findPlayer(id)->wage==requested);
    assert(same.findPlayer(id)->contractYear==same.season()+3);
    assert(same.myClub().budget<originalBudget);
    assert(same.transferCandidates(true).empty() || same.transferCandidates(true).front()!=id);
    auto another=same.transferCandidates().front();
    assert(another!=id);
    const int fee=same.findPlayer(another)->value*85/100;
    assert(same.transferBid(another,fee).find("отправлено")!=std::string::npos);
    assert(same.cancelNegotiation(another).find("прекращены")!=std::string::npos);
    assert(same.transferDeal(another)->stage==fu::DealStage::Cancelled);
    assert(same.findPlayer(another)->club!=same.teamId());
    assert(same.transferBid(another,fee).find("отправлено")!=std::string::npos);
    assert(same.transferDeal(another)->stage==fu::DealStage::ClubReview);
    assert(same.save("transfer_final.fus").find("сохранена")!=std::string::npos);
    fu::Game final;
    assert(final.load("transfer_final.fus").find("загружена")!=std::string::npos);
    assert(final.transferDeal(another)->stage==fu::DealStage::ClubReview);
    assert(final.transferDeal(id)->stage==fu::DealStage::Completed);
    std::cout<<"PASS: delayed club replies, counteroffer, agent talks, completed transfer, cancellation, v6 persistence\n";
}
