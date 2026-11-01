#include "Game.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <tuple>

namespace fu {
namespace {
bool transferWindow(int day){return (day>=0&&day<=61)||(day>=184&&day<=214);}
int group(const Player& player){
    if(player.position=="GK")return 0;
    if(player.position=="DL"||player.position=="DC"||player.position=="DR")return 1;
    if(player.position=="ST"||player.position=="AML"||player.position=="AMR")return 3;
    return 2;
}
int ability(const Player& player){
    const auto& s=player.skill;
    switch(group(player)){
        case 0:return (s[9]*6+s[8]*2+s[5]+s[7])/10;
        case 1:return (s[3]*4+s[5]*2+s[0]+s[7]+s[8]*2)/10;
        case 2:return (s[2]*3+s[4]*2+s[6]*2+s[7]+s[8]*2)/10;
        default:return (s[1]*4+s[4]*2+s[0]*2+s[8]+s[6])/10;
    }
}
struct Squad {
    int size=0;
    std::array<int,4> count{},sum{},best{};
    int64_t weeklyWages=0;
    int average(int position)const{return count[position]?sum[position]/count[position]:0;}
};
Squad squad(const std::vector<Player>& players,const std::vector<StaffMember>& staff,int club){
    Squad result;
    for(const auto& player:players){
        if(player.club==club){
            const int position=group(player),rating=ability(player);
            ++result.size;++result.count[position];result.sum[position]+=rating;
            result.best[position]=std::max(result.best[position],rating);
            result.weeklyWages+=int64_t(player.wage)*(player.parentClub>=0?player.loanWageShare:100)/100;
        }else if(player.parentClub==club)
            result.weeklyWages+=int64_t(player.wage)*(100-player.loanWageShare)/100;
    }
    for(const auto& person:staff)if(person.club==club)result.weeklyWages+=person.weeklyWage;
    return result;
}
int64_t commitments(const std::vector<DeferredPayment>& payments,int club){
    int64_t total=0;
    // Reserve even conditional appearance bonuses; uncertain future income is not
    // spendable cash, and an impending sale must not erase the buyer's liabilities.
    for(const auto& payment:payments)if(payment.buyerClub==club)total+=payment.amount;
    return total;
}
int proposedWage(const Player& player,const Club& seller,const Club& buyer){
    return int(std::clamp<int64_t>(int64_t(player.wage)*
        (110+std::max(0,seller.prestige-buyer.prestige)/2)/100,500,20000000));
}
bool upgrade(const Player& player,const Squad& buyer){
    const int position=group(player);
    int weakest=100;
    for(int i=0;i<4;++i)weakest=std::min(weakest,buyer.average(i));
    // Strengthen a weak unit instead of accumulating stars in an already strong
    // position. Ten points allows an untradeable goalkeeper unit to coexist with
    // sensible outfield recruitment while still prioritising positional need.
    return ability(player)>=buyer.average(position)+5&&buyer.average(position)<=weakest+10;
}
std::string amount(int fee){
    std::ostringstream out;
    if(fee>=1000000)out<<std::fixed<<std::setprecision(2)<<fee/1000000.0<<" млн";
    else if(fee>=1000)out<<std::fixed<<std::setprecision(1)<<fee/1000.0<<" тыс";
    else out<<fee;
    return out.str()+" €";
}
}

void Game::recordTransfer(int playerId,int fromClub,int toClub,int fee,int kind){
    const Player* player=findPlayer(playerId);
    if(!player||!findClub(fromClub)||!findClub(toClub)||fromClub==toClub||fee<0||kind<0||kind>2)return;
    transferHistory_.push_back({season_,day_,totalDays_,playerId,fromClub,toClub,fee,kind,player->name});
    if(transferHistory_.size()>1000)
        transferHistory_.erase(transferHistory_.begin(),transferHistory_.begin()+(transferHistory_.size()-1000));
}

bool Game::executeAITransfer(int playerId,int buyerClub,int fee,bool competing){
    if(hasLiveMatch()||!transferWindow(day_)||fee<1||fee>500000000||
       buyerClub<0||buyerClub>=int(clubs_.size())||buyerClub==teamId_)return false;
    Player* player=nullptr;
    for(auto& candidate:players_)if(candidate.id==playerId){player=&candidate;break;}
    if(!player||player->club<0||player->club>=int(clubs_.size())||player->club==teamId_||
       player->club==buyerClub||player->parentClub>=0||player->injured>0||player->suspension>0)return false;
    for(const auto& deal:deals_)if(int(deal.stage)>=0&&int(deal.stage)<=4){
        if(deal.exchangePlayerId==playerId)return false;
        if(deal.playerId==playerId&&(!competing||deal.rivalClub!=buyerClub))return false;
    }
    int weeklyCompleted=0;
    for(const auto& transfer:transferHistory_){
        if(transfer.playerId==playerId&&totalDays_-transfer.totalDay<60)return false;
        if(transfer.kind==0&&transfer.fromClub!=teamId_&&transfer.toClub!=teamId_&&
           transfer.totalDay/7==totalDays_/7)++weeklyCompleted;
    }
    if(weeklyCompleted>=2)return false;
    const int sellerId=player->club;
    const Club& seller=clubs_[sellerId];const Club& buyer=clubs_[buyerClub];
    if(player->id==seller.captainId||player->id==seller.viceCaptainId||buyer.prestige+25<seller.prestige)return false;
    const Squad selling=squad(players_,staff_,sellerId),buying=squad(players_,staff_,buyerClub);
    static constexpr std::array<int,4> minimum={2,5,5,3};
    const int position=group(*player);
    if(selling.size<=20||selling.count[position]<=minimum[position]||buying.size>=28)return false;
    for(int i=0;i<4;++i)if(selling.count[i]-(i==position?1:0)<minimum[i])return false;
    if(ability(*player)>=selling.best[position]&&selling.count[position]<=minimum[position]+2)return false;
    if(!upgrade(*player,buying))return false;
    const int wage=proposedWage(*player,seller,buyer);
    const int64_t reserve=(buying.weeklyWages+wage)*12+commitments(payments_,buyerClub);
    if(int64_t(buyer.budget)-fee<reserve)return false;
    // Apply cash movement atomically after every recipient has been checked for
    // overflow. A previous owner receives the existing sell-on clause in cash.
    std::vector<int64_t> changes(clubs_.size());
    changes[buyerClub]-=fee;changes[sellerId]+=fee;
    if(player->sellOnClub>=0&&player->sellOnClub<int(clubs_.size())&&player->sellOnPct>0){
        const int64_t share=int64_t(fee)*player->sellOnPct/100;
        changes[sellerId]-=share;changes[player->sellOnClub]+=share;
    }
    for(size_t i=0;i<clubs_.size();++i){
        const int64_t balance=int64_t(clubs_[i].budget)+changes[i];
        if(balance<std::numeric_limits<int>::min()||balance>std::numeric_limits<int>::max())return false;
    }
    for(size_t i=0;i<clubs_.size();++i)clubs_[i].budget=int(int64_t(clubs_[i].budget)+changes[i]);
    player->club=buyerClub;player->starter=false;player->wage=wage;
    player->contractYear=season_+3;player->releaseClause=0;player->sellOnClub=-1;player->sellOnPct=0;
    player->morale=std::clamp(player->morale+2,0,100);
    recordTransfer(playerId,sellerId,buyerClub,fee);
    pickLineup(sellerId);pickLineup(buyerClub);
    // Do not mutate deals_ here: the caller may hold a reference while resolving
    // an explicit rival offer. It sets RivalSigned only after this transaction.
    addMessage("Трансфер в футбольном мире",player->name+" перешёл из "+clubs_[sellerId].name+
        " в "+clubs_[buyerClub].name+" за "+amount(fee)+".");
    return true;
}

void Game::processAITransferMarket(){
    if(hasLiveMatch()||!transferWindow(day_)||totalDays_<=0||totalDays_%7!=0||clubs_.empty())return;
    int completed=0;
    for(const auto& transfer:transferHistory_)
        if(transfer.kind==0&&transfer.fromClub!=teamId_&&transfer.toClub!=teamId_&&
           transfer.totalDay/7==totalDays_/7)++completed;
    // Rotate the first club by week without consuming random numbers for every
    // candidate. Save/resume and opening market reports preserve future outcomes.
    const int first=int((int64_t(totalDays_/7)*13)%clubs_.size());
    for(size_t offset=0;offset<clubs_.size()&&completed<2;++offset){
        const int buyerId=(first+int(offset))%int(clubs_.size());
        if(buyerId==teamId_)continue;
        const Squad buying=squad(players_,staff_,buyerId);
        if(buying.size>=28||int64_t(clubs_[buyerId].budget)<=buying.weeklyWages*12+commitments(payments_,buyerId))continue;
        std::vector<std::tuple<int,int,int>> candidates; // utility, stable player ID, fee
        for(const auto& player:players_){
            if(player.club<0||player.club>=int(clubs_.size())||player.club==teamId_||player.club==buyerId||
               player.parentClub>=0||player.injured>0||player.suspension>0||!upgrade(player,buying))continue;
            if(clubs_[buyerId].prestige+25<clubs_[player.club].prestige)continue;
            const int fee=int(std::clamp<int64_t>(int64_t(player.value)*(player.starter?120:100)/100,1,500000000));
            const int wage=proposedWage(player,clubs_[player.club],clubs_[buyerId]);
            if(int64_t(clubs_[buyerId].budget)-fee<(buying.weeklyWages+wage)*12+commitments(payments_,buyerId))continue;
            const int utility=(ability(player)-buying.average(group(player)))*100+
                (player.starter?0:300)+(player.age<=25?60:0)-fee/1000000;
            candidates.emplace_back(utility,player.id,fee);
        }
        std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){
            if(std::get<0>(a)!=std::get<0>(b))return std::get<0>(a)>std::get<0>(b);
            return std::get<1>(a)<std::get<1>(b);
        });
        for(const auto& [utility,playerId,fee]:candidates)
            if(executeAITransfer(playerId,buyerId,fee)){++completed;break;}
    }
}

