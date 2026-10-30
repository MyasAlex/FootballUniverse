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
    leagues_.clear();clubs_.clear();players_.clear();matches_.clear();archive_.clear();inbox_.clear();live_={};
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
        leagues_.push_back({lid,country,league,false});
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
    return n?clamp(total/n-(11-n)*4,5,99):25;
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
void Game::createFixtures(){
    matches_.clear();int id=0;
    for(const auto& l:leagues_){
       auto clubs=leagueClubs(l.id);
       if(clubs.size()%2)clubs.push_back(-1);
       const int n=int(clubs.size());
       std::vector<std::pair<int,int>> round;
       int roundNum=0;
       for(int rep=0;rep<3;++rep){
           std::vector<int> rotation=clubs;
           for(int leg=0;leg<2;++leg){
               for(int r=0;r<n-1;++r){
                   for(int k=0;k<n/2;++k){
                       int a=rotation[k],b=rotation[n-1-k];if(a<0||b<0)continue;
                       if((r+leg)%2)std::swap(a,b);
                       Match m;m.id=id++;m.season=season_;m.day=3+10*roundNum;m.home=a;m.away=b;
                       matches_.push_back(m);
                   }
                   int last=rotation.back();
                   for(int j=n-1;j>1;--j)rotation[j]=rotation[j-1];
                   rotation[1]=last;
                   ++roundNum;
               }
               std::reverse(rotation.begin()+1,rotation.end());
           }
       }
    }
    std::stable_sort(matches_.begin(),matches_.end(),[](const Match&a,const Match&b){return std::tie(a.day,a.id)<std::tie(b.day,b.id);});
    for(size_t i=0;i<matches_.size();++i)matches_[i].id=int(i);
}
void Game::newCareer(std::string manager,int clubId,Role role,Difficulty difficulty,const std::vector<int>& activeLeagues){
    rng_.seed(std::random_device{}());season_=2026;day_=0;totalDays_=0;manager_=std::move(manager);role_=role;difficulty_=difficulty;
    seedWorld();
    if(clubId<0||clubId>=int(clubs_.size()))clubId=0;
    teamId_=clubId;
    for(auto& l:leagues_)l.active=std::find(activeLeagues.begin(),activeLeagues.end(),l.id)!=activeLeagues.end();
    leagues_[clubs_[teamId_].league].active=true;
    createSquads();for(const auto& c:clubs_)pickLineup(c.id);createFixtures();
    addMessage("Добро пожаловать в клуб","Сезон 2026/27 начался. Режим карьеры: собственная футбольная вселенная. База клубов демонстрационная, составы процедурные. Активные лиги симулируются подробно.");
    addMessage("Первые задачи","Изучите состав, настройте тактику, поговорите с футболистами и подготовьтесь к первому туру. Следующий день — на главной странице.");
    status_="Новая карьера создана";
}
const Club& Game::myClub()const{return clubs_.at(teamId_);} Club& Game::myClub(){return clubs_.at(teamId_);}
const Player* Game::findPlayer(int id)const{for(const auto& p:players_)if(p.id==id)return &p;return nullptr;}
const Club* Game::findClub(int id)const{return id>=0&&id<int(clubs_.size())?&clubs_[id]:nullptr;}
std::vector<int> Game::ownPlayers() const {std::vector<int> r;for(const auto& p:players_)if(p.club==teamId_)r.push_back(p.id);return r;}
std::vector<int> Game::transferCandidates() const {
    std::vector<int> r;
    for(const auto& p:players_)if(p.club!=teamId_&&leagues_[clubs_[p.club].league].active)r.push_back(p.id);
    std::sort(r.begin(),r.end(),[this](int a,int b){return findPlayer(a)->value>findPlayer(b)->value;});
    if(r.size()>100)r.resize(100);
    return r;
}
std::string Game::dateString()const{
    using namespace std::chrono;
    auto date=sys_days(year{season_}/7/1)+days{day_};
    year_month_day d{date};
    std::ostringstream s;s<<int(d.year())<<'-'<<std::setfill('0')<<std::setw(2)<<unsigned(d.month())<<'-'<<std::setw(2)<<unsigned(d.day());
    return s.str();
}
const Match* Game::upcomingMatch()const{
    for(const auto& m:matches_)if(!m.played&&(m.home==teamId_||m.away==teamId_))return &m;
    return nullptr;
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
    int injured=0,unhappy=0;for(const auto& p:players_)if(p.club==teamId_){injured+=p.injured>0;unhappy+=p.morale<45;}
    o<<"Травмировано: "<<injured<<"\nНедовольны: "<<unhappy<<"\n";
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
    o<<"Доверие фанатов: "<<pct(myClub().fans)<<"\n";
    o<<"Доверие руководства: "<<pct(myClub().board)<<"\n";
    o<<"Подготовка команды: "<<tactics[myClub().tactic]<<" / "<<formations[myClub().formation]<<"\n\n";
    if(auto m=upcomingMatch()){
        o<<"СЛЕДУЮЩИЙ МАТЧ\n";
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
    o<<"Сила команды: "<<rating(c)<<" (с учётом доступных игроков)\n\n";
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
    o<<"Доходы зависят от посещаемости, результатов и\nотношений с болельщиками.\n\n";
    o<<"Улучшить стадион: 3 млн €\nУлучшить базу: 2 млн €\nУлучшить академию: 2.5 млн €\n";
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
    return "FOOTBALL UNIVERSE 0.3 / ПРОТОТИП\n\n"
    "Это самостоятельная C++-игра с текстовым и 2D-отображением матча.\n"
    "Создайте карьеру, выберите должность, сложность, клуб и активные лиги.\n"
    "Меню слева открывает все игровые системы.\n\n"
    "Состав: выберите запасного, впишите ID игрока основы, нажмите «В основу».\n"
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
void Game::dailyTick(){
    auto& c=myClub();
    for(auto& p:players_){
        if(p.injured>0){--p.injured;p.fitness=clamp(p.fitness+2);continue;}
        if(p.club!=teamId_){p.fitness=clamp(p.fitness+1);continue;}
        int training=c.training;
        if(training==0)p.fitness=clamp(p.fitness+5);
        if(training==1){p.fitness=clamp(p.fitness+2);if(random(0,40)==0)p.skill[random(0,9)]=clamp(p.skill[random(0,9)]+1);}
        if(training==2){p.fitness=clamp(p.fitness-2);if(random(0,16)==0){int k=random(0,8);p.skill[k]=clamp(p.skill[k]+1);}}
        if(training==3){p.fitness=clamp(p.fitness);if(random(0,20)==0)p.skill[4]=clamp(p.skill[4]+1);}
        if(training==4){p.fitness=clamp(p.fitness+1);if(random(0,21)==0)p.skill[8]=clamp(p.skill[8]+1);}
        if(p.trainingFocus>=0&&p.trainingFocus<10){
            const int odds=(p.age<=23?11:p.age<=29?21:34);
            if(p.skill[p.trainingFocus]<p.potential&&random(1,odds)==1){
                p.skill[p.trainingFocus]=clamp(p.skill[p.trainingFocus]+1);
                p.fitness=clamp(p.fitness-1);
            }
        }
        if(training==2&&p.fitness<45&&random(0,300)==0){p.injured=random(4,16);p.starter=false;addMessage("Травма на тренировке",p.name+" выбыл на "+std::to_string(p.injured)+" дней.");pickLineup(teamId_);}
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
       int wages=0;for(const auto& p:players_)if(p.club==teamId_)wages+=p.wage*4;
       c.budget-=wages;
       addMessage("Финансовый отчёт","Ежемесячная выплата зарплат: "+money(wages)+". Остаток бюджета: "+money(c.budget)+".");
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
    int old=season_;
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
    for(const auto& m:matches_)if(m.played)archive_.push_back(m);
    for(const auto& c:clubs_)pickLineup(c.id);
    createFixtures();live_={};
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
    h.gf+=live.goalsH;h.ga+=live.goalsA;a.gf+=live.goalsA;a.ga+=live.goalsH;
    if(live.goalsH>live.goalsA){h.wins++;a.losses++;h.points+=3;}
    else if(live.goalsH<live.goalsA){a.wins++;h.losses++;a.points+=3;}
    else {h.draws++;a.draws++;h.points++;a.points++;}
    std::unordered_set<int> playedHome(live.usedHome.begin(),live.usedHome.end());
    std::unordered_set<int> playedAway(live.usedAway.begin(),live.usedAway.end());
    for(auto& p:players_){
        bool wasHome=playedHome.contains(p.id),wasAway=playedAway.contains(p.id);
        if(!wasHome&&!wasAway)continue;
        ++p.appearances;
        p.fitness=clamp(p.fitness-random(5,11));
        int diff=wasHome?live.goalsH-live.goalsA:live.goalsA-live.goalsH;
        p.morale=clamp(p.morale+std::clamp(diff,-2,2));
    }
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
        o<<"ИДЁТ МАТЧ • "<<l.minute<<"'\n\n";
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
        auto m=upcomingMatch();o<<"СЕГОДНЯ ИГРА\n\n"<<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n\n";
        o<<"Нажмите «Начать матч» для трансляции по минутам\nили «Мгновенно» для немедленного результата.\n";
        o<<"В интерактивном матче доступны замены и тактические решения.\n";
    }else if(auto m=upcomingMatch()){
        o<<"СЛЕДУЮЩИЙ МАТЧ\n\n"<<clubs_[m->home].name<<" — "<<clubs_[m->away].name<<"\n";
        o<<"До матча: "<<std::max(0,m->day-day_)<<" дн.\n\n";
        o<<"До игрового дня можно переключиться через «Следующий день».\n";
    }else o<<"Матчей в текущем календаре больше нет.\n";
    return o.str();
}
std::string Game::matchArchive()const{
    std::ostringstream o;o<<"АРХИВ МАТЧЕЙ СЕЗОНА "<<season_<<"/"<<(season_+1)%100<<"\n\n";
    int count=0;
    for(const auto& m:matches_)if(m.played&&(m.home==teamId_||m.away==teamId_)){
        o<<clubs_[m.home].name<<"  "<<m.homeGoals<<":"<<m.awayGoals<<"  "<<clubs_[m.away].name<<"\n";
        o<<"Удары: "<<m.shotsH<<" / "<<m.shotsA<<";  xG: "<<m.xgH/100.0<<" / "<<m.xgA/100.0<<"\n";
        for(const auto& e:m.events)o<<" "<<e.minute<<"' "<<e.text<<"\n";
        o<<"\n";count++;
    }
    o<<"\nПРЕДЫДУЩИЕ СЕЗОНЫ (ПОСЛЕДНИЕ 25 МАТЧЕЙ)\n\n";
    int previous=0;
    for(auto it=archive_.rbegin();it!=archive_.rend()&&previous<25;++it){
        const auto& m=*it;
        if(m.home!=teamId_&&m.away!=teamId_)continue;
        o<<m.season<<"/"<<(m.season+1)%100<<" "<<clubs_[m.home].name<<" "<<m.homeGoals<<":"<<m.awayGoals<<" "<<clubs_[m.away].name<<"\n";
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
std::string Game::transferBid(int playerId,int offered){
    if(!permissionSports()){status_="Нужны полномочия спортивного руководства.";return status_;}
    if(hasLiveMatch())return status_="Нельзя совершать трансферы во время матча.";
    Player* p=nullptr;for(auto& i:players_)if(i.id==playerId)p=&i;
    if(!p||p->club==teamId_){status_="Неверно выбран кандидат.";return status_;}
    if(offered<=0){status_="Предложите положительную сумму.";return status_;}
    if(!canFinance(offered)){status_="Не хватает бюджета для предложения.";return status_;}
    int minPrice=p->value*(difficulty_==Difficulty::Hard?130:difficulty_==Difficulty::Easy?85:100)/100;
    int sellerPrestige=clubs_[p->club].prestige;
    int interest=40+(myClub().prestige-sellerPrestige)/2+(offered-minPrice)/std::max(100000,p->value/55)+p->trust/12;
    bool accepted=offered>=minPrice&&random(1,100)<=clamp(interest,15,95);
    std::string old=clubs_[p->club].name;
    if(accepted){
        myClub().budget-=offered;
        clubs_[p->club].budget+=offered;
        int priorClub=p->club;
        p->club=teamId_;p->starter=false;p->contractYear=season_+3;p->morale=clamp(p->morale+7);p->trust=50;
        pickLineup(priorClub);pickLineup(teamId_);
        status_="Трансфер состоялся: "+p->name+" из "+old+" за "+money(offered);
        addMessage("Сделка завершена",status_);
    }else{
        status_="Предложение по "+p->name+" отклонено. Ориентир цены: "+money(minPrice);
        if(random(1,100)<30)addMessage("Трансферные слухи","Пресса узнала о неудачных переговорах с "+old+".");
    }
    return status_;
}
std::string Game::renewContract(int playerId,int wage,int years){
    if(!permissionSports()){status_="Нет полномочий на продление контракта.";return status_;}
    Player* p=nullptr;for(auto& i:players_)if(i.id==playerId)p=&i;
    if(!p||p->club!=teamId_){status_="Выберите игрока своего клуба.";return status_;}
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
    if(!permissionSports()){status_="Нет полномочий академии.";return status_;}
    const int cost=350000;
    if(!canFinance(cost)){status_="На набор молодёжи требуется "+money(cost);return status_;}
    myClub().budget-=cost;
    Player p;p.id=nextPlayerId_++;p.club=teamId_;p.age=random(16,18);
    p.name=firstNames[random(0,int(firstNames.size()-1))]+" "+lastNames[random(0,int(lastNames.size()-1))];
    p.position=positions[random(0,int(positions.size()-1))];p.personality=character[random(0,int(character.size()-1))];
    int base=clamp(31+myClub().academy*3+random(-12,13),20,75);
    for(int& skill:p.skill)skill=clamp(base+random(-14,14));
    p.potential=clamp(base+myClub().academy*2+random(11,31));
    p.wage=1500;p.value=120000+base*18000;p.contractYear=season_+4;
    players_.push_back(p);
    status_="Из академии приглашён "+p.name+", "+std::to_string(p.age)+" лет, потенциал "+std::to_string(p.potential)+"/100.";
    addMessage("Молодой талант",status_);
    return status_;
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
    f<<"FUSAVE_4\n";
    putString(f,manager_);putString(f,status_);
    f<<teamId_<<' '<<day_<<' '<<season_<<' '<<totalDays_<<' '<<nextPlayerId_<<' '<<int(role_)<<' '<<int(difficulty_)<<'\n';
    std::ostringstream rand;rand<<rng_;putString(f,rand.str());
    f<<leagues_.size()<<'\n';
    for(const auto& l:leagues_){f<<l.id<<' '<<l.active<<'\n';putString(f,l.country);putString(f,l.name);}
    f<<clubs_.size()<<'\n';
    for(const auto& c:clubs_){
        f<<c.id<<' '<<c.league<<' '<<c.budget<<' '<<c.fans<<' '<<c.board<<' '<<c.prestige<<' '<<c.stadium<<' '<<c.academy<<' '<<c.facilities<<' '
         <<c.ticket<<' '<<c.sponsorship<<' '<<c.wins<<' '<<c.draws<<' '<<c.losses<<' '<<c.gf<<' '<<c.ga<<' '<<c.points<<' '<<c.formation<<' '<<c.tactic<<' '<<c.training<<'\n';
        putString(f,c.name);putString(f,c.city);
    }
    f<<players_.size()<<'\n';
    for(const auto& p:players_){
        f<<p.id<<' '<<p.club<<' '<<p.age<<' '<<p.potential<<' ';
        for(auto k:p.skill)f<<k<<' ';
        f<<p.morale<<' '<<p.fitness<<' '<<p.trust<<' '<<p.loyalty<<' '<<p.family<<' '<<p.wage<<' '<<p.value<<' '<<p.contractYear<<' '<<p.injured<<' '<<p.goals<<' '<<p.appearances<<' '<<p.yellow<<' '<<p.red<<' '<<p.starter<<' '<<p.trainingFocus<<'\n';
        putString(f,p.name);putString(f,p.position);putString(f,p.personality);
    }
    f<<matches_.size()<<'\n';
    for(const auto& m:matches_){
        f<<m.id<<' '<<m.season<<' '<<m.day<<' '<<m.home<<' '<<m.away<<' '<<m.homeGoals<<' '<<m.awayGoals<<' '<<m.played<<' '
         <<m.shotsH<<' '<<m.shotsA<<' '<<m.xgH<<' '<<m.xgA<<' '<<m.events.size()<<'\n';
        for(const auto& e:m.events){f<<e.minute<<'\n';putString(f,e.text);}
    }
    f<<archive_.size()<<'\n';
    for(const auto& m:archive_){
        f<<m.id<<' '<<m.season<<' '<<m.day<<' '<<m.home<<' '<<m.away<<' '<<m.homeGoals<<' '<<m.awayGoals<<' '<<m.played<<' '
         <<m.shotsH<<' '<<m.shotsA<<' '<<m.xgH<<' '<<m.xgA<<' '<<m.events.size()<<'\n';
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
    return f.good()?"Карьера сохранена: "+path:"Ошибка записи сохранения";
}
std::string Game::load(const std::string& path){
    std::ifstream f(path,std::ios::binary);
    if(!f)return "Файл сохранения не найден: "+path;
    std::string header;std::getline(f,header);
    if(header!="FUSAVE_3"&&header!="FUSAVE_4")return "Неверный формат или версия сохранения";
    const bool legacy=(header=="FUSAVE_3");
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
        loaded.leagues_.clear();loaded.clubs_.clear();loaded.players_.clear();loaded.matches_.clear();loaded.archive_.clear();loaded.inbox_.clear();
        for(size_t i=0,n=size(300);i<n;++i){League l;if(!(f>>l.id>>l.active)||!getString(f,l.country)||!getString(f,l.name))throw std::runtime_error("Лиги");loaded.leagues_.push_back(l);}
        for(size_t i=0,n=size(100000);i<n;++i){
            Club c;if(!(f>>c.id>>c.league>>c.budget>>c.fans>>c.board>>c.prestige>>c.stadium>>c.academy>>c.facilities
                >>c.ticket>>c.sponsorship>>c.wins>>c.draws>>c.losses>>c.gf>>c.ga>>c.points>>c.formation>>c.tactic>>c.training)
                ||!getString(f,c.name)||!getString(f,c.city))throw std::runtime_error("Клубы");
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
            if(!getString(f,p.name)||!getString(f,p.position)||!getString(f,p.personality))
                throw std::runtime_error("Данные игрока");
            loaded.players_.push_back(std::move(p));
        }
        for(size_t i=0,n=size(1000000);i<n;++i){
            Match m;size_t count=0;
            if(!(f>>m.id>>m.season>>m.day>>m.home>>m.away>>m.homeGoals>>m.awayGoals>>m.played>>m.shotsH>>m.shotsA>>m.xgH>>m.xgA>>count)||count>2000)throw std::runtime_error("Матчи");
            for(size_t j=0;j<count;++j){Event e;if(!(f>>e.minute)||!getString(f,e.text))throw std::runtime_error("События матча");m.events.push_back(std::move(e));}
            loaded.matches_.push_back(std::move(m));
        }
        for(size_t i=0,n=size(1000000);i<n;++i){
            Match m;size_t count=0;
            if(!(f>>m.id>>m.season>>m.day>>m.home>>m.away>>m.homeGoals>>m.awayGoals>>m.played>>m.shotsH>>m.shotsA>>m.xgH>>m.xgA>>count)||count>2000)throw std::runtime_error("Архив");
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
        if(loaded.teamId_<0||loaded.teamId_>=int(loaded.clubs_.size()))throw std::runtime_error("Ваш клуб");
        for(const auto& c:loaded.clubs_)if(c.id<0||c.id>=int(loaded.clubs_.size())||c.league<0||c.league>=int(loaded.leagues_.size()))throw std::runtime_error("Ссылки клубов");
        for(const auto& p:loaded.players_)if(p.club<0||p.club>=int(loaded.clubs_.size()))throw std::runtime_error("Ссылки игроков");
        for(const auto& m:loaded.matches_)if(m.home<0||m.home>=int(loaded.clubs_.size())||m.away<0||m.away>=int(loaded.clubs_.size())||m.id<0||m.id>=int(loaded.matches_.size()))throw std::runtime_error("Ссылки матчей");
        if(loaded.live_.id < -1||loaded.live_.id>=int(loaded.matches_.size()))throw std::runtime_error("Ссылки трансляции");
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
