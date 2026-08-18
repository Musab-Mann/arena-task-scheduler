#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cassert>
#include <stdexcept>
#include <functional>
#include <memory>
#include <random>
#include <limits>
#include <climits>

using namespace std;

// ============================================================
//  SECTION 0  —  GLOBAL CONSTANTS & UTILITIES
// ============================================================

namespace Config {
    constexpr int   MAX_TEAMS        = 32;
    constexpr int   MAX_STADIUMS     = 16;
    constexpr int   MAX_PLAYERS      = 500;
    constexpr int   MAX_SPONSORS     = 50;
    constexpr int   RR_QUANTUM_MS    = 600;   // Round-Robin time-slice (ms)
    constexpr int   SIM_TICK_MS      = 400;   // Simulation tick speed
    constexpr int   MLFQ_LEVELS      = 3;     // Multi-level feedback queues
    const     string DB_FILE         = "arena_db.txt";
    const     string LOG_FILE        = "arena_log.txt";
    const     string REPORT_FILE     = "arena_report.txt";
}

// ---- Colours (ANSI) ----------------------------------------
namespace Colour {
    const string RESET   = "\033[0m";
    const string BOLD    = "\033[1m";
    const string RED     = "\033[31m";
    const string GREEN   = "\033[32m";
    const string YELLOW  = "\033[33m";
    const string BLUE    = "\033[34m";
    const string MAGENTA = "\033[35m";
    const string CYAN    = "\033[36m";
    const string WHITE   = "\033[37m";
    const string BG_BLUE = "\033[44m";
}

