#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>

static const int REPS = 50;
static const size_t CONFIG_FILE_IO_BUFFER_SIZE = 4096; // Define the buffer size

class Parameters {
public:
    std::string getPrefix() const { return "prefix_"; }
    int getWordSize() const { return 10; }
};

class ScaffoldContigs {
public:
    uint64_t operator[](int index) const { return static_cast<uint64_t>(index); }
    size_t size() const { return 10; }
};

class ScaffoldStrands {
public:
    char operator[](int index) const { return '+'; }
    size_t size() const { return 10; }
};

class ContigLengths {
public:
    int operator[](uint64_t index) const { return static_cast<int>(index * 2); }
};

namespace v_before {
void writeScaffoldData(const Parameters* m_parameters, const std::vector<ScaffoldContigs>& m_scaffoldContigs, const std::vector<ScaffoldStrands>& m_scaffoldStrands, const ContigLengths& m_contigLengths, const std::vector<std::vector<int>>& m_scaffoldGaps, std::vector<int>& m_allScaffoldLengths) {
    // write scaffold list
    std::ostringstream scaffoldList;
    scaffoldList << m_parameters->getPrefix() << "ScaffoldComponents.txt";

    std::ostringstream scaffoldLengths;
    scaffoldLengths << m_parameters->getPrefix() << "ScaffoldLengths.txt";
    std::ofstream f3(scaffoldLengths.str().c_str());
    std::ofstream f4(scaffoldList.str().c_str());

    for (int i = 0; i < static_cast<int>(m_scaffoldContigs.size()); i++) {
        int scaffoldName = i;
        int length = 0;

        for (int j = 0; j < static_cast<int>(m_scaffoldContigs[i].size()); j++) {
            uint64_t contigName = m_scaffoldContigs[i][j];
            char contigStrand = m_scaffoldStrands[i][j];
            int theLength = m_contigLengths[contigName] + m_parameters->getWordSize() - 1;
            f4 << "scaffold-" << scaffoldName << "\t" << "contig-" << contigName << "\t" << contigStrand << "\t" << theLength << std::endl;
            length += theLength;

            if (j != static_cast<int>(m_scaffoldContigs[i].size()) - 1) {
                int theLength = m_scaffoldGaps[i][j];
                f4 << "scaffold-" << scaffoldName << "\tgap\t-\t" << theLength << std::endl;
                length += theLength;
            }
        }
        f3 << "scaffold-" << scaffoldName << "\t" << length << std::endl;
        f4 << std::endl;
        m_allScaffoldLengths.push_back(length);
    }
    f3.close();
    f4.close();
}
}

