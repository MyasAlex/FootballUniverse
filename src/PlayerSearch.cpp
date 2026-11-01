#include "PlayerSearch.hpp"
#include "Game.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

namespace fu {
namespace {
char32_t lower(char32_t c){
    if((c>=U'A'&&c<=U'Z')||(c>=U'А'&&c<=U'Я'))return c+32;
    if(c==U'Ё')return U'ё';
    if((c>=0x00c0&&c<=0x00d6)||(c>=0x00d8&&c<=0x00de))return c+32;
    if(c==0x0178)return 0x00ff;
    if(c==0x0130)return U'i';
    if((c>=0x0100&&c<=0x012f)||(c>=0x0132&&c<=0x0137)||
       (c>=0x014a&&c<=0x0177))return c%2==0?c+1:c;
    if(c>=0x0139&&c<=0x0148)return c%2==1?c+1:c;
    return c;
}
std::optional<std::u32string> folded(std::string_view text){
    std::u32string result;
    for(size_t i=0;i<text.size();){
        const auto lead=static_cast<unsigned char>(text[i++]);
        char32_t cp=0;
        int continuation=0;
        if(lead<0x80)cp=lead;
        else if(lead>=0xc2&&lead<=0xdf){cp=lead&0x1f;continuation=1;}
        else if(lead>=0xe0&&lead<=0xef){cp=lead&0x0f;continuation=2;}
        else if(lead>=0xf0&&lead<=0xf4){cp=lead&0x07;continuation=3;}
        else return std::nullopt;
        if(i+continuation>text.size())return std::nullopt;
        for(int n=0;n<continuation;++n){
            const auto next=static_cast<unsigned char>(text[i++]);
            if((next&0xc0)!=0x80)return std::nullopt;
            cp=(cp<<6)|(next&0x3f);
        }
        if((continuation==1&&cp<0x80)||(continuation==2&&cp<0x800)||
           (continuation==3&&cp<0x10000)||cp>0x10ffff||
           (cp>=0xd800&&cp<=0xdfff))return std::nullopt;
        result.push_back(lower(cp));
    }
    return result;
}
void trim(std::u32string& text){
    const auto whitespace=[](char32_t c){return c==U' '||c==U'\t'||c==U'\r'||c==U'\n';};
    const auto first=std::find_if_not(text.begin(),text.end(),whitespace);
    const auto last=std::find_if_not(text.rbegin(),text.rend(),whitespace).base();
    text=first<last?std::u32string(first,last):std::u32string{};
}
bool validPosition(const std::u32string& position){
    static const std::array<std::u32string_view,13> known={
        U"",U"gk",U"dl",U"dc",U"dr",U"dm",U"mc",U"amc",U"ml",U"mr",U"aml",U"amr",U"st"};
    return std::find(known.begin(),known.end(),position)!=known.end();
}
} // namespace

int playerSearchAbility(const Player& player){
    std::array<int,10> skills{};
    for(size_t i=0;i<skills.size();++i)skills[i]=std::clamp(player.skill[i],0,100);
    const auto& a=skills;
    if(player.position=="GK")return (a[9]*6+a[8]*2+a[5]+a[7])/10;
    if(player.position=="DL"||player.position=="DC"||player.position=="DR")
        return (a[3]*4+a[5]*2+a[0]+a[7]+a[8]*2)/10;
    if(player.position=="ST"||player.position=="AML"||player.position=="AMR")
        return (a[1]*4+a[4]*2+a[0]*2+a[8]+a[6])/10;
    return (a[2]*3+a[4]*2+a[6]*2+a[7]+a[8]*2)/10;
}

std::vector<int> searchPlayers(const Game& game,const PlayerSearchFilter& filter){
    if(filter.minAge<0||filter.maxAge<filter.minAge||filter.maxAge>150||
       filter.maxValue<0||filter.maxWage<0||filter.minAbility<0||
       filter.minAbility>100||filter.minPotential<0||filter.minPotential>100||
       static_cast<int>(filter.sort)<0||static_cast<int>(filter.sort)>5)return {};
    auto name=folded(filter.name),position=folded(filter.position);
    if(!name||!position)return {};
    trim(*name);trim(*position);
    if(*position==U"all"||*position==U"все")position->clear();
    if(!validPosition(*position))return {};

    struct Candidate {const Player* player;int ability;std::u32string name;};
    std::vector<Candidate> candidates;
    for(const auto& player:game.players()){
        if(player.club==game.teamId()||player.age<filter.minAge||player.age>filter.maxAge||
           player.value>filter.maxValue||player.wage>filter.maxWage||
           player.potential<filter.minPotential||
           (filter.realOnly&&player.sourceQid.empty()))continue;
        const auto* club=game.findClub(player.club);
        if(!club)continue;
        if(filter.activeLeaguesOnly&&(club->league<0||club->league>=int(game.leagues().size())||
                                     !game.leagues()[club->league].active))continue;
        if(filter.shortlistOnly){
            const auto* report=game.scoutReport(player.id);
            if(!report||!report->shortlisted)continue;
        }
        if(!position->empty()){
            const auto actual=folded(player.position);
            if(!actual||*actual!=*position)continue;
        }
        const int ability=playerSearchAbility(player);
        if(ability<filter.minAbility)continue;
        auto playerName=folded(player.name);
        if(!playerName)continue;
        if(!name->empty()&&playerName->find(*name)==std::u32string::npos){
            const auto clubName=folded(club->name);
            if(!clubName||clubName->find(*name)==std::u32string::npos)continue;
        }
        candidates.push_back({&player,ability,std::move(*playerName)});
    }
    std::sort(candidates.begin(),candidates.end(),[&filter](const Candidate& a,const Candidate& b){
        switch(filter.sort){
            case PlayerSort::ValueAscending:
                if(a.player->value!=b.player->value)return a.player->value<b.player->value;
                break;
            case PlayerSort::AbilityDescending:
                if(a.ability!=b.ability)return a.ability>b.ability;
                break;
            case PlayerSort::PotentialDescending:
                if(a.player->potential!=b.player->potential)return a.player->potential>b.player->potential;
                break;
            case PlayerSort::AgeAscending:
                if(a.player->age!=b.player->age)return a.player->age<b.player->age;
                break;
            case PlayerSort::WageAscending:
                if(a.player->wage!=b.player->wage)return a.player->wage<b.player->wage;
                break;
            case PlayerSort::Name:
                if(a.name!=b.name)return a.name<b.name;
                break;
        }
        return a.player->id<b.player->id;
    });
    std::vector<int> result;
    result.reserve(candidates.size());
    for(const auto& candidate:candidates)result.push_back(candidate.player->id);
    return result;
}

} // namespace fu
