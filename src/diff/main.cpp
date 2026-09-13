// ─────────────────────────────────────────────────────────────────────────────
// dff_diff — the partition-invariance / reproducibility V&V tool.
//
//     dff_diff <dir1> <dir2> [dir3 ...]
//
// Each dir holds the per-federate .ndjson from ONE run (one partitioning, or one trial).
// The tool parses the truth, keys every sample by (entity id, logical step), and checks
// that ALL runs agree BIT-EXACTLY with the first (the reference). By transitivity of
// equality, all-match-reference => all runs equal -- so this proves invariance across
// *every* partitioning given (K=1 vs K=2 vs K=3 ...), and equally proves run-to-run
// reproducibility when the dirs are repeated trials of the same setup.
//
// It then reports the *execution* divergence the invariance holds across: the same truth
// produced by different owners and at different real (wall-clock) times.
//
// RTI-free, standalone, C++17. The NDJSON shape is fixed (we emit it), so a targeted
// scanner parses it without a JSON library. Exit 0 iff every run matches the reference
// (bit-exact, same key set); nonzero otherwise -- CI-friendly.
// ─────────────────────────────────────────────────────────────────────────────
#include <algorithm>
#include <cmath>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

struct Sample {
    double    pos[3]  = {0,0,0};
    double    vel[3]  = {0,0,0};
    double    quat[4] = {0,0,0,0};
    string    federate;    // who computed it (from meta / filename)
    long long wt = 0;      // wall-clock microseconds when computed
};
typedef pair<int,long long> Key;   // (entity id, logical step)
typedef map<Key,Sample>     Run;

// ---- targeted scanners over our fixed NDJSON shape ---------------------------
static bool findString(const string& s, const string& key, size_t from, string& out) {
    string pat = "\"" + key + "\":\"";
    size_t p = s.find(pat, from);
    if (p == string::npos) return false;
    p += pat.size();
    size_t q = s.find('"', p);
    if (q == string::npos) return false;
    out = s.substr(p, q - p);
    return true;
}
static bool findNumTok(const string& s, const string& key, size_t from, string& tok) {
    string pat = "\"" + key + "\":";
    size_t p = s.find(pat, from);
    if (p == string::npos) return false;
    p += pat.size();
    size_t q = p;
    while (q < s.size()) {
        char c = s[q];
        if ((c>='0'&&c<='9') || c=='-'||c=='+'||c=='.'||c=='e'||c=='E') q++;
        else break;
    }
    tok = s.substr(p, q - p);
    return q > p;
}
static bool findArray(const string& s, const string& key, size_t from, int n, double* out) {
    string pat = "\"" + key + "\":[";
    size_t p = s.find(pat, from);
    if (p == string::npos) return false;
    p += pat.size();
    for (int i = 0; i < n; i++) {
        size_t q = p;
        while (q < s.size() && s[q] != ',' && s[q] != ']') q++;
        if (q == p) return false;
        out[i] = stod(s.substr(p, q - p));
        p = q + 1;
    }
    return true;
}

static string baseName(const string& path) {
    string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();   // trim trailing slash
    size_t slash = p.find_last_of('/');
    return (slash == string::npos) ? p : p.substr(slash + 1);
}
static string stem(const string& path) {
    string base = baseName(path);
    if (base.size() > 7 && base.substr(base.size() - 7) == ".ndjson")
        base = base.substr(0, base.size() - 7);
    return base;
}

static vector<string> listNdjson(const string& dir) {
    vector<string> files;
    DIR* d = opendir(dir.c_str());
    if (!d) throw runtime_error("cannot open directory '" + dir + "'");
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        string name = e->d_name;
        if (name.size() > 7 && name.substr(name.size() - 7) == ".ndjson")
            files.push_back(dir + "/" + name);
    }
    closedir(d);
    sort(files.begin(), files.end());   // deterministic file order
    return files;
}

