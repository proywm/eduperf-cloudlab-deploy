// Differential + timing harness for PakDirectory::addFile
// Unit: smp_addFile_6e318f49642e (Arx Fatalis io/PakEntry.cpp)
// before: O(n) linear strcasecmp scan of the file list to reject duplicates
// after:  rely on the hash map's add() failing on an existing (case-insensitive) key
//
// Stubs: PakFile (name + intrusive list links), HashMap (case-insensitive
// string -> void* map with add() returning false on duplicate key), and a
// minimal PakDirectory holding only the members addFile touches.
// The addFile bodies are VERBATIM from before.cpp / after.cpp.

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <cassert>
#include <iostream>

#ifndef NULL
#define NULL 0
#endif

// ---- shared tiny stubs (identical for both namespaces) --------------------
static std::string lc(const std::string & s) {
	std::string r = s;
	for(char & c : r) c = (char)tolower((unsigned char)c);
	return r;
}

#define DEFINE_STUBS \
class PakFile { \
public: \
	std::string name; \
	size_t size, offset, flags, uncompressedSize; \
	PakFile * prev; \
	PakFile * next; \
	explicit PakFile(const std::string & n) : name(n), size(0), offset(0), flags(0), uncompressedSize(0), prev(NULL), next(NULL) {} \
}; \
class HashMap { \
	std::unordered_map<std::string, void *> m; \
public: \
	bool add(const std::string & key, void * value) { \
		return m.emplace(lc(key), value).second; /* false if already present */ \
	} \
	void * get(const std::string & key) const { \
		auto it = m.find(lc(key)); \
		return it == m.end() ? (void *)NULL : it->second; \
	} \
	size_t count() const { return m.size(); } \
}; \
class PakDirectory { \
public: \
	HashMap * filesMap; \
	PakFile * files; \
	unsigned int nbfiles; \
	PakDirectory() : filesMap(new HashMap), files(NULL), nbfiles(0) {} \
	~PakDirectory() { \
		delete filesMap; \
		PakFile * f = files; \
		while(f) { PakFile * n = f->next; delete f; f = n; } \
	} \
	PakFile * addFile(const std::string & name); \
};

namespace v_before {
DEFINE_STUBS
// ==== VERBATIM from before.cpp =============================================
PakFile * PakDirectory::addFile(const std::string& name) {

	PakFile * f = files;
	while(f) {
		if( !strcasecmp(f->name.c_str(), name.c_str() ) ) {
			// File already exists.
			return NULL;
		}
		f = f->next;
	}

	f = new PakFile(name);
	if(!f) {
		return NULL;
	}

	// Add file to hash map.
	if(filesMap) {
		if(!filesMap->add( name, (void *)f)) {
			delete f;
			return NULL;
		}
	} else {
		printf("file added befor initializing files map\n");
	}

	// Link file into list.
	f->prev = NULL;
	f->next = files;
	if(files) {
		files->prev = f;
	}
	files = f;

	nbfiles++;

	return f;
}
// ===========================================================================
}

namespace v_after {
DEFINE_STUBS
// ==== VERBATIM from after.cpp ==============================================
PakFile * PakDirectory::addFile(const std::string& name) {

	PakFile * f;

	f = new PakFile(name);
	if(!f) {
		return NULL;
	}

	// Add file to hash map.
	if(filesMap) {
		if(!filesMap->add(name, (void *)f)) {
			delete f;	// Probably already inserted in map...
			return NULL;
		}
	} else {
		printf("file added befor initializing files map\n");
	}

	// Link file into list.
	f->prev = NULL;
	f->next = files;
	if(files) {
		files->prev = f;
	}
	files = f;

	nbfiles++;

	return f;
}
// ===========================================================================
}

// ---- differential test -----------------------------------------------------
template <class Dir>
static std::string runSequence(const std::vector<std::string> & names) {
	// Returns a full observable-state fingerprint: per-call success/failure,
	// final nbfiles, list contents front-to-back, and map lookup results.
	Dir d;
	std::string out;
	for(const auto & n : names) {
		auto * f = d.addFile(n);
		out += f ? "A" : "0";
		if(f) { assert(f->name == n); }
	}
	out += "|nb=" + std::to_string(d.nbfiles) + "|list=";
	for(auto * f = d.files; f; f = f->next) { out += f->name; out += ";"; }
	out += "|map=";
	for(const auto & n : names) {
		void * g = d.filesMap->get(n);
		out += g ? (((decltype(d.files))g)->name + ",") : std::string("-,");
	}
	out += "|mapsz=" + std::to_string(d.filesMap->count());
	return out;
}

