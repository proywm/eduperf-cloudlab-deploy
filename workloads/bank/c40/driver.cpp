#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <random>

// ---- Tiny trivial stubs for Qt/project types ----
using QString = std::string;

struct QStringList : std::vector<QString> {
    bool contains(const QString &s) const {
        for (const auto &x : *this) if (x == s) return true;
        return false;
    }
};

// The alias table shared by both provider implementations.
// alias -> canonical name  (resolveAlias)
// name  -> list of aliases (listAliases)
struct Provider {
    std::unordered_map<QString, QString> aliasToName;      // resolveAlias source
    std::unordered_map<QString, QStringList> nameToAliases; // listAliases source

    // listAliases(name): returns aliases whose canonical is `name`
    QStringList listAliases(const QString &name) const {
        auto it = nameToAliases.find(name);
        if (it == nameToAliases.end()) return QStringList{};
        return it->second;
    }
    // resolveAlias(x): if x is an alias, return canonical name; else return x
    QString resolveAlias(const QString &x) const {
        auto it = aliasToName.find(x);
        if (it == aliasToName.end()) return x;
        return it->second;
    }
};

// MimeType private data
struct MimeTypePrivate {
    QString name;
    Provider *prov;
};

// ---- BEFORE version ----
namespace v_before {
    struct MimeType {
        MimeTypePrivate *d;
        QStringList aliases() const { return d->prov->listAliases(d->name); }
        bool matchesName(const QString &nameOrAlias) const {
            return d->name == nameOrAlias || aliases().contains(nameOrAlias);
        }
    };
}

// ---- AFTER version ----
namespace v_after {
    struct MimeType {
        MimeTypePrivate *d;
        bool matchesName(const QString &nameOrAlias) const {
            if (d->name == nameOrAlias)
                return true;
            return d->prov->resolveAlias(nameOrAlias) == d->name;
        }
    };
}

int main() {
    // Build a consistent alias table.
    Provider prov;
    // canonical names and their aliases
    std::vector<std::pair<QString, std::vector<QString>>> table = {
        {"text/csv", {"text/x-csv", "text/x-comma-separated-values"}},
        {"application/pdf", {"application/x-pdf"}},
        {"image/jpeg", {"image/jpg", "image/pjpeg"}},
        {"text/plain", {}},
        {"application/xml", {"text/xml"}},
        {"a", {"b","c","d","e","f","g","h","i","j","k"}},
        {"lonely/name", {}},
    };
    for (auto &e : table) {
        QStringList al;
        for (auto &a : e.second) { al.push_back(a); prov.aliasToName[a] = e.first; }
        prov.nameToAliases[e.first] = al;
    }

    // Build a diverse battery of test inputs (names, aliases, unknowns, boundaries)
    std::vector<QString> inputs = {
        "text/csv","text/x-csv","text/x-comma-separated-values",
        "application/pdf","application/x-pdf",
        "image/jpeg","image/jpg","image/pjpeg",
        "text/plain","application/xml","text/xml",
        "a","b","c","k","z",
        "lonely/name","","unknown/type","TEXT/CSV","text/csv ",
        " text/csv","texT/x-csv","nonexistent","image/JPEG",
    };
    // add random-ish strings
    std::mt19937 rng(12345);
    const char *alph = "abcdefghijk/xyz-";
    for (int i = 0; i < 200; ++i) {
        int len = rng() % 12;
        QString s;
        for (int j = 0; j < len; ++j) s += alph[rng() % 15];
        inputs.push_back(s);
    }

    // For each MimeType (each canonical name), compare before/after over all inputs
    bool ok = true;
    QString firstDiv;
    for (auto &e : table) {
        MimeTypePrivate d; d.name = e.first; d.prov = &prov;
        v_before::MimeType mb{&d};
        v_after::MimeType ma{&d};
        for (auto &in : inputs) {
            bool rb = mb.matchesName(in);
            bool ra = ma.matchesName(in);
            if (rb != ra) {
                ok = false;
                if (firstDiv.empty()) firstDiv = "name=" + e.first + " input='" + in + "'";
            }
        }
    }
    std::cout << "EQUIVALENT=" << (ok?"YES":"NO") << "\n";
    if (!ok) std::cout << "FIRST_DIVERGENCE: " << firstDiv << "\n";

    // ---- Timing: interleaved before/after ----
    // Use the "a" mimetype (10 aliases) as worst case for the linear scan.
    MimeTypePrivate dt; dt.name = "a"; dt.prov = &prov;
    v_before::MimeType tb{&dt};
    v_after::MimeType ta{&dt};

    std::vector<QString> query = {"a","b","c","d","e","f","g","h","i","j","k","z","unknown","","aa"};

    const int iters = 200000;
    std::vector<double> bns, ans;
    for (int rep = 0; rep < 11; ++rep) {
        volatile int sink = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; ++i) for (auto &q : query) sink ^= tb.matchesName(q);
        auto t1 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iters; ++i) for (auto &q : query) sink ^= ta.matchesName(q);
        auto t2 = std::chrono::high_resolution_clock::now();
        bns.push_back(std::chrono::duration<double,std::nano>(t1-t0).count());
        ans.push_back(std::chrono::duration<double,std::nano>(t2-t1).count());
        (void)sink;
    }
    std::sort(bns.begin(), bns.end());
    std::sort(ans.begin(), ans.end());
    double mb2 = bns[bns.size()/2], ma2 = ans[ans.size()/2];
    std::cout << "before_ns_median=" << mb2 << " after_ns_median=" << ma2 << "\n";
    std::cout << "SPEEDUP=" << (mb2/ma2) << "\n";
    return 0;
}
