#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <chrono>
#include <iostream>
#include <random>

using namespace std;

// ---- Tiny trivial stubs for project types ----
static const int CM_PRIVATE = 1;
static const int CM_SECRET  = 2;

struct userrec {
    int fd;
    char nick[64];
    int id;
};

struct chanrec {
    char name[64];
    int binarymodes;
    char topic[128];
    std::vector<int> members; // ids of users on channel
    bool HasUser(userrec* u) const {
        for (size_t k = 0; k < members.size(); ++k)
            if (members[k] == u->id) return true;
        return false;
    }
};

typedef std::unordered_map<std::string, chanrec*> chan_hash;

static int usercount_i(chanrec* c) { return (int)c->members.size(); }
static const char* chanmodes(chanrec* c, bool showsecret) {
    static char buf[8];
    buf[0] = showsecret ? 's' : 'n';
    buf[1] = (c->binarymodes & CM_PRIVATE) ? 'p' : '-';
    buf[2] = '\0';
    return buf;
}

static std::string g_out;
static void WriteServ(int fd, const char* fmt, ...) {
    (void)fd;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_out += buf;
    g_out += '\n';
}

// ---------------- BEFORE ----------------
namespace v_before {
    chan_hash chanlist;               // stands in for the extern global
    struct cmd_list { void Handle(char **parameters, int pcnt, userrec *user); };

    void cmd_list::Handle (char **parameters, int pcnt, userrec *user)
    {
        WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
        for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
        {
            // if the channel is not private/secret, OR the user is on the channel anyway
            if (((!(i->second->binarymodes & CM_PRIVATE)) && (!(i->second->binarymodes & CM_SECRET))) || (i->second->HasUser(user)))
            {
                WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount_i(i->second),chanmodes(i->second,i->second->HasUser(user)),i->second->topic);
            }
        }
        WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
    }
}

// ---------------- AFTER ----------------
namespace v_after {
    chan_hash chanlist;
    struct cmd_list { void Handle(char **parameters, int pcnt, userrec *user); };

    void cmd_list::Handle (char **parameters, int pcnt, userrec *user)
    {
        WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
        for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
        {
            // if the channel is not private/secret, OR the user is on the channel anyway
            bool n = i->second->HasUser(user);
            if (((!(i->second->binarymodes & CM_PRIVATE)) && (!(i->second->binarymodes & CM_SECRET))) || (n))
            {
                WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount_i(i->second),chanmodes(i->second,n),i->second->topic);
            }
        }
        WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
    }
}

struct Scenario {
    std::vector<chanrec> chans;
    userrec user;
};

// Build both maps identically off the SAME chans storage so iteration order matches.
static void build(Scenario& s, chan_hash& clb, chan_hash& cla, unsigned seed, int nchan, int maxmem) {
    std::mt19937 rng(seed);
    s.chans.clear();
    s.chans.resize(nchan);
    s.user.fd = 3;
    snprintf(s.user.nick, sizeof(s.user.nick), "nick%u", seed);
    s.user.id = (int)(rng() % (maxmem + 2));
    for (int c = 0; c < nchan; ++c) {
        chanrec& ch = s.chans[c];
        snprintf(ch.name, sizeof(ch.name), "#chan%d", c);
        snprintf(ch.topic, sizeof(ch.topic), "topic%d", c);
        ch.binarymodes = rng() % 4;
        int m = rng() % (maxmem + 1);
        for (int k = 0; k < m; ++k) ch.members.push_back((int)(rng() % (maxmem + 2)));
    }
    clb.clear(); cla.clear();
    for (int c = 0; c < nchan; ++c) {
        char key[64]; snprintf(key, sizeof(key), "#chan%d", c);
        clb[key] = &s.chans[c];
        cla[key] = &s.chans[c];
    }
}

int main() {
    // ---------- Differential test ----------
    bool allEq = true;
    std::string firstDiv;
    struct Cfg { int nchan; int maxmem; };
    std::vector<Cfg> cfgs = {{0,0},{1,0},{1,1},{5,3},{20,10},{100,50},{3,0},{2,200},{50,1},{7,7}};
    for (auto& cfg : cfgs) {
        for (unsigned seed = 0; seed < 400; ++seed) {
            Scenario s;
            build(s, v_before::chanlist, v_after::chanlist, seed, cfg.nchan, cfg.maxmem);

            g_out.clear();
            { v_before::cmd_list b; b.Handle(nullptr, 0, &s.user); }
            std::string ob = g_out;

            g_out.clear();
            { v_after::cmd_list a; a.Handle(nullptr, 0, &s.user); }
            std::string oa = g_out;

            if (ob != oa) {
                allEq = false;
                if (firstDiv.empty()) {
                    char d[128];
                    snprintf(d, sizeof(d), "nchan=%d maxmem=%d seed=%u", cfg.nchan, cfg.maxmem, seed);
                    firstDiv = d;
                }
            }
        }
    }
    std::cout << "EQUIVALENT=" << (allEq ? "YES" : "NO") << "\n";
    if (!allEq) std::cout << "DIVERGENT=" << firstDiv << "\n";

    // ---------- Timing (interleaved) ----------
    // Redundancy is only exercised when HasUser is called in BOTH the if-guard
    // and inside chanmodes. That happens when the channel is private/secret
    // (so the left side is false and the || must evaluate HasUser) AND the user
    // is a member (so the branch is entered). Make user the LAST member so each
    // scan is worst-case linear, maximizing the cost of the redundant 2nd call.
    Scenario st;
    build(st, v_before::chanlist, v_after::chanlist, 12345, 200, 300);
    st.user.id = 1000000; // unique sentinel id
    for (auto& ch : st.chans) {
        ch.binarymodes = CM_SECRET;              // private/secret -> if must call HasUser
        ch.members.push_back(st.user.id);        // append user at the END -> full scan
    }

    const int iters = 3000;
    std::vector<double> rb, ra;
    double before_total = 0.0, after_total = 0.0;
    volatile size_t sink = 0;
    for (int rep = 0; rep < iters; ++rep) {
        auto t0 = std::chrono::high_resolution_clock::now();
        g_out.clear();
        { v_before::cmd_list b; b.Handle(nullptr, 0, &st.user); }
        auto t1 = std::chrono::high_resolution_clock::now();
        sink += g_out.size();

        auto t2 = std::chrono::high_resolution_clock::now();
        g_out.clear();
        { v_after::cmd_list a; a.Handle(nullptr, 0, &st.user); }
        auto t3 = std::chrono::high_resolution_clock::now();
        sink += g_out.size();

        double before_elapsed = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
        double after_elapsed = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t3-t2).count();
        rb.push_back(before_elapsed); ra.push_back(after_elapsed);
        before_total += before_elapsed; after_total += after_elapsed;
    }
    std::sort(rb.begin(), rb.end());
    std::sort(ra.begin(), ra.end());
    double mb = rb[rb.size()/2];
    double ma = ra[ra.size()/2];
    std::cout << "MEDIAN_SAMPLE_B_NS=" << mb << " MEDIAN_SAMPLE_A_NS=" << ma << "\n";
    std::cout << "BEFORE_NS=" << before_total << "\nAFTER_NS=" << after_total << "\n";
    std::cout << "SPEEDUP=" << (before_total/after_total) << "\n";
    std::cout << "sink=" << sink << "\n";
    return 0;
}