std::string Game::worldMarketText(int playerId)const{
    std::ostringstream out;
    out<<"ТРАНСФЕРЫ ФУТБОЛЬНОГО МИРА\n\n";
    out<<(transferWindow(day_)?"Трансферное окно открыто":"Трансферное окно закрыто")
       <<" • день сезона "<<day_<<"\n";
    out<<"Окна автоматического рынка: дни 0–61 и 184–214. Клубы рассматривают сделки раз в 7 игровых дней.\n"
       <<"До двух завершённых переходов между AI-клубами за неделю; клубом пользователя управляете вы.\n";
    if(playerId>=0){
        const Player* player=findPlayer(playerId);
        std::string name=player?player->name:"";
        if(name.empty())for(auto it=transferHistory_.rbegin();it!=transferHistory_.rend();++it)
            if(it->playerId==playerId){name=it->playerName;break;}
        out<<"\nИСТОРИЯ ИГРОКА: "<<(name.empty()?"#"+std::to_string(playerId):name)<<"\n";
    }else out<<"\nПОСЛЕДНИЕ 20 ПОДТВЕРЖДЁННЫХ ПЕРЕХОДОВ\n";
    size_t shown=0;
    for(auto it=transferHistory_.rbegin();it!=transferHistory_.rend();++it){
        if(playerId>=0&&it->playerId!=playerId)continue;
        if(playerId<0&&shown>=20)break;
        const Club* from=findClub(it->fromClub);const Club* to=findClub(it->toClub);
        out<<it->season<<" • день "<<it->day<<" | "<<it->playerName<<"\n  "
           <<(from?from->name:"Неизвестный клуб")<<" → "<<(to?to->name:"Неизвестный клуб")
           <<" | "<<(it->kind==1?"аренда":it->kind==2?"возврат из аренды":"трансфер")
           <<" | "<<amount(it->fee)<<"\n";
        ++shown;
    }
    if(!shown)out<<"Подтверждённых переходов пока нет.\n";
    out<<"\nЖурнал хранит последние 1000 переходов; старые сохранения начинают историю с момента обновления.\n";
    return out.str();
}
} // namespace fu
