#include "WorldDatabase.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>
#include <unordered_set>

namespace fu {
namespace {
std::string lower(std::string_view s){
    std::string out;out.reserve(s.size());
    for(unsigned char c:s)out.push_back(char(std::tolower(c)));
    return out;
}
bool containsInsensitive(std::string_view haystack,const std::string& needle){
    if(needle.empty())return true;
    return lower(haystack).find(needle)!=std::string::npos;
}
bool matchPosition(std::string_view role,const std::string& requested){
    if(requested.empty())return true;
    const auto value=lower(role);
    if(requested=="forward")return value.find("forward")!=std::string::npos||
        value.find("striker")!=std::string::npos||value.find("winger")!=std::string::npos;
    if(requested=="defender")return value.find("defender")!=std::string::npos||
        value.find("back")!=std::string::npos;
    if(requested=="midfielder")return value.find("midfield")!=std::string::npos;
    if(requested=="goalkeeper")return value.find("goalkeeper")!=std::string::npos||value=="gk";
    return value.find(requested)!=std::string::npos;
}
std::vector<std::string> split(const std::string& s){
    std::vector<std::string> parts;
    size_t start=0;
    while(start<=s.size()){
        size_t end=s.find(';',start);
        if(end==std::string::npos){parts.push_back(s.substr(start));break;}
        parts.push_back(s.substr(start,end-start));start=end+1;
    }
    return parts;
}
bool validDate(std::string_view date){
    if(date.empty())return true;
    if(date.size()!=10||date[4]!='-'||date[7]!='-')return false;
    for(size_t i=0;i<date.size();++i)if(i!=4&&i!=7&&!std::isdigit(static_cast<unsigned char>(date[i])))return false;
    const int year=std::stoi(std::string(date.substr(0,4)));
    const int month=std::stoi(std::string(date.substr(5,2)));
    const int day=std::stoi(std::string(date.substr(8,2)));
    return year>=1890&&year<=2012&&month>=1&&month<=12&&day>=1&&day<=31;
}
bool simpleId(std::string_view id){
    if(id.size()<8||id.size()>40||id.substr(0,6)!="reep_p")return false;
    return std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isalnum(c)||c=='_';});
}
}
bool WorldDatabase::load(const std::string& path){
    std::ifstream file(path,std::ios::binary);
    if(!file){error_="Не найден каталог футболистов: "+path;return false;}
    std::string line;
    if(!std::getline(file,line)) {error_="Каталог пуст.";return false;}
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line!="source_id;wikidata_qid;name;position;birth_date;nationality"){
        error_="Неподдерживаемый формат каталога (ожидалось 6 колонок).";return false;
    }
    std::vector<WorldPlayer> loaded;
    loaded.reserve(180000);
    std::unordered_set<std::string> uniqueIds;
    uniqueIds.reserve(180000);
    while(std::getline(file,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line.size()>1000)continue;
        auto v=split(line);
        if(v.size()!=6||!simpleId(v[0])||v[2].empty()||v[2].size()>140||
            !validDate(v[4])||!uniqueIds.insert(v[0]).second)continue;
        WorldPlayer p;
        p.id=std::move(v[0]);p.wikidataQid=std::move(v[1]);
        p.name=std::move(v[2]);p.position=std::move(v[3]);
        p.birthDate=std::move(v[4]);p.nationality=std::move(v[5]);
        if(!p.birthDate.empty()){
            p.age=2026-std::stoi(p.birthDate.substr(0,4));
            if(p.birthDate.substr(5)>"11-02")--p.age;
        }
        loaded.push_back(std::move(p));
        if(loaded.size()>=500000)break; // bounded for untrusted CSV
    }
    if(loaded.empty()){error_="Каталог не содержит корректных записей.";return false;}
    std::unordered_map<std::string,size_t> index;
    index.reserve(loaded.size());
    for(size_t i=0;i<loaded.size();++i)index.emplace(loaded[i].id,i);
    players_=std::move(loaded);
    idLookup_=std::move(index);
    error_.clear();return true;
}
const WorldPlayer* WorldDatabase::at(size_t i)const{
    return i<players_.size()?&players_[i]:nullptr;
}
std::optional<size_t> WorldDatabase::indexById(const std::string& id) const{
    const auto found=idLookup_.find(id);
    if(found==idLookup_.end())return std::nullopt;
    return found->second;
}
WorldResults WorldDatabase::searchWatched(const WorldQuery& q,
                                           const std::vector<std::string>& watchIds) const{
    WorldResults result;
    const auto name=lower(q.name),position=lower(q.position),country=lower(q.country);
    const size_t limit=std::min(size_t(250),q.limit);
    for(const auto& id:watchIds){
        auto idx=indexById(id);
        if(!idx)continue;  // The snapshot changed or the CSV is unavailable.
        const auto& p=players_[*idx];
        if(q.maxAge>0&&(p.age<=0||p.age>q.maxAge))continue;
        if(!containsInsensitive(p.name,name)||!matchPosition(p.position,position)||
           !containsInsensitive(p.nationality,country))continue;
        if(result.total>=q.offset&&result.indices.size()<limit)
            result.indices.push_back(*idx);
        ++result.total;
    }
    return result;
}
WorldResults WorldDatabase::search(const WorldQuery& q) const{
    WorldResults result;
    const std::string name=lower(q.name),position=lower(q.position),country=lower(q.country);
    const size_t limit=std::min(size_t(250),q.limit);
    for(size_t i=0;i<players_.size();++i){
        const auto& p=players_[i];
        if(q.maxAge>0&&(p.age<=0||p.age>q.maxAge))continue;
        if(!containsInsensitive(p.name,name)||!matchPosition(p.position,position)||
           !containsInsensitive(p.nationality,country))continue;
        if(result.total>=q.offset&&result.indices.size()<limit)result.indices.push_back(i);
        ++result.total;
    }
    return result;
}
}
