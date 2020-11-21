#include "SVF-FE/LLVMUtil.h"
//#include "Graphs/SVFG.h"
//#include "WPA/Andersen.h"
//#include "SVF-FE/PAGBuilder.h"
#include "SABER/DoubleFreeChecker.h"

using namespace llvm;
using namespace SVF;
using namespace std;

static cl::opt<string> input_filename( // NOLINT(cert-err58-cpp)
        cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

struct SVFGPath {
    SVFGNode *head = nullptr;
    vector<pair<SVFGEdge *, SVFGNode *>> tail;
};

// Find all Null Pointer Dereference source-sink paths. The path is identified
// by storing null to a pointer and loading it twice.
std::vector<SVFGPath> find_npd_paths(SVFG *svfg) {
    // Nodes state tuple: (search state, prev node, prev edge)
    //   0: Not detected
    //   1: Null stored
    //   2: Null loaded
    //   3: Null deref
    Map<SVFGNode *, tuple<int, SVFGNode *, SVFGEdge *>> nodes_state;
    constexpr int ACCEPT_STATE = 3;
    FIFOWorkList<VFGNode *> worklist;
    Set<SVFGNode *> sinks;
    // Search for sources
    for (auto &node_iter: *svfg) {
        if (auto store_node = SVFUtil::dyn_cast<StoreVFGNode>(node_iter.second)) {
            if (auto store_instr = SVFUtil::dyn_cast<StoreInst>(store_node->getInst())) {
                if (SVFUtil::isa<ConstantPointerNull>(store_instr->getValueOperand())) {
                    worklist.push(node_iter.second);
                    nodes_state[node_iter.second] = make_tuple(1, nullptr, nullptr);
                }
            }
        }
    }
    // Forward search for sinks
    while (!worklist.empty()) {
        auto node = worklist.pop();
        auto prev_state = get<0>(nodes_state[node]);
        if (prev_state == ACCEPT_STATE) continue;
        for (auto edge = node->OutEdgeBegin(); edge != node->OutEdgeEnd(); ++edge) {
            auto succ_node = (*edge)->getDstNode();
            auto old_state = get<0>(nodes_state[succ_node]);
            if (SVFUtil::dyn_cast<LoadVFGNode>(succ_node)) {
                auto new_state = max(old_state, prev_state + 1);
                if (old_state != new_state) {
                    nodes_state[succ_node] = make_tuple(new_state, node, *edge);
                    if (new_state != ACCEPT_STATE)
                        worklist.push(succ_node);
                    else
                        sinks.insert(succ_node);
                }
            } else if (SVFUtil::isa<CopySVFGNode>(succ_node)
                    || SVFUtil::isa<PHISVFGNode>(succ_node)
                    || SVFUtil::isa<MSSAPHISVFGNode>(succ_node)) {
                auto new_state = max(old_state, prev_state);
                if (old_state != new_state) {
                    nodes_state[succ_node] = make_tuple(new_state, node, *edge);
                    worklist.push(succ_node);
                }
            }
        }
    }
    // Backward search to construct path
    std::vector<SVFGPath> paths;
    for (auto &sink: sinks) {
        SVFGPath path;
        path.head = sink;
        while (true) {
            auto state = nodes_state[path.head];
            if (get<1>(state)) {
                path.tail.emplace_back(make_pair(get<2>(state), path.head));
                path.head = get<1>(state);
            } else {
                break;
            }
        }
        reverse(path.tail.begin(), path.tail.end());
        paths.push_back(move(path));
    }
    return paths;
}

int main(int argc, char *argv[]) {
    DebugFlag = true;
    cl::ParseCommandLineOptions(argc, argv,"Defect Path Validator\n");
    SVFModule *module = LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(
            vector<string>{ input_filename }); // NOLINT(cppcoreguidelines-slicing)
    PAGBuilder builder;
    PAG *pag = builder.build(module);
    Andersen* ander = AndersenWaveDiff::createAndersenWaveDiff(pag);
    ICFG* icfg = pag->getICFG();
    SVFGBuilder svfBuilder;
    SVFG* svfg = svfBuilder.buildFullSVFGWithoutOPT(ander);
    auto paths = find_npd_paths(svfg);
    // Dump paths.
    // FIXME(Sun Ziping): cannot find the exact ICFG path for a SVFG path.
    //   Some information like which branch is lost. Can we recover it back?
    int i = 0;
    for (const auto &path: paths) {
        outs() << "SVFG Path (" << ++i << "):" << "\n";
        outs() << path.head->getId() << " ";
        for (const auto &pair: path.tail) {
            outs() << pair.second->getId() << " ";
        }
        outs() << "\n";
        outs() << "ICVFG Path (" << ++i << "):" << "\n";
        outs() << path.head->getICFGNode()->getId() << " ";
        for (const auto &pair: path.tail) {
            outs() << pair.second->getICFGNode()->getId() << " ";
        }
        outs() << "\n";
    }
    pag->dump("pag");
    icfg->dump("icfg");
    svfg->dump("svfg");
    return 0;
}