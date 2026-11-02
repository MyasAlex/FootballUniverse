#include "Game.hpp"
#include "WorldDatabase.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main(){
    using namespace fu;
    const auto temp=std::filesystem::temp_directory_path()/"football_universe_v017_watchlist";
    std::filesystem::create_directories(temp);
    const auto csv=temp/"world.csv", save=temp/"career.fus", old=temp/"career_v15.fus";
    {
        std::ofstream f(csv);
        f<<"source_id;wikidata_qid;name;position;birth_date;nationality\n"
          <<"reep_p000001;Q100;Alice Striker;Forward;2003-09-20;Norway\n"
          <<"reep_p000002;Q101;Bob Keeper;Goalkeeper;1997-02-18;England\n"
          <<"reep_p000003;;Carla Player;Midfielder;;Brazil\n";
    }
    WorldDatabase db;
    assert(db.load(csv.string()));
    assert(db.size()==3);
    assert(db.indexById("reep_p000001")&&*db.indexById("reep_p000001")==0);
    assert(!db.indexById("reep_p_missing"));
    auto ids=std::vector<std::string>{"reep_p000002","reep_p000001","reep_p_missing"};
    const auto initial=db.searchWatched({"","","",0,0,120},ids);
    assert(initial.total==2&&initial.indices.size()==2);
    assert(initial.indices[0]==1&&initial.indices[1]==0);
    auto p=db.searchWatched({"","","",0,1,1},ids);
    assert(p.total==2&&p.indices.size()==1&&p.indices[0]==0);
    assert(db.searchWatched({"","Goalkeeper","England",0,0,120},ids).total==1);
    assert(db.searchWatched({"Alice","","",24,0,120},ids).total==1);
    assert(db.searchWatched({"","","",20,0,120},ids).total==0);

    Game g;
    g.newCareer("Scout",0,Role::Coach,Difficulty::Realistic,{0});
    assert(g.worldWatchlist().empty());
    assert(g.noteWorldWatch(*db.at(0),std::string(401,'x')).find("слишком длинная")!=std::string::npos);
    assert(g.worldWatchlist().empty()); // failed note must not add watch
    assert(g.toggleWorldWatch(*db.at(0)).find("Добавлен")!=std::string::npos);
    assert(g.toggleWorldWatch(*db.at(1)).find("Добавлен")!=std::string::npos);
    assert(g.worldWatchlist().size()==2);
    assert(g.noteWorldWatch(*db.at(0),"Left-footed forward; watch again").find("сохранена")!=std::string::npos);
    assert(g.watchedWorldPlayer("reep_p000001"));
    assert(g.watchedWorldPlayer("reep_p000001")->note=="Left-footed forward; watch again");
    assert(g.noteWorldWatch(*db.at(1),"note\nunsafe").find("недопустимые")!=std::string::npos);
    assert(g.noteWorldWatch(*db.at(1),std::string(401,'x')).find("слишком длинная")!=std::string::npos);
    assert(g.save(save.string()).find("сохранена")!=std::string::npos);
    {
        std::ifstream f(save);std::string line;std::getline(f,line);assert(line=="FUSAVE_16");
    }
    Game loaded;
    assert(loaded.load(save.string()).find("загружена")!=std::string::npos);
    assert(loaded.worldWatchlist().size()==2);
    assert(loaded.watchedWorldPlayer("reep_p000001")&&loaded.watchedWorldPlayer("reep_p000001")->note=="Left-footed forward; watch again");
    assert(loaded.toggleWorldWatch(*db.at(0)).find("удалён")!=std::string::npos);
    assert(loaded.worldWatchlist().size()==1);
    loaded.newCareer("New",0,Role::Director,Difficulty::Realistic,{0});
    assert(loaded.worldWatchlist().empty());

    // True backward-compatibility test: remove the v16 trailer and switch header to v15.
    std::string text;
    {std::ifstream f(save,std::ios::binary);text.assign(std::istreambuf_iterator<char>(f),{});}
    const std::string trailer="2\n\"reep_p000001\"\n\"Alice Striker\"\n\"Forward\"\n\"Norway\"\n\"Left-footed forward; watch again\"\n0\n\"reep_p000002\"\n\"Bob Keeper\"\n\"Goalkeeper\"\n\"England\"\n\"\"\n0\n";
    assert(text.size()>trailer.size()&&text.substr(text.size()-trailer.size())==trailer);
    text.resize(text.size()-trailer.size());
    text.replace(0,std::string("FUSAVE_16").size(),"FUSAVE_15");
    {std::ofstream f(old,std::ios::binary);f<<text;}
    Game fromOld;
    assert(fromOld.load(old.string()).find("загружена")!=std::string::npos);
    assert(fromOld.worldWatchlist().empty());

    // Truncated v16 watchlist must NOT overwrite currently playing career.
    {std::ofstream f(save,std::ios::binary|std::ios::trunc);f<<text.substr(0,text.size()-20);}
    const int beforeDay=fromOld.day();const auto beforeManager=fromOld.manager();
    assert(fromOld.load(save.string()).find("Неверный формат")!=std::string::npos ||
           fromOld.load(save.string()).find("Не удалось загрузить")!=std::string::npos);
    assert(fromOld.day()==beforeDay&&fromOld.manager()==beforeManager);
    std::filesystem::remove_all(temp);
    std::cout<<"PASS: world scouting watchlist, fast catalogue lookup, notes, save migration\n";
}
