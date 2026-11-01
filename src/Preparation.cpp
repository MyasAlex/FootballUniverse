#include "Game.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <sstream>
#include <tuple>

namespace fu {
namespace {
// A fixture ID is unique within a season; prefer the current fixture if an
// imported archive also contains it. Reading reports never advances the RNG.
std::vector<const Match*> recentMatches(const Game& game,int clubId){
    std::map<std::pair<int,int>,const Match*> unique;
    const auto collect=[&](const std::vector<Match>& matches){
        for(const auto& match:matches)
            if(match.played&&(match.home==clubId||match.away==clubId))
                unique[{match.season,match.id}]=&match;
    };
    collect(game.archive());collect(game.matches());
    std::vector<const Match*> result;
    for(const auto& [key,match]:unique)result.push_back(match);
    std::sort(result.begin(),result.end(),[](const Match* a,const Match* b){
        return std::tie(a->season,a->day,a->id)<std::tie(b->season,b->day,b->id);
    });
    if(result.size()>5)result.erase(result.begin(),result.end()-5);
    return result;
}
const Match* preparationFixture(const Game& game){
    if(game.hasLiveMatch()&&game.live().id>=0&&game.live().id<int(game.matches().size()))
        return &game.matches()[game.live().id];
    return game.upcomingMatch();
}
std::vector<int> availableXI(const Game& game,int clubId){
    std::vector<int> ids;
    if(const Match* match=preparationFixture(game);game.hasLiveMatch()&&match){
        if(match->home==clubId)ids=game.live().homeXI;
        else if(match->away==clubId)ids=game.live().awayXI;
        else ids=game.starters(clubId);
    }else ids=game.starters(clubId);
    ids.erase(std::remove_if(ids.begin(),ids.end(),[&](int id){
        const Player* player=game.findPlayer(id);
        return !player||player->club!=clubId||player->injured>0||player->suspension>0;
    }),ids.end());
    return ids;
}
int averageFitness(const Game& game,const std::vector<int>& ids){
    if(ids.empty())return 0;
    int sum=0;
    for(int id:ids)sum+=game.findPlayer(id)->fitness;
    return sum/int(ids.size());
}
template<size_t N> const std::string& label(const std::array<std::string,N>& names,int index){
    return names[std::clamp(index,0,int(N)-1)];
}
const std::array<std::string,5> formationNames={"4-3-3","4-2-3-1","4-4-2","3-5-2","5-3-2"};
const std::array<std::string,5> styleNames={"Оборона","Баланс","Атака","Высокий прессинг","Контратаки"};
const std::array<std::string,3> pressingNames={"Низкий","Средний","Высокий"};
const std::array<std::string,3> tempoNames={"Медленный","Средний","Быстрый"};
const std::array<std::string,3> widthNames={"Узко","Средне","Широко"};
constexpr std::array<std::array<int,4>,5> planSettings={{{1,1,1,1},
    {1,1,0,0},{3,2,2,2},{4,0,2,2},{0,0,0,0}}};
}

std::string tacticalPlanName(TacticalPlan plan){
    switch(plan){
        case TacticalPlan::Balanced:return "Баланс";
        case TacticalPlan::Possession:return "Контроль мяча";
        case TacticalPlan::Gegenpress:return "Высокий прессинг";
        case TacticalPlan::Counter:return "Контратаки";
        case TacticalPlan::ProtectLead:return "Удержание счёта";
    }
    return "Неизвестный план";
}

std::string Game::clubForm(int clubId)const{
    if(!findClub(clubId))return "Клуб не найден";
    const auto recent=recentMatches(*this,clubId);
    if(recent.empty())return "Нет сыгранных матчей";
    std::ostringstream out;
    for(size_t i=0;i<recent.size();++i){
        const Match& match=*recent[i];
        const int own=match.home==clubId?match.homeGoals:match.awayGoals;
        const int other=match.home==clubId?match.awayGoals:match.homeGoals;
        if(i)out<<' ';
        out<<(own>other?"В":own<other?"П":"Н");
    }
    return out.str();
}

TacticalPlan Game::recommendedMatchPlan()const{
    const Match* match=preparationFixture(*this);
    if(!match)return TacticalPlan::Balanced;
    const int opponentId=match->home==teamId_?match->away:match->home;
    const Club* opponent=findClub(opponentId);
    if(!opponent)return TacticalPlan::Balanced;
    if(hasLiveMatch()&&live_.minute>=70){
        const int own=match->home==teamId_?live_.goalsH:live_.goalsA;
        const int other=match->home==teamId_?live_.goalsA:live_.goalsH;
        if(own>other)return TacticalPlan::ProtectLead;
    }
    const auto own=availableXI(*this,teamId_);
    const auto opposing=availableXI(*this,opponentId);
    const int fitness=averageFitness(*this,own);
    const int difference=lineupStrength(own)-lineupStrength(opposing);
    if(own.size()<11||fitness<70||difference<-7||opponent->pressing==2)
        return TacticalPlan::Counter;
    if(opponent->tactic==0)return TacticalPlan::Possession;
    if(fitness>=85&&difference>=5)return TacticalPlan::Gegenpress;
    return TacticalPlan::Balanced;
}

std::string Game::applyMatchPlan(TacticalPlan plan){
    const int index=int(plan);
    if(index<0||index>int(TacticalPlan::ProtectLead))return status_="Неизвестный тактический план.";
    if(!permissionCoaching())return status_="Нет полномочий менять тактический план.";
    // Formation and every live-match field remain untouched, including red cards
    // and substitutions. The existing save format already stores these settings.
    auto& club=myClub();
    const auto& chosen=planSettings[index];
    club.tactic=chosen[0];club.pressing=chosen[1];club.tempo=chosen[2];club.width=chosen[3];
    return status_="План принят: "+tacticalPlanName(plan)+". Схема и состав сохранены.";
}

std::string Game::opponentReport()const{
    std::ostringstream out;
    out<<"РАЗБОР СОПЕРНИКА\n\n";
    const Match* match=preparationFixture(*this);
    if(!match){out<<"Ближайший соперник ещё не определён.\n";return out.str();}
    const int opponentId=match->home==teamId_?match->away:match->home;
    const Club* opponent=findClub(opponentId);
    if(!opponent){out<<"Данные соперника недоступны.\n";return out.str();}
    const auto xi=availableXI(*this,opponentId);
    out<<opponent->name<<" | "<<(match->home==teamId_?"Играем дома":"Играем в гостях")<<"\n";
    out<<"Схема: "<<label(formationNames,opponent->formation)
       <<" | стиль: "<<label(styleNames,opponent->tactic)<<"\n";
    out<<"Прессинг: "<<label(pressingNames,opponent->pressing)
       <<" | темп: "<<label(tempoNames,opponent->tempo)
       <<" | ширина: "<<label(widthNames,opponent->width)<<"\n";
    out<<"Форма (от старого к новому): "<<clubForm(opponentId)<<"\n";
    out<<"В — победа, Н — ничья, П — поражение за 90 минут, включая кубки.\n";
    out<<"Доступная основа: "<<xi.size()<<"/11; средняя готовность: "
       <<averageFitness(*this,xi)<<"/100\n";
    out<<"Сила доступной основы: наша "<<lineupStrength(availableXI(*this,teamId_))
       <<", соперника "<<lineupStrength(xi)<<"\n\n";
    const auto recent=recentMatches(*this,opponentId);
    if(!recent.empty()){
        out<<"ПОСЛЕДНИЕ МАТЧИ\n";
        for(const auto* result:recent){
            const Club* home=findClub(result->home);const Club* away=findClub(result->away);
            if(home&&away)out<<result->season<<" • день "<<result->day<<": "
                <<home->name<<' '<<result->homeGoals<<':'<<result->awayGoals<<' '<<away->name<<"\n";
        }
    }else out<<"Истории результатов пока нет: оценка основана на текущем составе.\n";
    std::vector<const Player*> threats;
    for(int id:xi)if(const auto* player=findPlayer(id);player&&player->position!="GK")threats.push_back(player);
    std::sort(threats.begin(),threats.end(),[](const Player* a,const Player* b){
        const int scoreA=a->skill[1]*2+a->skill[4]+a->skill[0];
        const int scoreB=b->skill[1]*2+b->skill[4]+b->skill[0];
        return scoreA!=scoreB?scoreA>scoreB:a->id<b->id;
    });
    out<<"\nОПАСНЫЕ ИГРОКИ ДОСТУПНОЙ ОСНОВЫ\n";
    if(threats.empty())out<<"Нет доступных полевых игроков.\n";
    for(size_t i=0;i<std::min<size_t>(3,threats.size());++i){
        const Player& player=*threats[i];
        out<<player.name<<" • "<<player.position<<" | удар "<<player.skill[1]
           <<", пас "<<player.skill[2]<<", готовность "<<player.fitness<<"/100\n";
    }
    const TacticalPlan plan=recommendedMatchPlan();
    out<<"\nСОВЕТ ШТАБА: "<<tacticalPlanName(plan)<<"\n";
    switch(plan){
        case TacticalPlan::Counter:
            out<<"Низкий прессинг и быстрые атаки: вариант против давления, сильного соперника или при нехватке сил. Быстрый темп снижает точность передач.\n";break;
        case TacticalPlan::Possession:
            out<<"Медленный темп помогает точнее пасовать против низкого блока, но уменьшает частоту ударов.\n";break;
        case TacticalPlan::Gegenpress:
            out<<"Готовый состав может навязать инициативу. Высокий прессинг расходует силы и увеличивает риск фолов, быстрый темп снижает точность передач.\n";break;
        case TacticalPlan::ProtectLead:
            out<<"Ведём после 70-й минуты: снизить риск, темп и ширину. Оборонительный стиль уменьшает собственные атакующие возможности.\n";break;
        case TacticalPlan::Balanced:
            out<<"Средние прессинг, темп и ширина сохраняют баланс атакующих возможностей и точности передач.\n";break;
    }
    out<<"Это рекомендация по текущим данным, а не гарантия результата.\n";
    return out.str();
}

std::string Game::preparationText()const{
    std::ostringstream out;
    const auto xi=availableXI(*this,teamId_);
    int injured=0,suspended=0,available=0,expiring=0,tired=0;
    std::vector<const Player*> contracts;
    for(const auto& player:players_)if(player.club==teamId_){
        if(player.injured>0)++injured;
        if(player.suspension>0)++suspended;
        if(player.injured==0&&player.suspension==0)++available;
        if(player.contractYear<=season_+1){++expiring;contracts.push_back(&player);}
    }
    for(int id:xi)if(findPlayer(id)->fitness<70)++tired;
    out<<"ЦЕНТР ПОДГОТОВКИ К МАТЧУ\n\n";
    out<<"НАША ФОРМА: "<<clubForm(teamId_)<<" (последние 5, от старого к новому)\n";
    if(hasLiveMatch())out<<"Матч идёт: "<<live_.minute<<" мин., счёт "<<live_.goalsH<<':'<<live_.goalsA<<". План можно изменить без замены состава.\n";
    else if(const Match* match=upcomingMatch()){
        out<<"До следующего матча: "<<std::max(0,match->day-day_)<<" дн.\n";
        out<<(matchDue()?"ДЕЙСТВИЕ: откройте «Матч» и начните игру.\n":"ДЕЙСТВИЕ: изучите соперника и выберите план до следующей игры.\n");
    }else out<<"Новых матчей пока нет — можно заняться составом и контрактами.\n";
    out<<"\nГОТОВНОСТЬ КОМАНДЫ\n";
    out<<"Доступно: "<<available<<"; травмы: "<<injured<<"; дисквалификации: "<<suspended<<"\n";
    out<<"Доступная основа: "<<xi.size()<<"/11; средняя готовность: "<<averageFitness(*this,xi)<<"/100\n";
    if(xi.size()<11)out<<"ВНИМАНИЕ: не хватает игроков для полной основы. Проверьте «Состав».\n";
    if(tired)out<<"Усталых в основе (готовность ниже 70): "<<tired<<". Рассмотрите ротацию и восстановление во вкладке «Тренировки».\n";
    if(injured||suspended)out<<"Недоступные игроки не включаются в предматчевую основу. Проверьте запасных перед стартом.\n";
    const auto& club=myClub();
    const std::array<int,4> current={club.tactic,club.pressing,club.tempo,club.width};
    std::string planName="Свои установки";
    for(size_t i=0;i<planSettings.size();++i)
        if(current==planSettings[i])planName=tacticalPlanName(static_cast<TacticalPlan>(i));
    out<<"\nНАШ ПЛАН: "<<planName<<" | схема "<<label(formationNames,club.formation)<<"\n";
    out<<"Стиль: "<<label(styleNames,club.tactic)<<"; прессинг: "<<label(pressingNames,club.pressing)
       <<"; темп: "<<label(tempoNames,club.tempo)<<"; ширина: "<<label(widthNames,club.width)<<"\n";
    out<<"Баланс — средние установки для равной игры.\n"
       <<"Контроль мяча — медленно и узко: точнее передачи, меньше ударов.\n"
       <<"Высокий прессинг — быстрее и шире: больше давления и расхода сил, выше риск фолов.\n"
       <<"Контратаки — низкий прессинг и быстрый темп: меньше давления без мяча, ниже точность передач.\n"
       <<"Удержание счёта — оборона и медленный темп: меньше риска и собственных атак.\n";
    out<<"\nКОНТРАКТЫ И ПЕРЕГОВОРЫ\n";
    out<<"Контракты до лета "<<season_+1<<" или раньше: "<<expiring<<"\n";
    std::sort(contracts.begin(),contracts.end(),[](const Player* a,const Player* b){
        return std::tie(a->contractYear,a->id)<std::tie(b->contractYear,b->id);
    });
    for(size_t i=0;i<std::min<size_t>(3,contracts.size());++i)
        out<<contracts[i]->name<<" — "<<contracts[i]->contractYear<<". Откройте «Контракты» для продления.\n";
    int awaiting=0,actionable=0;
    for(const auto& deal:deals_){
        if(deal.stage==DealStage::ClubCounter||deal.stage==DealStage::AgentOffer||deal.stage==DealStage::AgentCounter)++actionable;
        else if(deal.stage==DealStage::ClubReview||deal.stage==DealStage::AgentReview)++awaiting;
    }
    out<<"Ждём ответ: "<<awaiting<<"; требуют решения: "<<actionable<<"\n";
    if(actionable)out<<"ДЕЙСТВИЕ: откройте «Трансферы» и ответьте на условия клуба или агента до истечения срока.\n";
    if(hasLiveMatch())out<<"\n"<<liveMatchAdviceText()<<"\n";
    if(!permissionCoaching())out<<"В этой должности тактику определяет тренер. Для ручного управления выберите тренерскую роль или свободную сложность.\n";
    out<<"\n"<<opponentReport();
    return out.str();
}
} // namespace fu
