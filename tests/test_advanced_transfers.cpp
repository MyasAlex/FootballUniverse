#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <string>

static void day(fu::Game& game){
    if(game.matchDue())assert(game.instantMatch());
    game.advance(1);
}
static void days(fu::Game& game,int count){
    for(int i=0;i<count;++i)day(game);
}
static void approve(fu::Game& game,int id){
    day(game);
    assert(game.transferDeal(id)->stage==fu::DealStage::AgentOffer);
    const int wage=game.transferDeal(id)->askingWage;
    assert(game.proposePlayerContract(id,wage,3).find("отправлены")!=std::string::npos);
    day(game);
    assert(game.transferDeal(id)->stage==fu::DealStage::Completed);
}
int main(){
    // Instalments, sell-on percentage, appearance bonus and resale.
    {
        fu::Game g;
        g.newCareer("Тест рассрочки",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
        int id=g.transferCandidates().front();
        const fu::Player* p=g.findPlayer(id);
        assert(p);
        int seller=p->club,fee=p->value*2;
        int originalSellerBudget=g.findClub(seller)->budget;
        int bonus=400000;
        assert(g.offerStructuredTransfer(id,fee,3,bonus,20).find("отправлено")!=std::string::npos);
        assert(g.transferDeal(id)->installments==3);
        assert(g.myClub().budget>0);
        assert(g.save("v06_pending.fus").find("сохранена")!=std::string::npos);
        fu::Game reloaded;
        assert(reloaded.load("v06_pending.fus").find("загружена")!=std::string::npos);
        assert(reloaded.transferDeal(id)->appearanceBonus==bonus);
        approve(reloaded,id);
        const fu::Player* bought=reloaded.findPlayer(id);
        assert(bought&&bought->club==reloaded.teamId());
        assert(bought->sellOnPct==20 && bought->sellOnClub==seller);
        assert(reloaded.deferredPayments().size()==3);
        const int upfront=fee/3+(fee%3);
        assert(reloaded.findClub(seller)->budget==originalSellerBudget+upfront);
        assert(reloaded.save("v06_instalments.fus").find("сохранена")!=std::string::npos);
        fu::Game resumed;
        assert(resumed.load("v06_instalments.fus").find("загружена")!=std::string::npos);
        assert(resumed.deferredPayments().size()==3);
        int sellerBefore=resumed.findClub(seller)->budget;
        days(resumed,31);
        // At least one deferred cash payment should have settled.
        assert(resumed.findClub(seller)->budget>=sellerBefore+fee/3);
        int retained=0;
        for(auto& debt:resumed.deferredPayments())
            if(debt.triggerAppearances==0)++retained;
        assert(retained<=1);
        const int boughtValue=resumed.findPlayer(id)->value;
        int newBuyer=-1;
        for(const auto& c:resumed.clubs()){
            if(c.id!=seller&&c.id!=resumed.teamId()&&c.budget>=boughtValue){
                newBuyer=c.id;break;
            }
        }
        assert(newBuyer>=0);
        const int beforeSellOn=resumed.findClub(seller)->budget;
        assert(resumed.sellPlayer(id,newBuyer,boughtValue).find("Продажа завершена")!=std::string::npos);
        assert(resumed.findPlayer(id)->club==newBuyer);
        assert(resumed.findClub(seller)->budget==beforeSellOn+boughtValue*20/100);
        assert(resumed.findPlayer(id)->sellOnPct==0);
    }

    // Loan with partial wage contribution, persisted return date and automatic return.
    {
        fu::Game g;
        g.newCareer("Тест аренды",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
        const int id=g.transferCandidates().front();
        const fu::Player* player=g.findPlayer(id);
        assert(player);const int owner=player->club;
        const int salary=player->wage;
        int fee=player->value/10+1000;
        assert(g.offerLoan(id,fee,70,30).find("направлено")!=std::string::npos);
        assert(g.transferDeal(id)->kind==1);
        assert(g.transferDeal(id)->loanWageShare==70);
        assert(g.save("v06_loan_pending.fus").find("сохранена")!=std::string::npos);
        fu::Game restored;
        assert(restored.load("v06_loan_pending.fus").find("загружена")!=std::string::npos);
        approve(restored,id);
        const fu::Player* loanee=restored.findPlayer(id);
        assert(loanee&&loanee->club==restored.teamId()&&loanee->parentClub==owner);
        assert(loanee->wage==salary);
        assert(loanee->loanWageShare==70);
        const int deadline=loanee->loanReturnDay;
        assert(deadline==restored.totalDays()+30);
        assert(restored.renewContract(id,15000,4).find("постоянным")!=std::string::npos);
        assert(restored.save("v06_loan_active.fus").find("сохранена")!=std::string::npos);
        fu::Game after;
        assert(after.load("v06_loan_active.fus").find("загружена")!=std::string::npos);
        days(after,30);
        loanee=after.findPlayer(id);
        assert(loanee&&loanee->club==owner&&loanee->parentClub==-1&&loanee->loanReturnDay==-1);
        assert(loanee->loanWageShare==100);
    }

    // Swap transaction moves both players to their respective receiving clubs.
    {
        fu::Game g;
        g.newCareer("Тест обмена",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
        const int target=g.transferCandidates().front();
        const int exchange=g.ownPlayers().front();
        const int oldSeller=g.findPlayer(target)->club;
        const int fee=g.findPlayer(target)->value*2;
        assert(g.offerStructuredTransfer(target,fee,1,0,0,exchange).find("отправлено")!=std::string::npos);
        approve(g,target);
        assert(g.findPlayer(target)->club==g.teamId());
        assert(g.findPlayer(exchange)->club==oldSeller);
    }

    // Buyout clause bypasses the selling club in sandbox mode.
    {
        fu::Game g;
        g.newCareer("Тест отступных",0,fu::Role::Sandbox,fu::Difficulty::Free,{0,1});
        const int id=g.transferCandidates().front();
        int clause=g.findPlayer(id)->value*2;
        assert(g.setReleaseClause(id,clause).find("записаны")!=std::string::npos);
        assert(g.findPlayer(id)->releaseClause==clause);
        assert(g.transferBid(id,clause).find("Отступные")!=std::string::npos ||
               g.transferDeal(id)->stage==fu::DealStage::AgentOffer);
        assert(g.transferDeal(id)->stage==fu::DealStage::AgentOffer);
    }
    std::cout<<"PASS v0.6: installments, deferred payments, sell-on, loans and returns, swaps, release clauses, FUSAVE_7\n";
}