static Run parseRun(const string& dir, set<string>& federatesOut) {
    Run run;
    vector<string> files = listNdjson(dir);
    if (files.empty()) throw runtime_error("no .ndjson files in '" + dir + "'");

    for (size_t fi = 0; fi < files.size(); ++fi) {
        ifstream in(files[fi].c_str());
        if (!in) throw runtime_error("cannot read '" + files[fi] + "'");

        string federate = stem(files[fi]);   // fallback until a meta line overrides
        double dt = 0.1;
        string line;
        while (getline(in, line)) {
            if (line.find("\"meta\"") != string::npos) {
                string f;   if (findString(line, "federate", 0, f)) federate = f;
                string dtk; if (findNumTok(line, "dt", 0, dtk))      dt = stod(dtk);
                continue;
            }
            string ttok;
            if (!findNumTok(line, "t", 0, ttok)) continue;   // not a frame
            long long step = llround(stod(ttok) / dt);       // clean integer key
            long long wt = 0;
            string wttok; if (findNumTok(line, "wt", 0, wttok)) wt = stoll(wttok);

            size_t p = 0;
            while ((p = line.find("{\"id\":", p)) != string::npos) {
                string idtok;
                if (!findNumTok(line, "id", p, idtok)) break;
                Sample smp;
                smp.federate = federate;
                smp.wt = wt;
                findArray(line, "pos",  p, 3, smp.pos);
                findArray(line, "vel",  p, 3, smp.vel);
                findArray(line, "quat", p, 4, smp.quat);
                run[Key(stoi(idtok), step)] = smp;
                federatesOut.insert(federate);
                p += 6;
            }
        }
    }
    return run;
}

static bool sameTruth(const Sample& a, const Sample& b) {
    for (int i = 0; i < 3; i++) if (a.pos[i]  != b.pos[i])  return false;   // exact ==
    for (int i = 0; i < 3; i++) if (a.vel[i]  != b.vel[i])  return false;
    for (int i = 0; i < 4; i++) if (a.quat[i] != b.quat[i]) return false;
    return true;
}

// ---- one reference-vs-other comparison, with the failure TYPE broken out -----
struct Cmp {
    size_t compared = 0, identical = 0, divergent = 0, onlyRef = 0, onlyOther = 0;
    bool   haveFirst = false;
    Key    firstKey = Key(0,0);
    Sample a, b;
    bool holds() const { return compared > 0 && divergent == 0 && onlyRef == 0 && onlyOther == 0; }
};

static Cmp compareRuns(const Run& ref, const Run& other) {
    Cmp c;
    for (Run::const_iterator it = ref.begin(); it != ref.end(); ++it) {
        Run::const_iterator jt = other.find(it->first);
        if (jt == other.end()) { c.onlyRef++; continue; }
        c.compared++;
        if (sameTruth(it->second, jt->second)) { c.identical++; continue; }
        c.divergent++;
        if (!c.haveFirst) { c.haveFirst = true; c.firstKey = it->first; c.a = it->second; c.b = jt->second; }
    }
    for (Run::const_iterator jt = other.begin(); jt != other.end(); ++jt)
        if (ref.find(jt->first) == ref.end()) c.onlyOther++;
    return c;
}

