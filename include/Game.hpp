#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace fu {

struct Player {
    int id=0, club=0, age=18, potential=70;
    std::string name, position, personality;
    std::array<int,10> skill{}; // pace, finishing, passing, defending, technique, strength, vision, stamina, decisions, goalkeeping
    int morale=65, fitness=90, trust=50, loyalty=50, family=65;
    int wage=1000, value=500000, contractYear=2028, injured=0;
    int goals=0, appearances=0, yellow=0, red=0;
    bool starter=false; // chosen in matchday starting XI
    int trainingFocus=-1; // -1 = team training; 0..9 = individual skill
};
struct Club {
    int id=0, league=0;
    std::string name, city;
    int budget=12000000, fans=65, board=65, prestige=60, stadium=20000, academy=2, facilities=2;
    int ticket=25, sponsorship=250000, wins=0, draws=0, losses=0, gf=0, ga=0, points=0;
    int formation=0, tactic=1, training=1;
    int captainId=-1, viceCaptainId=-1;
};
struct League { int id=0; std::string country, name; bool active=true; };
struct Event { int minute=0; std::string text; };
struct Match {
    int id=0, season=2026, day=0, home=0, away=0, homeGoals=0, awayGoals=0;
    bool played=false;
    int shotsH=0, shotsA=0, xgH=0, xgA=0; // xG in hundredths
    std::vector<Event> events;
};
struct Message { int day=0; std::string title, body; };
struct ScoutReport { int playerId=0, days=0, ability=0, potential=0; bool shortlisted=false; };
struct LiveMatch {
    int id=-1, minute=0, goalsH=0, goalsA=0, shotsH=0, shotsA=0, xgH=0, xgA=0;
    int subsH=0, subsA=0;
    std::vector<int> homeXI, awayXI, usedHome, usedAway;
    std::vector<Event> events;
};

enum class Role : int { Coach=0, Director=1, Owner=2, President=3, Sandbox=4 };
enum class Difficulty : int { Easy=0, Realistic=1, Hard=2, Free=3 };

class Game {
public:
    Game();
    void newCareer(std::string manager, int clubId, Role role, Difficulty difficulty, const std::vector<int>& activeLeagues);
    const std::vector<Club>& clubs() const { return clubs_; }
    const std::vector<League>& leagues() const { return leagues_; }
    const std::vector<Player>& players() const { return players_; }
    const std::vector<Match>& matches() const { return matches_; }
    const std::vector<Match>& archive() const { return archive_; }
    const std::vector<Message>& inbox() const { return inbox_; }
    const std::vector<ScoutReport>& scoutReports() const { return scouts_; }
    const ScoutReport* scoutReport(int playerId) const;
    const Club& myClub() const;
    Club& myClub();
    const Player* findPlayer(int id) const;
    const Club* findClub(int id) const;
    int teamId() const { return teamId_; }
    int day() const { return day_; }
    int season() const { return season_; }
    int totalDays() const { return totalDays_; }
    Role role() const { return role_; }
    Difficulty difficulty() const { return difficulty_; }
    const std::string& manager() const { return manager_; }
    const std::string& status() const { return status_; }
    std::string dateString() const;
    std::string standings(int leagueId) const;
    std::string squadText() const;
    std::string lineupText() const;
    std::vector<int> starters(int clubId) const;
    std::string swapStarter(int substituteId,int starterId);
    std::string autoPickLineup();
    std::string cycleTrainingFocus(int playerId);
    std::string substitute(int playerOutId,int playerInId);
    std::string homeText() const;
    std::string tacticsText() const;
    std::string financesText() const;
    std::string matchText() const;
    std::string matchArchive() const;
    std::string newsText() const;
    std::string helpText() const;
    std::vector<int> ownPlayers() const;
    std::vector<int> transferCandidates(bool shortlistOnly=false) const;
    std::string scoutPlayer(int playerId);
    std::string toggleShortlist(int playerId);
    std::string scoutText(int playerId) const;
    std::string setCaptain(int playerId, bool vice=false);
    std::string teamTalk(int tone);
    std::vector<int> leagueClubs(int leagueId) const;
    const Match* upcomingMatch() const;
    const LiveMatch& live() const { return live_; }
    bool hasLiveMatch() const { return live_.id>=0; }
    bool matchDue() const;
    void advance(int days);
    bool startLiveMatch();
    bool stepLiveMatch(int minutes);
    bool instantMatch();
    void changeTactic();
    void changeFormation();
    void changeTraining();
    std::string transferBid(int playerId, int offered);
    std::string renewContract(int playerId, int wage, int years);
    std::string talk(int playerId, const std::string& phrase);
    std::string speakToFans(const std::string& phrase);
    std::string spendMoney(const std::string& project);
    std::string recruitYouth();
    std::string negotiateSponsor();
    std::string adjustTickets(int change);
    std::string appointRole(Role role);
    void toggleLeague(int leagueId);
    std::string save(const std::string& path) const;
    std::string load(const std::string& path);
    bool importPlayers(const std::string& path);
    void post(std::string title, std::string body);
private:
    std::vector<Club> clubs_;
    std::vector<League> leagues_;
    std::vector<Player> players_;
    std::vector<Match> matches_;
    std::vector<Match> archive_;
    std::vector<Message> inbox_;
    std::vector<ScoutReport> scouts_;
    int lastTeamTalkDay_=-30;
    std::string manager_="Manager", status_;
    int teamId_=0, day_=0, season_=2026, totalDays_=0, nextPlayerId_=1;
    Role role_=Role::Coach;
    Difficulty difficulty_=Difficulty::Realistic;
    LiveMatch live_;
    std::mt19937 rng_;
    int random(int lo,int hi);
    void seedWorld();
    void createSquads();
    void autoAssignCaptains();
    void createFixtures();
    void pickLineup(int clubId);
    void initializeLineups(LiveMatch& live, const Match& match) const;
    int lineupStrength(const std::vector<int>& ids) const;
    void newSeason();
    void dailyTick();
    int rating(const Club& club) const;
    void playMinute(LiveMatch& match, bool detailed);
    void finishMatch(LiveMatch& match);
    void simulateOtherMatches();
    void simulateFast(Match& match);
    bool canFinance(int amount);
    bool permissionSports() const;
    bool permissionBusiness() const;
    void addMessage(std::string title, std::string body);
};
} // namespace fu