static std::vector<std::vector<std::string>> buildBattery() {
	std::vector<std::vector<std::string>> battery;
	// boundary / handcrafted cases
	battery.push_back({});
	battery.push_back({""});
	battery.push_back({"", ""});                       // duplicate empty name
	battery.push_back({"a"});
	battery.push_back({"a", "a"});
	battery.push_back({"a", "A"});                     // case-insensitive dup
	battery.push_back({"A", "a", "a", "A"});
	battery.push_back({"readme.txt", "README.TXT", "ReadMe.Txt"});
	battery.push_back({"file1", "file2", "file1", "file3", "FILE2"});
	battery.push_back({"data\\level1.pak", "data/level1.pak", "data\\LEVEL1.PAK"});
	battery.push_back({" ", "  ", " \t", " "});
	battery.push_back({".", "..", "...", ".."});
	battery.push_back({std::string(4096, 'x'), std::string(4096, 'X'), std::string(4095, 'x')});
	battery.push_back({"caf\xC3\xA9", "CAF\xC3\xA9", "caf\xC3\x89"});  // bytes > 127
	battery.push_back({"i", "I", "\xC4\xB0"});
	battery.push_back({"a.b", "a_b", "a-b", "A.B", "a b"});
	// randomized cases: mixed-case names drawn from small pools -> many dups
	std::mt19937 rng(20260702);
	const char * pool[] = {"snd", "graph", "misc", "loc", "data2", "level5.dlf",
	                       "player.tea", "wall.ftl", "IDX", "book.bmp"};
	for(int t = 0; t < 200; t++) {
		std::vector<std::string> seq;
		int len = 1 + (int)(rng() % 40);
		for(int i = 0; i < len; i++) {
			std::string s = pool[rng() % 10];
			if(rng() % 3 == 0) s += std::to_string(rng() % 5);
			for(char & c : s) if(rng() % 2) c = (char)toupper((unsigned char)c);
			seq.push_back(s);
		}
		battery.push_back(seq);
	}
	// randomized long unique-ish names
	for(int t = 0; t < 50; t++) {
		std::vector<std::string> seq;
		int len = 1 + (int)(rng() % 30);
		for(int i = 0; i < len; i++) {
			std::string s;
			int sl = 1 + (int)(rng() % 24);
			for(int k = 0; k < sl; k++) s += (char)(32 + rng() % 95); // printable ASCII
			seq.push_back(s);
			if(rng() % 4 == 0) seq.push_back(s); // exact dup
		}
		battery.push_back(seq);
	}
	return battery;
}

int main() {
	// ---- equivalence ----
	auto battery = buildBattery();
	size_t failures = 0;
	for(size_t i = 0; i < battery.size(); i++) {
		std::string b = runSequence<v_before::PakDirectory>(battery[i]);
		std::string a = runSequence<v_after::PakDirectory>(battery[i]);
		if(b != a) {
			failures++;
			std::cout << "DIVERGENCE case " << i << "\n  seq:";
			for(auto & s : battery[i]) std::cout << " [" << s.substr(0, 40) << "]";
			std::cout << "\n  before: " << b << "\n  after:  " << a << "\n";
			if(failures > 3) break;
		}
	}
	std::cout << "cases=" << battery.size() << " divergences=" << failures << "\n";
	if(failures) return 1;

	// ---- timing (interleaved) ----
	// Workload: build one directory with N adds where half are duplicate
	// attempts, matching real pak loading (before is O(N^2) list scan).
	const int N = 4000;
	std::vector<std::string> work;
	work.reserve(N);
	std::mt19937 rng(7);
	for(int i = 0; i < N; i++) {
		if(i % 2 == 0) work.push_back("dir\\file_" + std::to_string(i) + ".ftl");
		else           work.push_back("DIR\\FILE_" + std::to_string(rng() % (i + 1)) + ".FTL");
	}
	auto timeIt = [&](auto tag) -> long long {
		using D = decltype(tag);
		auto t0 = std::chrono::steady_clock::now();
		D d;
		size_t ok = 0;
		for(const auto & n : work) if(d.addFile(n)) ok++;
		auto t1 = std::chrono::steady_clock::now();
		volatile size_t sink = ok; (void)sink;
		return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
	};
	std::vector<double> ratios;
	for(int rep = 0; rep < 9; rep++) {
		long long tb = timeIt(v_before::PakDirectory());
		long long ta = timeIt(v_after::PakDirectory());
		ratios.push_back((double)tb / (double)ta);
		std::cout << "rep " << rep << ": before=" << tb << "ns after=" << ta
		          << "ns ratio=" << ratios.back() << "\n";
	}
	std::sort(ratios.begin(), ratios.end());
	std::cout << "median_speedup=" << ratios[ratios.size() / 2] << "\n";
	return 0;
}
