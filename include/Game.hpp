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
    int goals=0, appearances=0, yellow=0, red=0, assists=0, suspension=0;
    bool starter=false; // chosen in matchday starting XI
    int trainingFocus=-1; // -1 = team training; 0..9 = individual skill
    int releaseClause=0, sellOnClub=-1, sellOnPct=0;
    int parentClub=-1, loanReturnDay=-1, loanWageShare=100;
    int careerAppearances=0;
    int developmentXP=0, mentorId=-1, developedSkills=0;
    bool academyGraduate=false;
    std::string sourceQid; // Empty for generated players; Q... for real Wikidata identities.
};
struct Club {
    int id=0, league=0;
    std::string name, city;
    int budget=12000000, fans=65, board=65, prestige=60, stadium=20000, academy=2, facilities=2;
    int ticket=25, sponsorship=250000, wins=0, draws=0, losses=0, gf=0, ga=0, points=0;
    int formation=0, tactic=1, training=1;
    int pressing=1, tempo=1, width=1; // 0 low, 1 balanced, 2 high
    int captainId=-1, viceCaptainId=-1;
};
// systemId identifies one country pyramid; tier: 0 top, 1 second division.
struct League { int id=0; std::string country, name; bool active=true; int tier=0, systemId=0; };
struct Event { int minute=0; std::string text; };
struct MatchPlayerStat {
    int playerId=0, minutes=0, shots=0, onTarget=0, passes=0, completedPasses=0;
    int tackles=0, saves=0, goals=0, assists=0, yellows=0, reds=0;
    int rating10=60; // 3.5..10.0 after match, 6.0 before
};
struct Match {
    int id=0, season=2026, day=0, home=0, away=0, homeGoals=0, awayGoals=0;
    bool played=false;
    int shotsH=0, shotsA=0, xgH=0, xgA=0; // xG in hundredths
    std::vector<Event> events;
    int competition=-1, round=0; // -1 league, 0..5 national cup, 6 continental invitation
    int penaltiesH=-1, penaltiesA=-1; // only for drawn knockout matches
    int possessionH=50, onTargetH=0, onTargetA=0;
    int passesH=0, passesA=0, completedH=0, completedA=0;
    int foulsH=0, foulsA=0, cornersH=0, cornersA=0;
    std::vector<MatchPlayerStat> playerStats;
};
struct CupCompetition {
    int id=0, season=2026, leagueId=-1, round=0, champion=-1;
    int byeA=-1, byeB=-1;
    std::string name;
};
struct TrophyRecord { int season=2026, cupId=0, winner=-1, runnerUp=-1; };
struct LeagueRecord { int season=2026, leagueId=0, winner=-1, runnerUp=-1, points=0; };
struct LeagueMovement { int season=2026, systemId=0, promotedClub=-1, relegatedClub=-1; };
struct CoachRecord {
    int clubId=0, ability=55, jobSecurity=65, contractYear=2028, hiredDay=0;
    std::string name;
    bool vacant=false;
};
struct JobApplication {
    int clubId=-1, submittedDay=0, responseDay=0, expiresDay=0;
    int stage=-1, proposedSalary=0; // -1 none, 0 pending, 1 offered, 2 rejected, 3 accepted, 4 cancelled/expired
};
struct Message { int day=0; std::string title, body; };
struct ScoutReport { int playerId=0, days=0, ability=0, potential=0; bool shortlisted=false; };
enum class DealStage : int {
    ClubReview=0, ClubCounter=1, AgentOffer=2, AgentReview=3,
    AgentCounter=4, Completed=5, Rejected=6, Cancelled=7, RivalSigned=8
};
struct TransferDeal {
    int playerId=0, sellerClub=-1, fee=0, askingFee=0;
    int wage=0, askingWage=0, years=3;
    int submittedDay=0, replyDay=0, expiryDay=0;
    int rivalClub=-1, rounds=0;
    DealStage stage=DealStage::ClubReview;
    int kind=0; // 0 = permanent, 1 = temporary loan
    int installments=1, appearanceBonus=0, sellOnPct=0, exchangePlayerId=-1;
    int loanDays=180, loanWageShare=50;
};
struct DeferredPayment {
    int playerId=0, buyerClub=-1, sellerClub=-1, amount=0;
    int dueDay=0, triggerAppearances=0; // 0 = scheduled, positive = appearances milestone
};
struct StaffMember {
    int id=0, club=0, role=0, quality=55, weeklyWage=3500, contractYear=2029;
    std::string name;
};
struct YouthProspect {
    int id=0, club=0, age=16, ability=43, potential=70;
    std::string name, position;
};
struct Relationship {
    int first=0, second=0, affinity=0, lastInteractionDay=-30;
};
struct PitchActor {
    int playerId=0, team=0, x=0, y=0; // Normalized 0..1000, 0..700
};
struct PitchFrame {
    int minute=0, homeClub=-1, awayClub=-1;
    int homeGoals=0, awayGoals=0;
    int ballFromX=500, ballFromY=350, ballX=500, ballY=350;
    int carrierId=-1, receiverId=-1, action=0; // 0 possession, 1 pass, 2 shot, 3 goal
    std::string highlight;
    std::vector<PitchActor> actors;
};
struct LiveMatch {
    int id=-1, minute=0, goalsH=0, goalsA=0, shotsH=0, shotsA=0, xgH=0, xgA=0;
    int subsH=0, subsA=0;
    std::vector<int> homeXI, awayXI, usedHome, usedAway;
    std::vector<Event> events;
    int possessionTicksH=0, onTargetH=0, onTargetA=0;
    int passesH=0, passesA=0, completedH=0, completedA=0;
    int foulsH=0, foulsA=0, cornersH=0, cornersA=0;
    std::vector<MatchPlayerStat> playerStats;
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
    const std::vector<CupCompetition>& cups() const { return cups_; }
    const std::vector<TrophyRecord>& trophyHistory() const { return trophies_; }
    const std::vector<LeagueRecord>& leagueHistory() const { return leagueHistory_; }
    const std::vector<LeagueMovement>& leagueMovements() const { return movements_; }
    const std::vector<CoachRecord>& coaches() const { return coaches_; }
    const JobApplication& jobApplication() const { return job_; }
    int managerReputation() const { return reputation_; }
    std::string jobMarketText(int clubId=-1) const;
    std::string applyForJob(int clubId);
    std::string acceptJobOffer();
    std::string withdrawApplication();
    std::string divisionsText() const;
    std::string calendarText() const;
    std::string cupsText() const;
    std::string seasonHistoryText() const;
    std::string scorersText() const;
    int knockoutWinner(const Match& match) const;
    const std::vector<Message>& inbox() const { return inbox_; }
    const std::vector<ScoutReport>& scoutReports() const { return scouts_; }
    const ScoutReport* scoutReport(int playerId) const;
    const TransferDeal* transferDeal(int playerId) const;
    const std::vector<TransferDeal>& transferDeals() const { return deals_; }
    const std::vector<DeferredPayment>& deferredPayments() const { return payments_; }
    const std::vector<StaffMember>& staff() const { return staff_; }
    const std::vector<YouthProspect>& youthProspects() const { return youth_; }
    const std::vector<Relationship>& relationships() const { return relations_; }
    int staffQuality(int clubId,int role) const;
    std::string staffText() const;
    std::string academyText() const;
    std::string developmentText(int playerId) const;
    std::string relationshipsText(int playerId) const;
    std::string hireStaff(int role,int quality);
    std::string fireStaff(int staffId);
    std::string discoverYouth();
    std::string signYouthProspect(int prospectId);
    std::string assignMentor(int youngPlayerId,int mentorPlayerId);
    std::string improveRelationship(int firstPlayerId,int secondPlayerId,int type);
    std::string negotiationText(int playerId) const;
    std::string proposePlayerContract(int playerId, int weeklyWage, int years);
    std::string acceptNegotiationTerms(int playerId);
    std::string cancelNegotiation(int playerId);
    std::string offerStructuredTransfer(int playerId, int fee, int installments, int appearanceBonus, int sellOnPct, int exchangePlayerId=-1);
    std::string offerLoan(int playerId, int fee, int wageShare, int durationDays);
    std::string setReleaseClause(int playerId, int amount);
    std::string sellPlayer(int playerId, int buyerClub, int amount);
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
    std::string matchReport(int matchId) const;
    std::string latestMatchReport() const;
    void changePressing();
    void changeTempo();
    void changeWidth();
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
    PitchFrame pitchFrame() const;
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
    int realPlayerCount(int clubId=-1) const;
    void post(std::string title, std::string body);
private:
    std::vector<Club> clubs_;
    std::vector<League> leagues_;
    std::vector<Player> players_;
    std::vector<Match> matches_;
    std::vector<Match> archive_;
    std::vector<CupCompetition> cups_;
    std::vector<TrophyRecord> trophies_;
    std::vector<LeagueRecord> leagueHistory_;
    std::vector<LeagueMovement> movements_;
    std::vector<CoachRecord> coaches_;
    JobApplication job_;
    int reputation_=55;
    std::vector<Message> inbox_;
    std::vector<ScoutReport> scouts_;
    std::vector<TransferDeal> deals_;
    std::vector<DeferredPayment> payments_;
    std::vector<StaffMember> staff_;
    std::vector<YouthProspect> youth_;
    std::vector<Relationship> relations_;
    int nextStaffId_=1,nextProspectId_=1,lastYouthSearchDay_=-45;
    void seedStaff();
    void generateYouth(int clubId,int count);
    Relationship* getRelationship(int first,int second);
    void pruneRelationships();
    void processDeferredPayments();
    void processLoanReturns();
    void processTransferNegotiations();
    void finalizeTransfer(TransferDeal& deal);
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
    void appendSecondDivisions(bool addFixtures);
    void seedCoaches();
    void processCoachMarket();
    void processJobApplication();
    void processPromotionRelegation(int oldSeason);
    std::vector<int> rankedClubs(int leagueId) const;
    void addFixturesForLeague(int leagueId);
    void createCups(const std::vector<int>& continentalEntrants={});
    void progressCups();
    void addCupMatch(int competition,int round,int day,int home,int away);
    std::string matchLabel(const Match& match) const;
    void archiveLeagueChampions();
    void pickLineup(int clubId);
    void initializeLineups(LiveMatch& live, const Match& match) const;
    int lineupStrength(const std::vector<int>& ids) const;
    void newSeason();
    void dailyTick();
    int rating(const Club& club) const;
    void playMinute(LiveMatch& match, bool detailed);
    void finalizeRatings(LiveMatch& match);
    void applyMatchDiscipline(const Match& match);
    MatchPlayerStat* statFor(LiveMatch& match,int playerId);
    const Match* lastPlayedMatchForClub() const;
    void finishMatch(LiveMatch& match);
    void simulateOtherMatches();
    void simulateFast(Match& match);
    bool canFinance(int amount);
    bool permissionSports() const;
    bool permissionBusiness() const;
    void addMessage(std::string title, std::string body);
};
} // namespace fu
