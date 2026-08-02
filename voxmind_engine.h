// ============================================================
//  voxmind_engine.h  —  VoxMind OOP Engine
//  Compatible with: MinGW 64-bit AND MSVC 2022
//
//  KEY FIX: We do NOT include sphelper.h because it requires
//  ATL (atlbase.h) which MinGW does not have.
//  Instead we use CoCreateInstance + SpGetDefaultTokenFromCategoryId
//  directly — this works on ALL compilers.
// ============================================================

#pragma once
#ifndef VOXMIND_ENGINE_H
#define VOXMIND_ENGINE_H
   //prevent multiple inclusion errors

// ── These MUST come before any Windows header ────────────────
#ifndef _AMD64_
#define _AMD64_
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x06010000
#endif

// For warnings
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic ignored "-Wmultichar"
#endif

//Heders
#include <windows.h>
#include <shellapi.h> //For opening relative comand (except restart sleep shutdown etc)
#include <powrprof.h> //For sleep mode
#include <sapi.h>

// We implement what we need from it directly below.

#ifdef __GNUC__
#pragma GCC diagnostic pop //WARNING compiler
#endif

// Standard library
#include <string>
#include <vector>  //dynamic array
#include <algorithm> //transform and swap
#include <sstream>  //build strings
#include <fstream>  //file handling
#include <ctime>   //date and time
#include <cstdlib> //general utility  functions
#include <functional> //std::function, used by the fuzzy command table below

using namespace std;

// ── Utility functions ────────────────────────────────────────
static inline string vmLower(string s) {
    transform(s.begin(), s.end(), s.begin() , ::tolower); //s.begin()-> storage of string at  backend
    return s;
}
static inline bool vmHas(const string& s, const string& sub) {
    return s.find(sub) != string::npos; //serches of sub string inside string if it not exists it will return true
}

// ============================================================
//  LANGUAGE MODE — English / Urdu, switched explicitly by the
//  user (never auto-mixed — see VoxMind upgrade guide for why).
// ============================================================
enum class VoiceLanguage { English, Urdu };

// Detects an explicit "switch to urdu / switch to english" style
// command in whatever text was just recognized. Call this BEFORE
// CommandParser::parse() so a mode-switch phrase isn't mistaken
// for an unknown command.
static inline bool vmCheckLanguageSwitch(const string& rawUtterance, VoiceLanguage& currentLang) {
    string in = vmLower(rawUtterance);
    if (vmHas(in,"switch to urdu") || vmHas(in,"urdu mein") || vmHas(in,"speak urdu")) {
        currentLang = VoiceLanguage::Urdu;
        return true;
    }
    if (vmHas(in,"switch to english") || vmHas(in,"speak english")) {
        currentLang = VoiceLanguage::English;
        return true;
    }
    return false;
}

// ── SAPI helper: get default voice token without sphelper.h ──
static HRESULT VmGetDefaultVoiceToken(ISpObjectToken** ppToken) {
    if (!ppToken) return E_POINTER;
    *ppToken = nullptr;

    ISpObjectTokenCategory* pCat = nullptr;
    HRESULT hr = CoCreateInstance(
        __uuidof(SpObjectTokenCategory), nullptr,
        CLSCTX_ALL,
        __uuidof(ISpObjectTokenCategory),
        (void**)&pCat);
    if (FAILED(hr)) return hr;

    hr = pCat->SetId(SPCAT_VOICES, FALSE);
    if (SUCCEEDED(hr)) {
        IEnumSpObjectTokens* pEnum = nullptr;
        hr = pCat->EnumTokens(nullptr, nullptr, &pEnum);
        if (SUCCEEDED(hr) && pEnum) {
            hr = pEnum->Next(1, ppToken, nullptr);
            pEnum->Release();
        }
    }
    pCat->Release(); //Usage of "This pointer"
    return hr;
}

// ============================================================
//  LOGGER  —  OOP: Encapsulation
// ============================================================
class Logger {
private:
    ofstream logFile; //Restore history
    string   filename;

    string timestamp() {
        time_t now = time(0);
        char buf[32];
#ifdef _MSC_VER
        ctime_s(buf, sizeof(buf), &now);
#else
        // MinGW safe version
        strncpy(buf, ctime(&now), 31);
        buf[31] = '\0';
#endif
        string t(buf);
        while (!t.empty() && (t.back()=='\n'||t.back()=='\r')) t.pop_back();
        return t;
    }

public:
    Logger(const string& f = "voxmind_log.txt") : filename(f) //perameterized constructor
    {
        logFile.open(f, ios::app);
        if (logFile.is_open())
            logFile << "\n=== SESSION: " << timestamp() << " ===\n";
    }

    void log(const string& who, const string& msg) {
        if (logFile.is_open())
            logFile << "[" << who << "] " << msg << "\n";
    }

    ~Logger() {
        if (logFile.is_open()) {
            logFile << "=== END: " << timestamp() << " ===\n";
            logFile.close();
        }
    }
};

// ============================================================
//  ABSTRACT BASE CLASS  —  OOP: Abstraction
// ============================================================
class Command {
public:
    virtual string execute() = 0;
    virtual ~Command() {}

    // Most commands run instantly (volume, opening a fixed app,
    // etc.) and stay synchronous — no change in behavior for those.
    // A command that might do real work (like recursively searching
    // a whole drive) should override this to return true, so
    // MainWindow runs it on a background thread instead of freezing
    // the UI until it finishes.
    virtual bool isAsync() const { return false; }
};

// ============================================================
//  COMMAND CLASSES  —  OOP: Inheritance + Polymorphism
// ============================================================

class GreetCommand : public Command {
public:
    string execute() override {
        return "Hello! I am VoxMind, your personal voice assistant. How can I help you today?";
    }
};

class TimeCommand : public Command {
public:
    string execute() override {
        time_t now = time(0);
        char buf[32];
#ifdef _MSC_VER
        ctime_s(buf, sizeof(buf), &now);
#else
        strncpy(buf, ctime(&now), 31); buf[31]='\0';
#endif
        string t(buf);
        while (!t.empty() && (t.back()=='\n'||t.back()=='\r')) t.pop_back();
        return "The current date and time is: " + t;
    }
};

class JokeCommand : public Command {
private:
    vector<string> jokes;
    vector<int>    order;
    int            pos;

    void reshuffle() {
        order.clear();
        for (int i=0; i<(int)jokes.size(); i++) order.push_back(i);
        for (int i=(int)order.size()-1; i>0; i--) //indexing
        {
            int j = rand()%(i+1); //reshuffiling of jokes order
            swap(order[i], order[j]);
        }
        pos = 0;
    }

public:
    JokeCommand() : pos(0) {
        srand((unsigned)time(0));
        jokes.push_back("Why do programmers prefer dark mode? Because light attracts bugs!");
        jokes.push_back("Why did the programmer quit? Because he did not get arrays!");
        jokes.push_back("What is a computer's favourite snack? Microchips!");
        jokes.push_back("Why do Java developers wear glasses? Because they do not C sharp!");
        jokes.push_back("What did the computer say to the programmer? You can count on me!");
        jokes.push_back("Why was the computer cold? Because it left its Windows open!");
        jokes.push_back("How many programmers to change a light bulb? None, that is a hardware problem!");
        jokes.push_back("Why do programmers mix up Halloween and Christmas? Because Oct 31 equals Dec 25!");
        jokes.push_back("A SQL query walks into a bar and asks two tables: Can I join you?");
        jokes.push_back("Why did the developer go broke? Because he used up all his cache!");
        jokes.push_back("What is a programmer's favourite hangout? The Foo Bar!");
        jokes.push_back("Why do Python programmers wear glasses? Because they cannot C!");
        jokes.push_back("How do you comfort a JavaScript bug? You console it!");
        jokes.push_back("Why do computers never get hungry? They always have plenty of bytes!");
        jokes.push_back("Debugging: being the detective in a crime movie where you are also the murderer.");
        reshuffle();
    }

    string execute() override {
        if (pos >= (int)order.size()) reshuffle();
        return "Here is your joke: " + jokes[order[pos++]];
    }
};

class FactCommand : public Command {
private:
    vector<string> facts;
    vector<int>    order;
    int            pos;

