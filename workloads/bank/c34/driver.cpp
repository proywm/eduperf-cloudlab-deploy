// Faithful reproduction of apple/foundationdb 0fea3fb731a8
// "Save a bunch of copies in the trace thread": for (auto event : a.events)
// changed to for (const auto& event : a.events) to avoid copying each
// TraceEventFields (a vector of string key/value pairs) per iteration.
#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>

// --- Tiny faithful stubs modeling the real FDB types touched by the loop ---

// TraceEventFields: holds a list of (key,value) string pairs, like the real
// class. validateFormat() scans the fields; a formatter reads them out.
struct TraceEventFields {
	using Field = std::pair<std::string, std::string>;
	std::vector<Field> fields;
	// Mirrors real validateFormat(): checks no embedded newlines etc.
	// Returns an accumulator so we can compare observable results.
	uint64_t validateFormat() const {
		uint64_t acc = 0;
		for (const auto& f : fields) {
			for (char c : f.first) acc += (unsigned char)c;
			for (char c : f.second) acc = acc * 31 + (unsigned char)c;
		}
		return acc;
	}
};

// A minimal writer sink that accumulates a checksum of everything written.
struct Sink {
	uint64_t checksum = 0;
	void write(const std::string& s) {
		for (char c : s) checksum = checksum * 1000003ULL + (unsigned char)c;
		checksum += s.size();
	}
};

// A formatter that produces a formatted string per event (reads all fields).
struct Formatter {
	std::string formatEvent(const TraceEventFields& e) const {
		std::string out;
		out.reserve(64);
		for (const auto& f : e.fields) {
			out += f.first;
			out += '=';
			out += f.second;
			out += ' ';
		}
		return out;
	}
};

struct WriteBuffer {
	std::vector<TraceEventFields> events;
};

namespace v_before {
	void action(WriteBuffer& a, Sink& sink, const Formatter* formatter, uint64_t& valAcc) {
		for (auto event : a.events) {              // by value (copy each event)
			valAcc += event.validateFormat();
			sink.write(formatter->formatEvent(event));
		}
	}
}

namespace v_after {
	void action(WriteBuffer& a, Sink& sink, const Formatter* formatter, uint64_t& valAcc) {
		for (const auto& event : a.events) {       // by const reference (no copy)
			valAcc += event.validateFormat();
			sink.write(formatter->formatEvent(event));
		}
	}
}

// --- Build a WriteBuffer from a seed, with diverse/boundary content ---
static WriteBuffer makeBuffer(uint64_t seed, int nEvents, int maxFields, int strLen) {
	std::mt19937_64 rng(seed);
	WriteBuffer wb;
	wb.events.resize(nEvents);
	for (int i = 0; i < nEvents; ++i) {
		int nf = (maxFields == 0) ? 0 : (int)(rng() % (maxFields + 1));
		for (int j = 0; j < nf; ++j) {
			int kl = (strLen == 0) ? 0 : (int)(rng() % (strLen + 1));
			int vl = (strLen == 0) ? 0 : (int)(rng() % (strLen + 1));
			std::string k(kl, 0), v(vl, 0);
			for (auto& c : k) c = (char)(32 + rng() % 95);
			for (auto& c : v) c = (char)(32 + rng() % 95);
			wb.events[i].fields.emplace_back(std::move(k), std::move(v));
		}
	}
	return wb;
}

int main() {
	// (i) Differential correctness over a diverse/boundary/adversarial battery.
	struct Cfg { int n, mf, sl; uint64_t seed; };
	std::vector<Cfg> battery = {
		{0, 0, 0, 1}, {1, 0, 0, 2}, {1, 1, 0, 3}, {1, 1, 1, 4},
		{5, 3, 8, 5}, {10, 6, 20, 6}, {50, 4, 40, 7}, {100, 10, 64, 8},
		{3, 0, 0, 9}, {2, 20, 2, 10}, {200, 2, 128, 11}, {7, 5, 0, 12},
	};
	bool ok = true;
	std::string divergent;
	for (auto& c : battery) {
		WriteBuffer wb1 = makeBuffer(c.seed, c.n, c.mf, c.sl);
		WriteBuffer wb2 = makeBuffer(c.seed, c.n, c.mf, c.sl);
		Sink s1, s2; Formatter fmt; uint64_t va1 = 0, va2 = 0;
		v_before::action(wb1, s1, &fmt, va1);
		v_after::action(wb2, s2, &fmt, va2);
		if (s1.checksum != s2.checksum || va1 != va2) {
			ok = false;
			divergent = "n=" + std::to_string(c.n) + " mf=" + std::to_string(c.mf) +
			            " sl=" + std::to_string(c.sl) + " seed=" + std::to_string(c.seed);
			break;
		}
	}
	printf("EQUIVALENT: %s\n", ok ? "YES" : "NO");
	if (!ok) printf("DIVERGENT_INPUT: %s\n", divergent.c_str());

	// (ii) Interleaved timing on a realistic large workload.
	WriteBuffer timing = makeBuffer(9999, 2000, 8, 48);
	Formatter fmt;
	const int reps = 200;
	std::vector<double> ratios;
	for (int r = 0; r < reps; ++r) {
		Sink sb, sa; uint64_t vb = 0, vaa = 0;
		auto t0 = std::chrono::high_resolution_clock::now();
		v_before::action(timing, sb, &fmt, vb);
		auto t1 = std::chrono::high_resolution_clock::now();
		v_after::action(timing, sa, &fmt, vaa);
		auto t2 = std::chrono::high_resolution_clock::now();
		double bns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		double ans = std::chrono::duration<double, std::nano>(t2 - t1).count();
		volatile uint64_t sink = sb.checksum + sa.checksum + vb + vaa; (void)sink;
		if (ans > 0) ratios.push_back(bns / ans);
	}
	std::sort(ratios.begin(), ratios.end());
	double med = ratios.empty() ? 0.0 : ratios[ratios.size() / 2];
	printf("MEDIAN_SPEEDUP: %.4f\n", med);
	return 0;
}
