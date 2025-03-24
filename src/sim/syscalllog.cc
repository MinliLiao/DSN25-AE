#include "sim/syscalllog.hh"

namespace gem5 {
std::vector<syscalllogentry> syscalllogentry::entries {syscalllogentry()};
std::vector<int> syscalllogentry::current_entry {0};
std::vector<int> syscalllogentry::entryIndices{0};
std::vector<int> syscalllogentry::maxIndices{0};
}