    void reshuffle() {
        order.clear();
        for (int i=0; i<(int)facts.size(); i++) order.push_back(i);
        for (int i=(int)order.size()-1; i>0; i--) {
            int j = rand()%(i+1);
            swap(order[i], order[j]);
        }
        pos = 0;
    }

public:
    FactCommand() : pos(0) {
        srand((unsigned)time(0));
        facts.push_back("Honey never spoils. Archaeologists found 3000 year old honey in Egyptian tombs still edible.");
        facts.push_back("A group of flamingos is called a flamboyance.");
        facts.push_back("Octopuses have three hearts and blue blood.");
        facts.push_back("The first computer bug was an actual moth found inside a Harvard computer in 1947.");
        facts.push_back("Bananas are technically berries, but strawberries are not.");
        facts.push_back("There are more stars in the universe than grains of sand on all Earth beaches combined.");
        facts.push_back("The shortest war in history lasted only 38 minutes, Britain vs Zanzibar in 1896.");
        facts.push_back("A day on Venus is longer than a full year on Venus.");
        facts.push_back("Sharks are older than trees. Sharks 450 million years, trees only 350 million.");
        facts.push_back("Oxford University is older than the Aztec Empire.");
        facts.push_back("A bolt of lightning is five times hotter than the surface of the sun.");
        facts.push_back("Crows can recognize human faces and hold grudges against rude people.");
        facts.push_back("Cleopatra lived closer in time to the Moon landing than to the Great Pyramid.");
        facts.push_back("Water can boil and freeze at the same time. This is called the triple point.");
        facts.push_back("It is impossible to hum while holding your nose. You just tried it, did you not?");
        reshuffle();
    }

    string execute() override {
        if (pos >= (int)order.size()) reshuffle();
        return "Here is a fun fact: " + facts[order[pos++]];
    }
};

class CalculatorCommand : public Command {
private:
    double a, b;
    char   op;
public:
    CalculatorCommand(double x, double y, char o) : a(x), b(y), op(o) {}

    string execute() override {
        double r = 0;
        string w;
        switch (op) {
            case '+': r=a+b; w="plus";       break;
            case '-': r=a-b; w="minus";      break;
            case '*': r=a*b; w="times";      break;
            case '/':
                if (b==0) return "I cannot divide by zero!";
                r=a/b; w="divided by";       break;
            default:  return "I did not understand that calculation.";
        }
        ostringstream oss; //works as cout catering all the dat types
        oss << a << " " << w << " " << b << " equals " << r;
        return oss.str();
    }
};