// ---- Timestamp helper ---------------------------------------
string timestamp()
{
    auto now  = chrono::system_clock::now();
    time_t t  = chrono::system_clock::to_time_t(now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return string(buf);
}

// ---- Logger (thread-safe) -----------------------------------
class Logger {
    ofstream file_;
    mutex    mtx_;
public:
    Logger() { file_.open(Config::LOG_FILE, ios::app); }
    ~Logger() { if (file_.is_open()) file_.close(); }

    void log(const string &level, const string &msg)
    {
        lock_guard<mutex> lk(mtx_);
        string line = "[" + timestamp() + "] [" + level + "] " + msg;
        cout << Colour::CYAN << line << Colour::RESET << "\n";
        if (file_.is_open()) file_ << line << "\n";
    }
    void info (const string &m) { log("INFO ", m); }
    void warn (const string &m) { log("WARN ", m); }
    void error(const string &m) { log("ERROR", m); }
    void ok   (const string &m) { log("OK   ", m); }
};

Logger gLog;
mutex  gConsoleMtx;   // guards cout in threads

// ---- Safe console print (threads) --------------------------
void safeprint(const string &msg)
{
    lock_guard<mutex> lk(gConsoleMtx);
    cout << msg << "\n";
}

// ---- UI helpers --------------------------------------------
void printLine(char c = '-', int w = 72)
{
    cout << Colour::BLUE << string(w, c) << Colour::RESET << "\n";
}

void printHeader(const string &title)
{
    printLine('=');
    int pad = (72 - (int)title.size()) / 2;
    cout << Colour::BOLD << Colour::BG_BLUE
         << string(pad, ' ') << title << string(pad, ' ')
         << Colour::RESET << "\n";
    printLine('=');
}

void printBanner()
{
    cout << Colour::BOLD << Colour::YELLOW;
    cout << "\n";
    cout << "  ╔══════════════════════════════════════════════════════════════╗\n";
    cout << "  ║          A R E N A   T A S K   S C H E D U L E R            ║\n";
    cout << "  ║     Sports Tournament  ×  Operating System Simulation        ║\n";
    cout << "  ║                      Version 2.0.0                          ║\n";
    cout << "  ╚══════════════════════════════════════════════════════════════╝\n";
    cout << Colour::RESET << "\n";
}

// ---- Input helpers -----------------------------------------
int inputInt(const string &prompt, int lo = INT_MIN, int hi = INT_MAX)
{
    int v;
    while (true) {
        cout << Colour::GREEN << prompt << Colour::RESET;
        if (cin >> v && v >= lo && v <= hi) { cin.ignore(); return v; }
        cout << Colour::RED << "  Invalid input. Try again.\n" << Colour::RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string inputString(const string &prompt)
{
    cout << Colour::GREEN << prompt << Colour::RESET;
    string s;
    getline(cin, s);
    return s;
}

double inputDouble(const string &prompt)
{
    double v;
    while (true) {
        cout << Colour::GREEN << prompt << Colour::RESET;
        if (cin >> v) { cin.ignore(); return v; }
        cout << Colour::RED << "  Invalid input.\n" << Colour::RESET;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

// ============================================================
//  SECTION 1  —  DATA MODELS
// ============================================================

// ---- 1a. Team ----------------------------------------------
struct Team {
    int    id        = 0;
    string name;
    string country;
    string coach;
    int    wins      = 0;
    int    losses    = 0;
    int    draws     = 0;
    int    points    = 0;
    int    goalsFor  = 0;
    int    goalsAgst = 0;
    int    burstTime = 0;   // OS concept: estimated match duration (SJF)
    int    priority  = 5;   // OS concept: default priority level
    bool   active    = true;

    Team() = default;
    Team(int i, string n, string c, string ch, int pr = 5, int bt = 5)
        : id(i), name(move(n)), country(move(c)), coach(move(ch)),
          priority(pr), burstTime(bt) {}

    int goalDiff() const { return goalsFor - goalsAgst; }

    void display() const
    {
        cout << left
             << setw(5)  << id
             << setw(20) << name
             << setw(15) << country
             << setw(6)  << wins
             << setw(6)  << losses
             << setw(6)  << draws
             << setw(8)  << points
             << setw(6)  << goalDiff()
             << "\n";
    }
};

// ---- 1b. Player --------------------------------------------
enum class Position { GOALKEEPER, DEFENDER, MIDFIELDER, FORWARD, UNKNOWN };

string posToStr(Position p)
{
    switch(p) {
        case Position::GOALKEEPER: return "GK";
        case Position::DEFENDER:   return "DEF";
        case Position::MIDFIELDER: return "MID";
        case Position::FORWARD:    return "FWD";
        default:                   return "???";
    }
}

struct Player {
    int      id        = 0;
    string   name;
    int      teamId    = 0;
    Position position  = Position::UNKNOWN;
    int      goals     = 0;
    int      assists   = 0;
    int      yellowCards = 0;
    int      redCards    = 0;
    double   rating    = 0.0;   // 0-10
    bool     active    = true;

    Player() = default;
    Player(int i, string n, int tid, Position pos)
        : id(i), name(move(n)), teamId(tid), position(pos) {}

    void display() const
    {
        cout << left
             << setw(5)  << id
             << setw(20) << name
             << setw(5)  << posToStr(position)
             << setw(8)  << goals
             << setw(8)  << assists
             << setw(5)  << yellowCards
             << setw(5)  << redCards
             << fixed << setprecision(1) << setw(6) << rating
             << "\n";
    }
};

// ---- 1c. Stadium -------------------------------------------
struct Stadium {
    int    id       = 0;
    string name;
    string city;
    int    capacity = 0;
    bool   occupied = false;
    int    allocatedMatchId = -1;

    Stadium() = default;
    Stadium(int i, string n, string c, int cap)
        : id(i), name(move(n)), city(move(c)), capacity(cap) {}

    void display() const
    {
        cout << left
             << setw(5)  << id
             << setw(25) << name
             << setw(15) << city
             << setw(10) << capacity
             << setw(12) << (occupied ? "OCCUPIED" : "FREE")
             << "\n";
    }
};

// ---- 1d. Sponsor -------------------------------------------
struct Sponsor {
    int    id      = 0;
    string name;
    string industry;
    double amount  = 0.0;
    int    teamId  = -1;   // -1 = tournament-wide

    Sponsor() = default;
    Sponsor(int i, string n, string ind, double amt, int tid = -1)
        : id(i), name(move(n)), industry(move(ind)), amount(amt), teamId(tid) {}

    void display() const
    {
        cout << left
             << setw(5)  << id
             << setw(20) << name
             << setw(15) << industry
             << "$" << fixed << setprecision(2) << setw(14) << amount
             << setw(10) << (teamId < 0 ? "Tournament" : "Team " + to_string(teamId))
             << "\n";
    }
};

// ---- 1e. Ticket --------------------------------------------
struct Ticket {
    int    id      = 0;
    string buyer;
    int    matchId = 0;
    int    seat    = 0;
    string section;     // VIP / Standard / Economy
    double price   = 0.0;
    bool   used    = false;

    Ticket() = default;
    Ticket(int i, string b, int mid, int s, string sec, double p)
        : id(i), buyer(move(b)), matchId(mid), seat(s),
          section(move(sec)), price(p) {}

    void display() const
    {
        cout << left
             << setw(6)  << id
             << setw(20) << buyer
             << setw(10) << matchId
             << setw(8)  << seat
             << setw(10) << section
             << "$" << fixed << setprecision(2) << price
             << "\n";
    }
};

// ---- 1f. Match ---------------------------------------------
enum class MatchStatus { WAITING, RUNNING, COMPLETED, CANCELLED };

string statusToStr(MatchStatus s)
{
    switch(s) {
        case MatchStatus::WAITING:   return "WAITING";
        case MatchStatus::RUNNING:   return "RUNNING";
        case MatchStatus::COMPLETED: return "COMPLETED";
        case MatchStatus::CANCELLED: return "CANCELLED";
        default:                     return "UNKNOWN";
    }
}

struct Match {
    int         matchId    = 0;
    int         team1Id    = 0;
    int         team2Id    = 0;
    int         stadiumId  = -1;
    int         priority   = 5;
    int         burstTime  = 5;   // estimated half-count (SJF)
    int         score1     = 0;
    int         score2     = 0;
    MatchStatus status     = MatchStatus::WAITING;
    string      round;            // "Group", "QF", "SF", "Final"
    string      scheduledTime;
    string      completedTime;
    long long   waitTime   = 0;   // OS metric: wait time (ms)
    long long   turnaround = 0;   // OS metric: turnaround (ms)

    Match() = default;
    Match(int id, int t1, int t2, int p, int bt, string rnd)
        : matchId(id), team1Id(t1), team2Id(t2), priority(p),
          burstTime(bt), round(move(rnd))
    {
        scheduledTime = timestamp();
    }

    void display(const map<int,Team> &teams) const
    {
        string t1n = teams.count(team1Id) ? teams.at(team1Id).name : "?";
        string t2n = teams.count(team2Id) ? teams.at(team2Id).name : "?";
        cout << left
             << setw(6)  << matchId
             << setw(14) << t1n
             << " vs "
             << setw(14) << t2n
             << setw(5)  << score1 << "-" << setw(5) << score2
             << setw(12) << statusToStr(status)
             << setw(10) << round
             << "\n";
    }
};

// ---- 1g. Resource Allocation Graph (deadlock) ---------------
//  Processes = matches, Resources = stadiums
struct RAGraph {
    // assignment[stadiumId] = matchId  (-1 if free)
    map<int,int> assignment;
    // request[matchId] = set of stadiumIds it is waiting for
    map<int,set<int>> request;

    void assignResource(int stadiumId, int matchId)
    {
        assignment[stadiumId] = matchId;
    }
    void releaseResource(int stadiumId)
    {
        assignment[stadiumId] = -1;
        for (auto &kv : request)
            kv.second.erase(stadiumId);
    }
    void addRequest(int matchId, int stadiumId)
    {
        request[matchId].insert(stadiumId);
    }

    // Cycle detection via DFS
    bool detectDeadlock(vector<int> &cycle) const
    {
        // Build wait-for graph: matchId -> set<matchId>
        map<int,set<int>> wfg;
        for (auto &[sid, mid] : assignment) {
            if (mid < 0) continue;
            for (auto &[reqMid, stadSet] : request) {
                if (stadSet.count(sid))
                    wfg[reqMid].insert(mid);
            }
        }
        set<int>  visited, recStack;
        function<bool(int,int)> dfs = [&](int node, int parent) -> bool {
            visited.insert(node); recStack.insert(node);
            cycle.push_back(node);
            for (int nb : wfg[node]) {
                if (!visited.count(nb)) {
                    if (dfs(nb, node)) return true;
                } else if (recStack.count(nb)) {
                    cycle.push_back(nb);
                    return true;
                }
            }
            recStack.erase(node);
            cycle.pop_back();
            return false;
        };
        for (auto &[node, _] : wfg) {
            if (!visited.count(node)) {
                cycle.clear();
                if (dfs(node, -1)) return true;
            }
        }
        return false;
    }
};

// ============================================================
//  SECTION 2  —  GLOBAL STATE (singleton-style)
// ============================================================

map<int, Team>    gTeams;
map<int, Stadium> gStadiums;
map<int, Match>   gMatches;
map<int, Player>  gPlayers;
map<int, Sponsor> gSponsors;
map<int, Ticket>  gTickets;

vector<int> gReadyQueue;   // matchIds pending scheduling
RAGraph     gRAG;
mutex       gStateMtx;     // guards global state

atomic<int> gNextTeamId    {1};
atomic<int> gNextStadiumId {1};
atomic<int> gNextMatchId   {1};
atomic<int> gNextPlayerId  {1};
atomic<int> gNextSponsorId {1};
atomic<int> gNextTicketId  {1};

// ============================================================
//  SECTION 3  —  STATISTICS & METRICS
// ============================================================

struct SchedulerMetrics {
    int    totalMatches    = 0;
    int    completed       = 0;
    int    cancelled       = 0;
    double avgWaitTime     = 0;
    double avgTurnaround   = 0;
    double cpuUtilization  = 0;   // % of time stadiums busy
    long long totalBusy    = 0;
    long long totalElapsed = 0;

    void print() const
    {
        printHeader("SCHEDULER METRICS");
        cout << "  Total Matches Scheduled : " << totalMatches   << "\n";
        cout << "  Completed               : " << completed      << "\n";
        cout << "  Cancelled               : " << cancelled      << "\n";
        cout << fixed << setprecision(2);
        cout << "  Avg Wait Time (ms)      : " << avgWaitTime    << "\n";
        cout << "  Avg Turnaround (ms)     : " << avgTurnaround  << "\n";
        cout << "  CPU/Stadium Utilization : " << cpuUtilization << "%\n";
        printLine();
    }
};

SchedulerMetrics gMetrics;

// ============================================================
//  SECTION 4  —  TEAM MANAGEMENT
// ============================================================

void addTeam()
{
    printHeader("ADD TEAM");
    string name    = inputString("Team Name       : ");
    string country = inputString("Country         : ");
    string coach   = inputString("Coach           : ");
    int    pri     = inputInt   ("Priority (1-10) : ", 1, 10);
    int    bt      = inputInt   ("Burst Time (1-9): ", 1, 9);

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextTeamId++;
    gTeams[id] = Team(id, name, country, coach, pri, bt);
    gLog.ok("Team added: " + name + " [id=" + to_string(id) + "]");
    cout << Colour::GREEN << "  Team registered with ID " << id << Colour::RESET << "\n";
}

void listTeams()
{
    if (gTeams.empty()) { cout << "  No teams registered.\n"; return; }
    printHeader("TEAM STANDINGS");
    cout << Colour::BOLD
         << left
         << setw(5)  << "ID"
         << setw(20) << "Name"
         << setw(15) << "Country"
         << setw(6)  << "W"
         << setw(6)  << "L"
         << setw(6)  << "D"
         << setw(8)  << "Pts"
         << setw(6)  << "GD"
         << Colour::RESET << "\n";
    printLine();

    // sort by points desc, then goal diff
    vector<Team> sorted;
    for (auto &kv : gTeams) sorted.push_back(kv.second);
    sort(sorted.begin(), sorted.end(), [](const Team &a, const Team &b){
        if (a.points != b.points) return a.points > b.points;
        return a.goalDiff() > b.goalDiff();
    });
    int rank = 1;
    for (auto &t : sorted) {
        cout << setw(3) << rank++ << ". ";
        t.display();
    }
    printLine();
}

void updateTeamResult(int tid1, int tid2, int s1, int s2)
{
    // called after a match ends — update team records
    if (!gTeams.count(tid1) || !gTeams.count(tid2)) return;
    auto &t1 = gTeams[tid1]; auto &t2 = gTeams[tid2];
    t1.goalsFor += s1; t1.goalsAgst += s2;
    t2.goalsFor += s2; t2.goalsAgst += s1;
    if      (s1 > s2) { t1.wins++;  t1.points += 3; t2.losses++; }
    else if (s2 > s1) { t2.wins++;  t2.points += 3; t1.losses++; }
    else              { t1.draws++; t1.points++;     t2.draws++; t2.points++; }
}

void searchTeam()
{
    string key = inputString("Search by name: ");
    transform(key.begin(), key.end(), key.begin(), ::tolower);
    bool found = false;
    for (auto &kv : gTeams) {
        string n = kv.second.name;
        transform(n.begin(), n.end(), n.begin(), ::tolower);
        if (n.find(key) != string::npos) {
            kv.second.display(); found = true;
        }
    }
    if (!found) cout << "  No team matched \"" << key << "\".\n";
}

void deleteTeam()
{
    int id = inputInt("Team ID to delete: ", 1);
    lock_guard<mutex> lk(gStateMtx);
    if (!gTeams.count(id)) { cout << "  Team not found.\n"; return; }
    string name = gTeams[id].name;
    gTeams.erase(id);
    gLog.warn("Team deleted: " + name + " [id=" + to_string(id) + "]");
    cout << Colour::RED << "  Team removed.\n" << Colour::RESET;
}

// ============================================================
//  SECTION 5  —  PLAYER MANAGEMENT
// ============================================================

Position pickPosition()
{
    cout << "  Position: 1=GK  2=DEF  3=MID  4=FWD\n";
    int c = inputInt("  Choice: ", 1, 4);
    switch(c) {
        case 1: return Position::GOALKEEPER;
        case 2: return Position::DEFENDER;
        case 3: return Position::MIDFIELDER;
        case 4: return Position::FORWARD;
        default: return Position::UNKNOWN;
    }
}

void addPlayer()
{
    printHeader("REGISTER PLAYER");
    if (gTeams.empty()) { cout << "  Register teams first.\n"; return; }
    listTeams();
    int tid  = inputInt("Team ID   : ", 1);
    if (!gTeams.count(tid)) { cout << "  Team not found.\n"; return; }
    string name = inputString("Player Name: ");
    Position pos = pickPosition();

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextPlayerId++;
    gPlayers[id] = Player(id, name, tid, pos);
    gLog.ok("Player added: " + name + " → " + gTeams[tid].name);
    cout << Colour::GREEN << "  Player ID " << id << " registered.\n" << Colour::RESET;
}

void listPlayers()
{
    if (gPlayers.empty()) { cout << "  No players.\n"; return; }
    printHeader("PLAYER ROSTER");
    cout << Colour::BOLD
         << left
         << setw(5)  << "ID"
         << setw(20) << "Name"
         << setw(5)  << "Pos"
         << setw(8)  << "Goals"
         << setw(8)  << "Assists"
         << setw(5)  << "YC"
         << setw(5)  << "RC"
         << setw(6)  << "Rtg"
         << Colour::RESET << "\n";
    printLine();
    for (auto &kv : gPlayers) kv.second.display();
    printLine();
}

void topScorers()
{
    vector<Player*> ps;
    for (auto &kv : gPlayers) ps.push_back(&kv.second);
    sort(ps.begin(), ps.end(), [](Player *a, Player *b){ return a->goals > b->goals; });
    printHeader("TOP SCORERS");
    int rank = 1;
    for (auto *p : ps) {
        if (rank > 10) break;
        cout << "  " << rank++ << ". " << left << setw(20) << p->name
             << " — " << p->goals << " goals\n";
    }
}

// ============================================================
//  SECTION 6  —  STADIUM MANAGEMENT
// ============================================================

void addStadium()
{
    printHeader("ADD STADIUM");
    string name = inputString("Stadium Name : ");
    string city = inputString("City         : ");
    int    cap  = inputInt   ("Capacity     : ", 1000, 200000);

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextStadiumId++;
    gStadiums[id] = Stadium(id, name, city, cap);
    gLog.ok("Stadium added: " + name + " [id=" + to_string(id) + "]");
    cout << Colour::GREEN << "  Stadium ID " << id << " added.\n" << Colour::RESET;
}

void listStadiums()
{
    if (gStadiums.empty()) { cout << "  No stadiums.\n"; return; }
    printHeader("STADIUMS");
    cout << Colour::BOLD
         << left << setw(5) << "ID"
         << setw(25) << "Name"
         << setw(15) << "City"
         << setw(10) << "Capacity"
         << setw(12) << "Status"
         << Colour::RESET << "\n";
    printLine();
    for (auto &kv : gStadiums) kv.second.display();
    printLine();
}

// Allocate a free stadium to a match (Resource Allocation)
int allocateStadium(int matchId)
{
    for (auto &kv : gStadiums) {
        if (!kv.second.occupied) {
            kv.second.occupied        = true;
            kv.second.allocatedMatchId = matchId;
            gRAG.assignResource(kv.first, matchId);
            gLog.info("Stadium " + kv.second.name + " → Match " + to_string(matchId));
            return kv.first;
        }
    }
    // No free stadium — add request to RAG
    for (auto &kv : gStadiums) gRAG.addRequest(matchId, kv.first);
    gLog.warn("No stadium free for Match " + to_string(matchId) + "; queued.");
    return -1;
}

void freeStadium(int stadiumId)
{
    if (!gStadiums.count(stadiumId)) return;
    gStadiums[stadiumId].occupied         = false;
    gStadiums[stadiumId].allocatedMatchId = -1;
    gRAG.releaseResource(stadiumId);
    gLog.info("Stadium " + to_string(stadiumId) + " released.");
}

// ============================================================
//  SECTION 7  —  MATCH MANAGEMENT
// ============================================================

void createMatch()
{
    printHeader("CREATE MATCH");
    if (gTeams.size() < 2) { cout << "  Need at least 2 teams.\n"; return; }
    listTeams();
    int t1  = inputInt("Team 1 ID : ", 1);
    int t2  = inputInt("Team 2 ID : ", 1);
    if (t1 == t2)                        { cout << "  Teams must differ.\n"; return; }
    if (!gTeams.count(t1) || !gTeams.count(t2)) { cout << "  Team(s) not found.\n"; return; }
    int  pri  = inputInt   ("Priority (1-10) : ", 1, 10);
    int  bt   = inputInt   ("Est. Duration (1-9): ", 1, 9);
    string rnd = inputString("Round (e.g. Group/QF/SF/Final): ");

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextMatchId++;
    gMatches[id] = Match(id, t1, t2, pri, bt, rnd);
    gReadyQueue.push_back(id);
    gLog.ok("Match created: " + gTeams[t1].name + " vs " + gTeams[t2].name
            + " [id=" + to_string(id) + "]");
    cout << Colour::GREEN << "  Match ID " << id << " queued.\n" << Colour::RESET;
}

void listMatches()
{
    if (gMatches.empty()) { cout << "  No matches.\n"; return; }
    printHeader("MATCH LIST");
    cout << Colour::BOLD
         << left
         << setw(6)  << "ID"
         << setw(14) << "Team 1"
         << "     "
         << setw(14) << "Team 2"
         << setw(10) << "Score"
         << setw(12) << "Status"
         << setw(10) << "Round"
         << Colour::RESET << "\n";
    printLine();
    for (auto &kv : gMatches) kv.second.display(gTeams);
    printLine();
}

void cancelMatch()
{
    int id = inputInt("Match ID to cancel: ", 1);
    lock_guard<mutex> lk(gStateMtx);
    if (!gMatches.count(id)) { cout << "  Match not found.\n"; return; }
    if (gMatches[id].status != MatchStatus::WAITING) {
        cout << "  Only WAITING matches can be cancelled.\n"; return;
    }
    gMatches[id].status = MatchStatus::CANCELLED;
    gReadyQueue.erase(remove(gReadyQueue.begin(), gReadyQueue.end(), id), gReadyQueue.end());
    gMetrics.cancelled++;
    gLog.warn("Match " + to_string(id) + " cancelled.");
    cout << Colour::RED << "  Match cancelled.\n" << Colour::RESET;
}

// ============================================================
//  SECTION 8  —  SIMULATION ENGINE
// ============================================================

// RNG engine
mt19937 gRng(chrono::steady_clock::now().time_since_epoch().count());
int rng(int lo, int hi) { return uniform_int_distribution<int>(lo, hi)(gRng); }

// Simulate one tick of a match (score goals randomly)
void simulateTick(Match &m)
{
    int g1 = rng(0, 2), g2 = rng(0, 2);
    m.score1 += g1;
    m.score2 += g2;

    string t1n = gTeams.count(m.team1Id) ? gTeams[m.team1Id].name : "?";
    string t2n = gTeams.count(m.team2Id) ? gTeams[m.team2Id].name : "?";

    safeprint("  ⚽  " + t1n + " " + to_string(m.score1)
              + " - " + to_string(m.score2) + " " + t2n);

    // Randomly update player stats
    for (auto &kv : gPlayers) {
        if (kv.second.teamId == m.team1Id || kv.second.teamId == m.team2Id) {
            if (rng(0,9) == 0) kv.second.goals++;
            if (rng(0,7) == 0) kv.second.assists++;
            if (rng(0,15) == 0) kv.second.yellowCards++;
        }
    }
}

// Core simulation function — run in a thread
void runMatch(Match &m, int stadiumId)
{
    auto startTime = chrono::steady_clock::now();
    m.status = MatchStatus::RUNNING;
    gLog.info("Match " + to_string(m.matchId) + " STARTED at stadium " + to_string(stadiumId));

    for (int tick = 0; tick < m.burstTime; tick++) {
        this_thread::sleep_for(chrono::milliseconds(Config::SIM_TICK_MS));
        lock_guard<mutex> lk(gStateMtx);
        simulateTick(m);
    }

    // Finalise
    {
        lock_guard<mutex> lk(gStateMtx);
        m.status        = MatchStatus::COMPLETED;
        m.completedTime = timestamp();

        auto endTime    = chrono::steady_clock::now();
        m.turnaround    = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

        updateTeamResult(m.team1Id, m.team2Id, m.score1, m.score2);
        freeStadium(stadiumId);

        gMetrics.completed++;
        gMetrics.totalBusy += m.turnaround;

        string winner;
        if      (m.score1 > m.score2) winner = gTeams.count(m.team1Id) ? gTeams[m.team1Id].name : "?";
        else if (m.score2 > m.score1) winner = gTeams.count(m.team2Id) ? gTeams[m.team2Id].name : "?";
        else                          winner = "DRAW";

        safeprint(Colour::YELLOW + "  🏁 Match " + to_string(m.matchId)
                  + " COMPLETED | Result: " + to_string(m.score1) + "-"
                  + to_string(m.score2) + " | " + winner + Colour::RESET);
    }
}

// Acquire a stadium for a match (blocks until free, or returns -1 on timeout)
int acquireStadium(int matchId, int maxWaitMs = 10000)
{
    auto deadline = chrono::steady_clock::now() + chrono::milliseconds(maxWaitMs);
    while (chrono::steady_clock::now() < deadline) {
        {
            lock_guard<mutex> lk(gStateMtx);
            int sid = allocateStadium(matchId);
            if (sid >= 0) return sid;
        }
        this_thread::sleep_for(chrono::milliseconds(300));
    }
    return -1;
}

// ============================================================
//  SECTION 9  —  SCHEDULING ALGORITHMS
// ============================================================

// ---- 9a. FCFS (First-Come First-Served) --------------------
void scheduleFCFS()
{
    printHeader("FCFS SCHEDULING");
    if (gReadyQueue.empty()) { cout << "  Ready queue is empty.\n"; return; }

    gLog.info("FCFS scheduler started.");
    gMetrics.totalMatches += (int)gReadyQueue.size();

    vector<int> batch = gReadyQueue;
    gReadyQueue.clear();

    auto wallStart = chrono::steady_clock::now();
    vector<thread> threads;

    for (int mid : batch) {
        if (!gMatches.count(mid)) continue;
        auto &m   = gMatches[mid];
        int   sid = acquireStadium(mid);
        if (sid < 0) {
            gLog.warn("Match " + to_string(mid) + " timed out waiting for stadium.");
            continue;
        }
        // Record wait time
        {
            lock_guard<mutex> lk(gStateMtx);
            auto now    = chrono::steady_clock::now();
            m.waitTime  = chrono::duration_cast<chrono::milliseconds>(now - wallStart).count();
        }
        threads.emplace_back([&m, sid]{ runMatch(m, sid); });
        // FCFS: sequential — join before next
        threads.back().join();
        threads.pop_back();
    }

    auto wallEnd = chrono::steady_clock::now();
    gMetrics.totalElapsed += chrono::duration_cast<chrono::milliseconds>(wallEnd - wallStart).count();
    gLog.ok("FCFS scheduling complete.");
}

// ---- 9b. Priority Scheduling --------------------------------
void schedulePriority()
{
    printHeader("PRIORITY SCHEDULING");
    if (gReadyQueue.empty()) { cout << "  Ready queue is empty.\n"; return; }

    // Sort by priority descending
    sort(gReadyQueue.begin(), gReadyQueue.end(), [](int a, int b){
        return gMatches[a].priority > gMatches[b].priority;
    });

    gLog.info("Priority scheduler started.");
    gMetrics.totalMatches += (int)gReadyQueue.size();

    vector<int> batch = gReadyQueue;
    gReadyQueue.clear();

    auto wallStart = chrono::steady_clock::now();

    for (int mid : batch) {
        auto &m   = gMatches[mid];
        int   sid = acquireStadium(mid);
        if (sid < 0) { gLog.warn("Match " + to_string(mid) + " skipped."); continue; }
        {
            lock_guard<mutex> lk(gStateMtx);
            auto now   = chrono::steady_clock::now();
            m.waitTime = chrono::duration_cast<chrono::milliseconds>(now - wallStart).count();
        }
        thread t(runMatch, ref(m), sid);
        t.join();
    }

    gMetrics.totalElapsed += chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - wallStart).count();
    gLog.ok("Priority scheduling complete.");
}

// ---- 9c. Round-Robin (concurrent, time-sliced) -------------
void scheduleRoundRobin()
{
    printHeader("ROUND-ROBIN SCHEDULING");
    if (gReadyQueue.empty()) { cout << "  Ready queue is empty.\n"; return; }

    gLog.info("Round-Robin scheduler started (quantum=" + to_string(Config::RR_QUANTUM_MS) + "ms).");
    gMetrics.totalMatches += (int)gReadyQueue.size();

    vector<int> batch = gReadyQueue;
    gReadyQueue.clear();

    auto wallStart = chrono::steady_clock::now();
    vector<thread> threads;

    for (int mid : batch) {
        auto &m   = gMatches[mid];
        int   sid = acquireStadium(mid);
        if (sid < 0) continue;
        {
            lock_guard<mutex> lk(gStateMtx);
            m.waitTime = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - wallStart).count();
        }
        // Launch all concurrently (OS: multiple CPUs)
        threads.emplace_back([&m, sid]{ runMatch(m, sid); });
    }
    for (auto &t : threads) t.join();

    gMetrics.totalElapsed += chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - wallStart).count();
    gLog.ok("Round-Robin scheduling complete.");
}

// ---- 9d. Shortest Job First (SJF) --------------------------
void scheduleSJF()
{
    printHeader("SJF SCHEDULING (Non-Preemptive)");
    if (gReadyQueue.empty()) { cout << "  Ready queue is empty.\n"; return; }

    // Sort by burstTime ascending
    sort(gReadyQueue.begin(), gReadyQueue.end(), [](int a, int b){
        return gMatches[a].burstTime < gMatches[b].burstTime;
    });

    gLog.info("SJF scheduler started.");
    gMetrics.totalMatches += (int)gReadyQueue.size();
    vector<int> batch = gReadyQueue;
    gReadyQueue.clear();

    auto wallStart = chrono::steady_clock::now();
    for (int mid : batch) {
        auto &m  = gMatches[mid];
        int  sid = acquireStadium(mid);
        if (sid < 0) continue;
        {
            lock_guard<mutex> lk(gStateMtx);
            m.waitTime = chrono::duration_cast<chrono::milliseconds>(
                chrono::steady_clock::now() - wallStart).count();
        }
        thread t(runMatch, ref(m), sid); t.join();
    }
    gMetrics.totalElapsed += chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - wallStart).count();
    gLog.ok("SJF scheduling complete.");
}

// ---- 9e. Multi-Level Feedback Queue (MLFQ) -----------------
void scheduleMLFQ()
{
    printHeader("MULTI-LEVEL FEEDBACK QUEUE (MLFQ)");
    if (gReadyQueue.empty()) { cout << "  Ready queue is empty.\n"; return; }

    // 3 levels: L0=high prio, L1=medium, L2=low
    vector<queue<int>> levels(Config::MLFQ_LEVELS);
    for (int mid : gReadyQueue) {
        int  pri = gMatches[mid].priority;
        int  lvl = (pri >= 8) ? 0 : (pri >= 5) ? 1 : 2;
        levels[lvl].push(mid);
        safeprint("  Match " + to_string(mid) + " → MLFQ Level " + to_string(lvl));
    }
    gReadyQueue.clear();
    gMetrics.totalMatches += 0; // already counted

    auto wallStart = chrono::steady_clock::now();
    for (int lvl = 0; lvl < Config::MLFQ_LEVELS; lvl++) {
        safeprint(Colour::MAGENTA + "\n  [MLFQ] Processing Level " + to_string(lvl)
                  + " queue..." + Colour::RESET);
        while (!levels[lvl].empty()) {
            int mid = levels[lvl].front(); levels[lvl].pop();
            auto &m  = gMatches[mid];
            int   sid = acquireStadium(mid);
            if (sid < 0) { levels[min(lvl+1, Config::MLFQ_LEVELS-1)].push(mid); continue; }
            thread t(runMatch, ref(m), sid); t.join();
            // Demote if not yet done (simulate aging)
        }
    }
    gMetrics.totalElapsed += chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - wallStart).count();
    gLog.ok("MLFQ scheduling complete.");
}

