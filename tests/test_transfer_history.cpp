#include "Game.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sstream>
#include <vector>

namespace {
struct Files {
    std::filesystem::path dir=std::filesystem::temp_directory_path()/
        ("fu_history_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Files(){assert(std::filesystem::create_directory(dir));}
    ~Files(){std::filesystem::remove_all(dir);}
    std::string path(const char* name)const{return (dir/name).string();}
};
std::string read(const std::string& path){
    std::ifstream file(path,std::ios::binary);assert(file);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void write(const std::string& path,const std::string& text){
    std::ofstream file(path,std::ios::binary);file<<text;assert(file.good());
}
// Convert a fresh v15 fixture to the exact previous v14 layout, for an
// actual old-save migration regression test (not just a header substitution).
std::string makeV14(const std::string& source){
    std::istringstream in(source);
    std::vector<std::string> lines;std::string line;
    while(std::getline(in,line))lines.push_back(std::move(line));
    assert(lines.size()>10&&lines[0]=="FUSAVE_15");
    lines[0]="FUSAVE_14";
    const size_t leagues=std::stoul(lines.at(5));
    size_t i=6+leagues*3;
    const size_t clubs=std::stoul(lines.at(i++));
    for(size_t j=0;j<clubs;++j){
        auto& row=lines.at(i);
        for(int x=0;x<3;++x){
            auto pos=row.rfind(' ');assert(pos!=std::string::npos);
            row.erase(pos);
        }
        i+=3; // numeric fields, club name, city
    }
    // v15's live state adds a momentum counter (0 at kickoff).
    bool found=false;
    for(auto& row:lines)if(row=="1 -1 -1 0 0 0"){
        row="1 -1 -1 0 0";found=true;break;
    }
    assert(found);
    assert(lines.back()=="-1"); // last board budget request season
    lines.pop_back();
    std::ostringstream out;
    for(const auto& row:lines)out<<row<<'\n';
    return out.str();
}
void advance(fu::Game& game,int days){
    for(int i=0;i<days;++i){
        if(game.matchDue())assert(game.instantMatch());
        game.advance(1);
    }
}
void approve(fu::Game& game,int player){
    advance(game,1);
    assert(game.transferDeal(player)->stage==fu::DealStage::AgentOffer);
    assert(game.proposePlayerContract(player,game.transferDeal(player)->askingWage,3).find("отправлены")!=std::string::npos);
    advance(game,1);
    assert(game.transferDeal(player)->stage==fu::DealStage::Completed);
}
}
int main(){
    Files files;
    fu::Game game;
    game.newCareer("История",0,fu::Role::Sandbox,fu::Difficulty::Free,{0});
    const auto main=files.path("current.fus");
    assert(game.save(main).find("сохранена")!=std::string::npos);
    const auto original=read(main);
    assert(original.starts_with("FUSAVE_15\n"));
    assert(original.ends_with("-1\n"));
    // v14 adds the live ball state. Recreate an actual v13 save by removing
    // those extra five fields before testing the historical v12 migration.
    auto old14=makeV14(original);
    write(files.path("old14.fus"),old14);
    fu::Game oldV14;
    assert(oldV14.load(files.path("old14.fus")).find("загружена")!=std::string::npos);
    auto old13=old14;
    const std::string newMatchState="\n1 -1 -1 0 0\n";
    const auto matchStateOffset=old13.find(newMatchState);
    assert(matchStateOffset!=std::string::npos);
    old13.erase(matchStateOffset+1,newMatchState.size()-1);
    old13.replace(0,std::string("FUSAVE_14").size(),"FUSAVE_13");
    write(files.path("old13.fus"),old13);
    fu::Game oldV13;
    assert(oldV13.load(files.path("old13.fus")).find("загружена")!=std::string::npos);
    auto old=old13.substr(0,old13.size()-2);
    old.replace(0,std::string("FUSAVE_13").size(),"FUSAVE_12");
    write(files.path("old12.fus"),old);
    fu::Game migrated;
    assert(migrated.load(files.path("old12.fus")).find("загружена")!=std::string::npos);
    assert(migrated.transferHistory().empty());
    assert(migrated.day()==game.day()&&migrated.manager()==game.manager());
    assert(migrated.players().size()==game.players().size());
    assert(migrated.save(files.path("migrated13.fus")).find("сохранена")!=std::string::npos);
    assert(read(files.path("migrated13.fus")).starts_with("FUSAVE_15\n"));

    const int sold=game.ownPlayers().back();
    const auto name=game.findPlayer(sold)->name;
    assert(game.sellPlayer(sold,1,1).find("Продажа завершена")!=std::string::npos);
    assert(game.transferHistory().size()==1);
    const auto record=game.transferHistory().front();
    assert(record.playerId==sold&&record.playerName==name);
    assert(record.fromClub==0&&record.toClub==1&&record.fee==1&&record.kind==0);
    assert(record.season==game.season()&&record.day==0&&record.totalDay==0);
    assert(game.worldMarketText(sold).find(name)!=std::string::npos);
    assert(game.save(main).find("сохранена")!=std::string::npos);
    fu::Game loaded;
    assert(loaded.load(main).find("загружена")!=std::string::npos);
    assert(loaded.transferHistory().size()==1);
    assert(loaded.transferHistory()[0].playerName==name);
    assert(loaded.worldMarketText(sold)==game.worldMarketText(sold));
    const auto ledger=read(main);
    // Corrupt or missing history must fail transactionally, not silently erase it.
    assert(loaded.save(files.path("before.fus")).find("сохранена")!=std::string::npos);
    auto damaged=ledger.substr(0,ledger.size()-name.size()-10);
    write(files.path("damaged.fus"),damaged);
    assert(loaded.load(files.path("damaged.fus")).find("загружена")==std::string::npos);
    assert(loaded.save(files.path("after.fus")).find("сохранена")!=std::string::npos);
    assert(read(files.path("before.fus"))==read(files.path("after.fus")));

    // Returning a loanee is a separate history event with no fictitious fee.
    fu::Game loan;
    loan.newCareer("Аренда",0,fu::Role::Sandbox,fu::Difficulty::Free,{0});
    const int target=loan.transferCandidates().front();
    const int owner=loan.findPlayer(target)->club;
    assert(loan.offerLoan(target,loan.findPlayer(target)->value/10+1000,50,30).find("направлено")!=std::string::npos);
    approve(loan,target);
    assert(loan.transferHistory().back().kind==1);
    assert(loan.transferHistory().back().fromClub==owner);
    advance(loan,30);
    assert(loan.findPlayer(target)->club==owner);
    bool returned=false;
    for(const auto& transfer:loan.transferHistory())if(transfer.playerId==target&&transfer.kind==2){
        returned=true;assert(transfer.fromClub==0&&transfer.toClub==owner&&transfer.fee==0);
    }
    assert(returned);
    assert(loan.save(main).find("сохранена")!=std::string::npos);
    assert(loaded.load(main).find("загружена")!=std::string::npos);
    assert(loaded.transferHistory().size()==loan.transferHistory().size());
    std::cout<<"PASS: transfer ledger, human sales, loans and returns, FUSAVE_12 migration, FUSAVE_14 roundtrip and corrupt-save transaction\n";
}