static long long maxStepSpread(const Run& run) {
    map<long long, pair<long long,long long> > mm;   // step -> (minWt, maxWt)
    for (Run::const_iterator it = run.begin(); it != run.end(); ++it) {
        long long step = it->first.second, wt = it->second.wt;
        map<long long, pair<long long,long long> >::iterator m = mm.find(step);
        if (m == mm.end()) mm[step] = make_pair(wt, wt);
        else { m->second.first = min(m->second.first, wt); m->second.second = max(m->second.second, wt); }
    }
    long long worst = 0;
    for (map<long long, pair<long long,long long> >::iterator m = mm.begin(); m != mm.end(); ++m)
        worst = max(worst, m->second.second - m->second.first);
    return worst;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "usage: dff_diff <dir1> <dir2> [dir3 ...]\n"
                "  each dir holds the per-federate .ndjson from one run (partitioning or trial);\n"
                "  all runs are compared bit-exactly against the first.\n";
        return 2;
    }
    vector<string> dirs(argv + 1, argv + argc);

    vector<Run>         runs;
    vector<set<string> > feds;
    try {
        for (size_t i = 0; i < dirs.size(); ++i) {
            set<string> f;
            runs.push_back(parseRun(dirs[i], f));
            feds.push_back(f);
        }
    } catch (const std::exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 2;
    }

    cout << "=== Partition-invariance / reproducibility diff ===\n";
    for (size_t i = 0; i < dirs.size(); ++i) {
        cout << "  run " << i << ": " << dirs[i]
             << "  (" << feds[i].size() << " federate(s), " << runs[i].size() << " points)"
             << (i == 0 ? "   [reference]" : "") << "\n";
    }
    cout << "\n";

    // ---- 1) truth: every run vs the reference, bit-exact, keyed by (id, step) ----
    cout << "Truth comparison (bit-exact, keyed by (entity id, logical step)):\n";
    bool allHold = true;
    for (size_t j = 1; j < dirs.size(); ++j) {
        Cmp c = compareRuns(runs[0], runs[j]);
        bool ok = c.holds();
        allHold = allHold && ok;

        cout << "  run 0 vs run " << j << " (" << baseName(dirs[j]) << "): "
             << c.identical << "/" << c.compared << " identical";
        if (c.divergent || c.onlyRef || c.onlyOther)
            cout << "  [divergent=" << c.divergent
                 << ", ref-only=" << c.onlyRef << ", other-only=" << c.onlyOther << "]";
        cout << "  => " << (ok ? "MATCH" : "MISMATCH") << "\n";

        if (!ok) {
            // Name the failure TYPE, not just "fails".
            if (c.compared == 0)
                cout << "      reason: NO OVERLAP -- the runs share no (id,step) keys "
                        "(disjoint or empty).\n";
            if (c.divergent > 0) {
                cout << "      reason: VALUE DIVERGENCE -- " << c.divergent
                     << " shared point(s) computed to different bits.\n";
                const Sample& a = c.a; const Sample& b = c.b;
                cout << "      first at entity " << c.firstKey.first
                     << ", step " << c.firstKey.second << ":\n";
                cout << "        d(pos)  = ["
                     << (a.pos[0]-b.pos[0]) << ", " << (a.pos[1]-b.pos[1]) << ", " << (a.pos[2]-b.pos[2]) << "]\n";
                cout << "        d(vel)  = ["
                     << (a.vel[0]-b.vel[0]) << ", " << (a.vel[1]-b.vel[1]) << ", " << (a.vel[2]-b.vel[2]) << "]\n";
                cout << "        d(quat) = ["
                     << (a.quat[0]-b.quat[0]) << ", " << (a.quat[1]-b.quat[1]) << ", "
                     << (a.quat[2]-b.quat[2]) << ", " << (a.quat[3]-b.quat[3]) << "]\n";
            }
            if (c.onlyRef > 0 || c.onlyOther > 0)
                cout << "      reason: COVERAGE MISMATCH -- points present in one run but not the "
                        "other (ref-only=" << c.onlyRef << ", other-only=" << c.onlyOther << ").\n";
        }
    }
    cout << "\n  => PARTITION INVARIANCE " << (allHold ? "HOLDS (bit-exact across all runs)"
                                                       : "FAILS") << "\n\n";

    // ---- 2) execution divergence the invariance holds across --------------------
    cout << "Execution divergence (metadata -- what changed while the truth did not):\n";

    set<int> ids;
    vector<map<int,string> > owners(dirs.size());
    for (size_t i = 0; i < runs.size(); ++i)
        for (Run::const_iterator it = runs[i].begin(); it != runs[i].end(); ++it) {
            owners[i][it->first.first] = it->second.federate;
            ids.insert(it->first.first);
        }

    cout << "  ownership (same truth, different owner):\n";
    for (set<int>::iterator id = ids.begin(); id != ids.end(); ++id) {
        cout << "    entity " << *id << ":";
        for (size_t i = 0; i < dirs.size(); ++i)
            cout << "  run" << i << "=" << (owners[i].count(*id) ? owners[i][*id] : "(none)");
        cout << "\n";
    }
    cout << "  real-time interleaving (max wall-clock spread among federates at one step):\n";
    for (size_t i = 0; i < dirs.size(); ++i)
        cout << "    run " << i << " (" << baseName(dirs[i]) << "): "
             << maxStepSpread(runs[i]) / 1000.0 << " ms\n";

    cout << "\n  Interpretation: the same logical-time truth was produced by different\n"
            "  federates and at different real times, yet is bit-identical across all runs.\n";

    return allHold ? 0 : 1;
}