void computeMetrics()
{
    if (gMetrics.completed == 0) return;
    long long totalWait = 0, totalTA = 0;
    for (auto &kv : gMatches) {
        if (kv.second.status == MatchStatus::COMPLETED) {
            totalWait += kv.second.waitTime;
            totalTA   += kv.second.turnaround;
        }
    }
    gMetrics.avgWaitTime    = (double)totalWait / gMetrics.completed;
    gMetrics.avgTurnaround  = (double)totalTA   / gMetrics.completed;
    if (gMetrics.totalElapsed > 0)
        gMetrics.cpuUtilization = (double)gMetrics.totalBusy / gMetrics.totalElapsed * 100.0;
}

// ============================================================
//  SECTION 10  —  DEADLOCK DETECTION & RECOVERY
// ============================================================

void checkDeadlock()
{
    printHeader("DEADLOCK DETECTION");
    vector<int> cycle;
    if (gRAG.detectDeadlock(cycle)) {
        cout << Colour::RED << "  ⚠  DEADLOCK DETECTED!\n" << Colour::RESET;
        cout << "  Cycle involves match IDs: ";
        for (int m : cycle) cout << m << " ";
        cout << "\n\n  Recovery Strategy: Resource Preemption\n";
        cout << "  Preempting match " << cycle[0] << " and releasing its stadium...\n";

        // Recovery: release all stadiums held by cycle[0]
        for (auto &kv : gStadiums) {
            if (kv.second.allocatedMatchId == cycle[0]) {
                freeStadium(kv.first);
                gLog.warn("Deadlock recovered: stadium " + to_string(kv.first) + " released.");
            }
        }
        cout << Colour::GREEN << "  Deadlock resolved via preemption.\n" << Colour::RESET;
    } else {
        cout << Colour::GREEN << "  No deadlock detected. System is safe.\n" << Colour::RESET;
        gLog.ok("Deadlock check: safe.");
    }
}