namespace v_after {
void flushFileOperationBuffer(bool finalFlush, std::ostringstream* buffer, std::ofstream* file, size_t bufferSize) {
    if (buffer->str().size() >= bufferSize || finalFlush) {
        *file << buffer->str();
        buffer->str(""); // Clear the buffer
    }
}

void writeScaffoldData(const Parameters* m_parameters, const std::vector<ScaffoldContigs>& m_scaffoldContigs, const std::vector<ScaffoldStrands>& m_scaffoldStrands, const ContigLengths& m_contigLengths, const std::vector<std::vector<int>>& m_scaffoldGaps, std::vector<int>& m_allScaffoldLengths) {
    // write scaffold list
    std::ostringstream scaffoldList;
    scaffoldList << m_parameters->getPrefix() << "ScaffoldComponents.txt";

    std::ostringstream scaffoldLengths;
    scaffoldLengths << m_parameters->getPrefix() << "ScaffoldLengths.txt";
    std::ofstream scaffoldLengthFile(scaffoldLengths.str().c_str());
    std::ostringstream scaffoldLengthFile_Buffer;

    std::ofstream scaffoldComponentFile(scaffoldList.str().c_str());
    std::ostringstream scaffoldComponentFile_Buffer;

    for (int i = 0; i < static_cast<int>(m_scaffoldContigs.size()); i++) {
        int scaffoldName = i;
        int length = 0;

        for (int j = 0; j < static_cast<int>(m_scaffoldContigs[i].size()); j++) {
            uint64_t contigName = m_scaffoldContigs[i][j];
            char contigStrand = m_scaffoldStrands[i][j];
            int theLength = m_contigLengths[contigName] + m_parameters->getWordSize() - 1;
            scaffoldComponentFile_Buffer << "scaffold-" << scaffoldName << "\t" << "contig-" << contigName << "\t" << contigStrand << "\t" << theLength << std::endl;
            length += theLength;

            if (j != static_cast<int>(m_scaffoldContigs[i].size()) - 1) {
                int theLength = m_scaffoldGaps[i][j];
                scaffoldComponentFile_Buffer << "scaffold-" << scaffoldName << "\tgap\t-\t" << theLength << std::endl;
                length += theLength;
            }
        }
        scaffoldLengthFile_Buffer << "scaffold-" << scaffoldName << "\t" << length << std::endl;
        scaffoldComponentFile_Buffer << std::endl;
        m_allScaffoldLengths.push_back(length);

        flushFileOperationBuffer(false, &scaffoldComponentFile_Buffer, &scaffoldComponentFile, CONFIG_FILE_IO_BUFFER_SIZE);
        flushFileOperationBuffer(false, &scaffoldLengthFile_Buffer, &scaffoldLengthFile, CONFIG_FILE_IO_BUFFER_SIZE);
    }
    flushFileOperationBuffer(true, &scaffoldComponentFile_Buffer, &scaffoldComponentFile, CONFIG_FILE_IO_BUFFER_SIZE);
    scaffoldComponentFile.close();

    flushFileOperationBuffer(true, &scaffoldLengthFile_Buffer, &scaffoldLengthFile, CONFIG_FILE_IO_BUFFER_SIZE);
    scaffoldLengthFile.close();
}
}

static long long work(int version) {
    Parameters params;
    std::vector<ScaffoldContigs> scaffoldContigs(10);
    std::vector<ScaffoldStrands> scaffoldStrands(10);
    ContigLengths contigLengths;
    std::vector<std::vector<int>> scaffoldGaps(10, std::vector<int>(9));
    std::vector<int> allScaffoldLengths;

    for (int i = 0; i < REPS; ++i) {
        if (version == 0) {
            v_before::writeScaffoldData(&params, scaffoldContigs, scaffoldStrands, contigLengths, scaffoldGaps, allScaffoldLengths);
        } else {
            v_after::writeScaffoldData(&params, scaffoldContigs, scaffoldStrands, contigLengths, scaffoldGaps, allScaffoldLengths);
        }
    }

    long long checksum = 0;
    for (int length : allScaffoldLengths) {
        checksum ^= static_cast<long long>(length);
    }
    return checksum;
}

// ===== fixed harness (appended; do not edit) =====
#include <cstdio>
#include <chrono>
#include <climits>
#include <algorithm>
int main(){
    long long c0 = work(0);
    long long c1 = work(1);
    using clk = std::chrono::steady_clock;
    auto best_of = [](int v)->long long{
        long long best = LLONG_MAX;
        for(int r=0;r<9;r++){
            auto t0=clk::now();
            volatile long long sink=0;
            for(int k=0;k<REPS;k++) sink += work(v);
            auto t1=clk::now();
            long long ns=std::chrono::duration_cast<std::chrono::nanoseconds>(t1-t0).count();
            if(ns<best) best=ns;
        }
        return best;
    };
    // interleave before/after to control for drift; take min over passes
    long long b=LLONG_MAX,a=LLONG_MAX;
    for(int pass=0;pass<3;pass++){
        long long bb=best_of(0); if(bb<b) b=bb;
        long long aa=best_of(1); if(aa<a) a=aa;
    }
    printf("EQUIV=%d\n", (c0==c1)?1:0);
    printf("BEFORE_NS=%lld\n", b);
    printf("AFTER_NS=%lld\n", a);
    printf("READY=1\n");
    return 0;
}