class OpenURLCommand : public Command {
private:
    string url, name;
public:
    OpenURLCommand(const string& u, const string& n) : url(u), name(n) {}
    string execute() override {
        HINSTANCE r = ShellExecuteA(NULL,"open",url.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               "Opening " + name + " for you!" :
               "Sorry, could not open " + name + ".";
    }
};

class OpenAppCommand : public Command {
private:
    string path, name;
public:
    OpenAppCommand(const string& p, const string& n) : path(p), name(n) {}
    string execute() override {
        HINSTANCE r = ShellExecuteA(NULL,"open",path.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               name + " opened successfully!" :
               "Could not open " + name + ". Make sure it is installed.";
    }
};

// ============================================================
//  OPEN ANYTHING  —  handles site names not covered by the
//  specific OpenURLCommand rules above (e.g. shopping/streaming
//  sites the developer never explicitly listed).
//
//  Strategy: check a small curated list of real URLs first (so
//  well-known sites go straight there); anything not in that
//  list falls back to a Google search for the name, so the user
//  always lands somewhere useful instead of getting an error.
// ============================================================
class OpenAnythingCommand : public Command {
private:
    string query;
public:
    OpenAnythingCommand(const string& q) : query(q) {}
    string execute() override {
        if (query.empty()) return "Please tell me what you would like me to open.";

        string lowerQ = vmLower(query);

        // Curated aliases — add a line here any time you want a
        // specific site to open directly instead of via search.
        static const vector<pair<string,string>> siteAliases = {
            {"daraz","https://www.daraz.pk"},
            {"olx","https://www.olx.com.pk"},
            {"ebay","https://www.ebay.com"},
            {"aliexpress","https://www.aliexpress.com"},
            {"alibaba","https://www.alibaba.com"},
            {"temu","https://www.temu.com"},
            {"shein","https://www.shein.com"},
            {"flipkart","https://www.flipkart.com"},
            {"foodpanda","https://www.foodpanda.pk"},
            {"ubereats","https://www.ubereats.com"},
            {"airbnb","https://www.airbnb.com"},
            {"booking","https://www.booking.com"},
            {"linkedin","https://www.linkedin.com"},
            {"spotify","https://www.spotify.com"},
            {"reddit","https://www.reddit.com"},
        };
        for (auto& site : siteAliases) {
            if (vmHas(lowerQ, site.first)) {
                HINSTANCE r = ShellExecuteA(NULL,"open",site.second.c_str(),NULL,NULL,SW_SHOWNORMAL);
                return ((int)(intptr_t)r > 32) ?
                       "Opening " + query + " for you!" :
                       "Could not open " + query + ".";
            }
        }

        // Not in our curated list — search Google for it instead
        // of failing outright. This is what makes it "open
        // anything": unknown names still land somewhere relevant.
        string url = "https://www.google.com/search?q=";
        for (char c : query) url += (c==' ') ? "+" : string(1,c);
        HINSTANCE r = ShellExecuteA(NULL,"open",url.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               "I don't have " + query + " saved directly, so I searched it on Google for you!" :
               "Could not open browser.";
    }
};

class OpenFolderCommand : public Command {
private:
    string path, name;
public:
    OpenFolderCommand(const string& p, const string& n) : path(p), name(n) {}
    string execute() override {
        HINSTANCE r = ShellExecuteA(NULL,"open",path.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               "Opening " + name + " in Explorer!" :
               "Could not open " + name + ".";
    }
};

class CloseWindowCommand : public Command {
private:
    string title;
public:
    CloseWindowCommand(const string& t) : title(t) {}
    string execute() override {
        HWND h = FindWindowA(NULL, title.c_str()); //HWND->window  close anad window open

        if (h) { PostMessage(h,WM_CLOSE,0,0); return title + " closed successfully!"; }
        return "Could not find window: " + title + ". Make sure it is open.";
    }
};

class WeatherCommand : public Command {
public:
    string execute() override {
        HINSTANCE r = ShellExecuteA(NULL,"open","https://www.weather.com",NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               "Opening weather forecast for you!" :
               "Could not open weather website.";  //usage of ternary operator
    }
};

class ReminderCommand : public Command {
private:
    string reminder;
public:
    ReminderCommand(const string& r) : reminder(r) {}
    string execute() override {
        if (reminder.empty())
            return "Please tell me what to remind you about.";
        return "Got it! I will remind you to: " + reminder + ". Reminder saved!";
    }
};

class CoinFlipCommand : public Command {
public:
    string execute() override {
        srand((unsigned)time(0));
        return (rand()%2==0) ?
               "I flipped a coin and got: Heads!" :
               "I flipped a coin and got: Tails!";
    }
};

class DiceCommand : public Command {
public:
    string execute() override {
        srand((unsigned)time(0));
        int roll = (rand()%6)+1;
        ostringstream oss;
        oss << "I rolled the dice and got: " << roll << "!";
        return oss.str();
    }
};

class VolumeCommand : public Command {
private:
    string action;
public:
    VolumeCommand(const string& a) : action(a) {}
    string execute() override {
        if (action=="up") {
            keybd_event(VK_VOLUME_UP,0,0,0);
            keybd_event(VK_VOLUME_UP,0,KEYEVENTF_KEYUP,0);
            return "Volume up successful!";
        }
        if (action=="down") {
            keybd_event(VK_VOLUME_DOWN,0,0,0);
            keybd_event(VK_VOLUME_DOWN,0,KEYEVENTF_KEYUP,0);
            return "Volume down successful!";
        }
        if (action=="mute") {
            keybd_event(VK_VOLUME_MUTE,0,0,0);
            keybd_event(VK_VOLUME_MUTE,0,KEYEVENTF_KEYUP,0);
            return "Volume muted successfully!";
        }
        return "Say volume up, volume down, or mute.";
    }
};

class ScreenshotCommand : public Command {
public:
    string execute() override {
        keybd_event(VK_SNAPSHOT,0,0,0);
        keybd_event(VK_SNAPSHOT,0,KEYEVENTF_KEYUP,0);
        Sleep(300);
        return "Screenshot taken! Paste with Ctrl+V in Paint or any app.";
    }
};

class SystemCommand : public Command {
private:
    string action;
public:
    SystemCommand(const string& a) : action(a) {}
    string execute() override {
        if (action=="shutdown") { system("shutdown /s /t 30");
            return "Shutting down in 30 seconds. Say cancel shutdown to stop it."; }
        if (action=="restart")  { system("shutdown /r /t 30");
            return "Restarting in 30 seconds. Say cancel shutdown to stop it."; }
        if (action=="cancel")   { system("shutdown /a");
            return "Shutdown cancelled successfully!"; }
        if (action=="lock")     { LockWorkStation();
            return "Computer locked successfully!"; }
        if (action=="sleep")    {
            Sleep(1500);
            SetSuspendState(FALSE,FALSE,FALSE);
            return "Going to sleep now. Goodnight!"; }
        return "Unknown system command.";
    }
};

class SearchCommand : public Command {
private:
    string query;
public:
    SearchCommand(const string& q) : query(q) {}
    string execute() override {
        string url = "https://www.google.com/search?q=";
        string enc;
        for (char c : query) enc += (c==' ') ? "+" : string(1,c);
        url += enc;
        HINSTANCE r = ShellExecuteA(NULL,"open",url.c_str(),NULL,NULL,SW_SHOWNORMAL);
        return ((int)(intptr_t)r > 32) ?
               "Searching Google for " + query + "!" :
               "Could not open browser.";
    }
};

class BatteryCommand : public Command {
public:
    string execute() override {
        SYSTEM_POWER_STATUS s;
        if (GetSystemPowerStatus(&s))
        {
            ostringstream oss;
            if (s.BatteryLifePercent==255)
            {
                oss << "No battery detected. Running on AC power.";
            }
            else
            {
                oss << "Battery is " << (int)s.BatteryLifePercent << " percent.";
                if (s.ACLineStatus==1) oss << " Plugged in and charging.";
                else                   oss << " Running on battery power.";
            }
            return oss.str();
        }
        return "Could not read battery status.";
    }
};

class HelpCommand : public Command {
public:
    string execute() override {
        return
            "SHORT CODES — say the letters:\n"
            "  O=YouTube   G=Google    GH=GitHub   FB=Facebook\n"
            "  IG=Instagram W=Twitter  WA=WhatsApp GM=Gmail\n"
            "  NF=Netflix  AM=Amazon  MAP=Maps    ACC=Weather\n"
            "  CA=Calc  NP=Notepad  PT=Paint  EX=Explorer\n"
            "  D=Word  M=PowerPoint  FT=Excel\n"
            "  DC=DiskC  DD=DiskD  DW=Downloads  DS=Desktop  DO=Docs\n"
            "  VU=VolUp  VD=VolDown  VM=Mute\n"
            "  A=Screenshot  B=Battery  F=Lock  H=Help\n"
            "NUMBER CODES: seven=Shutdown  eight=Restart  nine=Sleep\n"
            "  ten=CancelShut  seventeen=Coin  eighteen=Dice\n"
            "FULL WORDS: exit  joke  fact  time  help  flip  dice";
    }
};
// WHO DESIGNED YOU COMMAND
class WhoDesignedCommand : public Command {
public:
    string execute() override {
        return "I am designed by AUR ( Affaf, Ubaid, Rameeha ). "
               "Thank you AUR for giving me life.";
    }
};
class ExitCommand : public Command {
public:
    string execute() override {
        return "Goodbye! It was great helping you. Have a wonderful day!";
    }
};

// ============================================================
//  SYNONYM NORMALIZER  —  fixes "different wording gives error"
//
//  How it works: before we give up on an unrecognized command,
//  we rewrite common synonym words/phrases to ONE canonical word
//  (e.g. "launch"/"start"/"run" all become "open"), then check
//  the result against a short table of canonical commands.
//
//  To teach VoxMind a new synonym, add ONE line to the table
//  below — you never need to touch CommandParser::parse().
// ============================================================
static const vector<pair<string,string>>& vmSynonymTable() {
    static const vector<pair<string,string>> table = {
        {"launch","open"}, {"start","open"}, {"run","open"}, {"fire up","open"},
        {"shut","close"}, {"kill","close"}, {"end","close"}, {"terminate","close"},
        {"raise","increase"}, {"boost","increase"}, {"pump up","increase"},
        {"lower","decrease"}, {"reduce","decrease"}, {"drop","decrease"},
        {"silence","mute"}, {"quiet","mute"}, {"shush","mute"},
        {"snap","screenshot"}, {"capture screen","screenshot"},
        {"reboot","restart"},
        {"power off","shutdown"}, {"turn off","shutdown"},
        {"hi there","hello"}, {"hey there","hello"},
        {"leave","exit"}, {"bye bye","exit"},
        {"make me laugh","joke"}, {"something funny","joke"},
        {"random fact","fact"}, {"fun fact","fact"},
        {"what time","time"}, {"current time","time"},
        {"how is the weather","weather"}, {"weather report","weather"},
    };
    return table;
}

// Replaces every known synonym word/phrase with its canonical form.
// Padded with spaces on each side so we only match whole words,
// never partial matches inside a longer word.
static inline string vmNormalizeSynonyms(const string& input) {
    string result = " " + vmLower(input) + " ";
    for (auto& pr : vmSynonymTable()) {
        string needle = " " + pr.first + " ";
        string repl   = " " + pr.second + " ";
        size_t pos = 0;
        while ((pos = result.find(needle, pos)) != string::npos) {
            result.replace(pos, needle.size(), repl);
            pos += repl.size();
        }
    }
    size_t start = result.find_first_not_of(' ');
    size_t end   = result.find_last_not_of(' ');
    return (start == string::npos) ? "" : result.substr(start, end - start + 1);
}

// Standard Levenshtein edit distance — used to tolerate small
// speech-recognition slips (e.g. "screenshor" vs "screenshot").
//
// NOTE: we deliberately never call a bare min(...)/max(...) here.
// <windows.h> (already included above) #defines min/max as raw
// text macros, which silently mangles std::min/std::max calls and
// causes exactly the kind of cascading syntax errors you saw.
static inline int vmEditDistance(const string& a, const string& b) {
    size_t n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
    for (size_t i = 0; i <= n; i++) dp[i][0] = (int)i;
    for (size_t j = 0; j <= m; j++) dp[0][j] = (int)j;
    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            int deleteCost = dp[i-1][j] + 1;
            int insertCost = dp[i][j-1] + 1;
            int substCost  = dp[i-1][j-1] + (a[i-1] != b[j-1] ? 1 : 0);
            int best = deleteCost;
            if (insertCost < best) best = insertCost;
            if (substCost  < best) best = substCost;
            dp[i][j] = best;
        }
    }
    return dp[n][m];
}

// Two words "match" if identical, or close enough by edit distance.
// NOTE: words under 4 letters are required to match exactly — this
// is deliberate. Fuzzy-matching short words (e.g. "cat" vs "car")
// causes false positives far more often than it helps, so we only
// apply typo-tolerance to longer, more distinctive words.
static inline bool vmFuzzyWordMatch(const string& w1, const string& w2) {
    if (w1 == w2) return true;
    if (w1.size() < 4 || w2.size() < 4) return false;
    int maxLen = (int)(w1.size() > w2.size() ? w1.size() : w2.size());
    int dist = vmEditDistance(w1, w2);
    return dist <= 1 + maxLen / 6;
}

static inline vector<string> vmTokenize(const string& s) {
    vector<string> out; string cur;
    for (char c : s) {
        if (isalnum((unsigned char)c)) cur += c;
        else { if (!cur.empty()) { out.push_back(cur); cur.clear(); } }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Each entry lists the canonical token(s) that MUST all be present
// (via exact or fuzzy match) for that action to fire. Use a single
// token for actions that are unambiguous on their own ("mute",
// "screenshot", "joke"); use two tokens only when the action word
// alone is too generic ("open" needs to know open WHAT).
struct CanonicalEntry {
    vector<string> tokens;
    string label;                 // for logging/debugging only
    function<Command*()> make;
};

static vector<CanonicalEntry>& vmCanonicalTable() {
    static vector<CanonicalEntry> table = {
        {{"open","browser"}, "OpenBrowser",  []{ return (Command*)new OpenURLCommand("https://www.google.com","Browser"); }},
        {{"open","google"}, "OpenGoogle",    []{ return (Command*)new OpenURLCommand("https://www.google.com","Google"); }},
        {{"open","youtube"}, "OpenYouTube",  []{ return (Command*)new OpenURLCommand("https://www.youtube.com","YouTube"); }},
        {{"open","github"}, "OpenGitHub",    []{ return (Command*)new OpenURLCommand("https://www.github.com","GitHub"); }},
        {{"open","facebook"}, "OpenFacebook",[]{ return (Command*)new OpenURLCommand("https://www.facebook.com","Facebook"); }},
        {{"open","instagram"}, "OpenInstagram",[]{ return (Command*)new OpenURLCommand("https://www.instagram.com","Instagram"); }},
        {{"open","whatsapp"}, "OpenWhatsApp",[]{ return (Command*)new OpenURLCommand("https://web.whatsapp.com","WhatsApp"); }},
        {{"open","gmail"}, "OpenGmail",      []{ return (Command*)new OpenURLCommand("https://mail.google.com","Gmail"); }},
        {{"open","netflix"}, "OpenNetflix",  []{ return (Command*)new OpenURLCommand("https://www.netflix.com","Netflix"); }},
        {{"open","amazon"}, "OpenAmazon",    []{ return (Command*)new OpenURLCommand("https://www.amazon.com","Amazon"); }},
        {{"open","notepad"}, "OpenNotepad",  []{ return (Command*)new OpenAppCommand("notepad.exe","Notepad"); }},
        {{"open","calculator"}, "OpenCalc",  []{ return (Command*)new OpenAppCommand("calc.exe","Calculator"); }},
        {{"open","explorer"}, "OpenExplorer",[]{ return (Command*)new OpenAppCommand("explorer.exe","Explorer"); }},
        {{"increase"}, "VolUp",              []{ return (Command*)new VolumeCommand("up"); }},
        {{"decrease"}, "VolDown",            []{ return (Command*)new VolumeCommand("down"); }},
        {{"mute"}, "Mute",                   []{ return (Command*)new VolumeCommand("mute"); }},
        {{"screenshot"}, "Screenshot",       []{ return (Command*)new ScreenshotCommand(); }},
        {{"battery"}, "Battery",             []{ return (Command*)new BatteryCommand(); }},
        {{"joke"}, "Joke",                   []{ return (Command*)new JokeCommand(); }},
        {{"fact"}, "Fact",                   []{ return (Command*)new FactCommand(); }},
        {{"weather"}, "Weather",             []{ return (Command*)new WeatherCommand(); }},
        {{"lock"}, "Lock",                   []{ return (Command*)new SystemCommand("lock"); }},
        {{"sleep"}, "Sleep",                 []{ return (Command*)new SystemCommand("sleep"); }},
        {{"shutdown"}, "Shutdown",           []{ return (Command*)new SystemCommand("shutdown"); }},
        {{"restart"}, "Restart",             []{ return (Command*)new SystemCommand("restart"); }},
        {{"exit"}, "Exit",                   []{ return (Command*)new ExitCommand(); }},
        {{"help"}, "Help",                   []{ return (Command*)new HelpCommand(); }},
    };
    return table;
}

// Called only when Layers 1-3 in CommandParser::parse() found
// nothing — this is the safety net that catches synonyms and
// rewordings the hardcoded rules above didn't anticipate.
static inline Command* vmFuzzyFallback(const string& raw) {
    string normalized = vmNormalizeSynonyms(raw);
    vector<string> inputTokens = vmTokenize(normalized);
    if (inputTokens.empty()) return nullptr;

    CanonicalEntry* best = nullptr;
    double bestScore = 0.0;

    for (auto& entry : vmCanonicalTable()) {
        int matchedTokens = 0;
        for (auto& reqTok : entry.tokens) {
            for (auto& inTok : inputTokens) {
                if (vmFuzzyWordMatch(reqTok, inTok)) { matchedTokens++; break; }
            }
        }
        double score = (double)matchedTokens / (double)entry.tokens.size();
        if (score > bestScore) { bestScore = score; best = &entry; }
    }

    // Require every token of the canonical phrase to be found —
    // partial matches are intentionally rejected to avoid firing
    // the wrong command on an ambiguous sentence.
    if (best && bestScore >= 0.99) {
        return best->make();
    }
    return nullptr;
}

// ============================================================
//  FILE & APP SEARCH  —  "open anything on my PC"
//
//  Searches your actual files (Desktop/Documents/Downloads) and
//  installed apps (Start Menu shortcuts) for a fuzzy name match,
//  so "open lecture 3 of oop" works without saying which folder
//  or semester it's actually in.
// ============================================================
#include <filesystem>
namespace vmfs = std::filesystem;

static vector<string> vmGetFileSearchRoots() {
    vector<string> roots;
    const char* userProfile = getenv("USERPROFILE");
    if (userProfile) {
        string base = userProfile;
        roots.push_back(base + "\\Desktop");
        roots.push_back(base + "\\Documents");
        roots.push_back(base + "\\Downloads");
        roots.push_back(base + "\\OneDrive\\Desktop");
        roots.push_back(base + "\\OneDrive\\Documents");
    }

    // D: drive — where the "2nd Semester" folder actually lives.
    // vmfs::exists() below just skips this quietly if D: doesn't
    // exist on a given machine, so it's safe to always include.
    roots.push_back("D:\\");

    // Add any other drive/folder your files live in here, e.g.:
    // roots.push_back("E:\\");
    // roots.push_back(string(userProfile) + "\\University");

    return roots;
}

static vector<string> vmGetAppSearchRoots() {
    vector<string> roots;
    const char* appData     = getenv("APPDATA");
    const char* programData = getenv("ProgramData");
    if (appData)     roots.push_back(string(appData) + "\\Microsoft\\Windows\\Start Menu\\Programs");
    if (programData) roots.push_back(string(programData) + "\\Microsoft\\Windows\\Start Menu\\Programs");
    return roots;
}

// Generic words people naturally say that are almost never part of
// an actual file/folder's real name — filtering these out before
// scoring stops them from diluting an otherwise-correct match
// (e.g. "calculus folder" shouldn't score worse than just "calculus").
static inline vector<string> vmStripFillerWords(const vector<string>& tokens) {
    static const vector<string> filler = {
        "folder","file","directory","the","a","an","in","of","on","for",
        "my","please","open","it","that","this","me"
    };
    vector<string> out;
    for (auto& t : tokens) {
        bool isFiller = false;
        for (auto& f : filler) if (t == f) { isFiller = true; break; }
        if (!isFiller) out.push_back(t);
    }
    return out.empty() ? tokens : out; // never strip EVERYTHING away
}

// Shared fuzzy search over a set of root folders. matchExt, if
// given, restricts matching to that extension (used for app search,
// which only cares about .lnk shortcuts). Matches BOTH files and
// folders — searching only files was a bug that stopped "open
// calculus folder" from ever finding an actual folder named
// Calculus. Also checks the immediate parent folder name, so
// "calculus in 2nd semester" matches a Calculus folder that lives
// directly inside a "2nd Semester" folder.
static string vmFuzzySearchFolders(const string& query, const vector<string>& roots,
                                    const string& matchExt = "", int maxDepth = 6) {
    vector<string> queryTokens = vmStripFillerWords(vmTokenize(vmLower(query)));
    if (queryTokens.empty()) return "";

    string bestPath; double bestScore = 0.0;

    for (auto& root : roots) {
        if (root.empty() || !vmfs::exists(root)) continue;
        try {
            for (auto it = vmfs::recursive_directory_iterator(root, vmfs::directory_options::skip_permission_denied);
                 it != vmfs::recursive_directory_iterator(); ++it) {
                if (it.depth() > maxDepth) { it.disable_recursion_pending(); continue; }

                bool isDir  = it->is_directory();
                bool isFile = it->is_regular_file();
                if (!isDir && !isFile) continue;
                if (!matchExt.empty()) {
                    if (!isFile) continue;
                    if (it->path().extension().string() != matchExt) continue;
                }

                string name   = isDir ? it->path().filename().string() : it->path().stem().string();
                string parent = it->path().parent_path().filename().string();
                vector<string> nameTokens = vmTokenize(vmLower(name + " " + parent));
                if (nameTokens.empty()) continue;

                int matched = 0;
                for (auto& qt : queryTokens)
                    for (auto& nt : nameTokens)
                        if (vmFuzzyWordMatch(qt, nt)) { matched++; break; }

                // Score against the SMALLER of the two token counts —
                // this way extra filler/context words on either side
                // ("2nd semester", or a long descriptive filename)
                // don't unfairly drag a genuinely correct match down.
                size_t denom = queryTokens.size() < nameTokens.size() ? queryTokens.size() : nameTokens.size();
                double score = (double)matched / (double)denom;
                if (score > bestScore) { bestScore = score; bestPath = it->path().string(); }
            }
        } catch (...) { continue; }
    }
    // Require most of the smaller token set to be found — avoids
    // opening the wrong file/app on a vague or unrelated request.
    return (bestScore >= 0.7) ? bestPath : "";
}

// ── Browser preference — "on chrome" / "on firefox" / "on edge" ──
// If the user names a browser, we try to launch that one specifically.
// If none is named, or the named one isn't found at its usual install
// path, we fall back to the system default browser.
static string vmDetectPreferredBrowser(const string& lowerFullSentence) {
    if (vmHas(lowerFullSentence,"chrome"))  return "chrome";
    if (vmHas(lowerFullSentence,"firefox")) return "firefox";
    if (vmHas(lowerFullSentence,"edge"))    return "edge";
    return "";
}

static bool vmOpenUrlInBrowser(const string& url, const string& preferredBrowser) {
    auto tryLaunch = [&](const string& exePath) -> bool {
        if (exePath.empty() || !vmfs::exists(exePath)) return false;
        HINSTANCE r = ShellExecuteA(NULL,"open",exePath.c_str(),url.c_str(),NULL,SW_SHOWNORMAL);
        return (int)(intptr_t)r > 32;
    };

    const char* pf   = getenv("ProgramFiles");
    const char* pf86 = getenv("ProgramFiles(x86)");

    if (preferredBrowser == "chrome") {
        if (pf   && tryLaunch(string(pf)   + "\\Google\\Chrome\\Application\\chrome.exe")) return true;
        if (pf86 && tryLaunch(string(pf86) + "\\Google\\Chrome\\Application\\chrome.exe")) return true;
    } else if (preferredBrowser == "firefox") {
        if (pf   && tryLaunch(string(pf)   + "\\Mozilla Firefox\\firefox.exe")) return true;
        if (pf86 && tryLaunch(string(pf86) + "\\Mozilla Firefox\\firefox.exe")) return true;
    } else if (preferredBrowser == "edge") {
        if (pf86 && tryLaunch(string(pf86) + "\\Microsoft\\Edge\\Application\\msedge.exe")) return true;
    }

    // No preference stated, or the named browser wasn't found at
    // its usual install path — fall back to the system default.
    HINSTANCE r = ShellExecuteA(NULL,"open",url.c_str(),NULL,NULL,SW_SHOWNORMAL);
    return (int)(intptr_t)r > 32;
}

// ============================================================
//  SMART OPEN  —  the real "open anything on my PC" command.
//  Resolution order:
//    1. An installed app (Start Menu shortcut) — Office, MovieBox, etc.
//    2. A file on your PC — "lecture 3 of oop", regardless of folder
//    3. The web — a known site alias, or a Google search as last resort
//
//  HONEST LIMITATION: "play the movie I asked for" can only really
//  work for sites with a public search URL — YouTube has one, so
//  that specific case is supported below. Closed platforms like
//  Netflix don't expose a way for outside apps to search-and-play
//  a title, so for those we can open the app/site, but not control
//  what happens inside it.
// ============================================================
class SmartOpenCommand : public Command {
private:
    string query;
    string fullSentence;
public:
    SmartOpenCommand(const string& q, const string& full) : query(q), fullSentence(full) {}

    // This command can recursively scan a whole drive — never run
    // it on the UI thread, or the app will visibly freeze until
    // the search finishes.
    bool isAsync() const override { return true; }

    string execute() override {
        // Extract a "play X" instruction if one was given, so we can
        // report honestly on whether we could follow through on it.
        // Also strips a trailing "on <app>" / "in <app>" clause —
        // that part specifies WHERE to play it (already resolved
        // via query above), it shouldn't be part of the play term
        // itself (e.g. "play gotham series on moviebox" -> just
        // "gotham series", not "gotham series on moviebox").
        string playTerm;
        {
            size_t playPos = vmLower(fullSentence).find("play ");
            if (playPos != string::npos) {
                playTerm = fullSentence.substr(playPos + 5);
                string lowerPlay = vmLower(playTerm);
                size_t onPos = lowerPlay.rfind(" on ");
                size_t inPos = lowerPlay.rfind(" in ");
                size_t cutPos = string::npos;
                if (onPos != string::npos) cutPos = onPos;
                if (inPos != string::npos && (cutPos == string::npos || inPos > cutPos)) cutPos = inPos;
                if (cutPos != string::npos) playTerm = playTerm.substr(0, cutPos);
            }
        }

        // 1) Installed app? (Netflix app, NetMirror, MovieBox, BiliBili, etc.)
        string appPath = vmFuzzySearchFolders(query, vmGetAppSearchRoots(), ".lnk", 4);
        if (!appPath.empty()) {
            HINSTANCE r = ShellExecuteA(NULL,"open",appPath.c_str(),NULL,NULL,SW_SHOWNORMAL);
            if ((int)(intptr_t)r <= 32) return "Found " + query + " but could not launch it.";
            string msg = "Opening " + query + "!";
            if (!playTerm.empty())
                msg += " Note: " + query + " does not support opening a specific title "
                       "automatically from a voice command, so please search for \"" +
                       playTerm + "\" yourself once it opens.";
            return msg;
        }

        // 2) A file on your PC?
        string filePath = vmFuzzySearchFolders(query, vmGetFileSearchRoots(), "", 6);
        if (!filePath.empty()) {
            HINSTANCE r = ShellExecuteA(NULL,"open",filePath.c_str(),NULL,NULL,SW_SHOWNORMAL);
            return ((int)(intptr_t)r > 32) ? "Opening " + query + "!" :
                   "Found the file but could not open it.";
        }

        // 3) Not installed, not a local file — try the web.
        string preferredBrowser = vmDetectPreferredBrowser(vmLower(fullSentence));
        string lowerQ = vmLower(query);

        static const vector<pair<string,string>> siteAliases = {
            {"netflix","https://www.netflix.com"},
            {"youtube","https://www.youtube.com"},
            {"bilibili","https://www.bilibili.tv"},
            {"daraz","https://www.daraz.pk"},
            {"olx","https://www.olx.com.pk"},
            {"ebay","https://www.ebay.com"},
            {"aliexpress","https://www.aliexpress.com"},
            {"alibaba","https://www.alibaba.com"},
            {"temu","https://www.temu.com"},
            {"shein","https://www.shein.com"},
            {"flipkart","https://www.flipkart.com"},
            {"foodpanda","https://www.foodpanda.pk"},
            {"spotify","https://www.spotify.com"},
        };
        for (auto& site : siteAliases) {
            if (vmHas(lowerQ, site.first)) {
                // Honest exception: YouTube supports a real search
                // URL, so "play X" can genuinely search for X there.
                if (site.first == "youtube" && !playTerm.empty()) {
                    string ytUrl = "https://www.youtube.com/results?search_query=";
                    for (char c : playTerm) ytUrl += (c==' ') ? "+" : string(1,c);
                    bool ok = vmOpenUrlInBrowser(ytUrl, preferredBrowser);
                    return ok ? "Searching YouTube for " + playTerm + "!" :
                                "Could not open YouTube.";
                }

                bool ok = vmOpenUrlInBrowser(site.second, preferredBrowser);
                if (!ok) return "Could not open " + query + ".";
                string msg = "I could not find " + query + " installed, so I opened it on the web!";
                if (!playTerm.empty())
                    msg += " Note: " + query + " does not support opening a specific title "
                           "automatically from a voice command, so please search for \"" +
                           playTerm + "\" yourself once it opens.";
                return msg;
            }
        }

        string url = "https://www.google.com/search?q=";
        for (char c : query) url += (c==' ') ? "+" : string(1,c);
        bool ok = vmOpenUrlInBrowser(url, preferredBrowser);
        return ok ?
            "I don't have " + query + " installed, so I searched it on the web for you!" :
            "Could not open browser.";
    }
};

// ============================================================
//  DEBUG SEARCH  —  diagnostic tool. Reports exactly which
//  folders VoxMind is actually searching for files/apps, and
//  whether each one exists on this machine. Say "debug search"
//  or "list search folders" to run it — use this instead of
//  guessing when "open X" can't find something that should exist.
// ============================================================
class DebugSearchCommand : public Command {
public:
    string execute() override {
        ostringstream out;
        out << "FILE SEARCH ROOTS:\n";
        for (auto& root : vmGetFileSearchRoots())
            out << (vmfs::exists(root) ? "[FOUND] " : "[MISSING] ") << root << "\n";

        out << "\nAPP SEARCH ROOTS:\n";
        for (auto& root : vmGetAppSearchRoots())
            out << (vmfs::exists(root) ? "[FOUND] " : "[MISSING] ") << root << "\n";

        const char* userProfile = getenv("USERPROFILE");
        out << "\nUSERPROFILE = " << (userProfile ? userProfile : "(NOT SET — this is likely the problem)");
        return out.str();
    }
};

// ============================================================
//  COMMAND PARSER  —  OOP: Single Responsibility Principle
// ============================================================
class CommandParser {
public:
    Command* parse(const string& raw) {
        string in = vmLower(raw);

        // ── LAYER 1: SHORT CODES ─────────────────────────────

        // apps and websites
        if(in=="o"||in=="oh"||in=="owe"||in=="zero")
            return new OpenURLCommand("https://www.youtube.com","YouTube");
        if(in=="g"||in=="gee"||in=="ji"||in=="jee"||in=="ge")
            return new OpenURLCommand("https://www.google.com","Google");
        if(in=="gh"||in=="g h"||in=="git"||in=="gee h")
            return new OpenURLCommand("https://www.github.com","GitHub");
        if(in=="fb"||in=="f b"||in=="face"||in=="f. b.")
            return new OpenURLCommand("https://www.facebook.com","Facebook");
        if(in=="ig"||in=="i g"||in=="insta"||in=="i. g.")
            return new OpenURLCommand("https://www.instagram.com","Instagram");
        if(in=="w"||in=="double u"||in=="double-u"||in=="tw")
            return new OpenURLCommand("https://www.twitter.com","Twitter");
        if(in=="wa"||in=="w a"||in=="whats"||in=="w. a.")
            return new OpenURLCommand("https://web.whatsapp.com","WhatsApp");
        if(in=="gm"||in=="g m"||in=="g. m.")
            return new OpenURLCommand("https://mail.google.com","Gmail");
        if(in=="nf"||in=="n f"||in=="n. f.")
            return new OpenURLCommand("https://www.netflix.com","Netflix");
        if(in=="am"||in=="a m"||in=="amaz"||in=="a. m.")
            return new OpenURLCommand("https://www.amazon.com","Amazon");
        if(in=="map"||in=="maps"||in=="gmap"||in=="g map")
            return new OpenURLCommand("https://maps.google.com","Google Maps");
        if(in=="acc"||in=="a c c"||in=="ac")
            return new WeatherCommand();
        if(in=="ca"||in=="c a"||in=="see a"||in=="c. a.")
            return new OpenAppCommand("calc.exe","Calculator");
        if(in=="np"||in=="n p"||in=="n. p.")
            return new OpenAppCommand("notepad.exe","Notepad");
        if(in=="pt"||in=="p t"||in=="p. t.")
            return new OpenAppCommand("mspaint.exe","Paint");
        if(in=="ex"||in=="e x"||in=="exp"||in=="explorer")
            return new OpenAppCommand("explorer.exe","Explorer");
        if(in=="d"||in=="dee")
            return new OpenAppCommand("winword.exe","Microsoft Word");
        if(in=="m"||in=="em")
            return new OpenAppCommand("powerpnt.exe","Microsoft PowerPoint");
        if(in=="ft"||in=="f t"||in=="f. t.")
            return new OpenAppCommand("excel.exe","Microsoft Excel");

        //This PC
        if(in=="dc"||in=="d c"||in=="disk c"||in=="local c")
            return new OpenFolderCommand("C:\\","Local Disk C");
        if(in=="dd"||in=="d d"||in=="disk d"||in=="local d")
            return new OpenFolderCommand("D:\\","Local Disk D");


        if(in=="dw"||in=="d w"||in=="downloads") {
            char* up = getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Downloads","Downloads");
        }
        if(in=="ds"||in=="d s"||in=="desktop") {
            char* up = getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Desktop","Desktop");
        }
        if(in=="do"||in=="d o"||in=="documents"||in=="docs") {
            char* up = getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Documents","Documents");
        }

        //shortcuts of accessories
        if(in=="vu"||in=="v u"||in=="v. u.") return new VolumeCommand("up");
        if(in=="vd"||in=="v d"||in=="v. d.") return new VolumeCommand("down");
        if(in=="vm"||in=="v m"||in=="v. m.") return new VolumeCommand("mute");
        if(in=="a"||in=="ay"||in=="eh")      return new ScreenshotCommand();
        if(in=="b"||in=="be"||in=="bee")     return new BatteryCommand();
        if(in=="f"||in=="ef"||in=="eff")     return new SystemCommand("lock");
        if(in=="h"||in=="?"||in=="aitch")    return new HelpCommand();

        // ── LAYER 2: NUMBER CODES ────────────────────────────
        if(in=="seven"||in=="7")      return new SystemCommand("shutdown");
        if(in=="eight"||in=="8")      return new SystemCommand("restart");
        if(in=="nine"||in=="9")       return new SystemCommand("sleep");
        if(in=="ten"||in=="10")       return new SystemCommand("cancel");
        if(in=="seventeen"||in=="17") return new CoinFlipCommand();
        if(in=="eighteen"||in=="18")  return new DiceCommand();
        if(in=="nineteen"||in=="19")  return new OpenURLCommand("https://www.youtube.com","YouTube");
        if(in=="twenty"||in=="20")    return new OpenURLCommand("https://www.google.com","Google");
        if(in=="thirty"||in=="30")    return new BatteryCommand();
        if(in=="forty"||in=="40")     return new ScreenshotCommand();
        if(in=="fifty"||in=="50")     return new OpenAppCommand("excel.exe","Microsoft Excel");
        if(in=="sixty"||in=="60")     return new VolumeCommand("up");
        if(in=="seventy"||in=="70")   return new VolumeCommand("down");
        if(in=="eighty"||in=="80")    return new VolumeCommand("mute");
        if(in=="ninety"||in=="90")    return new SystemCommand("lock");
        if(in=="sixteen"||in=="16")   return new OpenURLCommand("https://www.facebook.com","Facebook");

        // ── LAYER 3: FULL PHRASES ────────────────────────────

        // ── PLAY SONG ON YOUTUBE ─────────────────────────────
        // Handles: "play shape of you"
        //          "play believer"
        //          "play shape of you on youtube"
        // Must be checked FIRST before any other phrase handling
        if(vmHas(in,"play ")) {
            size_t p = in.find("play ");
            if(p != string::npos) {
                // Get everything after "play "
                string song = raw.substr(p + 5);
                string songLower = vmLower(song);

                // Strip trailing "on youtube" / "on yt" / "in youtube"
                auto stripSuffix = [&](const string& suffix){
                    size_t pos = songLower.find(suffix);
                    if(pos != string::npos) {
                        song = song.substr(0, pos);
                        songLower = vmLower(song);
                    }
                };
                stripSuffix(" on youtube");
                stripSuffix(" on yt");
                stripSuffix(" in youtube");
                stripSuffix(" on you tube");

                // Trim leading/trailing whitespace
                while(!song.empty() && song.front()==' ')
                    song.erase(song.begin());
                while(!song.empty() && song.back()==' ')
                    song.pop_back();

                if(!song.empty()) {
                    // Build YouTube search URL
                    string url = "https://www.youtube.com/results?search_query=";
                    for(char c : song)
                        url += (c==' ') ? "+" : string(1,c);
                    ShellExecuteA(NULL,"open",url.c_str(),NULL,NULL,SW_SHOWNORMAL);
                    return new SearchCommand("youtube " + song);
                }
            }
        }

        if(vmHas(in,"exit")||vmHas(in,"quit")||vmHas(in,"bye")||vmHas(in,"goodbye"))
            return new ExitCommand();
        if(vmHas(in,"joke")||vmHas(in,"funny")||vmHas(in,"laugh"))
            return new JokeCommand();
        if(vmHas(in,"fact")||vmHas(in,"did you know")||vmHas(in,"tell me fact"))
            return new FactCommand();
        if(vmHas(in,"restart")||vmHas(in,"reboot"))
            return new SystemCommand("restart");
        if(vmHas(in,"time")||vmHas(in,"date")||vmHas(in,"clock")||vmHas(in,"today"))
            return new TimeCommand();
        if(vmHas(in,"flip")||vmHas(in,"coin")||vmHas(in,"heads")||vmHas(in,"tails"))
            return new CoinFlipCommand();
        if(vmHas(in,"roll")||vmHas(in,"dice")||vmHas(in,"die"))
            return new DiceCommand();
        if(vmHas(in,"remind me to")||vmHas(in,"remind me"))
        {
            string task="";
            size_t p = in.find("remind me to");
            if(p!=string::npos&&p+13<raw.size()) task=raw.substr(p+13);
            else { p=in.find("remind me");
                   if(p!=string::npos&&p+10<raw.size()) task=raw.substr(p+10); }
            return new ReminderCommand(task);
        }
        if(vmHas(in,"search ")) {
            size_t p=in.find("search ");
            if(p!=string::npos&&p+7<raw.size())
                return new SearchCommand(raw.substr(p+7));
        }
        if(vmHas(in,"look up ")) {
            size_t p=in.find("look up ");
            if(p+8<raw.size()) return new SearchCommand(raw.substr(p+8));
        }
        if(vmHas(in,"calculate")||vmHas(in,"compute")||vmHas(in,"what is")) {
            double a=0,b=0; char op='+';
            if(sscanf(raw.c_str(),"%lf %c %lf",&a,&op,&b)==3)
                return new CalculatorCommand(a,b,op);
        }
        if(vmHas(in,"plus")||vmHas(in,"minus")||vmHas(in,"times")||
           vmHas(in,"divided by")||vmHas(in,"multiply")) {
            double a=0,b=0; char op='+';
            if     (vmHas(in,"plus"))    op='+';
            else if(vmHas(in,"minus"))   op='-';
            else if(vmHas(in,"times")||vmHas(in,"multiply")) op='*';
            else if(vmHas(in,"divided")) op='/';
            sscanf(raw.c_str(),"%lf",&a);
            return new CalculatorCommand(a,b,op);
        }
        if(vmHas(in,"shut down")||vmHas(in,"shutdown")||vmHas(in,"turn off"))
            return new SystemCommand("shutdown");
        if(vmHas(in,"cancel shutdown")||vmHas(in,"abort shutdown"))
            return new SystemCommand("cancel");
        if(vmHas(in,"lock computer")||vmHas(in,"lock screen"))
            return new SystemCommand("lock");
        if(vmHas(in,"sleep mode")||vmHas(in,"go to sleep")||vmHas(in,"hibernate"))
            return new SystemCommand("sleep");
        if(vmHas(in,"volume up")||vmHas(in,"louder")||vmHas(in,"turn up"))
            return new VolumeCommand("up");
        if(vmHas(in,"volume down")||vmHas(in,"quieter")||vmHas(in,"turn down"))
            return new VolumeCommand("down");
        if(vmHas(in,"mute")||vmHas(in,"silence")||vmHas(in,"no sound"))
            return new VolumeCommand("mute");
        if(vmHas(in,"screenshot")||vmHas(in,"screen shot")||vmHas(in,"capture"))
            return new ScreenshotCommand();
        if(vmHas(in,"battery")||vmHas(in,"how much charge")||vmHas(in,"power level"))
            return new BatteryCommand();
        if(vmHas(in,"weather")||vmHas(in,"forecast")||vmHas(in,"temperature"))
            return new WeatherCommand();
        if(vmHas(in,"hello")||in=="hi"||vmHas(in,"hey")||vmHas(in,"salam")||
           vmHas(in,"good morning")||vmHas(in,"good evening"))
            return new GreetCommand();
        if(vmHas(in,"youtube")||vmHas(in,"you tube"))
            return new OpenURLCommand("https://www.youtube.com","YouTube");
        if(vmHas(in,"open google")||vmHas(in,"go to google"))
            return new OpenURLCommand("https://www.google.com","Google");
        if(vmHas(in,"github")||vmHas(in,"git hub"))
            return new OpenURLCommand("https://www.github.com","GitHub");
        if(vmHas(in,"facebook")||vmHas(in,"face book"))
            return new OpenURLCommand("https://www.facebook.com","Facebook");
        if(vmHas(in,"instagram"))
            return new OpenURLCommand("https://www.instagram.com","Instagram");
        if(vmHas(in,"twitter"))
            return new OpenURLCommand("https://www.twitter.com","Twitter");
        if(vmHas(in,"whatsapp")||vmHas(in,"whats app"))
            return new OpenURLCommand("https://web.whatsapp.com","WhatsApp");
        if(vmHas(in,"gmail")||vmHas(in,"open mail"))
            return new OpenURLCommand("https://mail.google.com","Gmail");
        if(vmHas(in,"netflix"))
            return new OpenURLCommand("https://www.netflix.com","Netflix");
        if(vmHas(in,"amazon"))
            return new OpenURLCommand("https://www.amazon.com","Amazon");
        if(vmHas(in,"google map")||vmHas(in,"open map"))
            return new OpenURLCommand("https://maps.google.com","Google Maps");
        if(vmHas(in,"open calculator")||vmHas(in,"launch calculator"))
            return new OpenAppCommand("calc.exe","Calculator");
        if(vmHas(in,"notepad")||vmHas(in,"note pad"))
            return new OpenAppCommand("notepad.exe","Notepad");
        if(vmHas(in,"open paint")||vmHas(in,"ms paint"))
            return new OpenAppCommand("mspaint.exe","Paint");
        if(vmHas(in,"task manager"))
            return new OpenAppCommand("taskmgr.exe","Task Manager");
        if(vmHas(in,"file explorer")||vmHas(in,"open explorer")||vmHas(in,"my computer"))
            return new OpenAppCommand("explorer.exe","Explorer");
        if(vmHas(in,"microsoft word")||vmHas(in,"open word")||vmHas(in,"ms word"))
            return new OpenAppCommand("winword.exe","Microsoft Word");
        if(vmHas(in,"microsoft excel")||vmHas(in,"open excel")||vmHas(in,"ms excel"))
            return new OpenAppCommand("excel.exe","Microsoft Excel");
        if(vmHas(in,"powerpoint")||vmHas(in,"power point")||vmHas(in,"open slides"))
            return new OpenAppCommand("powerpnt.exe","Microsoft PowerPoint");
        if(vmHas(in,"command prompt")||vmHas(in,"open cmd")||vmHas(in,"terminal"))
            return new OpenAppCommand("cmd.exe","Command Prompt");
        if(vmHas(in,"snipping")||vmHas(in,"snip tool"))
            return new OpenAppCommand("snippingtool.exe","Snipping Tool");
        if(vmHas(in,"local disk c")||vmHas(in,"c drive"))
            return new OpenFolderCommand("C:\\","Local Disk C");
        if(vmHas(in,"local disk d")||vmHas(in,"d drive"))
            return new OpenFolderCommand("D:\\","Local Disk D");
        if(vmHas(in,"open downloads")||vmHas(in,"my downloads")) {
            char* up=getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Downloads","Downloads");
        }
        if(vmHas(in,"open desktop")||vmHas(in,"show desktop")) {
            char* up=getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Desktop","Desktop");
        }
        if(vmHas(in,"open documents")||vmHas(in,"my documents")) {
            char* up=getenv("USERPROFILE");
            return new OpenFolderCommand(string(up?up:"C:")+"\\Documents","Documents");
        }
        if(vmHas(in,"close")) {
            size_t p = in.find("close")+6;
            if(p < raw.size()) {
                string w = raw.substr(p);
                if(!w.empty()) w[0]=(char)toupper((unsigned char)w[0]);
                return new CloseWindowCommand(w);
            }
        }
        if(vmHas(in,"who designed you")||vmHas(in,"who made you")||
            vmHas(in,"who created you")||vmHas(in,"who built you")||
            vmHas(in,"who programmed you")||vmHas(in,"who are your creators"))
            return new WhoDesignedCommand();
        if(vmHas(in,"help")||vmHas(in,"what can you")||vmHas(in,"commands"))
            return new HelpCommand();

        if(vmHas(in,"debug search")||vmHas(in,"list search folders")||vmHas(in,"where are you searching"))
            return new DebugSearchCommand();

        // ── LAYER 3.5: OPEN ANYTHING (fallback for unlisted sites) ──
        // Everything above already had first chance to match a
        // specific known app/site. If we reach here and the user
        // said "open X" / "shop for X" / "buy X" and X wasn't one
        // of the named sites above, treat X as a general request —
        // open it directly if it's a known alias, otherwise search
        // Google for it. This is what lets VoxMind "open anything"
        // instead of only the sites hardcoded elsewhere in this file.
        {
            static const vector<string> openTriggers = {"open ","launch ","start "};
            static const vector<string> shopTriggers  = {"shop for ","buy ","order "};
            static const vector<string> trailers = {
                " on browser"," on chrome"," on google",
                " in browser"," in chrome"," online"," on the internet"
            };

            // Cuts off anything that isn't really part of the app/
            // file name: a trailing "on browser" clause, or a
            // secondary "and play X" instruction (that's handled
            // separately below and inside SmartOpenCommand — it
            // must not pollute the name we're searching for).
            auto cutName = [&](string n) -> string {
                string lowerName = vmLower(n);
                size_t cutPos = string::npos;
                for (auto& t : trailers) {
                    size_t tp = lowerName.find(t);
                    if (tp != string::npos && (cutPos == string::npos || tp < cutPos)) cutPos = tp;
                }
                size_t playCut = lowerName.find(" and play ");
                if (playCut == string::npos) playCut = lowerName.find(" then play ");
                if (playCut == string::npos) playCut = lowerName.find(" play ");
                if (playCut != string::npos && (cutPos == string::npos || playCut < cutPos)) cutPos = playCut;
                return (cutPos != string::npos) ? n.substr(0, cutPos) : n;
            };

            string name; bool found = false;

            for (auto& trig : openTriggers) {
                size_t p = in.find(trig);
                if (p != string::npos && p + trig.size() <= raw.size()) {
                    name = cutName(raw.substr(p + trig.size()));
                    found = !name.empty();
                    break;
                }
            }
            if (!found) {
                for (auto& trig : shopTriggers) {
                    size_t p = in.find(trig);
                    if (p != string::npos && p + trig.size() <= raw.size()) {
                        name = cutName(raw.substr(p + trig.size()));
                        found = !name.empty();
                        break;
                    }
                }
            }
            // "play X on Y" / "play X in Y" — a request that leads
            // with "play" instead of "open" still resolves to the
            // right app, e.g. "play gotham series on moviebox".
            if (!found) {
                size_t playPos = in.find("play ");
                if (playPos != string::npos) {
                    size_t onPos = in.rfind(" on ");
                    size_t inPos = in.rfind(" in ");
                    size_t sepPos = string::npos;
                    if (onPos != string::npos && onPos > playPos) sepPos = onPos;
                    if (inPos != string::npos && inPos > playPos && (sepPos == string::npos || inPos > sepPos)) sepPos = inPos;
                    if (sepPos != string::npos) {
                        string appName = cutName(raw.substr(sepPos + 4));
                        if (!appName.empty()) { name = appName; found = true; }
                    } else {
                        // "play X" with no "on/in" — default to YouTube search
                        string songName = raw.substr(playPos + 5);
                        // trim whitespace
                        while (!songName.empty() && songName.front()==' ')
                            songName.erase(songName.begin());
                        if (!songName.empty()) {
                            string ytUrl = "https://www.youtube.com/results?search_query=";
                            for (char c : songName)
                                ytUrl += (c==' ') ? "+" : string(1,c);
                            ShellExecuteA(NULL,"open",ytUrl.c_str(),NULL,NULL,SW_SHOWNORMAL);
                            return new SearchCommand("youtube " + songName);
                        }
                    }
                }
            }
            if (found) return new SmartOpenCommand(name, raw);
        }

        // ── LAYER 4: SYNONYM-AWARE FUZZY FALLBACK ────────────
        // Nothing above matched — before giving up, check whether
        // this is just a differently-worded version of a known
        // command (synonym, rewording, or a small typo).
        Command* fuzzy = vmFuzzyFallback(raw);
        if (fuzzy) return fuzzy;

        return nullptr;
    }
};

// ============================================================
//  VOICE OUTPUT HANDLER  —  OOP: Encapsulation
//  Fixed: Does NOT use sphelper.h
//         Uses CoCreateInstance directly (MinGW + MSVC safe)
// ============================================================
class VoiceOutputHandler {
private:
    ISpVoice* pVoice;
    bool      ready;

public:
    VoiceOutputHandler() : pVoice(nullptr), ready(false) {
        // Try CLSCTX_ALL first
        HRESULT hr = CoCreateInstance(
            __uuidof(SpVoice), nullptr,
            CLSCTX_ALL,
            __uuidof(ISpVoice),
            (void**)&pVoice);

        // Fallback CLSCTX_INPROC_SERVER
        if(FAILED(hr))
            hr = CoCreateInstance(
                __uuidof(SpVoice), nullptr,
                CLSCTX_INPROC_SERVER,
                __uuidof(ISpVoice),
                (void**)&pVoice);

        if(SUCCEEDED(hr) && pVoice) {
            pVoice->SetVolume(100);
            pVoice->SetRate(0);
            ready = true;
        }
    }

    // Fire-and-forget async speak (used by GUI)
    void speakAsync(const string& text) {
        if(ready && pVoice) {
            // Stop current speech
            pVoice->Speak(nullptr, SPF_ASYNC|SPF_PURGEBEFORESPEAK, nullptr);
            wstring ws(text.begin(), text.end());
            pVoice->Speak(ws.c_str(), SPF_ASYNC, nullptr);
        }
    }

    // Blocking speak (waits until done — used for short responses)
    void speak(const string& text) {
        if(ready && pVoice) {
            pVoice->Speak(nullptr, SPF_ASYNC|SPF_PURGEBEFORESPEAK, nullptr);
            wstring ws(text.begin(), text.end());
            pVoice->Speak(ws.c_str(), SPF_DEFAULT, nullptr);
        }
    }

    bool isReady() const { return ready; }

    ~VoiceOutputHandler() {
        if(pVoice) { pVoice->Release(); pVoice = nullptr; }
    }
};

// ============================================================
//  VOICE INPUT HANDLER  —  OOP: Encapsulation
//  Fixed: Uses CLSID_SpSharedRecognizer with Win32 event
//         No sphelper.h dependency
// ============================================================
class VoiceInputHandler {
private:
    ISpRecognizer*  pRecognizer;
    ISpRecoContext* pRecoCtx;
    ISpRecoGrammar* pGrammar;
    bool            ready;

public:
    VoiceInputHandler()
        : pRecognizer(nullptr), pRecoCtx(nullptr),
          pGrammar(nullptr), ready(false) {}

    bool initialize()
    {
        // Use shared recognizer (requires Windows Speech Recognition running)
        HRESULT hr = CoCreateInstance(
            __uuidof(SpSharedRecognizer), nullptr,
            CLSCTX_LOCAL_SERVER,
            __uuidof(ISpRecognizer),
            (void**)&pRecognizer);
        if(FAILED(hr)) return false;

        hr = pRecognizer->CreateRecoContext(&pRecoCtx);
        if(FAILED(hr)) return false;

        // Use Win32 event notification
        hr = pRecoCtx->SetNotifyWin32Event();
        if(FAILED(hr)) return false;

        // Subscribe to recognition events only
        hr = pRecoCtx->SetInterest(
            SPFEI(SPEI_RECOGNITION),
            SPFEI(SPEI_RECOGNITION));
        if(FAILED(hr)) return false;

        hr = pRecoCtx->CreateGrammar(0, &pGrammar);
        if(FAILED(hr)) return false;

        hr = pGrammar->LoadDictation(nullptr, SPLO_STATIC);
        if(FAILED(hr)) return false;

        hr = pGrammar->SetDictationState(SPRS_ACTIVE);
        ready = SUCCEEDED(hr);
        return ready;
    }

    string listen(int timeoutMs = 8000) {
        if(!ready || !pRecoCtx) return "";

        HANDLE hNotify = pRecoCtx->GetNotifyEventHandle();
        if(hNotify == INVALID_HANDLE_VALUE) return "";

        DWORD wr = WaitForSingleObject(hNotify, (DWORD)timeoutMs);
        if(wr != WAIT_OBJECT_0) return "";

        string result;
        SPEVENT ev;
        ULONG   fetched = 0;

        while(SUCCEEDED(pRecoCtx->GetEvents(1,&ev,&fetched)) && fetched>0) {
            if(ev.eEventId == SPEI_RECOGNITION) {
                ISpRecoResult* pRes =
                    reinterpret_cast<ISpRecoResult*>(ev.lParam);
                if(pRes) {
                    LPWSTR txt = nullptr;
                    if(SUCCEEDED(pRes->GetText(
                        SP_GETWHOLEPHRASE,
                        SP_GETWHOLEPHRASE,
                        TRUE, &txt, nullptr)) && txt) {
                        wstring ws(txt);
                        result = string(ws.begin(), ws.end());
                        CoTaskMemFree(txt);
                    }
                    pRes->Release();
                }
            }
        }
        return result;
    }

    bool isReady() const { return ready; }

    ~VoiceInputHandler() {
        if(pGrammar)    pGrammar->Release();
        if(pRecoCtx)    pRecoCtx->Release();
        if(pRecognizer) pRecognizer->Release();
    }
};

#endif // VOXMIND_ENGINE_H