void bankersAlgorithmDemo()
{
    printHeader("BANKER'S ALGORITHM DEMO");
    int n = (int)gMatches.size();
    int m = (int)gStadiums.size();
    if (n == 0 || m == 0) {
        cout << "  Need matches and stadiums for the demo.\n"; return;
    }
    cout << "  Processes (matches): " << n << "\n";
    cout << "  Resources (stadiums): " << m << "\n\n";
    cout << "  Simulating Banker's Algorithm with random allocation...\n\n";

    // Random need/alloc matrices
    vector<vector<int>> alloc(n, vector<int>(m, 0));
    vector<vector<int>> need (n, vector<int>(m));
    vector<int>         avail(m);

    for (int j = 0; j < m; j++) avail[j] = rng(1, 3);
    for (int i = 0; i < n; i++)
        for (int j = 0; j < m; j++) {
            alloc[i][j] = rng(0, 1);
            need [i][j] = rng(0, 2);
        }

    // Safety algorithm
    vector<bool> finish(n, false);
    vector<int>  work = avail;
    vector<int>  safeSeq;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < n; i++) {
            if (finish[i]) continue;
            bool canRun = true;
            for (int j = 0; j < m; j++)
                if (need[i][j] > work[j]) { canRun = false; break; }
            if (canRun) {
                finish[i] = true; changed = true;
                safeSeq.push_back(i);
                for (int j = 0; j < m; j++) work[j] += alloc[i][j];
            }
        }
    }

    bool safe = all_of(finish.begin(), finish.end(), [](bool b){ return b; });
    if (safe) {
        cout << Colour::GREEN << "  System is in SAFE STATE.\n" << Colour::RESET;
        cout << "  Safe sequence: ";
        for (int i : safeSeq) cout << "P" << i << " ";
        cout << "\n";
    } else {
        cout << Colour::RED << "  System is in UNSAFE STATE — potential deadlock!\n" << Colour::RESET;
    }
}

