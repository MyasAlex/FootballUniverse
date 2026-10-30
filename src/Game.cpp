#include "Game.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unordered_set>

namespace fu {
namespace {
std::string money(int v) {
    std::ostringstream o;
    if (std::abs(v)>=1000000) o<<std::fixed<<std::setprecision(2)<<v/1000000.0<<" млн";
    else if (std::abs(v)>=1000) o<<std::fixed<<std::setprecision(1)<<v/1000.0<<" тыс";
    else o<<v;
    return o.str()+" €";
}
std::string pct(int x) {return std::to_string(x)+"/100";}
std::string pad(const std::string& s, size_t width){ return s+std::string(width>s.size()?width-s.size():1,' '); }
const std::vector<std::string> firstNames={"Алексей","Артём","Максим","Дмитрий","Илья","Иван","Михаил","Андрей","Никита","Антон","Павел","Руслан","Матвей","Тимур","Денис","Кирилл","Лукас","Мартин","Даниэль","Роберт","Жюль","Леон","Матео","Феликс","Тео","Карлос","Андреа","Луис","Альваро","Хавьер","Рикардо","Виктор"};
const std::vector<std::string> lastNames={"Петров","Соколов","Кузнецов","Орлов","Морозов","Волков","Попов","Фёдоров","Зайцев","Иванов","Смирнов","Климов","Мартинес","Родригес","Силва","Рамирес","Мюллер","Шмидт","Фишер","Вебер","Гомес","Дюпон","Росси","Конти","Вильямс","Дэвис","Картер","Браун","Перейра","Коста","Нуньес","Фернандес"};
const std::vector<std::string> character={"Амбициозный","Лояльный","Сдержанный","Импульсивный","Командный","Независимый","Трудолюбивый","Обидчивый","Профессионал","Эмоциональный"};
const std::vector<std::string> formations={"4-3-3","4-2-3-1","4-4-2","3-5-2","5-3-2"};
const std::vector<std::string> tactics={"Оборонительная","Сбалансированная","Атакующая","Высокий прессинг","Контратаки"};
const std::vector<std::string> trainings={"Восстановление","Сбалансированная","Интенсивная","Техника","Тактика"};
const std::array<std::string,10> skillNames={"Скорость","Удар","Пас","Отбор","Техника","Сила","Видение","Выносливость","Решения","Вратарское мастерство"};
const std::vector<std::string> positions={"GK","GK","DL","DC","DC","DC","DR","DL","DR","DM","MC","MC","MC","AMC","ML","MR","AML","AMR","ST","ST","ST","ST","MC","DC"};
void putString(std::ostream& s,const std::string& x){s<<std::quoted(x)<<'\n';}
bool getString(std::istream& s,std::string& x){return static_cast<bool>(s>>std::quoted(x));}
int clamp(int x,int lo=0,int hi=100){return std::clamp(x,lo,hi);}
int positionGroup(const std::string& pos){
    if(pos=="GK")return 0;
    if(pos=="DL"||pos=="DC"||pos=="DR")return 1;
    if(pos=="ST"||pos=="AML"||pos=="AMR")return 3;
    return 2;
}
std::array<int,4> formationNeeds(int formation){
    // GK, defenders, midfield, attackers
    static constexpr std::array<std::array<int,4>,5> needs={{{1,4,3,3},{1,4,5,1},{1,4,4,2},{1,3,5,2},{1,5,3,2}}};
    return needs[std::clamp(formation,0,4)];
}
int playerRating(const Player& p){
    const auto& a=p.skill;
    int raw=0;
    switch(positionGroup(p.position)){
        case 0:raw=(a[9]*6+a[8]*2+a[5]+a[7])/10;break;
        case 1:raw=(a[3]*4+a[5]*2+a[0]+a[7]+a[8]*2)/10;break;
        case 2:raw=(a[2]*3+a[4]*2+a[6]*2+a[7]+a[8]*2)/10;break;
        default:raw=(a[1]*4+a[4]*2+a[0]*2+a[8]+a[6])/10;
    }
    return clamp((raw*80+p.fitness*10+p.morale*10)/100);
}
}
Game::Game():rng_(20261030U){ newCareer("Менеджер", 0, Role::Coach, Difficulty::Realistic,{0,1}); }
int Game::random(int lo,int hi){return std::uniform_int_distribution<int>(lo,hi)(rng_);}
void Game::seedWorld(){
    leagues_.clear();clubs_.clear();players_.clear();matches_.clear();cups_.clear();trophies_.clear();leagueHistory_.clear();movements_.clear();coaches_.clear();job_={};reputation_=55;archive_.clear();inbox_.clear();scouts_.clear();deals_.clear();payments_.clear();staff_.clear();youth_.clear();relations_.clear();nextStaffId_=1;nextProspectId_=1;lastYouthSearchDay_=-45;lastTeamTalkDay_=-30;live_={};
    const std::vector<std::tuple<std::string,std::string,std::vector<std::string>>> definitions={
      {"Англия","Премьер-лига",{"Арсенал","Ливерпуль","Манчестер Сити","Челси","Манчестер Юнайтед","Тоттенхэм"}},
      {"Испания","Ла Лига",{"Реал Мадрид","Барселона","Атлетико","Атлетик Бильбао","Вильярреал","Реал Сосьедад"}},
      {"Германия","Бундеслига",{"Бавария","Боруссия Д","Байер","РБ Лейпциг","Айнтрахт","Штутгарт"}},
      {"Италия","Серия А",{"Интер","Милан","Ювентус","Наполи","Рома","Аталанта"}},
      {"Франция","Лига 1",{"ПСЖ","Марсель","Монако","Лилль","Лион","Ницца"}},
      {"Россия","Премьер-лига",{"Зенит","Спартак","ЦСКА","Краснодар","Локомотив","Динамо Москва"}}
    };
    for(const auto& [country,league,teams]: definitions){
        int lid=static_cast<int>(leagues_.size());
        leagues_.push_back({lid,country,league,false,0,lid});
        for (size_t i=0;i<teams.size();++i){
            int id=static_cast<int>(clubs_.size());
            Club c;c.id=id;c.league=lid;c.name=teams[i];c.city=country;
            c.prestige=clamp(85-int(i)*6+random(-4,4),35,93);
            c.budget=6000000+c.prestige*600000+random(0,10000000);
            c.fans=clamp(50+c.prestige/3+random(-8,6));
            c.board=65;c.stadium=16000+c.prestige*420;
            c.sponsorship=80000+c.prestige*17000;
            clubs_.push_back(std::move(c));
        }
    }
    appendSecondDivisions(false);
}
void Game::appendSecondDivisions(bool addFixtures){
    if(leagues_.size()>=12)return;
    // Fictitious second-tier clubs: stable IDs 36..71, preserving all v0.8 club IDs.
    const std::array<std::array<std::string,6>,6> secondTier={{{{
        "Бристоль Атлетик","Шеффилд Таун","Ноттингем Сити",
        "Лидс Роверс","Бирмингем Каунти","Кардифф Юнайтед"
    }},{{
        "Валенсия Норте","Севилья Атлетико","Кордова Сити",
        "Малага Унион","Сарагоса Спорт","Овьедо Юнайтед"
    }},{{
        "Гамбург Атлетик","Бремен 04","Ганновер Шпорт",
        "Кёльн Юнайтед","Нюрнберг Сити","Дрезден Атлетик"
    }},{{
        "Палермо Юнайтед","Генуя Спорт","Парма Атлетик",
        "Болонья Норд","Бари Сити","Верона Олимпик"
    }},{{
        "Бордо Атлетик","Тулуза Юнион","Нант Сити",
        "Ренн Олимпик","Сент-Этьен Спорт","Страсбург Юнайтед"
    }},{{
        "Иркутск Атлетик","Томск Сити","Саратов Юнайтед",
        "Пермь Спорт","Омск Олимпик","Ярославль Атлетик"
    }}}};
    const std::array<std::string,6> lowerNames={
        "Чемпионшип (демо)","Сегунда (демо)","Вторая Бундеслига (демо)",
        "Серия Б (демо)","Лига 2 (демо)","Первая лига (демо)"
    };
    for(int top=0;top<6;++top){
        leagues_[top].tier=0;leagues_[top].systemId=top;
        const int lid=int(leagues_.size());
        leagues_.push_back({lid,leagues_[top].country,lowerNames[top],
                            leagues_[top].active,1,top});
        for(int i=0;i<6;++i){
            Club c;
            c.id=int(clubs_.size());c.league=lid;c.city=leagues_[top].country;
            c.name=secondTier[top][i];
            c.prestige=clamp(49-i*3+random(-5,5),28,59);
            c.budget=4000000+c.prestige*200000+random(0,3000000);
            c.fans=clamp(40+c.prestige/3+random(-8,6));
            c.board=66;c.stadium=10000+c.prestige*180;
            c.sponsorship=50000+c.prestige*8000;
            clubs_.push_back(std::move(c));
        }
    }
    if(!addFixtures)return;
    // Upgrade old saves without destroying their season, results, fixtures or player IDs.
    const int oldClubs=int(clubs_.size())-36;
    for(int cid=oldClubs;cid<int(clubs_.size());++cid){
        const Club& c=clubs_[cid];
        for(int i=0;i<24;++i){
            Player p;p.id=nextPlayerId_++;p.club=cid;
            p.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
                   lastNames[random(0,int(lastNames.size()-1))];
            p.age=random(17,35);p.position=positions[i];
            p.personality=character[random(0,int(character.size()-1))];
            const int base=clamp(c.prestige-20+random(-13,15),25,82);
            for(int& v:p.skill)v=clamp(base+random(-19,19),10,99);
            if(p.position=="GK")p.skill[9]=clamp(base+random(4,20),30,99);
            if(p.position=="ST")p.skill[1]=clamp(base+random(0,16),20,99);
            if(p.position=="DC")p.skill[3]=clamp(base+random(0,16),20,99);
            p.potential=clamp(base+random(6,24),30,99);
            p.morale=random(46,85);p.fitness=random(74,100);
            p.trust=random(36,75);p.loyalty=random(30,90);p.family=random(35,95);
            p.wage=1000+base*base*2+random(0,10000);
            p.value=100000+base*base*1500+random(100000,1000000);
            p.contractYear=season_+random(1,5);
            players_.push_back(std::move(p));
        }
        for(int role=0;role<4;++role){
            StaffMember member;
            member.id=nextStaffId_++;member.club=cid;member.role=role;
            member.quality=clamp(38+c.prestige/3+random(-12,12),20,90);
            member.weeklyWage=800+member.quality*65;
            member.contractYear=season_+3;
            member.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
                        lastNames[random(0,int(lastNames.size()-1))];
            staff_.push_back(std::move(member));
        }
        pickLineup(cid);
    }
    autoAssignCaptains();
    for(int lid=6;lid<12;++lid)addFixturesForLeague(lid);
    // Simulate only past matches of newly introduced background divisions.
    for(auto& match:matches_)if(match.competition==-1&&!match.played&&
          clubs_[match.home].league>=6&&match.day<day_)simulateFast(match);
}
void Game::createSquads(){
    nextPlayerId_=1;
    for(const auto& c:clubs_){
        for (int i=0;i<24;++i){
            Player p; p.id=nextPlayerId_++;p.club=c.id;
            p.name=firstNames[random(0,int(firstNames.size()-1))]+" "+lastNames[random(0,int(lastNames.size()-1))];
            p.age=random(17,35);p.position=positions[i];p.personality=character[random(0,int(character.size()-1))];
            int base=clamp(c.prestige-20+random(-13,15),28,91);
            for(int& skill:p.skill) skill=clamp(base+random(-19,19),10,99);
            if(p.position=="GK")p.skill[9]=clamp(base+random(4,20),30,99);
            if(p.position=="ST")p.skill[1]=clamp(base+random(0,16),20,99);
            if(p.position=="DC")p.skill[3]=clamp(base+random(0,16),20,99);
            p.potential=clamp(base+random(6,24),30,99);
            p.morale=random(46,85);p.fitness=random(74,100);p.trust=random(36,75);p.loyalty=random(30,90);
            p.family=random(35,95);
            p.wage=1000+base*base*2+random(0,10000);
            p.value=100000+base*base*1500+random(100000,1000000);
            p.contractYear=season_+random(1,5);
            players_.push_back(std::move(p));
        }
    }
}
void Game::autoAssignCaptains(){
    for(auto& club:clubs_){
        auto valid=[&](int id){const Player* p=findPlayer(id);return p&&p->club==club.id;};
        std::vector<const Player*> options;
        for(const auto& p:players_)if(p.club==club.id)options.push_back(&p);
        std::sort(options.begin(),options.end(),[](const Player*a,const Player*b){
            return a->loyalty+a->skill[8]+std::min(a->age,35)>
                   b->loyalty+b->skill[8]+std::min(b->age,35);
        });
        if(!valid(club.captainId))club.captainId=options.empty()?-1:options.front()->id;
        if(!valid(club.viceCaptainId)||club.viceCaptainId==club.captainId){
            club.viceCaptainId=-1;
            for(const auto* p:options)if(p->id!=club.captainId){club.viceCaptainId=p->id;break;}
        }
    }
}
std::string Game::setCaptain(int playerId,bool vice){
    if(!permissionSports())return status_="Нет полномочий назначать капитана.";
    if(hasLiveMatch())return status_="Во время матча назначение капитана недоступно.";
    Player* chosen=nullptr;
    for(auto& p:players_)if(p.id==playerId&&p.club==teamId_)chosen=&p;
    if(!chosen)return status_="Выберите футболиста своего клуба.";
    Club& c=myClub();
    int& slot=vice?c.viceCaptainId:c.captainId;
    if(slot==playerId)return status_="Этот игрок уже назначен на выбранную роль.";
    if((vice&&c.captainId==playerId)||(!vice&&c.viceCaptainId==playerId))
        return status_="Капитан и вице-капитан должны быть разными футболистами.";
    for(auto& p:players_)if(p.id==slot&&p.club==teamId_)p.morale=clamp(p.morale-3);
    slot=playerId;chosen->morale=clamp(chosen->morale+5);chosen->loyalty=clamp(chosen->loyalty+3);
    status_=chosen->name+(vice?" назначен вице-капитаном.":" назначен капитаном.");
    addMessage("Решение по капитанской повязке",status_);
    return status_;
}
std::vector<int> Game::starters(int clubId)const{
    std::vector<int> ids;
    for(const auto& p:players_)if(p.club==clubId&&p.starter)ids.push_back(p.id);
    return ids;
}
void Game::pickLineup(int clubId){
    if(clubId<0||clubId>=int(clubs_.size()))return;
    const auto needs=formationNeeds(clubs_[clubId].formation);
    for(auto& p:players_)if(p.club==clubId)p.starter=false;
    for(int group=0;group<4;++group){
        std::vector<Player*> eligible;
        for(auto& p:players_)
            if(p.club==clubId&&positionGroup(p.position)==group&&p.injured==0)eligible.push_back(&p);
        std::sort(eligible.begin(),eligible.end(),[](const Player*a,const Player*b){return playerRating(*a)>playerRating(*b);});
        for(int i=0;i<std::min(needs[group],int(eligible.size()));++i)eligible[i]->starter=true;
    }
    int n=int(starters(clubId).size());
    if(n<11){
        std::vector<Player*> available;
        for(auto& p:players_)if(p.club==clubId&&!p.starter&&p.injured==0)available.push_back(&p);
        std::sort(available.begin(),available.end(),[](const Player*a,const Player*b){return playerRating(*a)>playerRating(*b);});
        for(auto* p:available){if(n>=11)break;p->starter=true;++n;}
    }
}
std::string Game::autoPickLineup(){
    if(!permissionSports())return status_="Нет полномочий управлять составом.";
    if(hasLiveMatch())return status_="В ходе матча стартовый состав нельзя менять; используйте замену.";
    pickLineup(teamId_);
    return status_="Автоматически выбраны лучшие доступные 11 игроков по позициям.";
}
std::string Game::swapStarter(int substituteId,int starterId){
    if(!permissionSports())return status_="Нет полномочий выбирать состав.";
    if(hasLiveMatch())return status_="Изменять стартовый состав во время матча нельзя.";
    Player* in=nullptr;Player* out=nullptr;
    for(auto& p:players_){if(p.id==substituteId)in=&p;if(p.id==starterId)out=&p;}
    if(!in||!out||in==out||in->club!=teamId_||out->club!=teamId_)
        return status_="Укажите двух разных игроков вашего клуба (ID запасного и ID игрока основы).";
    if(in->starter||!out->starter)return status_="Первый игрок должен быть запасным, второй — в основе.";
    if(in->injured>0)return status_="Травмированного игрока нельзя выпускать в основе.";
    if(positionGroup(in->position)!=positionGroup(out->position))
        return status_="Позиции несовместимы: меняйте вратаря на вратаря, защитника на защитника и т.д.";
    in->starter=true;out->starter=false;
    return status_="Стартовый состав: "+in->name+" вместо "+out->name+".";
}
std::string Game::lineupText()const{
    std::ostringstream o;
    o<<"ОСНОВНОЙ СОСТАВ  •  "<<myClub().name<<"\n";
    o<<"Схема: "<<formations[myClub().formation]<<"\n\n";
    std::vector<const Player*> startersList, bench;
    for(const auto& p:players_)if(p.club==teamId_){
        (p.starter?startersList:bench).push_back(&p);
    }
    auto ordered=[](const Player*a,const Player*b){
        if(positionGroup(a->position)!=positionGroup(b->position))return positionGroup(a->position)<positionGroup(b->position);
        return playerRating(*a)>playerRating(*b);
    };
    std::sort(startersList.begin(),startersList.end(),ordered);std::sort(bench.begin(),bench.end(),ordered);
    o<<"ОСНОВА ("<<startersList.size()<<"/11)\n";
    for(const auto* p:startersList)o<<" #"<<p->id<<" ["<<p->position<<"] "<<p->name<<"   Ур. "<<playerRating(*p)<<"   Форма "<<p->fitness<<"\n";
    o<<"\nЗАПАС\n";
    for(const auto* p:bench)o<<" #"<<p->id<<" ["<<p->position<<"] "<<p->name<<(p->injured?" [ТРАВМА]":"")<<"\n";
    o<<"\nЧтобы поменять состав: выберите запасного в списке слева,\n";
    o<<"впишите ID игрока основы и нажмите «В основу».\n";
    o<<"Автосостав выбирает 11 по схеме и состоянию игроков.\n";
    return o.str();
}
std::string Game::cycleTrainingFocus(int playerId){
    if(!permissionSports())return status_="Нет полномочий назначать тренировки.";
    if(hasLiveMatch())return status_="Во время матча менять тренировку нельзя.";
    for(auto& p:players_)if(p.id==playerId&&p.club==teamId_){
        p.trainingFocus=(p.trainingFocus+2)%11-1; // -1 -> 0 -> 1 ... -> 9 -> -1
        return status_=p.name+": "+(p.trainingFocus<0?"командный план":skillNames[p.trainingFocus]);
    }
    return status_="Выберите своего игрока.";
}
void Game::initializeLineups(LiveMatch& live,const Match& match)const{
    live.homeXI=starters(match.home);live.awayXI=starters(match.away);
    live.usedHome=live.homeXI;live.usedAway=live.awayXI;
}
int Game::lineupStrength(const std::vector<int>& ids)const{
    if(ids.empty())return 25;
    int total=0,n=0;
    for(int id:ids)if(const Player* p=findPlayer(id)){
        int r=playerRating(*p);
        if(p->injured)r=std::max(5,r-30);
        total+=r;++n;
    }
    if(!n)return 25;
    int leadership=0;
    const Player* lead=findPlayer(ids.front());
    if(lead&&lead->club>=0&&lead->club<int(clubs_.size())){
        const auto& c=clubs_[lead->club];
        if(std::find(ids.begin(),ids.end(),c.captainId)!=ids.end())leadership+=2;
        if(std::find(ids.begin(),ids.end(),c.viceCaptainId)!=ids.end())leadership+=1;
    }
    int teamwork=0;
    for(const auto& relation:relations_){
        if(std::find(ids.begin(),ids.end(),relation.first)==ids.end()||
           std::find(ids.begin(),ids.end(),relation.second)==ids.end())continue;
        if(relation.affinity>=20)++teamwork;
        else if(relation.affinity<=-20)--teamwork;
    }
    teamwork=std::clamp(teamwork,-5,5);
    return clamp(total/n-(11-n)*4+leadership+teamwork,5,99);
}
std::string Game::substitute(int playerOutId,int playerInId){
    if(!permissionSports())return status_="Нет полномочий проводить замены.";
    if(!hasLiveMatch())return status_="Замены доступны только во время интерактивного матча.";
    const auto& m=matches_.at(live_.id);
    std::vector<int>& eleven=(m.home==teamId_?live_.homeXI:live_.awayXI);
    std::vector<int>& used=(m.home==teamId_?live_.usedHome:live_.usedAway);
    int& subs=(m.home==teamId_?live_.subsH:live_.subsA);
    if(subs>=5)return status_="Лимит замен (5) исчерпан.";
    const auto old=std::find(eleven.begin(),eleven.end(),playerOutId);
    const auto* incoming=findPlayer(playerInId);
    const auto* outgoing=findPlayer(playerOutId);
    if(old==eleven.end()||!incoming||!outgoing||incoming->club!=teamId_||incoming->injured>0)
        return status_="Выберите доступного запасного и ID футболиста, который сейчас на поле.";
    if(std::find(used.begin(),used.end(),playerInId)!=used.end())
        return status_="Этот футболист уже выходил на поле; повторный выход запрещён.";
    if((incoming->position=="GK")!=(outgoing->position=="GK"))
        return status_="Вратаря можно заменить только другим вратарём.";
    *old=playerInId;used.push_back(playerInId);++subs;
    live_.events.push_back({live_.minute,"ЗАМЕНА: "+outgoing->name+" → "+incoming->name+" ("+myClub().name+")"});
    return status_="Замена выполнена: "+incoming->name+" вместо "+outgoing->name+". Осталось "+std::to_string(5-subs)+" замен.";
}
std::vector<int> Game::leagueClubs(int leagueId) const {
    std::vector<int> result;
    for(const auto& c:clubs_)if(c.league==leagueId)result.push_back(c.id);
    return result;
}

namespace {
const std::array<std::string,4> staffTitles={"Тренер","Врач","Скаут","Тренер академии"};
}
void Game::seedStaff(){
    staff_.clear();nextStaffId_=1;
    for(const auto& c:clubs_){
        for(int role=0;role<4;++role){
            StaffMember member;
            member.id=nextStaffId_++;
            member.club=c.id;member.role=role;
            member.quality=clamp(38+c.prestige/3+random(-12,12),20,90);
            member.weeklyWage=800+member.quality*65;
            member.contractYear=season_+3;
            member.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
                        lastNames[random(0,int(lastNames.size()-1))];
            staff_.push_back(std::move(member));
        }
    }
}
int Game::staffQuality(int clubId,int role)const{
    if(clubId<0||clubId>=int(clubs_.size())||role<0||role>3)return 0;
    int best=0;
    for(const auto& s:staff_)
        if(s.club==clubId&&s.role==role)best=std::max(best,s.quality);
    return best;
}
std::string Game::staffText()const{
    std::ostringstream o;
    o<<"ШТАБ КЛУБА "<<myClub().name<<"\n\n";
    for(const auto& s:staff_)if(s.club==teamId_){
        o<<"#"<<s.id<<" | "<<staffTitles[s.role]<<" | "<<s.name
         <<" | качество "<<s.quality<<"/100 | "<<money(s.weeklyWage)
         <<" в неделю | до "<<s.contractYear<<"\n";
    }
    o<<"\nЭффекты: тренер ускоряет обучение, врач восстановление и снижает риск травмы,\n"
      <<"скаут повышает точность отчётов, наставник академии помогает молодым талантам.\n"
      <<"Нанять сотрудника: введите роль 0–3 и желаемое качество, например 1 80.\n"
      <<"Уволить: введите ID сотрудника и нажмите «Уволить».\n";
    return o.str();
}
std::string Game::hireStaff(int role,int quality){
    if(!permissionSports()&&!permissionBusiness())
        return status_="Нет полномочий назначать сотрудников.";
    if(hasLiveMatch())return status_="Во время матча менять штаб нельзя.";
    if(role<0||role>3||quality<20||quality>100)
        return status_="Роль 0–3, качество 20–100 (пример: 1 80).";
    int count=0;
    for(const auto& s:staff_)if(s.club==teamId_&&s.role==role)++count;
    if(count>=3)return status_="На этой позиции в клубе уже три сотрудника.";
    const int fee=quality*12000;
    if(!canFinance(fee))return status_="Найм требует "+money(fee)+".";
    myClub().budget-=fee;
    StaffMember member;
    member.id=nextStaffId_++;member.club=teamId_;member.role=role;
    member.quality=quality;member.weeklyWage=1000+quality*85;
    member.contractYear=season_+3;
    member.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
                lastNames[random(0,int(lastNames.size()-1))];
    staff_.push_back(member);
    status_="Нанят "+staffTitles[role]+" "+member.name+
            " ("+std::to_string(quality)+"/100), компенсация "+money(fee)+".";
    addMessage("Новое назначение",status_);
    return status_;
}
std::string Game::fireStaff(int staffId){
    if(!permissionSports()&&!permissionBusiness())
        return status_="Нет полномочий увольнять сотрудников.";
    if(hasLiveMatch())return status_="Во время матча менять штаб нельзя.";
    for(size_t i=0;i<staff_.size();++i)if(staff_[i].id==staffId&&staff_[i].club==teamId_){
        const int fee=staff_[i].weeklyWage*4;
        if(!canFinance(fee))return status_="Нет денег на выходное пособие "+money(fee)+".";
        const std::string name=staff_[i].name;
        myClub().budget-=fee;
        staff_.erase(staff_.begin()+i);
        status_=name+" покинул штаб. Выплачена компенсация "+money(fee)+".";
        addMessage("Увольнение специалиста",status_);
        return status_;
    }
    return status_="Сотрудник с таким ID в вашем штабе не найден.";
}
void Game::generateYouth(int clubId,int count){
    if(clubId<0||clubId>=int(clubs_.size()))return;
    const Club& c=clubs_[clubId];
    for(int i=0;i<count;++i){
        YouthProspect p;
        p.id=nextProspectId_++;p.club=clubId;p.age=random(16,18);
        p.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
               lastNames[random(0,int(lastNames.size()-1))];
        p.position=positions[random(0,int(positions.size()-1))];
        const int coach=staffQuality(clubId,3);
        p.ability=clamp(28+c.academy*3+coach/18+random(-8,12),20,78);
        p.potential=clamp(p.ability+10+c.academy+coach/14+random(3,23),35,99);
        youth_.push_back(std::move(p));
    }
}
std::string Game::academyText()const{
    std::ostringstream o;
    o<<"МОЛОДЁЖНАЯ АКАДЕМИЯ | "<<myClub().name<<"\n\n";
    o<<"Уровень академии: "<<myClub().academy<<"/10 | Тренер молодёжи: "
     <<staffQuality(teamId_,3)<<"/100\n";
    o<<"Талантов для просмотра: ";
    int count=0;
    for(const auto& p:youth_)if(p.club==teamId_)++count;
    o<<count<<"\n";
    for(const auto& p:youth_)if(p.club==teamId_)
        o<<"#"<<p.id<<" | "<<p.name<<" | "<<p.age<<" лет | "<<p.position
         <<" | уровень "<<p.ability<<" | потенциал ~"<<p.potential<<"\n";
    o<<"\nПодписать юниора: выберите кандидата слева (350 тыс. €).\n"
     <<"Поиск талантов: новые кандидаты каждые 30 дней (100 тыс. €).\n"
     <<"Наставник: укажите ID молодого и опытного игроков через пробел.\n"
     <<"Отношения: ID двух игроков и тип 0 (поддержка), 1 (примирение), 2 (конкуренция).\n\n";
    o<<staffText();
    return o.str();
}
std::string Game::discoverYouth(){
    if(!permissionSports())return status_="Нет полномочий руководить поиском молодёжи.";
    if(hasLiveMatch())return status_="Поиск недоступен во время матча.";
    if(totalDays_-lastYouthSearchDay_<30)
        return status_="Новый просмотр доступен через "+
               std::to_string(30-(totalDays_-lastYouthSearchDay_))+" дн.";
    const int cost=100000;
    if(!canFinance(cost))return status_="Для просмотра требуется "+money(cost)+".";
    myClub().budget-=cost;
    lastYouthSearchDay_=totalDays_;
    youth_.erase(std::remove_if(youth_.begin(),youth_.end(),
        [this](const YouthProspect& p){return p.club==teamId_;}),youth_.end());
    generateYouth(teamId_,4+myClub().academy/3);
    status_="Академия представила новый список талантов. Посмотрите его во вкладке «Академия».";
    addMessage("Просмотр молодых футболистов",status_);
    return status_;
}
std::string Game::signYouthProspect(int prospectId){
    if(!permissionSports())return status_="Нет полномочий заключать молодёжные контракты.";
    if(hasLiveMatch())return status_="Во время матча академия недоступна.";
    const auto it=std::find_if(youth_.begin(),youth_.end(),
        [&](const YouthProspect& p){return p.id==prospectId&&p.club==teamId_;});
    if(it==youth_.end())return status_="Выберите воспитанника из списка академии.";
    constexpr int cost=350000;
    if(!canFinance(cost))return status_="На контракт молодому игроку требуется 350 тыс. €.";
    const YouthProspect prospect=*it;
    myClub().budget-=cost;
    Player p;
    p.id=nextPlayerId_++;p.club=teamId_;p.age=prospect.age;
    p.name=prospect.name;p.position=prospect.position;
    p.personality=character[random(0,int(character.size()-1))];
    for(int& skill:p.skill)skill=clamp(prospect.ability+random(-12,12),10,99);
    if(p.position=="GK")p.skill[9]=clamp(prospect.ability+random(2,12),20,99);
    p.potential=clamp(prospect.potential+random(-3,3));
    p.wage=1100+prospect.ability*40;
    p.value=120000+prospect.ability*14500;
    p.contractYear=season_+4;
    p.academyGraduate=true;
    players_.push_back(p);
    youth_.erase(it);
    pickLineup(teamId_);
    status_="Из академии приглашён "+p.name+", "+std::to_string(p.age)+
            " лет. Потенциал "+std::to_string(p.potential)+"/100.";
    addMessage("Подписан молодой талант",status_);
    return status_;
}
Relationship* Game::getRelationship(int first,int second){
    if(first==second)return nullptr;
    if(first>second)std::swap(first,second);
    for(auto& r:relations_)
        if(r.first==first&&r.second==second)return &r;
    relations_.push_back({first,second,0,-30});
    return &relations_.back();
}
void Game::pruneRelationships(){
    relations_.erase(std::remove_if(relations_.begin(),relations_.end(),[this](const Relationship& r){
        const Player* a=findPlayer(r.first);
        const Player* b=findPlayer(r.second);
        return !a||!b||a->club!=b->club;
    }),relations_.end());
    for(auto& p:players_)if(p.mentorId>=0){
        const Player* mentor=findPlayer(p.mentorId);
        if(!mentor||mentor->club!=p.club||mentor->age<25)p.mentorId=-1;
    }
}
std::string Game::assignMentor(int youngId,int mentorId){
    if(!permissionSports())return status_="Нет полномочий назначать наставников.";
    if(hasLiveMatch())return status_="Во время матча менять наставника нельзя.";
    Player* young=nullptr;const Player* experienced=findPlayer(mentorId);
    for(auto& p:players_)if(p.id==youngId)young=&p;
    if(!young||!experienced||young->club!=teamId_||experienced->club!=teamId_||
       young->age>23||experienced->age<25||youngId==mentorId)
        return status_="Нужно выбрать своего молодого игрока (до 23 лет) и наставника от 25 лет.";
    int mentees=0;
    for(const auto& p:players_)if(p.mentorId==mentorId&&p.id!=youngId)++mentees;
    if(mentees>=3)return status_="Наставник уже занимается тремя футболистами.";
    young->mentorId=mentorId;
    young->trust=clamp(young->trust+3);
    Relationship* r=getRelationship(youngId,mentorId);
    if(r)r->affinity=clamp(r->affinity+15,-100,100);
    status_=experienced->name+" назначен наставником "+young->name+".";
    addMessage("Программа наставничества",status_);
    return status_;
}
std::string Game::improveRelationship(int firstId,int secondId,int type){
    if(!permissionSports())return status_="Нет полномочий проводить встречи игроков.";
    if(hasLiveMatch())return status_="Завершите матч перед встречей.";
    if(type<0||type>2)return status_="Тип встречи 0–2.";
    const Player* a=findPlayer(firstId),*b=findPlayer(secondId);
    if(!a||!b||firstId==secondId||a->club!=teamId_||b->club!=teamId_)
        return status_="Выберите двух разных игроков своей команды.";
    Relationship* r=getRelationship(firstId,secondId);
    if(!r)return status_="Невозможно создать встречу.";
    if(totalDays_-r->lastInteractionDay<7)
        return status_="Эта пара недавно общалась. Новая встреча через "+
               std::to_string(7-(totalDays_-r->lastInteractionDay))+" дней.";
    r->lastInteractionDay=totalDays_;
    const int original=r->affinity;
    const int delta=type==2?-12:type==1?25:15;
    r->affinity=clamp(r->affinity+delta,-100,100);
    for(auto& p:players_)if(p.id==firstId||p.id==secondId){
        p.morale=clamp(p.morale+(delta>0?2:-2));
        p.trust=clamp(p.trust+(delta>0?1:-2));
    }
    const std::string label=type==0?"Командная поддержка":type==1?"Примирение":"Внутренняя конкуренция";
    status_=label+": отношения "+std::to_string(original)+" → "+std::to_string(r->affinity)+".";
    addMessage("Событие в раздевалке",a->name+" и "+b->name+": "+status_);
    return status_;
}
std::string Game::relationshipsText(int playerId)const{
    const Player* player=findPlayer(playerId);
    if(!player)return "Футболист не найден.";
    std::ostringstream o;
    o<<"ОТНОШЕНИЯ В КОМАНДЕ: "<<player->name<<"\n";
    int count=0;
    for(const auto& r:relations_)if(r.first==playerId||r.second==playerId){
        const Player* other=findPlayer(r.first==playerId?r.second:r.first);
        if(!other||other->club!=player->club)continue;
        o<<" - "<<other->name<<": "<<r.affinity
         <<(r.affinity>=20?" (товарищи)":r.affinity<=-20?" (конфликт)":" (нейтрально)")<<"\n";
        ++count;
    }
    if(!count)o<<"Связи ещё не сформированы.\n";
    return o.str();
}
std::string Game::developmentText(int playerId)const{
    const Player* p=findPlayer(playerId);
    if(!p)return "Выберите футболиста.";
    std::ostringstream o;
    o<<"РАЗВИТИЕ ИГРОКА\n";
    o<<"Потенциал: "<<p->potential<<"/100 | Опыт тренировки: "<<p->developmentXP
     <<" | Улучшений навыков: "<<p->developedSkills<<"\n";
    if(p->academyGraduate)o<<"Выпускник академии клуба\n";
    if(p->mentorId>=0)if(const Player* mentor=findPlayer(p->mentorId))
        o<<"Наставник: "<<mentor->name<<" (#"<<mentor->id<<")\n";
    o<<"Тренер: "<<staffQuality(p->club,0)<<"/100 | База: "<<clubs_[p->club].facilities<<"/10\n";
    o<<"Развитие зависит от возраста, потенциала, формы, тренера и наставничества.\n";
    return o.str();
}
void Game::addFixturesForLeague(int leagueId){
    auto clubs=leagueClubs(leagueId);
    if(clubs.size()<2)return;
    if(clubs.size()%2)clubs.push_back(-1);
    const int n=int(clubs.size());
    int roundNum=0;
    for(int rep=0;rep<3;++rep){
        std::vector<int> rotation=clubs;
        for(int leg=0;leg<2;++leg){
            for(int round=0;round<n-1;++round){
                for(int k=0;k<n/2;++k){
                    int home=rotation[k],away=rotation[n-1-k];
                    if(home<0||away<0)continue;
                    if((round+leg)%2)std::swap(home,away);
                    Match m;m.id=int(matches_.size());
                    m.season=season_;m.day=3+10*roundNum;
                    m.home=home;m.away=away;
                    matches_.push_back(std::move(m));
                }
                const int last=rotation.back();
                for(int j=n-1;j>1;--j)rotation[j]=rotation[j-1];
                rotation[1]=last;
                ++roundNum;
            }
            std::reverse(rotation.begin()+1,rotation.end());
        }
    }
}
void Game::createFixtures(){
    matches_.clear();
    for(const auto& l:leagues_)addFixturesForLeague(l.id);
    std::stable_sort(matches_.begin(),matches_.end(),[](const Match&a,const Match&b){
        return std::tie(a.day,a.id)<std::tie(b.day,b.id);
    });
    for(size_t i=0;i<matches_.size();++i)matches_[i].id=int(i);
}
std::vector<int> Game::rankedClubs(int leagueId)const{
    auto ids=leagueClubs(leagueId);
    std::sort(ids.begin(),ids.end(),[this](int a,int b){
        const Club& x=clubs_[a],&y=clubs_[b];
        return std::tuple(x.points,x.gf-x.ga,x.gf,x.wins,x.prestige)>
               std::tuple(y.points,y.gf-y.ga,y.gf,y.wins,y.prestige);
    });
    return ids;
}
void Game::processPromotionRelegation(int oldSeason){
    for(int system=0;system<6;++system){
        const auto top=rankedClubs(system),bottom=rankedClubs(system+6);
        if(top.size()!=6||bottom.size()!=6)continue;
        const int relegated=top.back(),promoted=bottom.front();
        clubs_[relegated].league=system+6;
        clubs_[promoted].league=system;
        clubs_[promoted].prestige=clamp(clubs_[promoted].prestige+4);
        clubs_[relegated].prestige=clamp(clubs_[relegated].prestige-3);
        clubs_[promoted].budget+=2000000;
        clubs_[relegated].budget-=1000000;
        movements_.push_back({oldSeason,system,promoted,relegated});
        addMessage("Повышение и вылет",
            leagues_[system].country+": "+clubs_[promoted].name+
            " выходит в "+leagues_[system].name+
            ", "+clubs_[relegated].name+" вылетает в "+
            leagues_[system+6].name+".");
        if(teamId_==relegated||teamId_==promoted)
            leagues_[clubs_[teamId_].league].active=true;
    }
}
std::string Game::divisionsText()const{
    std::ostringstream out;
    out<<"НАЦИОНАЛЬНЫЕ СИСТЕМЫ ЛИГ\n";
    out<<"6 стран • два уровня • 6 клубов в дивизионе\n";
    out<<"Последняя команда высшей лиги вылетает; чемпион второго уровня поднимается.\n\n";
    for(int sys=0;sys<6&&sys+6<int(leagues_.size());++sys){
        out<<leagues_[sys].country<<": "<<leagues_[sys].name
           <<" / "<<leagues_[sys+6].name<<"\n";
        if(!movements_.empty()){
            for(auto it=movements_.rbegin();it!=movements_.rend();++it){
                if(it->systemId!=sys)continue;
                out<<"  "<<it->season<<" ↑ "<<clubs_[it->promotedClub].name
                   <<"  ↓ "<<clubs_[it->relegatedClub].name<<"\n";
                break;
            }
        }
    }
    return out.str();
}
void Game::seedCoaches(){
    coaches_.clear();
    for(const auto& c:clubs_){
        CoachRecord coach;
        coach.clubId=c.id;
        coach.ability=clamp(c.prestige+random(-15,12),25,95);
        coach.jobSecurity=clamp(65+random(-10,15),25,99);
        coach.hiredDay=totalDays_;
        coach.contractYear=season_+random(1,4);
        coach.name=c.id==teamId_?manager_:
            firstNames[random(0,int(firstNames.size()-1))]+" "+
            lastNames[random(0,int(lastNames.size()-1))];
        if(c.id==teamId_)coach.ability=reputation_;
        coaches_.push_back(std::move(coach));
    }
    job_={};
}
void Game::processCoachMarket(){
    for(auto& coach:coaches_){
        if(coach.clubId==teamId_)continue;
        const Club& club=clubs_[coach.clubId];
        if(coach.vacant){
            if(totalDays_-coach.hiredDay>=12){
                coach.vacant=false;
                coach.name=firstNames[random(0,int(firstNames.size()-1))]+" "+
                           lastNames[random(0,int(lastNames.size()-1))];
                coach.ability=clamp(club.prestige+random(-18,15),20,96);
                coach.contractYear=season_+random(1,3);
                coach.jobSecurity=65;
                coach.hiredDay=totalDays_;
                if(random(1,4)==1)addMessage("Новый тренер",club.name+" нанял "+coach.name+".");
            }
            continue;
        }
        const int matches=club.wins+club.draws+club.losses;
        if(matches>=10&&totalDays_%28==0){
            const int performance=(club.points*100)/(std::max(1,matches)*3);
            const int expected=clamp(club.prestige/2,20,55);
            coach.jobSecurity=clamp(coach.jobSecurity+(performance>=expected?5:-12),0,99);
            if(coach.jobSecurity<24){
                coach.vacant=true;coach.hiredDay=totalDays_;coach.name="";
                addMessage("Тренер уволен",club.name+" отправил главного тренера в отставку.");
            }
        }
    }
}
std::string Game::jobMarketText(int clubId)const{
    std::ostringstream out;
    out<<"КАРЬЕРА ТРЕНЕРА • "<<manager_<<"\n";
    out<<"Репутация: "<<reputation_<<"/100 | Текущий клуб: "<<myClub().name<<"\n";
    out<<"Вакансии отмечены словом «СВОБОДНО». Можно откликаться и на занятые места.\n";
    out<<"Ответ руководства приходит через 2 игровых дня, предложение действует до 10-го дня.\n\n";
    if(job_.stage>=0){
        const std::string club=(job_.clubId>=0&&job_.clubId<int(clubs_.size()))?
                               clubs_[job_.clubId].name:"неизвестный клуб";
        static const std::array<const char*,5> statuses={
            "Рассматривается","Контракт предложен","Отказ",
            "Принято","Закрыто"
        };
        out<<"ПОСЛЕДНЯЯ ЗАЯВКА\n"<<club<<" • "
           <<(job_.stage<=4?statuses[job_.stage]:"неизвестно")<<"\n";
        if(job_.stage==0)out<<"Осталось "<<std::max(0,job_.responseDay-totalDays_)<<" дн.\n";
        if(job_.stage==1)out<<"Зарплата: "<<money(job_.proposedSalary)
                             <<"/нед. • предложение до дня "<<job_.expiresDay<<"\n";
        out<<"\n";
    }
    if(clubId>=0&&clubId<int(clubs_.size())){
        const Club& c=clubs_[clubId];
        const CoachRecord& coach=coaches_[clubId];
        out<<"КЛУБ: "<<c.name<<"\n";
        out<<"Страна: "<<leagues_[c.league].country<<"\n";
        out<<"Дивизион: "<<leagues_[c.league].name<<"\n";
        out<<"Престиж: "<<c.prestige<<" | бюджет: "<<money(c.budget)<<"\n";
        out<<"Тренер: "<<(clubId==teamId_?"ВЫ":coach.vacant?"СВОБОДНО":coach.name)<<"\n";
        out<<"Стабильность штаба: "<<coach.jobSecurity<<"/100\n";
        out<<"Выберите клуб слева и нажмите «Подать заявку».\n";
    }else{
        int vacant=0;
        for(const auto& c:coaches_)if(c.vacant)++vacant;
        out<<"Свободных мест: "<<vacant<<" из "<<coaches_.size()<<"\n";
        out<<"Выберите клуб в списке для просмотра условий.\n";
    }
    if(role_!=Role::Coach&&role_!=Role::Sandbox)
        out<<"\nДля смены работы используйте карьеру главного тренера или песочницу.\n";
    return out.str();
}
std::string Game::applyForJob(int clubId){
    if(role_!=Role::Coach&&role_!=Role::Sandbox)
        return status_="Смена клуба доступна в роли главного тренера или в песочнице.";
    if(hasLiveMatch())return status_="Сначала завершите матч.";
    if(clubId<0||clubId>=int(clubs_.size())||clubId==teamId_)
        return status_="Выберите другой клуб из списка.";
    if(job_.stage==0||job_.stage==1)
        return status_="Сначала получите ответ или закройте предыдущую заявку.";
    job_.clubId=clubId;
    job_.submittedDay=totalDays_;
    job_.responseDay=totalDays_+2;
    job_.expiresDay=totalDays_+10;
    job_.stage=0;
    job_.proposedSalary=5000+clubs_[clubId].prestige*900;
    return status_="Заявка в "+clubs_[clubId].name+
                   " отправлена. Решение руководства через 2 игровых дня.";
}
void Game::processJobApplication(){
    if(job_.stage==0&&totalDays_>=job_.responseDay){
        const int cid=job_.clubId;
        if(cid<0||cid>=int(clubs_.size())){job_.stage=4;return;}
        const CoachRecord& coach=coaches_[cid];
        const int required=clubs_[cid].prestige-(coach.vacant?20:0);
        const bool success=difficulty_==Difficulty::Free||
                           reputation_+random(-8,14)>=required;
        job_.stage=success?1:2;
        if(success){
            addMessage("Предложение о работе",clubs_[cid].name+
                       " готов пригласить вас. Зарплата "+money(job_.proposedSalary)+
                       "/нед. Согласие требуется до "+std::to_string(job_.expiresDay)+
                       "-го игрового дня.");
        }else{
            addMessage("Отказ по заявке",clubs_[cid].name+
                       " решил не менять тренера либо выбрал более опытного кандидата.");
        }
    }
    if(job_.stage==1&&totalDays_>job_.expiresDay){
        addMessage("Истекло предложение",clubs_[job_.clubId].name+
                   " закрыл предложение после истечения срока.");
        job_.stage=4;
    }
}
std::string Game::acceptJobOffer(){
    if(hasLiveMatch())return status_="Сначала завершите текущий матч.";
    if(job_.stage!=1||job_.clubId<0||job_.clubId>=int(clubs_.size()))
        return status_="Нет действующего предложения о работе.";
    if(totalDays_>job_.expiresDay){job_.stage=4;return status_="Предложение уже истекло.";}
    const int previous=teamId_,next=job_.clubId;
    coaches_[previous].vacant=true;coaches_[previous].name="";
    coaches_[previous].hiredDay=totalDays_;coaches_[previous].jobSecurity=20;
    teamId_=next;
    coaches_[next].vacant=false;
    coaches_[next].name=manager_;coaches_[next].ability=reputation_;
    coaches_[next].hiredDay=totalDays_;
    coaches_[next].jobSecurity=70;
    leagues_[myClub().league].active=true;
    // Career continues at the same date, with the new club's fixtures and squad.
    job_.stage=3;
    lastYouthSearchDay_=totalDays_-30;
    addMessage("Новая работа","Вы возглавили "+myClub().name+
               ". Карьера, кубки и результаты других команд продолжаются.");
    return status_="Новая работа: "+myClub().name+".";
}
std::string Game::withdrawApplication(){
    if(job_.stage!=0&&job_.stage!=1)
        return status_="Нет активной заявки для отзыва.";
    job_.stage=4;
    return status_="Заявка на тренерскую работу отозвана.";
}
void Game::newCareer(std::string manager,int clubId,Role role,Difficulty difficulty,const std::vector<int>& activeLeagues){
    rng_.seed(std::random_device{}());season_=2026;day_=0;totalDays_=0;manager_=std::move(manager);role_=role;difficulty_=difficulty;
    seedWorld();
    if(clubId<0||clubId>=int(clubs_.size()))clubId=0;
    teamId_=clubId;
    for(auto& l:leagues_)l.active=std::find(activeLeagues.begin(),activeLeagues.end(),l.id)!=activeLeagues.end();
    leagues_[clubs_[teamId_].league].active=true;
    createSquads();
    seedStaff();
    seedCoaches();
    generateYouth(teamId_,4+myClub().academy/3);
    autoAssignCaptains();for(const auto& c:clubs_)pickLineup(c.id);createFixtures();createCups();
    addMessage("Добро пожаловать в клуб","Сезон 2026/27 начался. Режим карьеры: собственная футбольная вселенная. База клубов демонстрационная, составы процедурные. Активные лиги симулируются подробно.");
    addMessage("Первые задачи","Изучите состав, настройте тактику, поговорите с футболистами и подготовьтесь к первому туру. Следующий день — на главной странице.");
    status_="Новая карьера создана";
}
const Club& Game::myClub()const{return clubs_.at(teamId_);} Club& Game::myClub(){return clubs_.at(teamId_);}
const Player* Game::findPlayer(int id)const{for(const auto& p:players_)if(p.id==id)return &p;return nullptr;}
const Club* Game::findClub(int id)const{return id>=0&&id<int(clubs_.size())?&clubs_[id]:nullptr;}
std::vector<int> Game::ownPlayers() const {std::vector<int> r;for(const auto& p:players_)if(p.club==teamId_)r.push_back(p.id);return r;}
const ScoutReport* Game::scoutReport(int playerId)const{
    for(const auto& r:scouts_)if(r.playerId==playerId)return &r;
    return nullptr;
}
std::string Game::scoutText(int playerId)const{
    const Player* p=findPlayer(playerId);
    if(!p)return "Футболист не найден.";
    const ScoutReport* r=scoutReport(playerId);
    if(!r)return "Скаут не назначен. Нажмите «Скаутировать».";
    std::ostringstream o;
    o<<"СКАУТСКИЙ ОТЧЁТ: "<<p->name<<"\n";
    o<<"Короткий список: "<<(r->shortlisted?"ДА":"нет")<<"\n";
    if(r->days<5)o<<"Наблюдение: "<<r->days<<"/5 дней. Ожидается доклад специалиста.\n";
    else o<<"Наблюдение завершено.\nОценка скаута: "<<r->ability
          <<"/100\nОценка потенциала: "<<r->potential<<"/100\n";
    o<<"Стоимость: "<<money(p->value)<<"\n";
    return o.str();
}
std::string Game::scoutPlayer(int playerId){
    if(!permissionSports())return status_="Нет полномочий отправлять скаутов.";
    if(hasLiveMatch())return status_="Сначала завершите матч.";
    const Player* p=findPlayer(playerId);
    if(!p||p->club==teamId_)return status_="Выберите футболиста другого клуба.";
    for(auto& r:scouts_)if(r.playerId==playerId){
        if(r.days>=5)return status_="Отчёт по "+p->name+" уже готов.";
        return status_="Скаут уже наблюдает за "+p->name+".";
    }
    scouts_.push_back({playerId,0,0,0,false});
    return status_="Скаут отправлен изучать "+p->name+". Отчёт появится через 5 игровых дней.";
}
std::string Game::toggleShortlist(int playerId){
    if(!permissionSports())return status_="Нет полномочий на работу с трансферным списком.";
    const Player* p=findPlayer(playerId);
    if(!p||p->club==teamId_)return status_="Выберите футболиста другого клуба.";
    for(auto& r:scouts_)if(r.playerId==playerId){
        r.shortlisted=!r.shortlisted;
        return status_=p->name+(r.shortlisted?" добавлен в короткий список.":" удалён из короткого списка.");
    }
    scouts_.push_back({playerId,0,0,0,true});
    return status_=p->name+" добавлен в короткий список (наблюдение началось).";
}
std::vector<int> Game::transferCandidates(bool shortlistOnly) const {
    std::vector<int> r;
    for(const auto& p:players_)if(p.club!=teamId_&&leagues_[clubs_[p.club].league].active&&(!shortlistOnly||(scoutReport(p.id)&&scoutReport(p.id)->shortlisted)))r.push_back(p.id);
    std::sort(r.begin(),r.end(),[this](int a,int b){
        bool sa=scoutReport(a)&&scoutReport(a)->shortlisted;
        bool sb=scoutReport(b)&&scoutReport(b)->shortlisted;
        if(sa!=sb)return sa;
        return findPlayer(a)->value>findPlayer(b)->value;
    });
    if(r.size()>100)r.resize(100);
    return r;
}

void Game::addCupMatch(int competition,int round,int day,int home,int away){
    if(home<0||away<0||home==away)throw std::runtime_error("Invalid cup pairing");
    Match m;
    m.id=int(matches_.size());
    m.season=season_;
    m.day=day;
    m.home=home;
    m.away=away;
    m.competition=competition;
    m.round=round;
    matches_.push_back(std::move(m));
}
void Game::createCups(const std::vector<int>& continentalEntrants){
    cups_.clear();
    // Six domestic six-club cups: four clubs contest the preliminary round,
    // while the two seeds enter directly at the semifinal stage.
    for(const auto& league:leagues_){
        std::vector<int> entries=leagueClubs(league.id);
        std::sort(entries.begin(),entries.end(),[this](int a,int b){
            return std::tie(clubs_[a].prestige,clubs_[a].budget)>
                   std::tie(clubs_[b].prestige,clubs_[b].budget);
        });
        if(entries.size()!=6)continue;
        CupCompetition cup;
        cup.id=int(cups_.size());cup.season=season_;cup.leagueId=league.id;
        cup.name="Кубок: "+league.country;
        cup.byeA=entries[0];cup.byeB=entries[1];
        cups_.push_back(cup);
        addCupMatch(cup.id,0,21,entries[2],entries[5]);
        addCupMatch(cup.id,0,21,entries[3],entries[4]);
    }

    std::vector<int> qualified=continentalEntrants;
    if(qualified.empty()){
        for(const auto& league:leagues_){
            std::vector<int> entries=leagueClubs(league.id);
            std::sort(entries.begin(),entries.end(),[this](int a,int b){
                return clubs_[a].prestige>clubs_[b].prestige;
            });
            if(!entries.empty())qualified.push_back(entries[0]);
        }
        std::vector<int> wildcards;
        for(const auto& club:clubs_)
            if(std::find(qualified.begin(),qualified.end(),club.id)==qualified.end())
                wildcards.push_back(club.id);
        std::sort(wildcards.begin(),wildcards.end(),[this](int a,int b){
            return clubs_[a].prestige>clubs_[b].prestige;
        });
        for(int id:wildcards)if(qualified.size()<8)qualified.push_back(id);
    }
    // Deduplicate qualifiers and fill two additional places if necessary.
    std::vector<int> unique;
    for(int id:qualified)
        if(id>=0&&id<int(clubs_.size())&&
           std::find(unique.begin(),unique.end(),id)==unique.end())
            unique.push_back(id);
    for(const auto& club:clubs_)if(unique.size()<8&&
        std::find(unique.begin(),unique.end(),club.id)==unique.end())unique.push_back(club.id);
    if(unique.size()>8)unique.resize(8);
    if(unique.size()==8){
        CupCompetition cup;
        cup.id=int(cups_.size());cup.season=season_;cup.leagueId=-1;
        cup.name="Международный кубок клубов";
        cups_.push_back(cup);
        // Top qualifiers and remaining participants are paired in reverse order.
        for(int i=0;i<4;++i)addCupMatch(cup.id,0,51,unique[i],unique[7-i]);
    }
}
int Game::knockoutWinner(const Match& m)const{
    if(!m.played||m.competition<0)return -1;
    if(m.homeGoals>m.awayGoals)return m.home;
    if(m.homeGoals<m.awayGoals)return m.away;
    if(m.penaltiesH>m.penaltiesA&&m.penaltiesH>=0)return m.home;
    if(m.penaltiesA>m.penaltiesH&&m.penaltiesA>=0)return m.away;
    return -1;
}
std::string Game::matchLabel(const Match& m)const{
    if(m.competition<0)return "Чемпионат: "+leagues_[clubs_[m.home].league].name;
    if(m.competition>=int(cups_.size()))return "Кубковый матч";
    const char* stage=m.round==0?(cups_[m.competition].leagueId<0?"1/4 финала":"Предварительный раунд"):
                      m.round==1?"1/2 финала":"Финал";
    return cups_[m.competition].name+" / "+stage;
}
void Game::progressCups(){
    for(auto& cup:cups_){
        if(cup.champion>=0)continue;
        std::vector<int> winners;
        int gameCount=0;
        bool ready=true;
        int finalLoser=-1;
        for(const auto& match:matches_)if(match.competition==cup.id&&match.round==cup.round){
            ++gameCount;
            if(!match.played){ready=false;break;}
            int winner=knockoutWinner(match);
            if(winner<0){ready=false;break;}
            winners.push_back(winner);
            if(cup.round==2)finalLoser=winner==match.home?match.away:match.home;
        }
        if(!ready||!gameCount)continue;
        if(cup.round==0){
            if(cup.leagueId>=0){
                if(winners.size()!=2)continue;
                const int winner1=winners[0],winner2=winners[1];
                cup.round=1;
                addCupMatch(cup.id,1,111,cup.byeA,winner2);
                addCupMatch(cup.id,1,111,cup.byeB,winner1);
            }else{
                if(winners.size()!=4)continue;
                cup.round=1;
                addCupMatch(cup.id,1,151,winners[0],winners[3]);
                addCupMatch(cup.id,1,151,winners[1],winners[2]);
            }
        }else if(cup.round==1){
            if(winners.size()!=2)continue;
            cup.round=2;
            addCupMatch(cup.id,2,cup.leagueId>=0?201:261,winners[0],winners[1]);
        }else if(cup.round==2&&winners.size()==1){
            cup.champion=winners[0];
            trophies_.push_back({season_,cup.id,winners[0],finalLoser});
            const int prize=cup.leagueId<0?2500000:650000;
            clubs_[winners[0]].budget+=prize;
            addMessage("Победитель турнира",clubs_[winners[0]].name+" выиграл "+cup.name+
                       " ("+std::to_string(season_)+"/"+std::to_string((season_+1)%100)+").");
        }
    }
}
void Game::archiveLeagueChampions(){
    for(const auto& league:leagues_){
        auto teams=leagueClubs(league.id);
        if(teams.size()<2)continue;
        std::sort(teams.begin(),teams.end(),[this](int a,int b){
            const Club& first=clubs_[a];const Club& second=clubs_[b];
            return std::tuple(first.points,first.gf-first.ga,first.gf)>
                   std::tuple(second.points,second.gf-second.ga,second.gf);
        });
        leagueHistory_.push_back({season_,league.id,teams[0],teams[1],clubs_[teams[0]].points});
    }
}
std::string Game::cupsText()const{
    std::ostringstream out;
    out<<"КУБКОВЫЕ СОРЕВНОВАНИЯ • "<<season_<<"/"<<(season_+1)%100<<"\n\n";
    for(const auto& cup:cups_){
        out<<cup.name;
        if(cup.champion>=0)out<<" • ПОБЕДИТЕЛЬ: "<<clubs_[cup.champion].name;
        out<<"\n";
        if(cup.leagueId>=0)
            out<<"Сетка на 6 команд: 2 предварительных игры, 2 полуфинала, финал.\n";
        else out<<"Международный турнир: 8 приглашённых клубов, 1/4, 1/2 и финал.\n";
        std::vector<const Match*> fixtures;
        for(const auto& m:matches_)if(m.competition==cup.id)fixtures.push_back(&m);
        std::sort(fixtures.begin(),fixtures.end(),[](auto a,auto b){
            return std::tie(a->round,a->day,a->id)<std::tie(b->round,b->day,b->id);
        });
        for(const auto* m:fixtures){
            out<<"  День "<<m->day<<" | "
               <<(m->round==0?(cup.leagueId<0?"1/4":"Отбор"):m->round==1?"1/2":"Финал")
               <<" | "<<clubs_[m->home].name<<" — "<<clubs_[m->away].name;
            if(m->played){
                out<<" "<<m->homeGoals<<":"<<m->awayGoals;
                if(m->penaltiesH>=0)out<<" (пен. "<<m->penaltiesH<<":"<<m->penaltiesA<<")";
            }else out<<" [ОЖИДАНИЕ]";
            out<<"\n";
        }
        if(cup.leagueId>=0&&cup.round==0)
            out<<"  Прямой выход в полуфинал: "<<clubs_[cup.byeA].name
               <<" и "<<clubs_[cup.byeB].name<<"\n";
        out<<"\n";
    }
    if(cups_.empty())out<<"Турниры появятся в начале следующего сезона.\n";
    return out.str();
}
std::string Game::calendarText()const{
    std::ostringstream out;
    out<<"КАЛЕНДАРЬ КЛУБА • "<<clubs_[teamId_].name<<"\n\n";
    std::vector<const Match*> fixtures;
    for(const auto& match:matches_)
        if(match.home==teamId_||match.away==teamId_)fixtures.push_back(&match);
    std::sort(fixtures.begin(),fixtures.end(),[](const auto*a,const auto*b){
        return std::tie(a->day,a->id)<std::tie(b->day,b->id);
    });
    int upcoming=0,recent=0;
    out<<"СЛЕДУЮЩИЕ МАТЧИ\n";
    for(const auto* m:fixtures)if(!m->played&&upcoming++<18){
        out<<"День "<<m->day<<" • "<<matchLabel(*m)<<"\n  "
           <<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n";
    }
    if(!upcoming)out<<"Нет предстоящих игр.\n";
    out<<"\nПОСЛЕДНИЕ РЕЗУЛЬТАТЫ\n";
    for(auto it=fixtures.rbegin();it!=fixtures.rend()&&recent<10;++it){
        const auto* m=*it;
        if(!m->played)continue;
        ++recent;
        out<<"День "<<m->day<<" • "<<matchLabel(*m)<<"\n  "
           <<clubs_[m->home].name<<" "<<m->homeGoals<<":"<<m->awayGoals<<" "
           <<clubs_[m->away].name;
        if(m->penaltiesH>=0)out<<" [пен. "<<m->penaltiesH<<":"<<m->penaltiesA<<"]";
        out<<"\n";
    }
    return out.str();
}
std::string Game::seasonHistoryText()const{
    std::ostringstream out;
    out<<"ИСТОРИЯ ПОБЕДИТЕЛЕЙ\n\n";
    if(!leagueHistory_.empty()){
        out<<"ЧЕМПИОНЫ НАЦИОНАЛЬНЫХ ЛИГ\n";
        for(auto it=leagueHistory_.rbegin();it!=leagueHistory_.rend();++it)
            out<<it->season<<"/"<<(it->season+1)%100<<" • "
               <<leagues_[it->leagueId].country<<": "<<clubs_[it->winner].name
               <<" ("<<it->points<<" очков)\n";
    }
    if(!trophies_.empty()){
        out<<"\nКУБКОВЫЕ ПОБЕДИТЕЛИ\n";
        for(auto it=trophies_.rbegin();it!=trophies_.rend();++it){
            out<<it->season<<"/"<<(it->season+1)%100<<" • "
               <<(it->cupId==6?"Международный кубок":("Кубок: "+leagues_[it->cupId].country))
               <<": "<<clubs_[it->winner].name<<"\n";
        }
    }
    if(leagueHistory_.empty()&&trophies_.empty())
        out<<"История будет пополняться после финалов и окончания сезонов.\n";
    return out.str();
}
std::string Game::scorersText()const{
    std::vector<const Player*> players;
    for(const auto& player:players_)players.push_back(&player);
    std::sort(players.begin(),players.end(),[](const Player*a,const Player*b){
        return std::tie(a->goals,a->appearances)>std::tie(b->goals,b->appearances);
    });
    std::ostringstream out;
    out<<"БОМБАРДИРЫ СЕЗОНА "<<season_<<"/"<<(season_+1)%100<<"\n";
    out<<"Учитываются голы во всех соревнованиях текущего сезона.\n\n";
    for(size_t i=0;i<std::min<size_t>(20,players.size());++i){
        const Player& p=*players[i];
        out<<i+1<<". "<<p.name<<" | "<<clubs_[p.club].name
           <<" | "<<p.goals<<" голов / "<<p.appearances<<" матчей\n";
    }
    return out.str();
}
std::string Game::dateString()const{
    using namespace std::chrono;
    auto date=sys_days(year{season_}/7/1)+days{day_};
    year_month_day d{date};
    std::ostringstream s;s<<int(d.year())<<'-'<<std::setfill('0')<<std::setw(2)<<unsigned(d.month())<<'-'<<std::setw(2)<<unsigned(d.day());
    return s.str();
}
const Match* Game::upcomingMatch()const{
    const Match* earliest=nullptr;
    for(const auto& m:matches_)if(!m.played&&(m.home==teamId_||m.away==teamId_)){
        if(!earliest||std::tie(m.day,m.id)<std::tie(earliest->day,earliest->id))earliest=&m;
    }
    return earliest;
}
bool Game::matchDue()const{auto m=upcomingMatch();return m&&m->day<=day_;}
int Game::rating(const Club& club)const{
    return lineupStrength(starters(club.id))+club.facilities;
}
std::string Game::standings(int leagueId)const{
    std::vector<const Club*> table;
    for(const auto& c:clubs_)if(c.league==leagueId)table.push_back(&c);
    std::sort(table.begin(),table.end(),[](const Club*a,const Club*b){
        return std::tuple(a->points,a->gf-a->ga,a->gf)>std::tuple(b->points,b->gf-b->ga,b->gf);
    });
    if(leagueId<0||leagueId>=int(leagues_.size()))return "Нет лиги";
    std::ostringstream o;
    o<<leagues_[leagueId].country<<" • "<<leagues_[leagueId].name<<(leagues_[leagueId].active?" [ПОДРОБНО]":" [ФОНОВАЯ]")<<"\n\n";
    o<<"  #  Клуб                           И  В  Н  П   Мячи    Очки\n";
    for(size_t i=0;i<table.size();++i){const Club& c=*table[i];
        o<<std::setw(3)<<i+1<<"  "<<pad(c.name,29)<<" "<<std::setw(2)<<c.wins+c.draws+c.losses
        <<" "<<std::setw(2)<<c.wins<<" "<<std::setw(2)<<c.draws<<" "<<std::setw(2)<<c.losses<<"  "
        <<c.gf<<":"<<c.ga<<"    "<<c.points<<(c.id==teamId_?"   < ВАШ КЛУБ":"")<<"\n";
    }
    return o.str();
}
std::string Game::squadText()const{
    std::ostringstream o;o<<"СОСТАВ: "<<myClub().name<<"\n\n";
    o<<"Игроки и индивидуальные характеристики доступны при выборе в списке.\n\n";
    o<<"Шкала навыков: 0–100. Состояние, мораль, контракты и\nотношения изменяются в ходе карьеры.\n\n";
    o<<"Количество футболистов: "<<ownPlayers().size()<<"\n";
    o<<"Основа: "<<starters(teamId_).size()<<"/11\n";
    if(const Player* p=findPlayer(myClub().captainId))o<<"Капитан: "<<p->name<<"\n";
    if(const Player* p=findPlayer(myClub().viceCaptainId))o<<"Вице-капитан: "<<p->name<<"\n";
    int injured=0,unhappy=0,loanees=0;
    for(const auto& p:players_)if(p.club==teamId_){
        injured+=p.injured>0;unhappy+=p.morale<45;loanees+=p.parentClub>=0;
    }
    o<<"Травмировано: "<<injured<<"\nНедовольны: "<<unhappy
     <<"\nАрендованные: "<<loanees<<"\n";
    o<<"Штаб: тренер "<<staffQuality(teamId_,0)<<"/100, врач "<<staffQuality(teamId_,1)<<"/100\n";
    return o.str();
}
std::string Game::homeText()const{
    std::ostringstream o;
    o<<"ФУТБОЛЬНАЯ ВСЕЛЕННАЯ  /  "<<dateString()<<"\n\n";
    o<<"КЛУБ: "<<myClub().name<<"\n";
    o<<"Менеджер: "<<manager_<<"\n";
    o<<"Роль: "<<(role_==Role::Coach?"Главный тренер":role_==Role::Director?"Спортивный директор":role_==Role::Owner?"Владелец":role_==Role::President?"Президент":"Песочница")<<"\n";
    o<<"Сложность: "<<(difficulty_==Difficulty::Easy?"Лёгкая":difficulty_==Difficulty::Realistic?"Реализм":difficulty_==Difficulty::Hard?"Сложная":"Свободная песочница")<<"\n\n";
    o<<"Бюджет: "<<money(myClub().budget)<<"\n";
    int liveDeals=0;for(const auto& d:deals_)if(int(d.stage)<=4)++liveDeals;
    o<<"Активные трансферные переговоры: "<<liveDeals<<"\n";
    o<<"Доверие фанатов: "<<pct(myClub().fans)<<"\n";
    o<<"Доверие руководства: "<<pct(myClub().board)<<"\n";
    o<<"Подготовка команды: "<<tactics[myClub().tactic]<<" / "<<formations[myClub().formation]<<"\n\n";
    if(auto m=upcomingMatch()){
        o<<"СЛЕДУЮЩИЙ МАТЧ\n";
        o<<matchLabel(*m)<<"\n";
        o<<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n";
        o<<"Через дней: "<<std::max(0,m->day-day_)<<"\n\n";
    }
    o<<"НОВОСТИ И СООБЩЕНИЯ\n";
    for(int i=int(inbox_.size())-1,n=0;i>=0&&n<4;--i,++n)o<<" • "<<inbox_[i].title<<"\n";
    return o.str();
}
std::string Game::tacticsText()const{
    const auto& c=myClub();
    std::ostringstream o;
    o<<"ТАКТИКА КОМАНДЫ\n\n";
    o<<"Схема: "<<formations[c.formation]<<"\n";
    o<<"Стиль: "<<tactics[c.tactic]<<"\n";
    o<<"Тренировки: "<<trainings[c.training]<<"\n\n";
    o<<"Сила команды: "<<rating(c)<<" (с учётом доступных игроков)\n";
    o<<"Тренер: "<<staffQuality(teamId_,0)<<"/100; врач: "<<staffQuality(teamId_,1)<<"/100\n";
    if(const Player* cap=findPlayer(c.captainId))o<<"Капитан: "<<cap->name<<"\n";
    o<<"Собрание команды: раз в 7 дней во вкладке «Разговоры»\n\n";
    o<<"Тактика влияет на вероятность атаки, голевые моменты,\nусталость и риск пропустить гол.\n";
    o<<"Тренировки влияют на форму, мораль и развитие навыков.\n";
    o<<"Выберите индивидуальный фокус во вкладке «Тренировки».\n";
    return o.str();
}
std::string Game::financesText()const{
    const auto& c=myClub();
    std::ostringstream o;o<<"ФИНАНСЫ И ИНФРАСТРУКТУРА\n\n";
    o<<"Клубный бюджет: "<<money(c.budget)<<"\n";
    o<<"Спонсорские поступления за домашний матч: "<<money(c.sponsorship)<<"\n";
    o<<"Цена билета: "<<c.ticket<<" €\n";
    o<<"Вместимость стадиона: "<<c.stadium<<"\n";
    o<<"Тренировочная база: "<<c.facilities<<"/10\n";
    o<<"Академия: "<<c.academy<<"/10\n";
    int staffCount=0,staffWages=0;
    for(const auto& p:staff_)if(p.club==teamId_){
        ++staffCount;staffWages+=p.weeklyWage*4;
    }
    o<<"Штаб: "<<staffCount<<" чел., выплаты за 4 недели "<<money(staffWages)<<"\n";
    o<<"Доходы зависят от посещаемости, результатов и\nотношений с болельщиками.\n\n";
    o<<"Улучшить стадион: 3 млн €\nУлучшить базу: 2 млн €\nУлучшить академию: 2.5 млн €\n";
    o<<"\nОТЛОЖЕННЫЕ ТРАНСФЕРНЫЕ ПЛАТЕЖИ\n";
    int count=0;
    for(const auto& p:payments_){
        if(p.buyerClub!=teamId_&&p.sellerClub!=teamId_)continue;
        if(++count>18){o<<"... и другие обязательства\n";break;}
        o<<(p.buyerClub==teamId_?"Расход":"Доход")<<" "<<money(p.amount)
         <<" | игрок #"<<p.playerId<<" | ";
        if(p.triggerAppearances>0){
            const Player* player=findPlayer(p.playerId);
            o<<"после "<<std::max(0,p.triggerAppearances-(player?player->careerAppearances:0))
             <<" матчей\n";
        }else o<<"через "<<std::max(0,p.dueDay-totalDays_)<<" дн.\n";
    }
    if(!count)o<<"Активных обязательств нет.\n";
    return o.str();
}
void Game::addMessage(std::string title,std::string body){
    if(inbox_.size()>1000)inbox_.erase(inbox_.begin(),inbox_.begin()+100);
    inbox_.push_back({totalDays_,std::move(title),std::move(body)});
}
void Game::post(std::string title,std::string body){addMessage(std::move(title),std::move(body));}
std::string Game::newsText()const{
    std::ostringstream o;o<<"ПОЧТА / НОВОСТИ\n\n";
    for(int i=int(inbox_.size())-1,n=0;i>=0&&n<35;--i,++n){
        o<<"[День "<<inbox_[i].day<<"] "<<inbox_[i].title<<"\n";
        o<<inbox_[i].body<<"\n\n";
    }
    return o.str();
}
std::string Game::helpText()const{
    return "FOOTBALL UNIVERSE 0.8 / ПРОТОТИП\n\n"
    "Это самостоятельная C++-игра с текстовым и 2D-отображением матча.\n"
    "Календарь и кубки: вкладка «Архив» показывает матчи, сетку кубков и историю чемпионов.\n"
    "В шести странах: национальные кубки из 6 клубов, один международный кубок из 8.\n"
    "Кубковые ничьи решаются пенальти, кубковые матчи не прибавляют очков в таблицу.\n"
    "Результаты финалов и победители чемпионатов сохраняются между сезонами.\n"
    "Создайте карьеру, выберите должность, сложность, клуб и активные лиги.\n"
    "Меню слева открывает все игровые системы.\n\n"
    "Состав: выберите запасного, впишите ID игрока основы, нажмите «В основу».\n"
    "Развитие: очки опыта накапливаются за индивидуальные занятия, тренера и наставничество.\n"
    "Академия: наймите штаб, изучайте молодых талантов и подписывайте выбранных юниоров.\n"
    "Наставник: молодой игрок до 23 лет, наставник от 25. Роль врача ускоряет восстановление.\n"
    "Отношения: проводите встречи игроков, примиряйте конфликтующие пары.\n"
    "Капитан и вице-капитан: выделите своего игрока и назначьте во вкладке «Состав».\n"
    "Разговоры: проводите командные собрания с моральными последствиями раз в 7 дней.\n"
    "Трансферы: наблюдение скаута, короткий список и многоэтапные переговоры.\n"
    "Введите цену (евро) и нажмите «Предложить». Клуб ответит на следующий день.\n"
    "При согласии клуба отправьте агенту зарплату и срок: 55000 4.\n"
    "Можно принять встречные условия или отменить переговоры.\n"
    "Письма о переговорах приходят в почту; ответы занимают игровые дни.\n"
    "Усложнённый оффер: deal 15000000 3 1000000 20 (цена, платежи, бонус, %).\n"
    "Обмен: swap 5000000 123 (денежная доплата и ID вашего футболиста).\n"
    "Аренда: loan 100000 50 180 (плата, доля зарплаты %, срок в днях).\n"
    "Контракты: продать игрока клубу по ID, выставить отступные (в песочнице — любому).\n"
    "Оплата рассрочек идёт раз в 30 дней, бонус начисляется после 10 матчей.\n"
    "Тренировки: выберите игрока и переключайте индивидуальный фокус.\n"
    "Матч: в интерактивном режиме выберите запасного, введите ID выходящего игрока,\n"
    "нажмите «Заменить». До 5 замен, повторный выход запрещён.\n\n"
    "Активные лиги: подробные матчи и события.\n"
    "Неактивные: упрощённая фоновая симуляция.\n"
    "В день матча: ИГРАТЬ — для пошагового управления\n"
    "или МГНОВЕННО — для быстрого результата.\n\n"
    "Настоящие исторические составы и сотни лиг пока отсутствуют.\n"
    "Встроено 36 известных названий клубов и процедурные футболисты.\n"
    "Файл players.csv можно использовать для подстановки собственных данных.\n"
    "Система переговоров и диалогов работает по правилам, без облачного ИИ.\n\n"
    "Проект не связан с Sports Interactive или Football Manager.\n";
}
std::string Game::teamTalk(int tone){
    if(!permissionSports())return status_="Нет полномочий проводить командные собрания.";
    if(hasLiveMatch())return status_="Собрание доступно вне матча.";
    if(tone<0||tone>2)return status_="Неизвестный тип собрания.";
    if(totalDays_-lastTeamTalkDay_<7)
        return status_="Команде нужен отдых от собраний. Новая встреча через "+std::to_string(7-(totalDays_-lastTeamTalkDay_))+" дн.";
    lastTeamTalkDay_=totalDays_;
    int positive=0,negative=0;
    const int played=myClub().wins+myClub().draws+myClub().losses;
    for(auto& p:players_)if(p.club==teamId_){
        int change=0;
        if(tone==0)change=3+(p.loyalty>65?2:0);
        else if(tone==1)change=played==0||myClub().wins*2>=played?5:0;
        else change=p.personality=="Профессионал"||p.personality=="Трудолюбивый"?2:-6;
        if(p.id==myClub().captainId)change+=2;
        p.morale=clamp(p.morale+change);
        p.trust=clamp(p.trust+(change>0?2:change<0?-3:0));
        if(change>0)++positive;
        else if(change<0)++negative;
    }
    for(auto& relation:relations_)if(const Player* p=findPlayer(relation.first)){
        if(p->club!=teamId_)continue;
        relation.affinity=clamp(relation.affinity+(tone==2?-2:2),-100,100);
    }
    const std::string kind=tone==0?"Мотивационная встреча":tone==1?"Похвала команде":"Жёсткий разговор";
    status_=kind+": поддержали "+std::to_string(positive)+", недовольны "+std::to_string(negative)+" игроков.";
    addMessage("Собрание в раздевалке",status_);
    return status_;
}
void Game::dailyTick(){
    processLoanReturns();
    pruneRelationships();
    processTransferNegotiations();
    processDeferredPayments();
    for(auto& report:scouts_)if(report.days<5){
        ++report.days;
        if(report.days==5){
            if(const Player* p=findPlayer(report.playerId)){
                const int accuracy=std::max(2,13-staffQuality(teamId_,2)/10);
                report.ability=clamp(playerRating(*p)+random(-accuracy,accuracy));
                report.potential=clamp(p->potential+random(-accuracy-2,accuracy+2));
                addMessage("Скаутский отчёт готов",p->name+": оценка "+std::to_string(report.ability)+
                           "/100, потенциал "+std::to_string(report.potential)+"/100. Откройте «Трансферы».");
            }
        }
    }
    auto& c=myClub();
    for(auto& p:players_){
        if(p.injured>0){
            --p.injured;
            if(p.club==teamId_&&staffQuality(teamId_,1)>=75&&totalDays_%3==0&&p.injured>0)
                --p.injured;
            p.fitness=clamp(p.fitness+2+std::min(3,staffQuality(p.club,1)/30));
            continue;
        }
        if(p.club!=teamId_){p.fitness=clamp(p.fitness+1);continue;}
        int training=c.training;
        if(training==0)p.fitness=clamp(p.fitness+5);
        if(training==1){p.fitness=clamp(p.fitness+2);if(random(0,40)==0)p.skill[random(0,9)]=clamp(p.skill[random(0,9)]+1);}
        if(training==2){p.fitness=clamp(p.fitness-2);if(random(0,16)==0){int k=random(0,8);p.skill[k]=clamp(p.skill[k]+1);}}
        if(training==3){p.fitness=clamp(p.fitness);if(random(0,20)==0)p.skill[4]=clamp(p.skill[4]+1);}
        if(training==4){p.fitness=clamp(p.fitness+1);if(random(0,21)==0)p.skill[8]=clamp(p.skill[8]+1);}
        if(p.trainingFocus>=0&&p.trainingFocus<10){
            const int coach=staffQuality(teamId_,0);
            const bool hasMentor=p.mentorId>=0&&findPlayer(p.mentorId)&&
                                 findPlayer(p.mentorId)->club==p.club;
            int gained=1+myClub().facilities/3+coach/25+
                       (p.morale>=65?1:0)+(p.academyGraduate?1:0)+(hasMentor?2:0);
            if(p.age>30)gained=std::max(1,gained/3);
            else if(p.age>25)gained=std::max(1,gained*2/3);
            if(c.training==0)gained=std::max(1,gained/2);
            else if(c.training==2)gained+=2;
            p.developmentXP=std::min(10000,p.developmentXP+gained);
            const int threshold=140+(p.age>25?(p.age-25)*14:0);
            if(p.developmentXP>=threshold&&p.skill[p.trainingFocus]<p.potential){
                p.skill[p.trainingFocus]=clamp(p.skill[p.trainingFocus]+1);
                p.developmentXP-=threshold;
                ++p.developedSkills;
                if(p.developedSkills%4==0)
                    addMessage("Прогресс тренировки",p.name+" улучшил "+skillNames[p.trainingFocus]+
                               " до "+std::to_string(p.skill[p.trainingFocus])+".");
            }
        }
        const int medical=staffQuality(teamId_,1);
        if(training==2&&p.fitness<45&&random(0,300+medical*5)==0){
            p.injured=std::max(2,random(4,16)-medical/25);
            p.starter=false;
            addMessage("Травма на тренировке",p.name+" выбыл на "+std::to_string(p.injured)+" дней.");
            pickLineup(teamId_);
        }
        if(random(0,160)==0) p.morale=clamp(p.morale+random(-3,3));
        if(p.family<30&&random(0,90)==0){p.morale=clamp(p.morale-3);p.trust=clamp(p.trust-2);}
    }
    // Context-sensitive stories: events result from existing club and player conditions.
    // Their consequences are persistent; the player can react through the relevant tabs.
    if(totalDays_>0&&random(1,27)==1){
        std::vector<Player*> own;
        for(auto& player:players_)if(player.club==teamId_)own.push_back(&player);
        Player* person=own.empty()?nullptr:own[random(0,int(own.size()-1))];
        if(c.budget<1000000&&random(0,2)!=0){
            c.board=clamp(c.board-7);c.fans=clamp(c.fans-3);
            addMessage("Финансовые трудности","В бюджете клуба недостаточно средств. Совет директоров требует новых источников дохода, а фанаты опасаются распродажи состава.");
        }else if(c.fans<45&&random(0,1)==0){
            c.board=clamp(c.board-2);
            addMessage("Фанатский протест","Недовольные результатами и решениями руководства болельщики требуют открытой встречи. Обращение через вкладку «Болельщики» может изменить ситуацию.");
        }else if(person&&person->family<55&&random(0,1)==0){
            person->morale=clamp(person->morale-5);
            addMessage("Трудности адаптации",person->name+" переживает семейные проблемы. Помощь через личный разговор может улучшить его психологическое состояние.");
        }else if(person&&person->morale<46){
            person->trust=clamp(person->trust-4);
            addMessage("Недовольство игровым временем",person->name+" недоволен положением в команде и обсуждает свои перспективы с агентом. Возможны переговоры о будущем.");
        }else if(c.fans>=75&&random(0,2)==0){
            c.budget+=100000;
            addMessage("Высокая поддержка клуба","Благодаря хорошим отношениям с болельщиками местные партнёры перечислили клубу дополнительный бонус 100 тыс. €.");
        }else if(person){
            person->trust=clamp(person->trust+2);
            addMessage("Инициатива раздевалки",person->name+" предложил провести дополнительную встречу команды. Внутри клуба обсуждают цели сезона.");
        }
    }
    if(totalDays_>0&&totalDays_%30==0){
       int wages=0;
       for(const auto& p:players_){
           if(p.club==teamId_)wages+=p.wage*4*(p.parentClub>=0?p.loanWageShare:100)/100;
           else if(p.parentClub==teamId_)wages+=p.wage*4*(100-p.loanWageShare)/100;
       }
       int staffWages=0;
       for(const auto& person:staff_)if(person.club==teamId_)staffWages+=person.weeklyWage*4;
       c.budget-=wages+staffWages;
       addMessage("Финансовый отчёт","Зарплаты игроков: "+money(wages)+
                  ", зарплаты штаба: "+money(staffWages)+
                  ". Остаток бюджета: "+money(c.budget)+".");
       if(c.budget<0){c.board=clamp(c.board-5);c.fans=clamp(c.fans-2);addMessage("Дефицит бюджета","Руководство обеспокоено долгами клуба. Необходимо восстановить финансовую устойчивость.");}
    }
}
void Game::advance(int days){
    if(hasLiveMatch()){status_="Матч идёт. Завершите его перед продолжением.";return;}
    for(int i=0;i<days;++i){
        if(matchDue()){status_="Сегодня матч! Откройте вкладку «Матч» и выберите режим.";return;}
        ++day_;++totalDays_;dailyTick();
        if(day_>=364){newSeason();continue;}
        if(matchDue()){status_="Игровой день: выбирайте интерактивную или мгновенную симуляцию.";return;}
        simulateOtherMatches();
    }
    status_="Дата: "+dateString();
}
void Game::newSeason(){
    const int old=season_;
    archiveLeagueChampions();
    // Qualification: six league winners + two strongest runners-up.
    std::vector<int> continental;
    std::vector<int> runnersUp;
    for(const auto& record:leagueHistory_)if(record.season==old){
        continental.push_back(record.winner);
        runnersUp.push_back(record.runnerUp);
    }
    std::sort(runnersUp.begin(),runnersUp.end(),[this](int a,int b){
        const Club& x=clubs_[a],&y=clubs_[b];
        return std::tuple(x.points,x.gf-x.ga,x.prestige)>
               std::tuple(y.points,y.gf-y.ga,y.prestige);
    });
    for(int club:runnersUp)if(continental.size()<8)continental.push_back(club);
    season_++;day_=0;
    for(auto& c:clubs_){c.wins=c.draws=c.losses=c.gf=c.ga=c.points=0;c.budget+=c.sponsorship*4;}
    for(auto& p:players_){
        p.age++;p.goals=p.appearances=p.yellow=p.red=0;
        if(p.age>33){for(int& skill:p.skill)if(random(0,3)==0)skill=clamp(skill-1);}
        else if(p.age<25){int k=random(0,8);p.skill[k]=clamp(p.skill[k]+random(0,2));}
        p.fitness=clamp(p.fitness+15);p.injured=0;
        if(p.contractYear<season_){p.contractYear=season_+1;p.wage=p.wage*105/100;}
    }
    // Every club receives a few procedural regens; older players may retire.
    for(int cid=0;cid<int(clubs_.size());++cid){
        int exits=0;
        for(auto it=players_.begin();it!=players_.end();){
            if(it->club==cid&&it->age>=37&&random(0,100)<35&&exits<3){it=players_.erase(it);++exits;}
            else ++it;
        }
        for(int n=0;n<std::max(1,exits);++n){
            Player p;p.id=nextPlayerId_++;p.club=cid;p.name=firstNames[random(0,int(firstNames.size()-1))]+" "+lastNames[random(0,int(lastNames.size()-1))];
            p.age=random(16,19);p.position=positions[random(0,int(positions.size()-1))];p.personality=character[random(0,int(character.size()-1))];
            int base=clamp(clubs_[cid].prestige-31+random(-12,12),22,70);
            for(int& s:p.skill)s=clamp(base+random(-15,15));
            p.potential=clamp(base+random(15,36));p.wage=1500+base*100;p.value=60000+base*12000;p.contractYear=season_+4;
            players_.push_back(p);
        }
    }
    for(auto& d:deals_){
        if(int(d.stage)>4)continue;
        if(!findPlayer(d.playerId))d.stage=DealStage::Cancelled;
    }
    processLoanReturns();
    // Conditional bonuses must not refer to players who retired or changed club.
    processDeferredPayments();
    for(auto& d:deals_)if(int(d.stage)<=4){
        const Player* p=findPlayer(d.playerId);
        if(!p||p->club!=d.sellerClub||p->parentClub>=0)d.stage=DealStage::Cancelled;
    }
    pruneRelationships();
    youth_.erase(std::remove_if(youth_.begin(),youth_.end(),
        [this](const YouthProspect& prospect){return prospect.club==teamId_;}),youth_.end());
    generateYouth(teamId_,4+myClub().academy/3);
    lastYouthSearchDay_=totalDays_-30;
    for(auto& s:staff_)if(s.contractYear<=season_){
        s.contractYear=season_+2;
        s.weeklyWage=std::min(20000000,s.weeklyWage*104/100);
    }
    for(const auto& m:matches_)if(m.played)archive_.push_back(m);
    autoAssignCaptains();
    for(const auto& c:clubs_)pickLineup(c.id);
    createFixtures();createCups(continental);live_={};
    addMessage("Новый сезон", "Сезон "+std::to_string(old)+"/"+std::to_string((old+1)%100)+" завершён. Начинается "+std::to_string(season_)+"/"+std::to_string((season_+1)%100)+". Регены добавлены, игроки постарели.");
    status_="Новый сезон "+std::to_string(season_);
}
void Game::playMinute(LiveMatch& live,bool detailed){
    Match& m=matches_.at(live.id);
    const Club& h=clubs_[m.home],&a=clubs_[m.away];
    const int rh=lineupStrength(live.homeXI),ra=lineupStrength(live.awayXI);
    // Team strength, home advantage and tactics affect possession and chance frequency.
    const int initiative=clamp(54+(rh-ra)*2+(h.tactic-1)*3-(a.tactic-1)*3,16,84);
    const bool home=random(1,100)<=initiative;
    const int attackingId=home?m.home:m.away;
    const Club& attacking=clubs_[attackingId];
    const Club& defending=clubs_[home?m.away:m.home];
    const std::vector<int>& teamXI=home?live.homeXI:live.awayXI;
    const std::vector<int>& defenceXI=home?live.awayXI:live.homeXI;
    int urgency=attacking.tactic==2?4:attacking.tactic==3?3:attacking.tactic==0?-5:0;
    int shotThreshold=clamp(17+urgency+(lineupStrength(teamXI)-lineupStrength(defenceXI))/5,9,31);
    if(random(1,100)<=shotThreshold){
        std::vector<const Player*> shooters;
        for(int id:teamXI)if(const Player* p=findPlayer(id);p&&p->position!="GK"&&p->injured==0){
            int weight=(positionGroup(p->position)==3?7:positionGroup(p->position)==2?4:1)+p->skill[1]/15;
            for(int k=0;k<weight;++k)shooters.push_back(p);
        }
        if(shooters.empty())return;
        const Player* striker=shooters[random(0,int(shooters.size()-1))];
        std::vector<const Player*> providers;
        for(int id:teamXI)if(const Player* p=findPlayer(id);p&&p->id!=striker->id&&p->position!="GK")providers.push_back(p);
        const Player* assister=providers.empty()?striker:providers[random(0,int(providers.size()-1))];
        int quality=clamp(13+(striker->skill[1]-55)/3+(striker->skill[4]-55)/8+
            (assister->skill[2]-50)/15+(lineupStrength(teamXI)-lineupStrength(defenceXI))/6+
            (attacking.tactic==2?3:0)+(defending.tactic==0?-3:0),5,46);
        bool goal=random(1,100)<=quality;
        if(home){++live.shotsH;live.xgH+=quality;if(goal)++live.goalsH;}
        else{++live.shotsA;live.xgA+=quality;if(goal)++live.goalsA;}
        if(goal){for(auto& p:players_)if(p.id==striker->id){++p.goals;break;}}
        if(detailed||goal){
            std::string chain=attacking.name+": "+assister->name+" начинает комбинацию, переводит мяч на "+striker->name+". ";
            if(goal)chain+="ГОЛ! Точный удар! Счёт "+std::to_string(live.goalsH)+":"+std::to_string(live.goalsA)+".";
            else chain+=(random(0,1)?"Удар в створ, вратарь спасает.":"Удар с выгодной позиции — мимо ворот.");
            live.events.push_back({live.minute,chain});
        }
    }else if(detailed&&random(1,100)<=13){
        const std::vector<std::string> comments={"Перестроение атаки: команда ищет свободные зоны.",
          "Борьба в середине поля, защитники перекрывают коридоры.",
          "Фланговый проход, навес перехвачен соперником.",
          "Прессинг заставляет соперника отдать мяч назад.",
          "Длинная передача за спины защитников, положение вне игры.",
          "Розыгрыш мяча возле штрафной; оборона сохраняет компактность."};
        live.events.push_back({live.minute,attacking.name+": "+comments[random(0,int(comments.size()-1))]});
    }
    // Cards and injuries have persistent consequences. Disqualified players leave the pitch.
    if(random(1,900)==1){
        std::vector<int>& xi=home?live.homeXI:live.awayXI;
        if(!xi.empty()){
            int idx=random(0,int(xi.size()-1));int pid=xi[idx];
            for(auto& p:players_)if(p.id==pid){
                if(random(1,30)==1){
                    ++p.red;xi.erase(xi.begin()+idx);
                    live.events.push_back({live.minute,"ПРЯМАЯ КРАСНАЯ КАРТОЧКА: "+p.name+" ("+attacking.name+") удалён!"});
                }else{
                    ++p.yellow;
                    live.events.push_back({live.minute,"Жёлтая карточка: "+p.name+" ("+attacking.name+")."});
                }
                break;
            }
        }
    }
    if(random(1,3900)==1){
        const std::vector<int>& xi=home?live.homeXI:live.awayXI;
        if(!xi.empty()){
            int pid=xi[random(0,int(xi.size()-1))];
            for(auto& p:players_)if(p.id==pid){
                p.injured=random(3,19);
                live.events.push_back({live.minute,"ТРАВМА: "+p.name+" не может играть в полную силу. Можно провести замену."});
                break;
            }
        }
    }
}
void Game::finishMatch(LiveMatch& live){
    Match& m=matches_.at(live.id);
    if(m.played)return;
    m.played=true;m.homeGoals=live.goalsH;m.awayGoals=live.goalsA;m.shotsH=live.shotsH;m.shotsA=live.shotsA;m.xgH=live.xgH;m.xgA=live.xgA;m.events=live.events;
    Club& h=clubs_[m.home];Club& a=clubs_[m.away];
    if(m.competition<0){
        h.gf+=live.goalsH;h.ga+=live.goalsA;a.gf+=live.goalsA;a.ga+=live.goalsH;
        if(live.goalsH>live.goalsA){h.wins++;a.losses++;h.points+=3;}
        else if(live.goalsH<live.goalsA){a.wins++;h.losses++;a.points+=3;}
        else {h.draws++;a.draws++;h.points++;a.points++;}
    }else if(live.goalsH==live.goalsA){
        // Guaranteed winner: a simplified shootout after 90 minutes.
        const bool homeWins=random(0,1)==0;
        m.penaltiesH=homeWins?random(4,6):random(2,3);
        m.penaltiesA=homeWins?random(2,3):random(4,6);
        m.events.push_back({90,"Серия пенальти: "+std::to_string(m.penaltiesH)+
                            ":"+std::to_string(m.penaltiesA)});
    }
    std::unordered_set<int> playedHome(live.usedHome.begin(),live.usedHome.end());
    std::unordered_set<int> playedAway(live.usedAway.begin(),live.usedAway.end());
    for(auto& p:players_){
        bool wasHome=playedHome.contains(p.id),wasAway=playedAway.contains(p.id);
        if(!wasHome&&!wasAway)continue;
        ++p.appearances;++p.careerAppearances;
        p.fitness=clamp(p.fitness-random(5,11));
        int diff=wasHome?live.goalsH-live.goalsA:live.goalsA-live.goalsH;
        p.morale=clamp(p.morale+std::clamp(diff,-2,2));
    }
    processDeferredPayments();
    if(m.home==teamId_||m.away==teamId_){
        int diff=m.home==teamId_?live.goalsH-live.goalsA:live.goalsA-live.goalsH;
        myClub().fans=clamp(myClub().fans+(diff>0?2:diff<0?-2:0));
        myClub().board=clamp(myClub().board+(diff>0?1:diff<0?-1:0));
        addMessage("Результат матча",h.name+" "+std::to_string(live.goalsH)+":"+std::to_string(live.goalsA)+" "+a.name+
           ". Удары: "+std::to_string(live.shotsH)+"–"+std::to_string(live.shotsA)+". xG: "+std::to_string(live.xgH/100.0)+" / "+std::to_string(live.xgA/100.0)+".");
    }
    int attendance=clamp(h.fans+h.prestige-50,25,100)*h.stadium/100;
    int matchIncome=attendance*h.ticket+h.sponsorship;
    h.budget+=matchIncome;
    if(m.competition>=0){
        const int winningClub=knockoutWinner(m);
        if(winningClub>=0)clubs_[winningClub].budget+=m.competition==6?180000:60000;
    }
}
void Game::simulateFast(Match& match){
    LiveMatch l;l.id=match.id;initializeLineups(l,match);
    int rh=rating(clubs_[match.home]),ra=rating(clubs_[match.away]);
    int bias=clamp(50+(rh-ra)*2,15,85);
    for(int min=1;min<=90;++min){l.minute=min;
        if(random(1,100)<17){
            bool home=random(1,100)<=bias;int chance=random(8,32);
            if(home){l.shotsH++;l.xgH+=chance;if(random(1,100)<=chance)l.goalsH++;}
            else {l.shotsA++;l.xgA+=chance;if(random(1,100)<=chance)l.goalsA++;}
        }
    }
    l.events.push_back({90,"Фоновая симуляция завершена. Итоговый счёт: "+std::to_string(l.goalsH)+":"+std::to_string(l.goalsA)});
    finishMatch(l);
}
void Game::simulateOtherMatches(){
    for(auto& m:matches_){
        if(m.day!=day_||m.played||(m.home==teamId_||m.away==teamId_))continue;
        if(!leagues_[clubs_[m.home].league].active){simulateFast(m);continue;}
        LiveMatch sim;sim.id=m.id;initializeLineups(sim,m);
        for(int minute=1;minute<=90;++minute){sim.minute=minute;playMinute(sim,true);}
        finishMatch(sim);
    }
    // Append follow-up fixtures only AFTER iterating the match array.
    // Reallocation during the range-for would invalidate its references.
    progressCups();
}
bool Game::startLiveMatch(){
    if(hasLiveMatch()){status_="Матч уже начат.";return false;}
    if(!matchDue()){status_="Сегодня нет вашего матча.";return false;}
    auto m=upcomingMatch();
    live_={};live_.id=m->id;initializeLineups(live_,*m);live_.events.push_back({0,"Стартовый свисток!"});
    status_="Интерактивная трансляция запущена";
    return true;
}
bool Game::stepLiveMatch(int minutes){
    if(!hasLiveMatch()){status_="Сначала начните матч.";return false;}
    for(int i=0;i<minutes&&live_.minute<90;++i){live_.minute++;playMinute(live_,true);}
    if(live_.minute>=90){
        live_.events.push_back({90,"Финальный свисток!"});
        finishMatch(live_);live_={};simulateOtherMatches();status_="Матч завершён, результат сохранён";
    }else status_="Матч: "+std::to_string(live_.minute)+"-я минута";
    return true;
}
bool Game::instantMatch(){
    if(hasLiveMatch()){status_="Завершите текущую трансляцию.";return false;}
    if(!matchDue()){status_="Сегодня нет вашего матча.";return false;}
    auto m=upcomingMatch();LiveMatch sim;sim.id=m->id;initializeLineups(sim,*m);
    for(int minute=1;minute<=90;++minute){sim.minute=minute;playMinute(sim,true);}
    sim.events.push_back({90,"Финальный свисток!"});
    finishMatch(sim);simulateOtherMatches();status_="Мгновенная симуляция завершена";return true;
}
std::string Game::matchText()const{
    std::ostringstream o;
    if(hasLiveMatch()){
        const auto& l=live_;const auto& m=matches_[l.id];
        o<<"ИДЁТ МАТЧ • "<<l.minute<<"'\n";
        o<<matchLabel(m)<<"\n\n";
        o<<clubs_[m.home].name<<"  "<<l.goalsH<<":"<<l.goalsA<<"  "<<clubs_[m.away].name<<"\n";
        o<<"Удары: "<<l.shotsH<<"–"<<l.shotsA<<"   xG: "<<std::fixed<<std::setprecision(2)<<l.xgH/100.0<<"–"<<l.xgA/100.0<<"\n";
        o<<"Тактика: "<<tactics[myClub().tactic]<<"\n";
        o<<"Замены: "<<(m.home==teamId_?l.subsH:l.subsA)<<"/5\n";
        o<<"\nНА ПОЛЕ (ID): ";
        for(int id:(m.home==teamId_?l.homeXI:l.awayXI))o<<id<<" ";
        o<<"\nВыберите запасного слева, введите ID футболиста с поля\n";
        o<<"и нажмите «Заменить» (доступно в интерактивном матче).\n\n";
        for(const auto&e:l.events)o<<std::setw(3)<<e.minute<<"' "<<e.text<<"\n";
    }else if(matchDue()){
        auto m=upcomingMatch();o<<"СЕГОДНЯ ИГРА\n"<<matchLabel(*m)<<"\n\n"
           <<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n\n";
        o<<"Нажмите «Начать матч» для трансляции по минутам\nили «Мгновенно» для немедленного результата.\n";
        o<<"В интерактивном матче доступны замены и тактические решения.\n";
    }else if(auto m=upcomingMatch()){
        o<<"СЛЕДУЮЩИЙ МАТЧ\n"<<matchLabel(*m)<<"\n\n"
         <<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n";
        o<<"До матча: "<<std::max(0,m->day-day_)<<" дн.\n\n";
        o<<"До игрового дня можно переключиться через «Следующий день».\n";
    }else o<<"Матчей в текущем календаре больше нет.\n";
    return o.str();
}
std::string Game::matchArchive()const{
    std::ostringstream o;o<<"АРХИВ МАТЧЕЙ СЕЗОНА "<<season_<<"/"<<(season_+1)%100<<"\n\n";
    int count=0;
    for(const auto& m:matches_)if(m.played&&(m.home==teamId_||m.away==teamId_)){
        o<<matchLabel(m)<<" • День "<<m.day<<"\n";
        o<<clubs_[m.home].name<<"  "<<m.homeGoals<<":"<<m.awayGoals<<"  "<<clubs_[m.away].name;
        if(m.penaltiesH>=0)o<<" (пен. "<<m.penaltiesH<<":"<<m.penaltiesA<<")";
        o<<"\n";
        o<<"Удары: "<<m.shotsH<<" / "<<m.shotsA<<";  xG: "<<m.xgH/100.0<<" / "<<m.xgA/100.0<<"\n";
        for(const auto& e:m.events)o<<" "<<e.minute<<"' "<<e.text<<"\n";
        o<<"\n";count++;
    }
    o<<"\nПРЕДЫДУЩИЕ СЕЗОНЫ (ПОСЛЕДНИЕ 25 МАТЧЕЙ)\n\n";
    int previous=0;
    for(auto it=archive_.rbegin();it!=archive_.rend()&&previous<25;++it){
        const auto& m=*it;
        if(m.home!=teamId_&&m.away!=teamId_)continue;
        o<<m.season<<"/"<<(m.season+1)%100<<" "<<matchLabel(m)<<"\n";
        o<<clubs_[m.home].name<<" "<<m.homeGoals<<":"<<m.awayGoals<<" "<<clubs_[m.away].name;
        if(m.penaltiesH>=0)o<<" (пен. "<<m.penaltiesH<<":"<<m.penaltiesA<<")";
        o<<"\n";
        for(const auto& e:m.events)if(e.text.find("ГОЛ!")!=std::string::npos)o<<"  "<<e.minute<<"' "<<e.text<<"\n";
        ++previous;
    }
    if(!count&&!previous)o<<"Сыгранных матчей пока нет.\n";
    return o.str();
}
void Game::changeTactic(){
    if(!permissionSports()){status_="Должность не позволяет менять тактику при этой сложности.";return;}
    myClub().tactic=(myClub().tactic+1)%int(tactics.size());
    status_="Выбрано: "+tactics[myClub().tactic];
}
void Game::changeFormation(){
    if(!permissionSports()){status_="Нет полномочий на изменение схемы.";return;}
    myClub().formation=(myClub().formation+1)%int(formations.size());
    if(!hasLiveMatch())pickLineup(teamId_);
    status_="Схема: "+formations[myClub().formation];
}
void Game::changeTraining(){
    if(!permissionSports()){status_="Нет полномочий управлять тренировками.";return;}
    myClub().training=(myClub().training+1)%int(trainings.size());
    status_="Новый план тренировок: "+trainings[myClub().training];
}
bool Game::permissionSports()const{
    return difficulty_==Difficulty::Free||role_==Role::Coach||role_==Role::Sandbox||role_==Role::Director;
}
bool Game::permissionBusiness()const{
    return difficulty_==Difficulty::Free||difficulty_==Difficulty::Easy||role_==Role::Owner||role_==Role::President||role_==Role::Sandbox;
}
bool Game::canFinance(int amount){
    if(difficulty_==Difficulty::Free)return true;
    return myClub().budget>=amount;
}
const TransferDeal* Game::transferDeal(int playerId)const{
    for(auto it=deals_.rbegin();it!=deals_.rend();++it)
        if(it->playerId==playerId)return &*it;
    return nullptr;
}
std::string Game::negotiationText(int playerId)const{
    const auto* deal=transferDeal(playerId);
    if(!deal)return "ПЕРЕГОВОРЫ: пока не начаты.\nВведите сумму в евро и нажмите «Предложить».\n";
    const Player* player=findPlayer(playerId);
    if(!player)return "Футболист не найден.\n";
    std::ostringstream o;
    static const std::array<const char*,9> stages={
        "Клуб изучает предложение","Клуб сделал встречное предложение",
        "Клуб согласен: переговоры с агентом","Агент обсуждает контракт",
        "Агент выдвинул встречные условия","Трансфер завершён",
        "Переговоры отклонены","Переговоры отменены","Игрок выбрал другой клуб"
    };
    const int stage=int(deal->stage);
    o<<"ПЕРЕГОВОРЫ: "<<(stage>=0&&stage<9?stages[stage]:"неизвестно")<<"\n";
    if(const Club* seller=findClub(deal->sellerClub))o<<"Продавец: "<<seller->name<<"\n";
    o<<"Тип сделки: "<<(deal->kind==1?"аренда":"покупка")<<"\n";
    o<<"Ваше предложение: "<<money(deal->fee)<<"\n";
    if(deal->kind==1){
        o<<"Срок аренды: "<<deal->loanDays<<" дней\n";
        o<<"Доля зарплаты принимающей стороны: "<<deal->loanWageShare<<"%\n";
    }else{
        o<<"Рассрочка: "<<deal->installments<<" платежей\n";
        if(deal->appearanceBonus)o<<"Бонус после 10 матчей: "<<money(deal->appearanceBonus)<<"\n";
        if(deal->sellOnPct)o<<"Процент от последующей продажи: "<<deal->sellOnPct<<"%\n";
        if(deal->exchangePlayerId>=0)o<<"Футболист в обмене: #"<<deal->exchangePlayerId<<"\n";
    }
    if(deal->askingFee>0)o<<"Требование клуба: "<<money(deal->askingFee)<<"\n";
    if(deal->stage==DealStage::ClubReview||deal->stage==DealStage::AgentReview)
        o<<"Ответ ожидается через "<<std::max(0,deal->replyDay-totalDays_)<<" дн.\n";
    if(deal->stage==DealStage::AgentOffer||deal->stage==DealStage::AgentCounter||deal->stage==DealStage::AgentReview){
        o<<"Ожидания агента: "<<money(deal->askingWage)<<" в неделю\n";
        if(deal->wage)o<<"Ваша зарплата: "<<money(deal->wage)<<" в неделю, "<<deal->years<<" лет\n";
    }
    if(deal->rivalClub>=0&&(stage<=4)){
        if(const Club* rival=findClub(deal->rivalClub))
            o<<"Интерес конкурента: "<<rival->name<<"\n";
    }
    if(stage<=4)o<<"Срок переговоров: "<<std::max(0,deal->expiryDay-totalDays_)<<" дн.\n";
    if(deal->stage==DealStage::ClubCounter)
        o<<"Введите новое предложение или нажмите «Принять условия».\n";
    if(deal->stage==DealStage::AgentOffer||deal->stage==DealStage::AgentCounter)
        o<<(deal->kind==1?"Согласуйте с игроком временный переход (кнопка «Принять условия»).\n":
             "Введите недельную зарплату и срок: 55000 4\nИли нажмите «Принять условия».\n");
    o<<"Форматы: 15000000 (обычная покупка), deal 15000000 3 1000000 20\n";
    o<<"(сумма, платежи, бонус за 10 матчей, % перепродажи);\n";
    o<<"swap 5000000 123 (сумма и ID игрока в обмен); loan 100000 50 180\n";
    o<<"(арендная плата, % зарплаты, дней).\n";
    return o.str();
}
std::string Game::transferBid(int playerId,int offered){
    return offerStructuredTransfer(playerId,offered,1,0,0,-1);
}
std::string Game::offerStructuredTransfer(int playerId,int fee,int installments,int appearanceBonus,int sellOnPct,int exchangePlayerId){
    if(!permissionSports())return status_="Нужны полномочия спортивного руководства.";
    if(hasLiveMatch())return status_="Нельзя вести переговоры во время матча.";
    const Player* p=findPlayer(playerId);
    if(!p||p->club==teamId_||p->parentClub>=0)return status_="Выберите доступного игрока другого клуба.";
    if(fee<1||fee>500000000||installments<1||installments>12||
       appearanceBonus<0||appearanceBonus>50000000||sellOnPct<0||sellOnPct>40)
        return status_="Неверные параметры: сумма до 500 млн, 1–12 платежей, бонус до 50 млн, перепродажа до 40%.";
    const Player* exchanged=nullptr;
    if(exchangePlayerId>=0){
        exchanged=findPlayer(exchangePlayerId);
        if(!exchanged||exchanged->club!=teamId_||exchanged->parentClub>=0||exchangePlayerId==playerId)
            return status_="Для обмена выберите собственного футболиста, не находящегося в аренде.";
    }
    const int upfront=fee/installments + (fee%installments);
    if(!canFinance(upfront))return status_="Недостаточно бюджета для первого взноса.";
    TransferDeal* current=nullptr;
    for(auto it=deals_.rbegin();it!=deals_.rend();++it)
        if(it->playerId==playerId){current=&*it;break;}
    if(current&&int(current->stage)<=4&&current->stage!=DealStage::ClubCounter)
        return status_="Переговоры уже идут. Посмотрите их статус справа.";
    if(current&&current->stage==DealStage::ClubCounter){
        if(current->kind!=0)return status_="Это аренда. Для изменения условий используйте кнопку «Аренда».";
        if(current->rounds>=4)return status_="Клуб отказался обсуждать дополнительные предложения.";
        const bool simple=(installments==1&&appearanceBonus==0&&sellOnPct==0&&exchangePlayerId<0);
        current->fee=fee;
        if(!simple){
            current->installments=installments;current->appearanceBonus=appearanceBonus;
            current->sellOnPct=sellOnPct;current->exchangePlayerId=exchangePlayerId;
        }
        current->stage=DealStage::ClubReview;
        current->replyDay=totalDays_+1;
        ++current->rounds;
        return status_="Новое предложение отправлено. Ответ клуба ожидается завтра.";
    }
    const int totalAsking=int(std::min<int64_t>(500000000LL,
        int64_t(p->value)*(difficulty_==Difficulty::Hard?145:difficulty_==Difficulty::Easy?100:120)/100+
        (p->contractYear>season_+2?p->value/10:0)));
    const int askingCash=std::max(1,totalAsking-(exchanged?exchanged->value*70/100:0)-
          std::min(appearanceBonus,50000000)/4);
    int rival=-1;
    if(random(0,3)==0){
        int top=-1;
        for(const auto& c:clubs_)
            if(c.id!=teamId_&&c.id!=p->club&&leagues_[c.league].active&&c.prestige>top){
                top=c.prestige;rival=c.id;
            }
    }
    TransferDeal d;
    d.playerId=playerId;d.sellerClub=p->club;d.fee=fee;d.askingFee=askingCash;
    d.submittedDay=totalDays_;d.replyDay=totalDays_+1;d.expiryDay=totalDays_+21;d.rivalClub=rival;
    d.installments=installments;d.appearanceBonus=appearanceBonus;d.sellOnPct=sellOnPct;
    d.exchangePlayerId=exchangePlayerId;
    // Paying a published release clause in full bypasses the selling club's approval.
    if(p->releaseClause>0&&fee>=p->releaseClause&&installments==1&&exchangePlayerId<0){
        d.stage=DealStage::AgentOffer;
        const int gap=clubs_[d.sellerClub].prestige-myClub().prestige;
        d.askingWage=int(std::min<int64_t>(20000000LL,
            std::max<int64_t>(500,int64_t(p->wage)*(125+std::max(0,gap)/2)/100)));
    }
    deals_.push_back(d);
    if(d.stage==DealStage::AgentOffer){
        addMessage("Активированы отступные",p->name+": предложение "+money(fee)+
                   " удовлетворяет условию выкупа. Теперь обсудите зарплату с агентом.");
        return status_="Активированы отступные. Перейдите к личному контракту.";
    }
    return status_="Официальное предложение "+money(fee)+" по "+p->name+
                   " отправлено в "+clubs_[p->club].name+". Ответ через игровой день.";
}
std::string Game::offerLoan(int playerId,int fee,int wageShare,int durationDays){
    if(!permissionSports())return status_="Нет полномочий на аренду.";
    if(hasLiveMatch())return status_="Сначала завершите матч.";
    const Player* p=findPlayer(playerId);
    if(!p||p->club==teamId_||p->parentClub>=0)return status_="Футболист недоступен для аренды.";
    if(fee<0||fee>100000000||wageShare<0||wageShare>100||durationDays<30||durationDays>330)
        return status_="Аренда: сумма 0–100 млн €, доля зарплаты 0–100%, срок 30–330 дней.";
    if(!canFinance(fee))return status_="Недостаточно средств на аренду.";
    TransferDeal* current=nullptr;
    for(auto it=deals_.rbegin();it!=deals_.rend();++it)
        if(it->playerId==playerId){current=&*it;break;}
    if(current&&int(current->stage)<=4&&current->stage!=DealStage::ClubCounter)
        return status_="По футболисту уже идут переговоры.";
    if(current&&current->stage==DealStage::ClubCounter&&current->kind!=1)
        return status_="По этому футболисту идёт переговорный процесс о покупке.";
    const int requested=std::min(100000000,
        p->value/20+std::max(0,60-wageShare)*p->value/600);
    if(current&&current->stage==DealStage::ClubCounter){
        current->fee=fee;current->loanWageShare=wageShare;current->loanDays=durationDays;
        current->stage=DealStage::ClubReview;current->replyDay=totalDays_+1;
        ++current->rounds;
        return status_="Новые условия аренды отправлены клубу.";
    }
    TransferDeal d;
    d.kind=1;d.playerId=playerId;d.sellerClub=p->club;
    d.fee=fee;d.askingFee=requested;d.loanWageShare=wageShare;d.loanDays=durationDays;
    d.submittedDay=totalDays_;d.replyDay=totalDays_+1;d.expiryDay=totalDays_+14;
    deals_.push_back(d);
    return status_="Предложение аренды "+p->name+" направлено клубу. Ответ через день.";
}
std::string Game::proposePlayerContract(int playerId,int weeklyWage,int years){
    if(!permissionSports())return status_="Нет полномочий согласовывать личные контракты.";
    if(hasLiveMatch())return status_="Переговоры недоступны во время матча.";
    TransferDeal* d=nullptr;
    for(auto it=deals_.rbegin();it!=deals_.rend();++it)if(it->playerId==playerId){d=&*it;break;}
    if(!d||(d->stage!=DealStage::AgentOffer&&d->stage!=DealStage::AgentCounter))
        return status_="Сначала согласуйте трансферную стоимость с клубом.";
    if(weeklyWage<500||weeklyWage>20000000||years<1||years>7)
        return status_="Зарплата от 500 до 20 млн € в неделю, контракт от 1 до 7 лет.";
    if(d->kind==1&&weeklyWage!=d->askingWage)
        return status_="При аренде действующая зарплата игрока не изменяется. Примите условия клуба.";
    const int upfront=d->kind==1?d->fee:d->fee/d->installments+(d->fee%d->installments);
    const int bonus=d->kind==1?0:std::min(weeklyWage*2,40000000);
    if(!canFinance(upfront+bonus))
        return status_="Недостаточно бюджета на первый платёж и подъёмные.";
    d->wage=weeklyWage;d->years=years;
    d->stage=DealStage::AgentReview;d->replyDay=totalDays_+1;
    ++d->rounds;
    return status_="Условия отправлены агенту. Ответ придёт на следующий игровой день.";
}
std::string Game::acceptNegotiationTerms(int playerId){
    const auto* d=transferDeal(playerId);
    if(!d)return status_="Переговоры ещё не начинались.";
    if(d->stage==DealStage::ClubCounter){
        if(d->kind==1)return offerLoan(playerId,d->askingFee,d->loanWageShare,d->loanDays);
        return offerStructuredTransfer(playerId,d->askingFee,d->installments,
                                       d->appearanceBonus,d->sellOnPct,d->exchangePlayerId);
    }
    if(d->stage==DealStage::AgentOffer||d->stage==DealStage::AgentCounter)
        return proposePlayerContract(playerId,d->askingWage,d->kind==1?1:3);
    return status_="Сейчас нет встречных условий, которые можно принять.";
}
std::string Game::cancelNegotiation(int playerId){
    if(!permissionSports())return status_="Нет полномочий отменить переговоры.";
    if(hasLiveMatch())return status_="Завершите матч перед переговорами.";
    for(auto it=deals_.rbegin();it!=deals_.rend();++it)
        if(it->playerId==playerId&&int(it->stage)<=4){
            it->stage=DealStage::Cancelled;
            return status_="Переговоры по "+(findPlayer(playerId)?findPlayer(playerId)->name:"игроку")+" прекращены.";
        }
    return status_="Активных переговоров с этим футболистом нет.";
}
void Game::finalizeTransfer(TransferDeal& d){
    Player* player=nullptr;
    for(auto& p:players_)if(p.id==d.playerId){player=&p;break;}
    if(!player||player->club!=d.sellerClub||player->parentClub>=0){
        d.stage=DealStage::Rejected;
        addMessage("Трансфер сорвался","Игрок больше не принадлежит клубу-продавцу.");
        return;
    }
    const int previousClub=player->club;
    Club& seller=clubs_[previousClub],&buyer=myClub();
    if(d.kind==1){
        if(!canFinance(d.fee)){
            d.stage=DealStage::Rejected;
            addMessage("Аренда сорвалась","Не хватает денег на согласованный арендный платёж.");
            return;
        }
        if(buyer.prestige+35<seller.prestige){
            d.stage=DealStage::Rejected;
            addMessage("Игрок отказался от аренды",player->name+
                       " не хочет переходить в клуб со значительно более низким статусом.");
            return;
        }
        buyer.budget-=d.fee;
        seller.budget+=d.fee;
        player->parentClub=previousClub;
        player->club=teamId_;
        player->loanReturnDay=totalDays_+d.loanDays;
        player->loanWageShare=d.loanWageShare;
        player->starter=false;
        player->morale=clamp(player->morale+3);
        d.stage=DealStage::Completed;
        autoAssignCaptains();pickLineup(previousClub);pickLineup(teamId_);
        addMessage("Аренда подтверждена",player->name+" переходит в "+buyer.name+
                   " на "+std::to_string(d.loanDays)+" дней, оплата "+
                   std::to_string(d.loanWageShare)+"% зарплаты и "+money(d.fee)+".");
        return;
    }

    Player* exchanged=nullptr;
    int exchangeValue=0;
    if(d.exchangePlayerId>=0){
        for(auto& p:players_)if(p.id==d.exchangePlayerId){exchanged=&p;break;}
        if(!exchanged||exchanged->club!=teamId_||exchanged->parentClub>=0){
            d.stage=DealStage::Rejected;
            addMessage("Обмен сорвался","Футболист, включённый в обмен, больше не доступен.");
            return;
        }
        exchangeValue=exchanged->value*70/100;
    }
    const int signingBonus=std::min(d.wage*2,40000000);
    const int upfront=d.fee/d.installments+(d.fee%d.installments);
    if(!canFinance(upfront+signingBonus)){
        d.stage=DealStage::Rejected;
        addMessage("Трансфер сорвался","Не хватает средств на первый платёж и подъёмные.");
        return;
    }
    // A cash-first payment plan is binding: later instalments are paid by date.
    buyer.budget-=upfront+signingBonus;
    seller.budget+=upfront;
    for(int i=1;i<d.installments;++i){
        const int amount=d.fee/d.installments;
        if(amount>0)payments_.push_back({d.playerId,teamId_,previousClub,amount,totalDays_+i*30,0});
    }
    if(d.appearanceBonus>0)
        payments_.push_back({d.playerId,teamId_,previousClub,d.appearanceBonus,-1,player->careerAppearances+10});
    // A prior sell-on clause is owed by the selling club when the player changes owner.
    if(player->sellOnClub>=0&&player->sellOnClub<int(clubs_.size())&&player->sellOnPct>0){
        const int share=int(int64_t(d.fee+exchangeValue)*player->sellOnPct/100);
        seller.budget-=share;clubs_[player->sellOnClub].budget+=share;
    }
    if(exchanged){
        if(exchanged->sellOnClub>=0&&exchanged->sellOnClub<int(clubs_.size())&&exchanged->sellOnPct>0){
            const int share=int(int64_t(exchangeValue)*exchanged->sellOnPct/100);
            buyer.budget-=share;clubs_[exchanged->sellOnClub].budget+=share;
        }
        exchanged->club=previousClub;exchanged->starter=false;exchanged->sellOnClub=-1;
        exchanged->sellOnPct=0;exchanged->releaseClause=0;exchanged->morale=clamp(exchanged->morale-2);
    }
    player->club=teamId_;player->starter=false;player->wage=d.wage;
    player->contractYear=season_+d.years;
    player->releaseClause=0;player->sellOnClub=d.sellOnPct?previousClub:-1;
    player->sellOnPct=d.sellOnPct;
    player->morale=clamp(player->morale+5);
    player->trust=clamp(player->trust+6);player->loyalty=clamp(player->loyalty+3);
    d.stage=DealStage::Completed;
    autoAssignCaptains();pickLineup(previousClub);pickLineup(teamId_);
    addMessage("Трансфер завершён",player->name+" перешёл в "+buyer.name+" из "+seller.name+
               " за "+money(d.fee)+" ("+std::to_string(d.installments)+
               " платежей). Зарплата "+money(d.wage)+" в неделю."+
               (exchanged?" Дополнительно состоялся обмен игроками.":""));
}
void Game::processDeferredPayments(){
    for(size_t i=0;i<payments_.size();){
        const DeferredPayment p=payments_[i];
        bool pay=false,discard=false;
        if(p.triggerAppearances>0){
            const Player* player=findPlayer(p.playerId);
            if(!player||player->club!=p.buyerClub||player->parentClub>=0)discard=true;
            else if(player->careerAppearances>=p.triggerAppearances)pay=true;
        }else if(totalDays_>=p.dueDay)pay=true;
        if(pay){
            clubs_[p.buyerClub].budget-=p.amount;
            clubs_[p.sellerClub].budget+=p.amount;
            if(p.buyerClub==teamId_||p.sellerClub==teamId_)
                addMessage(p.triggerAppearances>0?"Бонус за матчи":"Трансферный платёж",
                    "Платёж "+money(p.amount)+" по игроку #"+std::to_string(p.playerId)+
                    " перечислен клубу "+clubs_[p.sellerClub].name+".");
        }
        if(pay||discard)payments_.erase(payments_.begin()+i);
        else ++i;
    }
}
void Game::processLoanReturns(){
    bool changed=false;
    for(auto& p:players_)if(p.parentClub>=0&&p.loanReturnDay>=0&&totalDays_>=p.loanReturnDay){
        const int from=p.club,to=p.parentClub;
        p.club=to;p.parentClub=-1;p.loanReturnDay=-1;p.loanWageShare=100;p.starter=false;
        changed=true;
        if(from==teamId_||to==teamId_)
            addMessage("Завершена аренда",p.name+" возвращается в "+
                       clubs_[to].name+" после окончания арендного соглашения.");
    }
    if(changed){
        autoAssignCaptains();
        for(const auto& c:clubs_)pickLineup(c.id);
    }
}
std::string Game::setReleaseClause(int playerId,int amount){
    if(!permissionSports())return status_="Нет полномочий для изменения условий контракта.";
    if(hasLiveMatch())return status_="Изменения контракта недоступны во время матча.";
    Player* p=nullptr;for(auto& candidate:players_)if(candidate.id==playerId)p=&candidate;
    if(!p||((p->club!=teamId_)&&difficulty_!=Difficulty::Free)||p->parentClub>=0)
        return status_="В обычной карьере изменять отступные можно только своим игрокам.";
    if(amount<0||amount>500000000)return status_="Отступные: от 0 до 500 млн €. Ноль отключает условие.";
    if(amount>0&&amount<p->value)return status_="Отступные должны быть не ниже рыночной стоимости игрока.";
    // Simple sandbox contract amendment. A negotiated extension is still handled separately.
    p->releaseClause=amount;
    addMessage("Контрактное условие",p->name+(amount?" получил отступные "+money(amount):" больше не имеет отступных."));
    return status_=amount?"Отступные записаны в контракт.":"Условие выкупа удалено.";
}
std::string Game::sellPlayer(int playerId,int buyerClub,int amount){
    if(!permissionSports())return status_="Нет полномочий продавать игроков.";
    if(hasLiveMatch())return status_="Сначала завершите матч.";
    Player* p=nullptr;for(auto& candidate:players_)if(candidate.id==playerId)p=&candidate;
    if(!p||p->club!=teamId_||p->parentClub>=0)return status_="Можно продать только собственного футболиста.";
    if(buyerClub<0||buyerClub>=int(clubs_.size())||buyerClub==teamId_)
        return status_="Неверный ID клуба-покупателя.";
    if(amount<1||amount>500000000)return status_="Цена продажи: 1–500 млн €.";
    Club& buyer=clubs_[buyerClub];
    if(buyer.budget<amount)return status_="У клуба "+buyer.name+" недостаточно средств.";
    if(amount>int64_t(p->value)*160/100)
        return status_="Клуб "+buyer.name+" отказался от завышенной цены.";
    const int oldSellOnClub=p->sellOnClub;
    const int oldSellOnPct=p->sellOnPct;
    buyer.budget-=amount;
    myClub().budget+=amount;
    if(oldSellOnClub>=0&&oldSellOnClub<int(clubs_.size())&&oldSellOnPct>0){
        const int share=int(int64_t(amount)*oldSellOnPct/100);
        myClub().budget-=share;clubs_[oldSellOnClub].budget+=share;
    }
    const std::string name=p->name;
    p->club=buyerClub;p->starter=false;p->releaseClause=0;
    p->sellOnClub=-1;p->sellOnPct=0;p->morale=clamp(p->morale+2);
    for(auto& d:deals_)if(d.playerId==playerId&&int(d.stage)<=4)d.stage=DealStage::Cancelled;
    autoAssignCaptains();pickLineup(teamId_);pickLineup(buyerClub);
    addMessage("Игрок продан",name+" перешёл в "+buyer.name+" за "+money(amount)+".");
    return status_="Продажа завершена: "+name+" в "+buyer.name+".";
}
void Game::processTransferNegotiations(){
    for(auto& d:deals_){
        if(int(d.stage)>4)continue;
        const Player* p=findPlayer(d.playerId);
        if(!p||p->club!=d.sellerClub||p->parentClub>=0){
            d.stage=DealStage::Rejected;continue;
        }
        if(totalDays_>d.expiryDay){
            d.stage=DealStage::Rejected;
            addMessage("Срок сделки истёк","Переговоры с "+p->name+" прекращены после долгого ожидания.");
            continue;
        }
        // Competition is simulated only for offers with an interested rival.
        if(d.rivalClub>=0&&totalDays_-d.submittedDay>=8&&random(1,25)==1){
            d.stage=DealStage::RivalSigned;
            addMessage("Конкурент перехватил игрока",p->name+" предпочёл предложение "+
                       clubs_[d.rivalClub].name+". Ваши переговоры завершены.");
            continue;
        }
        if((d.stage!=DealStage::ClubReview&&d.stage!=DealStage::AgentReview)||totalDays_<d.replyDay)
            continue;
        if(d.stage==DealStage::ClubReview){
            if(d.fee>=d.askingFee){
                d.stage=DealStage::AgentOffer;
                const int prestigeGap=clubs_[d.sellerClub].prestige-myClub().prestige;
                const int premium=std::max(0,prestigeGap)/2;
                d.askingWage=d.kind==1?p->wage:int(std::min<int64_t>(20000000LL,
                   std::max<int64_t>(500,int64_t(p->wage)*(125+premium)/100)));
                addMessage("Клуб принял предложение",clubs_[d.sellerClub].name+
                           (d.kind==1?" согласен отдать в аренду ":" согласен продать ")+
                           p->name+" за "+money(d.fee)+". Агент просит "+money(d.askingWage)+
                           " в неделю. Откройте «Трансферы».");
            }else if(d.fee<int64_t(d.askingFee)*60/100||d.rounds>=4){
                d.stage=DealStage::Rejected;
                addMessage("Предложение отклонено",clubs_[d.sellerClub].name+
                           " отклонил условия по "+p->name+" на сумму "+money(d.fee)+".");
            }else{
                d.stage=DealStage::ClubCounter;
                addMessage("Встречное предложение",clubs_[d.sellerClub].name+" хочет "+
                           money(d.askingFee)+" за "+p->name+".");
            }
        }else if(d.stage==DealStage::AgentReview){
            if(d.wage>=d.askingWage){
                finalizeTransfer(d);
            }else if(d.rounds>=5||d.wage<int64_t(d.askingWage)*55/100){
                d.stage=DealStage::Rejected;
                addMessage("Агент отказался",p->name+" отклонил личные условия контракта.");
            }else{
                d.stage=DealStage::AgentCounter;
                addMessage("Встречное предложение агента",p->name+" просит "+
                           money(d.askingWage)+" в неделю. Поднимите зарплату или отмените сделку.");
            }
        }
    }
}
std::string Game::renewContract(int playerId,int wage,int years){
    if(!permissionSports()){status_="Нет полномочий на продление контракта.";return status_;}
    Player* p=nullptr;for(auto& i:players_)if(i.id==playerId)p=&i;
    if(!p||p->club!=teamId_||p->parentClub>=0){status_="Выберите футболиста с постоянным контрактом клуба.";return status_;}
    if(wage<500||years<1||years>7){status_="Допустимо: зарплата от 500 € в неделю, срок 1–7 лет.";return status_;}
    int expected=p->wage*(p->morale<40?150:110)/100;
    int probability=clamp(45+(wage-expected)/std::max(100,p->wage/25)+p->trust/5,10,95);
    if(random(1,100)<=probability){
        p->wage=wage;p->contractYear=season_+years;p->loyalty=clamp(p->loyalty+7);
        status_="Контракт продлён с "+p->name+" до "+std::to_string(p->contractYear)+" года.";
        addMessage("Контракт подписан",status_);
    }else status_=p->name+" отказался: агент ожидает примерно "+money(expected)+" в неделю.";
    return status_;
}
namespace {
std::string lowerForSearch(const std::string& s){
    std::string o;
    for(size_t i=0;i<s.size();++i){
        unsigned char a=(unsigned char)s[i];
        if(i+1<s.size()){
            unsigned char b=(unsigned char)s[i+1];
            if(a==0xD0&&b>=0x90&&b<=0x9F){o.push_back(char(0xD0));o.push_back(char(b+0x20));i++;continue;}
            if(a==0xD0&&b>=0xA0&&b<=0xAF){o.push_back(char(0xD1));o.push_back(char(b-0x20));i++;continue;}
            if(a==0xD0&&b==0x81){o.push_back(char(0xD1));o.push_back(char(0x91));i++;continue;}
        }
        o.push_back(char(a>='A'&&a<='Z'?a+32:a));
    }
    return o;
}
bool anyWords(const std::string&s, std::initializer_list<std::string_view> words){
    for(auto w:words)if(s.find(w)!=std::string::npos)return true;
    return false;
}
}
std::string Game::talk(int playerId,const std::string& phrase){
    Player* p=nullptr;for(auto& v:players_)if(v.id==playerId)p=&v;
    if(!p||p->club!=teamId_){status_="Для разговора выберите футболиста клуба.";return status_;}
    if(phrase.empty()){status_="Введите своё сообщение футболисту.";return status_;}
    std::string t=lowerForSearch(phrase),response;
    if(anyWords(t,{"семь","дом","ребён","ребен","жен","родител","помощ","адаптац"})){
        p->family=clamp(p->family+random(6,15));p->trust=clamp(p->trust+8);p->morale=clamp(p->morale+3);
        response="Спасибо, что спросили о моей семье. Такая поддержка действительно много значит. Я постараюсь сосредоточиться на футболе.";
    }else if(anyWords(t,{"зарплат","деньг","контракт","преми"})){
        p->trust=clamp(p->trust-1);
        response="Давайте обсудим цифры с моим агентом. Для меня важны и деньги, и уверенность в будущем.";
    }else if(anyWords(t,{"трансфер","уйти","продать","переход"})){
        p->loyalty=clamp(p->loyalty-5);p->morale=clamp(p->morale-3);
        response="Это серьёзный разговор. Мне нужно подумать о своём будущем и поговорить с агентом.";
    }else if(anyWords(t,{"молодец","отлично","спасибо","хвал","верю","горжусь"})){
        p->morale=clamp(p->morale+7);p->trust=clamp(p->trust+5);
        response="Спасибо за доверие. Я приложу все силы, чтобы оправдать ожидания.";
    }else if(anyWords(t,{"плохо","слаб","скамейк","штраф","ужас","недовол","дубль"})){
        p->morale=clamp(p->morale-7);p->trust=clamp(p->trust-4);
        response=p->personality=="Импульсивный"?"Я не согласен! Вы не видите, сколько я выкладываюсь на тренировках.":"Понимаю вашу позицию. Я постараюсь ответить игрой, но неприятно слышать такое.";
    }else if(anyWords(t,{"извин","прост","ошиб"})){
        p->trust=clamp(p->trust+4);response="Хорошо, давайте оставим прошлое позади и работать дальше.";
    }else if(anyWords(t,{"состав","минут","играть","матч","основ"})){
        p->morale=clamp(p->morale+2);
        response="Я хочу играть больше. Если вы дадите шанс, буду готов доказать свою пользу.";
    }else{
        int kind=random(0,3);
        response=kind==0?"Я вас понял. Спасибо за разговор.":kind==1?"Интересная мысль. Посмотрим, как всё сложится.":kind==2?"Думаю, у нас получится найти общий язык.":"Я ценю, что вы обсуждаете это со мной лично.";
        p->trust=clamp(p->trust+1);
    }
    status_=p->name+": «"+response+"»";
    addMessage("Личный разговор с "+p->name,"Вы: «"+phrase+"»\n"+p->name+": «"+response+"»");
    return status_;
}
std::string Game::speakToFans(const std::string& phrase){
    if(phrase.empty()){status_="Введите обращение к болельщикам.";return status_;}
    std::string t=lowerForSearch(phrase),reaction;
    if(anyWords(t,{"спасибо","поддерж","уваж","вместе"})){
        myClub().fans=clamp(myClub().fans+5);reaction="Объединения болельщиков приветствуют открытый разговор.";
    }else if(anyWords(t,{"билет","цен","стадион"})){
        myClub().fans=clamp(myClub().fans+3);reaction="Фанаты ждут конкретных шагов по ценам и условиям на стадионе.";
    }else if(anyWords(t,{"побед","обеща","чемпион"})){
        myClub().fans=clamp(myClub().fans+2);reaction="Реакция положительная, но болельщики запомнят обещание.";
    }else if(anyWords(t,{"молч","замол","глуп","недоволь"})){
        myClub().fans=clamp(myClub().fans-10);reaction="В фанатской среде распространяется недовольство вашим заявлением.";
    }else{
        myClub().fans=clamp(myClub().fans+1);reaction="Некоторые фанаты оценили внимание, другие требуют конкретных решений.";
    }
    status_=reaction+" Поддержка: "+pct(myClub().fans);
    addMessage("Обращение к болельщикам","Вы: «"+phrase+"»\n"+reaction);
    return status_;
}
std::string Game::spendMoney(const std::string& project){
    if(!permissionBusiness()){status_="Это решение должен утверждать владелец или президент.";return status_;}
    int cost=0;
    if(project=="stadium"){cost=3000000;if(myClub().stadium>140000){status_="Максимальная вместимость достигнута.";return status_;}}
    if(project=="training"){cost=2000000;if(myClub().facilities>=10){status_="База полностью модернизирована.";return status_;}}
    if(project=="academy"){cost=2500000;if(myClub().academy>=10){status_="Академия полностью модернизирована.";return status_;}}
    if(cost==0)return "Неизвестный проект.";
    if(!canFinance(cost)){status_="Недостаточно средств: нужно "+money(cost);return status_;}
    myClub().budget-=cost;
    if(project=="stadium")myClub().stadium+=5000;
    if(project=="training")myClub().facilities++;
    if(project=="academy")myClub().academy++;
    myClub().fans=clamp(myClub().fans+2);
    status_="Инвестиция одобрена: "+project+"; стоимость "+money(cost);
    addMessage("Инфраструктура",status_);
    return status_;
}
std::string Game::recruitYouth(){
    int bestId=-1,best=-1;
    for(const auto& prospect:youth_)if(prospect.club==teamId_&&prospect.potential>best){
        best=prospect.potential;bestId=prospect.id;
    }
    if(bestId<0)return status_="В академии пока нет кандидатов. Запустите новый просмотр.";
    return signYouthProspect(bestId);
}
std::string Game::negotiateSponsor(){
    if(!permissionBusiness()){status_="Спонсорские переговоры доступны руководству.";return status_;}
    int roll=random(1,100);
    if(roll<40){status_="Новый спонсор отказал, переговоры не дали результата.";}
    else{int bonus=25000+random(0,myClub().prestige*4000);myClub().sponsorship+=bonus;
        status_="Заключено улучшенное спонсорское соглашение: +"+money(bonus)+" за домашнюю игру.";}
    addMessage("Переговоры со спонсором",status_);
    return status_;
}
std::string Game::adjustTickets(int change){
    if(!permissionBusiness()){status_="Цены на билеты утверждает руководство.";return status_;}
    myClub().ticket=std::clamp(myClub().ticket+change,5,250);
    myClub().fans=clamp(myClub().fans+(change<0?2:-2));
    status_="Цена билета: "+std::to_string(myClub().ticket)+" €. Отношение болельщиков изменилось.";
    addMessage("Билетная политика",status_);return status_;
}
std::string Game::appointRole(Role role){
    if(difficulty_!=Difficulty::Free&&role_!=Role::Sandbox){status_="Менять полномочия по ходу карьеры можно в свободной песочнице.";return status_;}
    role_=role;status_="Должность изменена.";return status_;
}
void Game::toggleLeague(int leagueId){
    if(leagueId<0||leagueId>=int(leagues_.size()))return;
    if(leagueId==myClub().league){status_="Нельзя отключить лигу своего клуба.";return;}
    leagues_[leagueId].active=!leagues_[leagueId].active;
    status_=leagues_[leagueId].name+std::string(leagues_[leagueId].active?" включена для детальной симуляции.":" переведена на фоновую симуляцию.");
}
std::string Game::save(const std::string& path)const{
    std::ofstream f(path,std::ios::binary|std::ios::trunc);
    if(!f)return "Не удалось открыть файл для записи: "+path;
    f<<"FUSAVE_9\n";
    putString(f,manager_);putString(f,status_);
    f<<teamId_<<' '<<day_<<' '<<season_<<' '<<totalDays_<<' '<<nextPlayerId_<<' '<<int(role_)<<' '<<int(difficulty_)<<'\n';
    std::ostringstream rand;rand<<rng_;putString(f,rand.str());
    f<<leagues_.size()<<'\n';
    for(const auto& l:leagues_){f<<l.id<<' '<<l.active<<'\n';putString(f,l.country);putString(f,l.name);}
    f<<clubs_.size()<<'\n';
    for(const auto& c:clubs_){
        f<<c.id<<' '<<c.league<<' '<<c.budget<<' '<<c.fans<<' '<<c.board<<' '<<c.prestige<<' '<<c.stadium<<' '<<c.academy<<' '<<c.facilities<<' '
         <<c.ticket<<' '<<c.sponsorship<<' '<<c.wins<<' '<<c.draws<<' '<<c.losses<<' '<<c.gf<<' '<<c.ga<<' '<<c.points<<' '<<c.formation<<' '<<c.tactic<<' '<<c.training<<' '<<c.captainId<<' '<<c.viceCaptainId<<'\n';
        putString(f,c.name);putString(f,c.city);
    }
    f<<players_.size()<<'\n';
    for(const auto& p:players_){
        f<<p.id<<' '<<p.club<<' '<<p.age<<' '<<p.potential<<' ';
        for(auto k:p.skill)f<<k<<' ';
        f<<p.morale<<' '<<p.fitness<<' '<<p.trust<<' '<<p.loyalty<<' '<<p.family<<' '<<p.wage<<' '<<p.value<<' '<<p.contractYear<<' '<<p.injured<<' '<<p.goals<<' '<<p.appearances<<' '<<p.yellow<<' '<<p.red<<' '<<p.starter<<' '<<p.trainingFocus<<' '
        <<p.releaseClause<<' '<<p.sellOnClub<<' '<<p.sellOnPct<<' '
        <<p.parentClub<<' '<<p.loanReturnDay<<' '<<p.loanWageShare<<' '
        <<p.careerAppearances<<' '<<p.developmentXP<<' '<<p.mentorId
        <<' '<<p.developedSkills<<' '<<p.academyGraduate<<'\n';
        putString(f,p.name);putString(f,p.position);putString(f,p.personality);
    }
    f<<matches_.size()<<'\n';
    for(const auto& m:matches_){
        f<<m.id<<' '<<m.season<<' '<<m.day<<' '<<m.home<<' '<<m.away<<' '<<m.homeGoals<<' '<<m.awayGoals<<' '<<m.played<<' '
         <<m.shotsH<<' '<<m.shotsA<<' '<<m.xgH<<' '<<m.xgA<<' '<<m.events.size()<<' '<<m.competition<<' '<<m.round<<' '<<m.penaltiesH<<' '<<m.penaltiesA<<'\n';
        for(const auto& e:m.events){f<<e.minute<<'\n';putString(f,e.text);}
    }
    f<<archive_.size()<<'\n';
    for(const auto& m:archive_){
        f<<m.id<<' '<<m.season<<' '<<m.day<<' '<<m.home<<' '<<m.away<<' '<<m.homeGoals<<' '<<m.awayGoals<<' '<<m.played<<' '
         <<m.shotsH<<' '<<m.shotsA<<' '<<m.xgH<<' '<<m.xgA<<' '<<m.events.size()<<' '<<m.competition<<' '<<m.round<<' '<<m.penaltiesH<<' '<<m.penaltiesA<<'\n';
        for(const auto& e:m.events){f<<e.minute<<'\n';putString(f,e.text);}
    }
    f<<inbox_.size()<<'\n';
    for(const auto& n:inbox_){f<<n.day<<'\n';putString(f,n.title);putString(f,n.body);}
    f<<live_.id<<' '<<live_.minute<<' '<<live_.goalsH<<' '<<live_.goalsA<<' '<<live_.shotsH<<' '<<live_.shotsA<<' '<<live_.xgH<<' '<<live_.xgA<<' '<<live_.events.size()<<'\n';
    for(const auto&e:live_.events){f<<e.minute<<'\n';putString(f,e.text);}
    f<<live_.subsH<<' '<<live_.subsA<<'\n';
    auto writeIds=[&](const std::vector<int>& ids){
        f<<ids.size();for(int id:ids)f<<' '<<id;f<<'\n';
    };
    writeIds(live_.homeXI);writeIds(live_.awayXI);
    writeIds(live_.usedHome);writeIds(live_.usedAway);
    f<<lastTeamTalkDay_<<' '<<scouts_.size()<<'\n';
    for(const auto& r:scouts_)f<<r.playerId<<' '<<r.days<<' '<<r.ability<<' '<<r.potential<<' '<<r.shortlisted<<'\n';
    f<<deals_.size()<<'\n';
    for(const auto& d:deals_)
        f<<d.playerId<<' '<<d.sellerClub<<' '<<d.fee<<' '<<d.askingFee<<' '
         <<d.wage<<' '<<d.askingWage<<' '<<d.years<<' '<<d.submittedDay<<' '
         <<d.replyDay<<' '<<d.expiryDay<<' '<<d.rivalClub<<' '<<d.rounds<<' '
         <<int(d.stage)<<' '<<d.kind<<' '<<d.installments<<' '
         <<d.appearanceBonus<<' '<<d.sellOnPct<<' '<<d.exchangePlayerId<<' '
         <<d.loanDays<<' '<<d.loanWageShare<<'\n';
    f<<payments_.size()<<'\n';
    for(const auto& p:payments_)f<<p.playerId<<' '<<p.buyerClub<<' '<<p.sellerClub
        <<' '<<p.amount<<' '<<p.dueDay<<' '<<p.triggerAppearances<<'\n';
    f<<nextStaffId_<<' '<<nextProspectId_<<' '<<lastYouthSearchDay_<<'\n';
    f<<staff_.size()<<'\n';
    for(const auto& p:staff_){
        f<<p.id<<' '<<p.club<<' '<<p.role<<' '<<p.quality
         <<' '<<p.weeklyWage<<' '<<p.contractYear<<'\n';
        putString(f,p.name);
    }
    f<<youth_.size()<<'\n';
    for(const auto& p:youth_){
        f<<p.id<<' '<<p.club<<' '<<p.age<<' '<<p.ability
         <<' '<<p.potential<<'\n';
        putString(f,p.name);putString(f,p.position);
    }
    f<<relations_.size()<<'\n';
    for(const auto& p:relations_)f<<p.first<<' '<<p.second<<' '
        <<p.affinity<<' '<<p.lastInteractionDay<<'\n';
    f<<cups_.size()<<'\n';
    for(const auto& cup:cups_){
        f<<cup.id<<' '<<cup.season<<' '<<cup.leagueId<<' '<<cup.round<<' '
         <<cup.champion<<' '<<cup.byeA<<' '<<cup.byeB<<'\n';
        putString(f,cup.name);
    }
    f<<trophies_.size()<<'\n';
    for(const auto& t:trophies_)
        f<<t.season<<' '<<t.cupId<<' '<<t.winner<<' '<<t.runnerUp<<'\n';
    f<<leagueHistory_.size()<<'\n';
    for(const auto& r:leagueHistory_)
        f<<r.season<<' '<<r.leagueId<<' '<<r.winner<<' '<<r.runnerUp<<' '<<r.points<<'\n';
    return f.good()?"Карьера сохранена: "+path:"Ошибка записи сохранения";
}
std::string Game::load(const std::string& path){
    std::ifstream f(path,std::ios::binary);
    if(!f)return "Файл сохранения не найден: "+path;
    std::string header;std::getline(f,header);
    if(header!="FUSAVE_3"&&header!="FUSAVE_4"&&header!="FUSAVE_5"&&header!="FUSAVE_6"&&header!="FUSAVE_7"&&header!="FUSAVE_8"&&header!="FUSAVE_9")return "Неверный формат или версия сохранения";
    const bool legacy=(header=="FUSAVE_3");
    const bool version5=(header=="FUSAVE_5"||header=="FUSAVE_6"||header=="FUSAVE_7"||header=="FUSAVE_8"||header=="FUSAVE_9");
    const bool version6=(header=="FUSAVE_6"||header=="FUSAVE_7"||header=="FUSAVE_8"||header=="FUSAVE_9");
    const bool version7=(header=="FUSAVE_7"||header=="FUSAVE_8"||header=="FUSAVE_9");
    const bool version8=(header=="FUSAVE_8"||header=="FUSAVE_9");
    const bool version9=(header=="FUSAVE_9");
    Game loaded;
    if(!getString(f,loaded.manager_)||!getString(f,loaded.status_))return "Повреждённое сохранение";
    int role=0,diff=0;
    if(!(f>>loaded.teamId_>>loaded.day_>>loaded.season_>>loaded.totalDays_>>loaded.nextPlayerId_>>role>>diff))return "Повреждены данные карьеры";
    if(role<0||role>4||diff<0||diff>3)return "Некорректная сложность или должность";
    loaded.role_=Role(role);loaded.difficulty_=Difficulty(diff);
    std::string state;if(!getString(f,state))return "Повреждено состояние генератора";
    std::istringstream randomState(state);if(!(randomState>>loaded.rng_))return "Ошибка генератора мира";
    auto size=[&](size_t limit)->size_t{size_t n=0;if(!(f>>n)||n>limit)throw std::runtime_error("Некорректное число записей");return n;};
    try{
        loaded.leagues_.clear();loaded.clubs_.clear();loaded.players_.clear();loaded.matches_.clear();loaded.archive_.clear();loaded.cups_.clear();loaded.trophies_.clear();loaded.leagueHistory_.clear();loaded.inbox_.clear();loaded.scouts_.clear();loaded.deals_.clear();loaded.payments_.clear();loaded.staff_.clear();loaded.youth_.clear();loaded.relations_.clear();
        for(size_t i=0,n=size(300);i<n;++i){League l;if(!(f>>l.id>>l.active)||!getString(f,l.country)||!getString(f,l.name))throw std::runtime_error("Лиги");loaded.leagues_.push_back(l);}
        for(size_t i=0,n=size(100000);i<n;++i){
            Club c;if(!(f>>c.id>>c.league>>c.budget>>c.fans>>c.board>>c.prestige>>c.stadium>>c.academy>>c.facilities
                >>c.ticket>>c.sponsorship>>c.wins>>c.draws>>c.losses>>c.gf>>c.ga>>c.points>>c.formation>>c.tactic>>c.training))
                throw std::runtime_error("Клубы");
            if(version5&&!(f>>c.captainId>>c.viceCaptainId))throw std::runtime_error("Руководство раздевалки");
            if(!getString(f,c.name)||!getString(f,c.city))throw std::runtime_error("Названия клубов");
            loaded.clubs_.push_back(c);
        }
        for(size_t i=0,n=size(1000000);i<n;++i){
            Player p;if(!(f>>p.id>>p.club>>p.age>>p.potential))throw std::runtime_error("Игроки");
            for(int& v:p.skill)if(!(f>>v))throw std::runtime_error("Навыки");
            if(!(f>>p.morale>>p.fitness>>p.trust>>p.loyalty>>p.family>>p.wage>>p.value>>p.contractYear>>p.injured
                >>p.goals>>p.appearances>>p.yellow>>p.red))throw std::runtime_error("Контракты");
            if(!legacy){
                if(!(f>>p.starter>>p.trainingFocus)||p.trainingFocus < -1||p.trainingFocus>9)
                    throw std::runtime_error("Стартовый состав и тренировки");
            }
            if(version7){
                if(!(f>>p.releaseClause>>p.sellOnClub>>p.sellOnPct>>p.parentClub>>
                       p.loanReturnDay>>p.loanWageShare>>p.careerAppearances)||
                   p.releaseClause<0||p.releaseClause>500000000||
                   p.sellOnPct<0||p.sellOnPct>40||
                   p.sellOnClub<-1||p.sellOnClub>=int(loaded.clubs_.size())||
                   p.parentClub<-1||p.parentClub>=int(loaded.clubs_.size())||
                   p.loanWageShare<0||p.loanWageShare>100||
                   p.careerAppearances<0||
                   (p.parentClub<0&&(p.loanReturnDay!=-1||p.loanWageShare!=100))||
                   (p.parentClub>=0&&(p.parentClub==p.club||p.loanReturnDay<loaded.totalDays_)))
                   throw std::runtime_error("Условия контракта или аренды");
            }else p.careerAppearances=p.appearances;
            if(version8){
                if(!(f>>p.developmentXP>>p.mentorId>>p.developedSkills>>p.academyGraduate)||
                   p.developmentXP<0||p.developmentXP>10000||p.mentorId<-1||
                   p.developedSkills<0||p.developedSkills>100000)
                    throw std::runtime_error("Прогресс развития");
            }
            if(!getString(f,p.name)||!getString(f,p.position)||!getString(f,p.personality))
                throw std::runtime_error("Данные игрока");
            loaded.players_.push_back(std::move(p));
        }
        for(size_t i=0,n=size(1000000);i<n;++i){
            Match m;size_t count=0;
            if(!(f>>m.id>>m.season>>m.day>>m.home>>m.away>>m.homeGoals>>m.awayGoals>>m.played>>m.shotsH>>m.shotsA>>m.xgH>>m.xgA>>count)||count>2000)throw std::runtime_error("Матчи");
            if(version9&&(!(f>>m.competition>>m.round>>m.penaltiesH>>m.penaltiesA)||
               m.competition < -1||m.competition>6||m.round<0||m.round>2||
               ((m.penaltiesH<0)!=(m.penaltiesA<0))||
               (m.competition<0&&(m.penaltiesH>=0||m.penaltiesA>=0))||
               (m.competition>=0&&m.played&&m.homeGoals==m.awayGoals&&m.penaltiesH==m.penaltiesA)))
                throw std::runtime_error("Данные кубкового матча");
            for(size_t j=0;j<count;++j){Event e;if(!(f>>e.minute)||!getString(f,e.text))throw std::runtime_error("События матча");m.events.push_back(std::move(e));}
            loaded.matches_.push_back(std::move(m));
        }
        for(size_t i=0,n=size(1000000);i<n;++i){
            Match m;size_t count=0;
            if(!(f>>m.id>>m.season>>m.day>>m.home>>m.away>>m.homeGoals>>m.awayGoals>>m.played>>m.shotsH>>m.shotsA>>m.xgH>>m.xgA>>count)||count>2000)throw std::runtime_error("Архив");
            if(version9&&(!(f>>m.competition>>m.round>>m.penaltiesH>>m.penaltiesA)||
               m.competition < -1||m.competition>6||m.round<0||m.round>2||
               ((m.penaltiesH<0)!=(m.penaltiesA<0))||
               (m.competition<0&&(m.penaltiesH>=0||m.penaltiesA>=0))))
                throw std::runtime_error("Кубковый архив");
            for(size_t j=0;j<count;++j){Event e;if(!(f>>e.minute)||!getString(f,e.text))throw std::runtime_error("Архив матчей");m.events.push_back(std::move(e));}
            loaded.archive_.push_back(std::move(m));
        }
        for(size_t i=0,n=size(100000);i<n;++i){Message m;if(!(f>>m.day)||!getString(f,m.title)||!getString(f,m.body))throw std::runtime_error("Почта");loaded.inbox_.push_back(std::move(m));}
        size_t n=0;
        if(!(f>>loaded.live_.id>>loaded.live_.minute>>loaded.live_.goalsH>>loaded.live_.goalsA>>loaded.live_.shotsH>>loaded.live_.shotsA>>loaded.live_.xgH>>loaded.live_.xgA>>n)||n>2000)throw std::runtime_error("Трансляция");
        for(size_t i=0;i<n;++i){Event e;if(!(f>>e.minute)||!getString(f,e.text))throw std::runtime_error("Трансляция");loaded.live_.events.push_back(std::move(e));}
        if(!legacy){
            if(!(f>>loaded.live_.subsH>>loaded.live_.subsA)||loaded.live_.subsH<0||loaded.live_.subsH>5||loaded.live_.subsA<0||loaded.live_.subsA>5)
                throw std::runtime_error("Лимит замен");
            auto readIds=[&](std::vector<int>& out){
                auto count=size(40);
                for(size_t i=0;i<count;++i){int id=0;if(!(f>>id))throw std::runtime_error("Участники матча");out.push_back(id);}
            };
            readIds(loaded.live_.homeXI);readIds(loaded.live_.awayXI);
            readIds(loaded.live_.usedHome);readIds(loaded.live_.usedAway);
        }
        if(version5){
            if(!(f>>loaded.lastTeamTalkDay_))throw std::runtime_error("Собрания");
            for(size_t i=0,n=size(1000000);i<n;++i){
                ScoutReport report;
                if(!(f>>report.playerId>>report.days>>report.ability>>report.potential>>report.shortlisted)||
                   report.days<0||report.days>5||report.ability<0||report.ability>100||
                   report.potential<0||report.potential>100)
                    throw std::runtime_error("Скаутские отчёты");
                loaded.scouts_.push_back(report);
            }
        }
        if(version6){
            for(size_t i=0,n=size(100000);i<n;++i){
                TransferDeal d;
                int stage=0;
                if(!(f>>d.playerId>>d.sellerClub>>d.fee>>d.askingFee
                    >>d.wage>>d.askingWage>>d.years>>d.submittedDay
                    >>d.replyDay>>d.expiryDay>>d.rivalClub>>d.rounds>>stage)||
                    stage<0||stage>8||d.fee<0||d.fee>500000000||
                    d.askingFee<0||d.askingFee>500000000||
                    d.wage<0||d.wage>20000000||d.askingWage<0||d.askingWage>20000000||
                    d.years<1||d.years>7||d.rounds<0||d.rounds>30||
                    d.submittedDay<0||d.replyDay<0||d.expiryDay<d.submittedDay||
                    d.sellerClub<0||d.sellerClub>=int(loaded.clubs_.size())||
                    d.rivalClub>=int(loaded.clubs_.size())||d.rivalClub < -1)
                    throw std::runtime_error("Трансферные переговоры");
                if(version7){
                    if(!(f>>d.kind>>d.installments>>d.appearanceBonus>>d.sellOnPct>>
                          d.exchangePlayerId>>d.loanDays>>d.loanWageShare)||
                       (d.kind!=0&&d.kind!=1)||d.installments<1||d.installments>12||
                       d.appearanceBonus<0||d.appearanceBonus>50000000||
                       d.sellOnPct<0||d.sellOnPct>40||
                       d.exchangePlayerId<-1||
                       d.loanDays<30||d.loanDays>330||
                       d.loanWageShare<0||d.loanWageShare>100)
                        throw std::runtime_error("Дополнительные условия сделки");
                    if(stage<=4&&d.exchangePlayerId>=0&&!loaded.findPlayer(d.exchangePlayerId))
                        throw std::runtime_error("Игрок для обмена отсутствует");
                }
                d.stage=DealStage(stage);
                loaded.deals_.push_back(d);
            }
        }
        if(version7){
            for(size_t i=0,n=size(100000);i<n;++i){
                DeferredPayment payment;
                if(!(f>>payment.playerId>>payment.buyerClub>>payment.sellerClub>>
                      payment.amount>>payment.dueDay>>payment.triggerAppearances)||
                    (payment.triggerAppearances>0&&!loaded.findPlayer(payment.playerId))||
                    payment.buyerClub<0||payment.buyerClub>=int(loaded.clubs_.size())||
                    payment.sellerClub<0||payment.sellerClub>=int(loaded.clubs_.size())||
                    payment.amount<0||payment.amount>500000000||
                    ((payment.triggerAppearances==0&&payment.dueDay<loaded.totalDays_)||
                     (payment.triggerAppearances>0&&payment.dueDay!=-1)))
                    throw std::runtime_error("Отложенные платежи");
                loaded.payments_.push_back(payment);
            }
        }
        if(version8){
            if(!(f>>loaded.nextStaffId_>>loaded.nextProspectId_>>loaded.lastYouthSearchDay_)||
               loaded.nextStaffId_<1||loaded.nextProspectId_<1||
               loaded.lastYouthSearchDay_>loaded.totalDays_)
                throw std::runtime_error("Молодёжный штаб");
            for(size_t i=0,n=size(10000);i<n;++i){
                StaffMember p;
                if(!(f>>p.id>>p.club>>p.role>>p.quality>>p.weeklyWage>>p.contractYear)||
                   p.id<1||p.id>=loaded.nextStaffId_||
                   p.club<0||p.club>=int(loaded.clubs_.size())||
                   p.role<0||p.role>3||p.quality<1||p.quality>100||
                   p.weeklyWage<0||p.weeklyWage>20000000||
                   !getString(f,p.name))
                    throw std::runtime_error("Персонал клуба");
                loaded.staff_.push_back(std::move(p));
            }
            for(size_t i=0,n=size(10000);i<n;++i){
                YouthProspect p;
                if(!(f>>p.id>>p.club>>p.age>>p.ability>>p.potential)||
                   p.id<1||p.id>=loaded.nextProspectId_||
                   p.club<0||p.club>=int(loaded.clubs_.size())||
                   p.age<15||p.age>20||p.ability<1||p.ability>100||
                   p.potential<1||p.potential>100||
                   !getString(f,p.name)||!getString(f,p.position))
                    throw std::runtime_error("Кандидаты академии");
                loaded.youth_.push_back(std::move(p));
            }
            for(size_t i=0,n=size(100000);i<n;++i){
                Relationship p;
                if(!(f>>p.first>>p.second>>p.affinity>>p.lastInteractionDay)||
                   p.first<1||p.second<1||p.first>=p.second||
                   p.affinity< -100||p.affinity>100||
                   p.lastInteractionDay>loaded.totalDays_)
                    throw std::runtime_error("Отношения футболистов");
                const Player* first=loaded.findPlayer(p.first);
                const Player* second=loaded.findPlayer(p.second);
                if(!first||!second||first->club!=second->club)
                    throw std::runtime_error("Некорректные отношения");
                loaded.relations_.push_back(p);
            }
            for(const auto& p:loaded.players_)if(p.mentorId>=0){
                const Player* mentor=loaded.findPlayer(p.mentorId);
                if(!mentor||mentor->club!=p.club||mentor->age<25||p.age>23||mentor->id==p.id)
                    throw std::runtime_error("Некорректное наставничество");
            }
        }else{
            loaded.seedStaff();
            loaded.generateYouth(loaded.teamId_,4+loaded.myClub().academy/3);
        }
        if(version9){
            for(size_t i=0,n=size(20);i<n;++i){
                CupCompetition c;
                if(!(f>>c.id>>c.season>>c.leagueId>>c.round>>c.champion>>c.byeA>>c.byeB)||
                    c.id!=int(i)||c.season!=loaded.season_||
                    c.leagueId < -1||c.leagueId>=int(loaded.leagues_.size())||
                    c.round<0||c.round>2||
                    c.champion < -1||c.champion>=int(loaded.clubs_.size())||
                    c.byeA < -1||c.byeA>=int(loaded.clubs_.size())||
                    c.byeB < -1||c.byeB>=int(loaded.clubs_.size())||
                    !getString(f,c.name))
                    throw std::runtime_error("Кубковые соревнования");
                loaded.cups_.push_back(std::move(c));
            }
            for(size_t i=0,n=size(100000);i<n;++i){
                TrophyRecord t;
                if(!(f>>t.season>>t.cupId>>t.winner>>t.runnerUp)||
                    t.season<2026||t.season>loaded.season_||t.cupId<0||t.cupId>6||
                    t.winner<0||t.winner>=int(loaded.clubs_.size())||
                    t.runnerUp<0||t.runnerUp>=int(loaded.clubs_.size())||
                    t.runnerUp==t.winner)
                    throw std::runtime_error("История кубковых чемпионов");
                loaded.trophies_.push_back(t);
            }
            for(size_t i=0,n=size(100000);i<n;++i){
                LeagueRecord h;
                if(!(f>>h.season>>h.leagueId>>h.winner>>h.runnerUp>>h.points)||
                    h.season<2026||h.season>=loaded.season_||
                    h.leagueId<0||h.leagueId>=int(loaded.leagues_.size())||
                    h.winner<0||h.winner>=int(loaded.clubs_.size())||
                    h.runnerUp<0||h.runnerUp>=int(loaded.clubs_.size())||
                    h.winner==h.runnerUp||h.points<0)
                    throw std::runtime_error("История чемпионатов");
                loaded.leagueHistory_.push_back(h);
            }
            for(const auto& m:loaded.matches_)if(m.competition>=int(loaded.cups_.size()))
                throw std::runtime_error("Турнир матча не найден");
        }else{
            // Old career migration: start cups now only if the first cup day
            // has not passed. Otherwise start them next season.
            if(loaded.day_<20)loaded.createCups();
        }
        if(loaded.teamId_<0||loaded.teamId_>=int(loaded.clubs_.size()))throw std::runtime_error("Ваш клуб");
        for(const auto& c:loaded.clubs_)if(c.id<0||c.id>=int(loaded.clubs_.size())||c.league<0||c.league>=int(loaded.leagues_.size()))throw std::runtime_error("Ссылки клубов");
        for(const auto& p:loaded.players_)if(p.club<0||p.club>=int(loaded.clubs_.size()))throw std::runtime_error("Ссылки игроков");
        for(const auto& m:loaded.matches_)if(m.home<0||m.home>=int(loaded.clubs_.size())||m.away<0||m.away>=int(loaded.clubs_.size())||m.id<0||m.id>=int(loaded.matches_.size()))throw std::runtime_error("Ссылки матчей");
        if(loaded.live_.id < -1||loaded.live_.id>=int(loaded.matches_.size()))throw std::runtime_error("Ссылки трансляции");
        if(!version5)loaded.autoAssignCaptains();
        else for(const auto& c:loaded.clubs_){
            const Player* captain=loaded.findPlayer(c.captainId);
            const Player* vice=loaded.findPlayer(c.viceCaptainId);
            if((c.captainId>=0&&(!captain||captain->club!=c.id))||
               (c.viceCaptainId>=0&&(!vice||vice->club!=c.id))||
               (c.captainId>=0&&c.captainId==c.viceCaptainId))
                throw std::runtime_error("Капитанский состав");
        }
        for(const auto& r:loaded.scouts_)
            if(!loaded.findPlayer(r.playerId))throw std::runtime_error("Игрок из скаутского отчёта");
        for(size_t i=0;i<loaded.deals_.size();++i){
            const auto& deal=loaded.deals_[i];
            const Player* p=loaded.findPlayer(deal.playerId);
            if(int(deal.stage)<=4&&(!p||p->club!=deal.sellerClub))
                throw std::runtime_error("Активная сделка с недоступным игроком");
            if(int(deal.stage)<=4){
                for(size_t j=0;j<i;++j)
                    if(loaded.deals_[j].playerId==deal.playerId&&int(loaded.deals_[j].stage)<=4)
                        throw std::runtime_error("Повторяющиеся активные переговоры");
            }
        }
        if(legacy){
            for(const auto& c:loaded.clubs_)loaded.pickLineup(c.id);
            if(loaded.live_.id>=0)loaded.initializeLineups(loaded.live_,loaded.matches_[loaded.live_.id]);
        }else{
            auto validIds=[&](const std::vector<int>& ids,int cid){
                for(int id:ids){const Player* p=loaded.findPlayer(id);if(!p||p->club!=cid)return false;}
                return true;
            };
            if(loaded.live_.id>=0){
                const auto& m=loaded.matches_[loaded.live_.id];
                if(!validIds(loaded.live_.homeXI,m.home)||!validIds(loaded.live_.awayXI,m.away)||
                   !validIds(loaded.live_.usedHome,m.home)||!validIds(loaded.live_.usedAway,m.away))
                    throw std::runtime_error("Неверный состав матча");
            }
        }
    }catch(const std::exception&e){return std::string("Не удалось загрузить: ")+e.what();}
    *this=std::move(loaded);
    status_="Карьера загружена: "+path;
    return status_;
}
bool Game::importPlayers(const std::string& path){
    std::ifstream f(path);if(!f)return false;
    std::string line;int imported=0;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#'||line.find("club_id;")==0)continue;
        std::vector<std::string> v;std::stringstream s(line);std::string token;
        while(std::getline(s,token,';'))v.push_back(token);
        if(v.size()<15)continue;
        try{
            int club=std::stoi(v[0]);if(club<0||club>=int(clubs_.size()))continue;
            int age=std::stoi(v[3]);if(age<15||age>50)continue;
            Player p;p.id=nextPlayerId_++;p.club=club;p.name=v[1];p.position=v[2];p.age=age;
            for(int k=0;k<10;++k)p.skill[k]=clamp(std::stoi(v[4+k]));
            p.potential=clamp(std::stoi(v[14]));
            p.personality="Профессионал";p.wage=4000;p.value=1000000;p.contractYear=season_+3;
            // Replace one procedurally generated player in that club with the same position if available.
            auto it=std::find_if(players_.begin(),players_.end(),[&](const Player& q){return q.club==club&&q.position==p.position;});
            if(it!=players_.end())players_.erase(it);
            players_.push_back(std::move(p));imported++;
        }catch(...){continue;}
    }
    for(const auto& c:clubs_)pickLineup(c.id);
    status_="Импортировано футболистов: "+std::to_string(imported);
    if(imported>0)addMessage("База данных обновлена",status_);
    return imported>0;
}
} // namespace fu