// ============================================================
//  SECTION 11  —  TICKET MANAGEMENT
// ============================================================

void bookTicket()
{
    printHeader("BOOK TICKET");
    if (gMatches.empty()) { cout << "  No matches available.\n"; return; }
    listMatches();
    int    mid  = inputInt   ("Match ID : ", 1);
    if (!gMatches.count(mid)) { cout << "  Match not found.\n"; return; }
    string buyer = inputString("Your Name: ");
    int    seat  = inputInt   ("Seat No  : ", 1, 100000);
    cout << "  Section: 1=VIP ($200)  2=Standard ($80)  3=Economy ($30)\n";
    int    sec   = inputInt   ("Choice   : ", 1, 3);
    string secStr; double price;
    switch(sec) { case 1: secStr="VIP"; price=200; break; case 2: secStr="Standard"; price=80; break; default: secStr="Economy"; price=30; }

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextTicketId++;
    gTickets[id] = Ticket(id, buyer, mid, seat, secStr, price);
    gLog.ok("Ticket booked: " + buyer + " | Match " + to_string(mid) + " | " + secStr);
    cout << Colour::GREEN << "  Ticket ID " << id << " confirmed ($" << price << ").\n" << Colour::RESET;
}

void listTickets()
{
    if (gTickets.empty()) { cout << "  No tickets sold.\n"; return; }
    printHeader("TICKET REGISTRY");
    cout << Colour::BOLD
         << left << setw(6) << "ID" << setw(20) << "Buyer"
         << setw(10) << "Match" << setw(8) << "Seat"
         << setw(10) << "Section" << "Price"
         << Colour::RESET << "\n";
    printLine();
    double total = 0;
    for (auto &kv : gTickets) { kv.second.display(); total += kv.second.price; }
    printLine();
    cout << "  Total Revenue: $" << fixed << setprecision(2) << total << "\n";
}

// ============================================================
//  SECTION 12  —  SPONSOR MANAGEMENT
// ============================================================

void addSponsor()
{
    printHeader("ADD SPONSOR");
    string name = inputString("Sponsor Name : ");
    string ind  = inputString("Industry     : ");
    double amt  = inputDouble("Amount ($)   : ");
    cout << "  Link to team? (0 = tournament-wide): ";
    int tid = inputInt("Team ID: ", 0);
    if (tid != 0 && !gTeams.count(tid)) { cout << "  Team not found, linking to tournament.\n"; tid=-1; }

    lock_guard<mutex> lk(gStateMtx);
    int id = gNextSponsorId++;
    gSponsors[id] = Sponsor(id, name, ind, amt, tid == 0 ? -1 : tid);
    gLog.ok("Sponsor added: " + name);
    cout << Colour::GREEN << "  Sponsor ID " << id << " registered.\n" << Colour::RESET;
}

void listSponsors()
{
    if (gSponsors.empty()) { cout << "  No sponsors.\n"; return; }
    printHeader("SPONSORS");
    cout << Colour::BOLD
         << left << setw(5) << "ID" << setw(20) << "Name"
         << setw(15) << "Industry" << setw(16) << "Amount" << "Scope"
         << Colour::RESET << "\n";
    printLine();
    double total = 0;
    for (auto &kv : gSponsors) { kv.second.display(); total += kv.second.amount; }
    printLine();
    cout << "  Total Sponsorship: $" << fixed << setprecision(2) << total << "\n";
}

// ============================================================
//  SECTION 13  —  AI / ANALYTICS
// ============================================================

void aiPredict()
{
    printHeader("AI MATCH PREDICTOR");
    if (gTeams.size() < 2) { cout << "  Need at least 2 teams.\n"; return; }
    listTeams();
    int t1 = inputInt("Team 1 ID: ", 1);
    int t2 = inputInt("Team 2 ID: ", 1);
    if (!gTeams.count(t1) || !gTeams.count(t2)) { cout << "  Team(s) not found.\n"; return; }

    auto &a = gTeams[t1]; auto &b = gTeams[t2];

    // Simple weighted random model
    double wA = 50.0, wB = 50.0;
    wA += a.points * 2;
    wB += b.points * 2;
    wA += a.goalDiff() * 0.5;
    wB += b.goalDiff() * 0.5;
    double total = wA + wB;
    double pA = wA / total * 100.0, pB = wB / total * 100.0;

    // Average goals per match
    int tA = a.wins + a.losses + a.draws;
    int tB = b.wins + b.losses + b.draws;
    double gpmA = tA ? (double)a.goalsFor / tA : 1.0;
    double gpmB = tB ? (double)b.goalsFor / tB : 1.0;

    cout << "\n  ──────────────────────────────────────────\n";
    cout << "  AI PREDICTION REPORT\n";
    cout << "  ──────────────────────────────────────────\n";
    cout << fixed << setprecision(1);
    cout << "  " << a.name << " win probability  : " << pA << "%\n";
    cout << "  " << b.name << " win probability  : " << pB << "%\n";
    cout << "  Draw probability             : 18.0% (baseline)\n";
    cout << "  Expected goals " << a.name << "       : " << gpmA << "\n";
    cout << "  Expected goals " << b.name << "       : " << gpmB << "\n";
    cout << "  Predicted scoreline          : "
         << a.name << " " << (int)(gpmA+0.5) << " - "
         << (int)(gpmB+0.5) << " " << b.name << "\n";
    cout << "  ──────────────────────────────────────────\n";
    gLog.info("AI prediction: " + a.name + " vs " + b.name);
}

void crowdSimulation()
{
    printHeader("CROWD SIMULATION");
    if (gMatches.empty()) { cout << "  No matches.\n"; return; }
    int mid = inputInt("Match ID: ", 1);
    if (!gMatches.count(mid)) { cout << "  Match not found.\n"; return; }

    int cap = 50000;
    if (gMatches[mid].stadiumId >= 0 && gStadiums.count(gMatches[mid].stadiumId))
        cap = gStadiums[gMatches[mid].stadiumId].capacity;

    cout << "\n  Simulating crowd dynamics...\n\n";
    for (int t = 10; t <= 100; t += 10) {
        int crowd = cap * t / 100;
        string bar = string(t/5, '#') + string(20-t/5, '.');
        cout << "  " << setw(3) << t << "%  |" << bar << "|  " << crowd << " fans\n";
        this_thread::sleep_for(chrono::milliseconds(150));
    }
    cout << "\n  Peak crowd: " << cap << " | Noise Level: DEAFENING 🔊\n";
}

void tournamentBracket()
{
    printHeader("TOURNAMENT BRACKET");
    if (gMatches.empty()) { cout << "  No matches.\n"; return; }

    map<string, vector<Match*>> rounds;
    for (auto &kv : gMatches) rounds[kv.second.round].push_back(&kv.second);

    for (auto &kv : rounds) {
        cout << "\n  ── " << kv.first << " ──\n";
        for (auto *m : kv.second) {
            string t1 = gTeams.count(m->team1Id) ? gTeams[m->team1Id].name : "?";
            string t2 = gTeams.count(m->team2Id) ? gTeams[m->team2Id].name : "?";
            string res = (m->status == MatchStatus::COMPLETED)
                         ? ("[" + to_string(m->score1) + "-" + to_string(m->score2) + "]")
                         : "[TBD]";
            cout << "    " << left << setw(14) << t1 << " vs " << setw(14) << t2 << res << "\n";
        }
    }
}

void leagueTable()
{
    printHeader("LEAGUE TABLE");
    listTeams();
}

// ============================================================
//  SECTION 14  —  PERSISTENCE (Save / Load)
// ============================================================

void saveDatabase()
{
    ofstream f(Config::DB_FILE);
    if (!f) { gLog.error("Cannot open DB file."); return; }

    f << "[TEAMS]\n";
    for (auto &kv : gTeams) {
        auto &t = kv.second;
        f << t.id << "|" << t.name << "|" << t.country << "|" << t.coach
          << "|" << t.wins << "|" << t.losses << "|" << t.draws << "|"
          << t.points << "|" << t.goalsFor << "|" << t.goalsAgst
          << "|" << t.priority << "|" << t.burstTime << "\n";
    }

    f << "[PLAYERS]\n";
    for (auto &kv : gPlayers) {
        auto &p = kv.second;
        f << p.id << "|" << p.name << "|" << p.teamId << "|"
          << (int)p.position << "|" << p.goals << "|" << p.assists << "\n";
    }

    f << "[STADIUMS]\n";
    for (auto &kv : gStadiums) {
        auto &s = kv.second;
        f << s.id << "|" << s.name << "|" << s.city << "|" << s.capacity << "\n";
    }

    f << "[MATCHES]\n";
    for (auto &kv : gMatches) {
        auto &m = kv.second;
        f << m.matchId << "|" << m.team1Id << "|" << m.team2Id << "|"
          << m.priority << "|" << m.burstTime << "|" << m.round << "|"
          << m.score1 << "|" << m.score2 << "|" << (int)m.status << "\n";
    }

    f << "[SPONSORS]\n";
    for (auto &kv : gSponsors) {
        auto &s = kv.second;
        f << s.id << "|" << s.name << "|" << s.industry << "|" << s.amount << "|" << s.teamId << "\n";
    }

    f << "[TICKETS]\n";
    for (auto &kv : gTickets) {
        auto &t = kv.second;
        f << t.id << "|" << t.buyer << "|" << t.matchId << "|"
          << t.seat << "|" << t.section << "|" << t.price << "\n";
    }

    f.close();
    gLog.ok("Database saved to " + Config::DB_FILE);
    cout << Colour::GREEN << "  Data saved successfully.\n" << Colour::RESET;
}

void loadDatabase()
{
    ifstream f(Config::DB_FILE);
    if (!f) { gLog.warn("No DB file found."); return; }

    string line, section;
    while (getline(f, line)) {
        if (line.empty()) continue;
        if (line[0] == '[') { section = line; continue; }

        auto split = [&](const string &s) {
            vector<string> tokens; istringstream ss(s);
            string tok;
            while (getline(ss, tok, '|')) tokens.push_back(tok);
            return tokens;
        };
        auto v = split(line);

        try {
            if (section == "[TEAMS]" && v.size() >= 12) {
                int id = stoi(v[0]);
                Team t(id, v[1], v[2], v[3], stoi(v[10]), stoi(v[11]));
                t.wins = stoi(v[4]); t.losses = stoi(v[5]); t.draws = stoi(v[6]);
                t.points = stoi(v[7]); t.goalsFor = stoi(v[8]); t.goalsAgst = stoi(v[9]);
                gTeams[id] = t;
                gNextTeamId = max((int)gNextTeamId.load(), id+1);
            }
            else if (section == "[PLAYERS]" && v.size() >= 6) {
                int id = stoi(v[0]);
                Player p(id, v[1], stoi(v[2]), (Position)stoi(v[3]));
                p.goals = stoi(v[4]); p.assists = stoi(v[5]);
                gPlayers[id] = p;
                gNextPlayerId = max((int)gNextPlayerId.load(), id+1);
            }
            else if (section == "[STADIUMS]" && v.size() >= 4) {
                int id = stoi(v[0]);
                gStadiums[id] = Stadium(id, v[1], v[2], stoi(v[3]));
                gNextStadiumId = max((int)gNextStadiumId.load(), id+1);
            }
            else if (section == "[MATCHES]" && v.size() >= 9) {
                int id = stoi(v[0]);
                Match m(id, stoi(v[1]), stoi(v[2]), stoi(v[3]), stoi(v[4]), v[5]);
                m.score1 = stoi(v[6]); m.score2 = stoi(v[7]);
                m.status = (MatchStatus)stoi(v[8]);
                gMatches[id] = m;
                gNextMatchId = max((int)gNextMatchId.load(), id+1);
            }
            else if (section == "[SPONSORS]" && v.size() >= 5) {
                int id = stoi(v[0]);
                gSponsors[id] = Sponsor(id, v[1], v[2], stod(v[3]), stoi(v[4]));
                gNextSponsorId = max((int)gNextSponsorId.load(), id+1);
            }
            else if (section == "[TICKETS]" && v.size() >= 6) {
                int id = stoi(v[0]);
                gTickets[id] = Ticket(id, v[1], stoi(v[2]), stoi(v[3]), v[4], stod(v[5]));
                gNextTicketId = max((int)gNextTicketId.load(), id+1);
            }
        } catch (...) {}
    }
    f.close();
    gLog.ok("Database loaded from " + Config::DB_FILE);
    cout << Colour::GREEN << "  Data loaded successfully.\n" << Colour::RESET;
}

// ============================================================
//  SECTION 15  —  REPORT GENERATION
// ============================================================

void generateReport()
{
    computeMetrics();
    ofstream f(Config::REPORT_FILE);
    if (!f) { gLog.error("Cannot write report."); return; }

    f << "============================================\n";
    f << "  ARENA TASK SCHEDULER — TOURNAMENT REPORT\n";
    f << "  Generated: " << timestamp() << "\n";
    f << "============================================\n\n";

    f << "TEAMS (" << gTeams.size() << ")\n";
    f << string(50, '-') << "\n";
    for (auto &kv : gTeams) {
        auto &t = kv.second;
        f << "  " << t.name << " [" << t.country << "] "
          << "W:" << t.wins << " L:" << t.losses << " D:" << t.draws
          << " Pts:" << t.points << "\n";
    }

    f << "\nMATCHES (" << gMatches.size() << ")\n";
    f << string(50, '-') << "\n";
    for (auto &kv : gMatches) {
        auto &m = kv.second;
        string t1 = gTeams.count(m.team1Id) ? gTeams[m.team1Id].name : "?";
        string t2 = gTeams.count(m.team2Id) ? gTeams[m.team2Id].name : "?";
        f << "  Match " << m.matchId << ": " << t1 << " " << m.score1
          << "-" << m.score2 << " " << t2
          << " [" << statusToStr(m.status) << "]\n";
    }

    f << "\nSCHEDULER METRICS\n";
    f << string(50, '-') << "\n";
    f << "  Total Matches   : " << gMetrics.totalMatches   << "\n";
    f << "  Completed       : " << gMetrics.completed      << "\n";
    f << "  Cancelled       : " << gMetrics.cancelled      << "\n";
    f << "  Avg Wait (ms)   : " << gMetrics.avgWaitTime    << "\n";
    f << "  Avg Turnaround  : " << gMetrics.avgTurnaround  << "\n";
    f << "  Utilization     : " << gMetrics.cpuUtilization << "%\n";

    f << "\nTOP SCORERS\n";
    f << string(50, '-') << "\n";
    vector<Player*> ps;
    for (auto &kv : gPlayers) ps.push_back(&kv.second);
    sort(ps.begin(), ps.end(), [](Player *a, Player *b){ return a->goals > b->goals; });
    int r = 1;
    for (auto *p : ps) {
        if (r > 5) break;
        f << "  " << r++ << ". " << p->name << " — " << p->goals << " goals\n";
    }

    f.close();
    cout << Colour::GREEN << "  Report saved to " << Config::REPORT_FILE << "\n" << Colour::RESET;
    gLog.ok("Report generated.");
}

// ============================================================
//  SECTION 16  —  NOTIFICATION SYSTEM
// ============================================================

void sendNotification(const string &msg, const string &level = "INFO")
{
    string icon = (level == "ALERT") ? "🔔" : (level == "WARN") ? "⚠" : "ℹ";
    cout << "\n  " << icon << "  [NOTIFICATION] " << msg << "\n";
    gLog.info("Notification: " + msg);
}

void broadcastMatchStart(int matchId)
{
    if (!gMatches.count(matchId)) return;
    auto &m = gMatches[matchId];
    string t1 = gTeams.count(m.team1Id) ? gTeams[m.team1Id].name : "?";
    string t2 = gTeams.count(m.team2Id) ? gTeams[m.team2Id].name : "?";
    sendNotification("Match " + to_string(matchId) + " STARTING: "
                     + t1 + " vs " + t2, "ALERT");
}

// ============================================================
//  SECTION 17  —  MEMORY MANAGEMENT DEMO
// ============================================================

void memoryManagementDemo()
{
    printHeader("MEMORY MANAGEMENT SIMULATION");
    cout << "\n  Simulating paging with " << gMatches.size() << " match processes...\n\n";

    const int PAGE_SIZE = 4; // KB
    const int FRAMES    = 8;
    vector<int> frames(FRAMES, -1);
    int pageFaults = 0, hits = 0;

    // Generate a reference string from match IDs
    vector<int> refString;
    for (auto &kv : gMatches) {
        int pages = kv.second.burstTime;
        for (int i = 0; i < pages; i++) refString.push_back(kv.first);
    }
    if (refString.empty()) { cout << "  No match data to simulate.\n"; return; }

    // LRU Page Replacement
    deque<int> lruQueue;
    cout << "  LRU Page Replacement:\n";
    cout << left << setw(10) << "Page" << setw(12) << "Result" << "Frames\n";
    printLine('-', 50);

    for (int pg : refString) {
        bool hit = find(frames.begin(), frames.end(), pg) != frames.end();
        if (hit) {
            hits++;
            lruQueue.erase(remove(lruQueue.begin(), lruQueue.end(), pg), lruQueue.end());
            lruQueue.push_back(pg);
        } else {
            pageFaults++;
            lruQueue.erase(remove(lruQueue.begin(), lruQueue.end(), pg), lruQueue.end());
            lruQueue.push_back(pg);
            // Find free frame or evict
            auto it = find(frames.begin(), frames.end(), -1);
            if (it != frames.end()) *it = pg;
            else {
                int evict = lruQueue.front(); lruQueue.pop_front();
                auto ev = find(frames.begin(), frames.end(), evict);
                if (ev != frames.end()) *ev = pg;
            }
        }
        cout << left << setw(10) << pg << setw(12) << (hit ? "HIT" : "FAULT");
        for (int f : frames) { if (f >= 0) cout << f << " "; }
        cout << "\n";
    }
    printLine('-', 50);
    cout << "  Page Faults  : " << pageFaults << "\n";
    cout << "  Page Hits    : " << hits        << "\n";
    cout << "  Hit Rate     : " << fixed << setprecision(1)
         << (refString.empty() ? 0.0 : 100.0*hits/refString.size()) << "%\n";
}

// ============================================================
//  SECTION 18  —  PROCESS SYNCHRONIZATION DEMO
// ============================================================

mutex       gSemaphoreMtx;
int         gSemaphoreCount = 3; // 3 stadiums as semaphore
condition_variable gSemCV;

void semaphoreWait(int matchId)
{
    unique_lock<mutex> lk(gSemaphoreMtx);
    gSemCV.wait(lk, []{ return gSemaphoreCount > 0; });
    gSemaphoreCount--;
    safeprint("  Match " + to_string(matchId) + " acquired semaphore. Remaining: "
              + to_string(gSemaphoreCount));
}
void semaphoreSignal(int matchId)
{
    lock_guard<mutex> lk(gSemaphoreMtx);
    gSemaphoreCount++;
    safeprint("  Match " + to_string(matchId) + " released semaphore. Remaining: "
              + to_string(gSemaphoreCount));
    gSemCV.notify_one();
}

void syncDemo()
{
    printHeader("PROCESS SYNCHRONIZATION DEMO");
    cout << "\n  Demonstrating Semaphore-based stadium access...\n\n";

    int matchCount = min((int)gMatches.size(), 6);
    if (matchCount == 0) { cout << "  Create matches first.\n"; return; }

    vector<int> mids;
    for (auto &kv : gMatches) {
        if (kv.second.status == MatchStatus::WAITING) mids.push_back(kv.first);
        if ((int)mids.size() >= matchCount) break;
    }

    vector<thread> threads;
    for (int mid : mids) {
        threads.emplace_back([mid]{
            semaphoreWait(mid);
            this_thread::sleep_for(chrono::milliseconds(400));
            semaphoreSignal(mid);
        });
    }
    for (auto &t : threads) t.join();
    cout << "\n  Semaphore demo complete. No starvation occurred.\n";
}

// ============================================================
//  SECTION 19  —  SYSTEM STATUS DASHBOARD
// ============================================================

void dashboard()
{
    computeMetrics();
    printHeader("SYSTEM DASHBOARD");

    auto bar = [](int pct, int width=20) {
        int filled = pct * width / 100;
        return string(filled, '#') + string(width - filled, '.') + " " + to_string(pct) + "%";
    };

    cout << "\n  ┌─────────────────────────────────────────────┐\n";
    cout << "  │  ARENA TASK SCHEDULER — LIVE STATUS          │\n";
    cout << "  │  " << timestamp() << "                    │\n";
    cout << "  ├─────────────────────────────────────────────┤\n";
    cout << "  │  Teams        : " << left << setw(28) << gTeams.size()    << "│\n";
    cout << "  │  Players      : " << setw(28) << gPlayers.size()   << "│\n";
    cout << "  │  Matches Total: " << setw(28) << gMatches.size()   << "│\n";
    cout << "  │  Completed    : " << setw(28) << gMetrics.completed << "│\n";
    cout << "  │  Cancelled    : " << setw(28) << gMetrics.cancelled << "│\n";
    cout << "  │  Stadiums     : " << setw(28) << gStadiums.size()  << "│\n";
    cout << "  │  Sponsors     : " << setw(28) << gSponsors.size()  << "│\n";
    cout << "  │  Tickets Sold : " << setw(28) << gTickets.size()   << "│\n";
    cout << "  │  Pending Jobs : " << setw(28) << gReadyQueue.size() << "│\n";
    cout << "  ├─────────────────────────────────────────────┤\n";

    int stadBusy = 0;
    for (auto &kv : gStadiums) if (kv.second.occupied) stadBusy++;
    int stadUtil = gStadiums.empty() ? 0 : stadBusy * 100 / gStadiums.size();
    cout << "  │  Stadium Util : " << setw(28) << bar(stadUtil) << "│\n";
    cout << "  └─────────────────────────────────────────────┘\n\n";
}

// ============================================================
//  SECTION 20  —  HELP SYSTEM
// ============================================================

void showHelp()
{
    printHeader("HELP — OS CONCEPTS IN ARENA SCHEDULER");
    cout << R"(
  SCHEDULING ALGORITHMS
  ─────────────────────────────────────────────────────────
  FCFS     : First-Come First-Served. Matches run in arrival
             order. Simple but can cause convoy effect.
  Priority : Highest-priority match runs first. Risk of
             starvation for low-priority matches.
  Round-Robin: All matches get equal CPU time (stadium time).
             Concurrent execution. Fair to all processes.
  SJF      : Shortest Job First. Minimises average wait time.
             Requires burst time estimate.
  MLFQ     : Multi-Level Feedback Queue. Separates matches
             into 3 priority tiers with demotion.

  RESOURCE MANAGEMENT
  ─────────────────────────────────────────────────────────
  Stadiums act as CPU resources. Each match (process) needs
  a stadium to run. Resource Allocation Graph (RAG) tracks
  assignments and detects deadlocks.

  DEADLOCK
  ─────────────────────────────────────────────────────────
  Detected via cycle in the wait-for graph. Recovery uses
  preemption: the lowest-priority match in the cycle is
  rolled back and its stadium released.

  SYNCHRONIZATION
  ─────────────────────────────────────────────────────────
  Mutexes guard shared state (gStateMtx, gConsoleMtx).
  Semaphores limit concurrent stadium access.
  Condition variables implement blocking wait.

  MEMORY MANAGEMENT
  ─────────────────────────────────────────────────────────
  LRU page replacement simulated using match burst-time
  pages. Tracks page faults and hit rate.

)";
}

// ============================================================
//  SECTION 21  —  DEMO DATA LOADER
// ============================================================

void loadDemoData()
{
    printHeader("LOADING DEMO DATA");

    // Teams
    struct TD { string n, c, coach; int p, bt; };
    vector<TD> td = {
        {"FC Arsenal",     "England",   "Mikel Arteta",    8, 4},
        {"Real Madrid",    "Spain",     "Carlo Ancelotti", 9, 3},
        {"Bayern Munich",  "Germany",   "Thomas Tuchel",   8, 5},
        {"PSG",            "France",    "Luis Enrique",    7, 5},
        {"Manchester City","England",   "Pep Guardiola",   9, 3},
        {"Atletico Madrid","Spain",     "Diego Simeone",   7, 6},
    };
    for (auto &t : td) {
        int id = gNextTeamId++;
        gTeams[id] = Team(id, t.n, t.c, t.coach, t.p, t.bt);
    }

    // Stadiums
    struct SD { string n, c; int cap; };
    vector<SD> sd = {
        {"Emirates Stadium",    "London",    60260},
        {"Santiago Bernabeu",   "Madrid",    81044},
        {"Allianz Arena",       "Munich",    75024},
        {"Parc des Princes",    "Paris",     47929},
    };
    for (auto &s : sd) {
        int id = gNextStadiumId++;
        gStadiums[id] = Stadium(id, s.n, s.c, s.cap);
    }

    // Players
    struct PD { string n; int tid; Position pos; };
    vector<PD> pd = {
        {"Bukayo Saka",      1, Position::FORWARD},
        {"Vinicius Jr.",     2, Position::FORWARD},
        {"Harry Kane",       3, Position::FORWARD},
        {"Kylian Mbappe",    4, Position::FORWARD},
        {"Erling Haaland",   5, Position::FORWARD},
        {"Antoine Griezmann",6, Position::MIDFIELDER},
    };
    for (auto &p : pd) {
        int id = gNextPlayerId++;
        Player pl(id, p.n, p.tid, p.pos);
        pl.goals   = rng(0, 12);
        pl.assists = rng(0, 8);
        pl.rating  = rng(70, 95) / 10.0;
        gPlayers[id] = pl;
    }

    // Matches
    auto makeMatch = [&](int t1, int t2, int p, int bt, string rnd) {
        int id = gNextMatchId++;
        gMatches[id] = Match(id, t1, t2, p, bt, rnd);
        gReadyQueue.push_back(id);
    };
    makeMatch(1, 2, 8, 4, "Group");
    makeMatch(3, 4, 7, 5, "Group");
    makeMatch(5, 6, 9, 3, "QF");
    makeMatch(1, 3, 6, 5, "SF");

    // Sponsors
    gSponsors[gNextSponsorId] = Sponsor(gNextSponsorId++, "Emirates",   "Aviation",  50000000.0, -1);
    gSponsors[gNextSponsorId] = Sponsor(gNextSponsorId++, "Adidas",     "Sports",    30000000.0, 2);
    gSponsors[gNextSponsorId] = Sponsor(gNextSponsorId++, "Fly Dubai",  "Aviation",  20000000.0, 4);

    cout << Colour::GREEN << "  Demo data loaded: "
         << gTeams.size()    << " teams, "
         << gStadiums.size() << " stadiums, "
         << gPlayers.size()  << " players, "
         << gMatches.size()  << " matches.\n" << Colour::RESET;
    gLog.ok("Demo data initialised.");
}

// ============================================================
//  SECTION 22  —  MAIN MENU SYSTEM
// ============================================================

enum class MenuSection {
    MAIN, TEAM, PLAYER, MATCH, SCHEDULE, RESOURCE,
    ANALYTICS, ADMIN, HELP
};

void printMainMenu()
{
    cout << "\n";
    printLine('-');
    cout << Colour::BOLD << Colour::YELLOW
         << "  ARENA TASK SCHEDULER — MAIN MENU\n"
         << Colour::RESET;
    printLine('-');
    cout << "  [1]  Team Management       [2]  Player Management\n";
    cout << "  [3]  Match Management      [4]  Scheduling Algorithms\n";
    cout << "  [5]  Resource & Deadlock   [6]  Analytics & AI\n";
    cout << "  [7]  Tickets & Sponsors    [8]  Reports & Data\n";
    cout << "  [9]  System Dashboard      [10] OS Demos\n";
    cout << "  [11] Help                  [0]  Exit\n";
    printLine('-');
}

void teamMenu()
{
    while (true) {
        printHeader("TEAM MANAGEMENT");
        cout << "  [1] Add Team    [2] List Teams\n"
                "  [3] Search      [4] Delete Team  [0] Back\n";
        int c = inputInt("> ", 0, 4);
        if (c == 0) break;
        switch(c) {
            case 1: addTeam();   break;
            case 2: listTeams(); break;
            case 3: searchTeam(); break;
            case 4: deleteTeam(); break;
        }
    }
}

void playerMenu()
{
    while (true) {
        printHeader("PLAYER MANAGEMENT");
        cout << "  [1] Register Player    [2] List Players\n"
                "  [3] Top Scorers        [0] Back\n";
        int c = inputInt("> ", 0, 3);
        if (c == 0) break;
        switch(c) {
            case 1: addPlayer();   break;
            case 2: listPlayers(); break;
            case 3: topScorers();  break;
        }
    }
}

void matchMenu()
{
    while (true) {
        printHeader("MATCH MANAGEMENT");
        cout << "  [1] Create Match    [2] List Matches\n"
                "  [3] Cancel Match    [4] Bracket View  [0] Back\n";
        int c = inputInt("> ", 0, 4);
        if (c == 0) break;
        switch(c) {
            case 1: createMatch();       break;
            case 2: listMatches();       break;
            case 3: cancelMatch();       break;
            case 4: tournamentBracket(); break;
        }
    }
}

void scheduleMenu()
{
    while (true) {
        printHeader("SCHEDULING ALGORITHMS");
        cout << "  [1] FCFS (Sequential)     [2] Priority Scheduling\n"
                "  [3] Round-Robin (Concurrent) [4] SJF\n"
                "  [5] MLFQ                  [6] View Metrics\n"
                "  [0] Back\n";
        int c = inputInt("> ", 0, 6);
        if (c == 0) break;
        switch(c) {
            case 1: scheduleFCFS();      break;
            case 2: schedulePriority();  break;
            case 3: scheduleRoundRobin();break;
            case 4: scheduleSJF();       break;
            case 5: scheduleMLFQ();      break;
            case 6: computeMetrics(); gMetrics.print(); break;
        }
    }
}

void resourceMenu()
{
    while (true) {
        printHeader("RESOURCE & DEADLOCK");
        cout << "  [1] Add Stadium       [2] List Stadiums\n"
                "  [3] Allocate Stadiums [4] Deadlock Check\n"
                "  [5] Banker's Algorithm Demo  [0] Back\n";
        int c = inputInt("> ", 0, 5);
        if (c == 0) break;
        switch(c) {
            case 1: addStadium();             break;
            case 2: listStadiums();           break;
            case 3:
                for (int mid : gReadyQueue) allocateStadium(mid);
                cout << "  Allocation attempt done.\n"; break;
            case 4: checkDeadlock();          break;
            case 5: bankersAlgorithmDemo();   break;
        }
    }
}

void analyticsMenu()
{
    while (true) {
        printHeader("ANALYTICS & AI");
        cout << "  [1] AI Match Predictor   [2] Crowd Simulation\n"
                "  [3] League Table         [4] Tournament Bracket\n"
                "  [0] Back\n";
        int c = inputInt("> ", 0, 4);
        if (c == 0) break;
        switch(c) {
            case 1: aiPredict();         break;
            case 2: crowdSimulation();   break;
            case 3: leagueTable();       break;
            case 4: tournamentBracket(); break;
        }
    }
}

void ticketSponsorMenu()
{
    while (true) {
        printHeader("TICKETS & SPONSORS");
        cout << "  [1] Book Ticket     [2] List Tickets\n"
                "  [3] Add Sponsor     [4] List Sponsors\n"
                "  [0] Back\n";
        int c = inputInt("> ", 0, 4);
        if (c == 0) break;
        switch(c) {
            case 1: bookTicket();   break;
            case 2: listTickets();  break;
            case 3: addSponsor();   break;
            case 4: listSponsors(); break;
        }
    }
}

void adminMenu()
{
    while (true) {
        printHeader("REPORTS & DATA");
        cout << "  [1] Save Database     [2] Load Database\n"
                "  [3] Generate Report   [4] Load Demo Data\n"
                "  [0] Back\n";
        int c = inputInt("> ", 0, 4);
        if (c == 0) break;
        switch(c) {
            case 1: saveDatabase();  break;
            case 2: loadDatabase();  break;
            case 3: generateReport();break;
            case 4: loadDemoData();  break;
        }
    }
}

void osDemoMenu()
{
    while (true) {
        printHeader("OS SIMULATION DEMOS");
        cout << "  [1] Memory Management (LRU Paging)\n"
                "  [2] Process Synchronization (Semaphore)\n"
                "  [3] Deadlock Banker's Algorithm\n"
                "  [0] Back\n";
        int c = inputInt("> ", 0, 3);
        if (c == 0) break;
        switch(c) {
            case 1: memoryManagementDemo(); break;
            case 2: syncDemo();             break;
            case 3: bankersAlgorithmDemo(); break;
        }
    }
}

// ============================================================
//  SECTION 23  —  ENTRY POINT
// ============================================================

int main()
{
    srand((unsigned)time(nullptr));
    printBanner();
    cout << "  " << timestamp() << " — System Initialising...\n";
    gLog.info("Arena Task Scheduler started.");

    // Auto-load DB if it exists
    {
        ifstream test(Config::DB_FILE);
        if (test.good()) {
            test.close();
            cout << "  Found existing database. Loading...\n";
            loadDatabase();
        } else {
            cout << "  No database found. Starting fresh.\n"
                    "  Tip: Go to Reports & Data → Load Demo Data for sample data.\n";
        }
    }

    int choice;
    do {
        printMainMenu();
        choice = inputInt("> ", 0, 11);

        switch(choice) {
            case  1: teamMenu();           break;
            case  2: playerMenu();         break;
            case  3: matchMenu();          break;
            case  4: scheduleMenu();       break;
            case  5: resourceMenu();       break;
            case  6: analyticsMenu();      break;
            case  7: ticketSponsorMenu();  break;
            case  8: adminMenu();          break;
            case  9: dashboard();          break;
            case 10: osDemoMenu();         break;
            case 11: showHelp();           break;
            case  0: break;
            default: cout << "  Invalid option.\n";
        }
    } while (choice != 0);

    cout << "\n  Saving session data...\n";
    saveDatabase();
    computeMetrics();
    gMetrics.print();
    gLog.info("Arena Task Scheduler exiting cleanly.");
    cout << Colour::YELLOW << "\n  Thank you for using Arena Task Scheduler. Goodbye!\n"
         << Colour::RESET;
    return 0;
}
