#include "common.h"
#include "mtTaskPartition.h"

#include <map>
#include <numeric>
#include <queue>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <dirent.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace {

bool localGraph = false;
std::unordered_map<Node*, std::vector<Node*>> boundaryNext;

std::unordered_set<Node*> snapshotMems;
std::unordered_map<Node*, std::vector<int>> snapshotWriterParts;
std::vector<Node*> snapshotOrder;

int repCutThreadsFromEnv() {
  const char* text = std::getenv("GSIM_THREADS");
  if (text == nullptr || text[0] == '\0') return 1;
  char* end = nullptr;
  long value = std::strtol(text, &end, 10);
  if (end == text || *end != '\0' || value < 1 || value > 64) {
    fprintf(stderr, "[repcut] GSIM_THREADS must be an integer in [1, 64], got '%s'\n", text);
    exit(1);
  }
  return static_cast<int>(value);
}

int partOf(SuperNode* super) {
  int part = -1;
  for (Node* node : super->member) {
    if (node->rcPart < 0) continue;
    if (part < 0) part = node->rcPart;
    else if (part != node->rcPart) return -2;
  }
  return part;
}

bool commitOnlySuper(SuperNode* super) {
  bool any = false;
  for (Node* node : super->member) {
    if (node->status != VALID_NODE) continue;
    any = true;
    if (!node->rcCommitOnly) return false;
  }
  return any;
}

template <typename Fn>
void walkEnode(ENode* root, Fn&& fn) {
  if (root == nullptr) return;
  std::vector<ENode*> pending{root};
  for (size_t index = 0; index < pending.size(); ++index) {
    ENode* node = pending[index];
    fn(node);
    for (ENode* child : node->child) if (child != nullptr) pending.push_back(child);
  }
}

template <typename Fn>
void walkAssign(Node* node, Fn&& fn) {
  for (ExpTree* tree : node->assignTree) {
    if (tree == nullptr) continue;
    walkEnode(tree->getRoot(), fn);
    walkEnode(tree->getlval(), fn);
  }
}

void retargetTree(ExpTree* tree, int part,
                  const std::unordered_map<Node*, std::unordered_map<int, Node*>>& version) {
  if (tree == nullptr) return;
  auto rewrite = [&](ENode* node) {
    if (node->nodePtr == nullptr) return;
    auto found = version.find(node->nodePtr);
    if (found == version.end()) return;
    auto replacement = found->second.find(part);
    if (replacement != found->second.end()) node->nodePtr = replacement->second;
  };
  walkEnode(tree->getRoot(), rewrite);
  walkEnode(tree->getlval(), rewrite);
}

int scaledWeight(double value, double scale) {
  const double scaled = value * scale;
  if (scaled < 1) return 1;
  if (scaled > 2000000000.0) return 2000000000;
  return static_cast<int>(scaled + 0.5);
}

// Balance sink vertices and minimize km1 = sum (lambda(e)-1)*w(e), the
// replication cost of an intersection hypergraph. This is the local fallback
// when the KaHyPar binary is unavailable.
std::vector<int> partitionKm1(const std::vector<double>& weight,
                              const std::vector<std::vector<int>>& pins,
                              const std::vector<double>& edgeWeight,
                              int threads) {
  const int vertices = static_cast<int>(weight.size());
  std::vector<int> part(vertices, 0);
  if (vertices == 0 || threads <= 1) return part;
  std::vector<int> order(vertices);
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int left, int right) {
    if (weight[left] != weight[right]) return weight[left] > weight[right];
    return left < right;
  });
  std::vector<double> load(threads, 0);
  for (int vertex : order) {
    int best = 0;
    for (int block = 1; block < threads; ++block) {
      if (load[block] < load[best]) best = block;
    }
    part[vertex] = best;
    load[best] += weight[vertex];
  }
  if (pins.empty()) return part;

  const int edges = static_cast<int>(pins.size());
  std::vector<std::vector<int>> incident(vertices);
  std::vector<int> pinCount(static_cast<size_t>(edges) * threads, 0);
  auto countAt = [&](int edge, int block) -> int& {
    return pinCount[static_cast<size_t>(edge) * threads + block];
  };
  for (int edge = 0; edge < edges; ++edge) {
    for (int pin : pins[edge]) {
      incident[pin].push_back(edge);
      countAt(edge, part[pin]) += 1;
    }
  }
  const double total = std::accumulate(weight.begin(), weight.end(), 0.0);
  const double limit = 1.03 * total / threads;
  for (int pass = 0; pass < 6; ++pass) {
    bool moved = false;
    for (int vertex : order) {
      if (incident[vertex].empty()) continue;
      const int from = part[vertex];
      int bestBlock = from;
      double bestDelta = 0;
      for (int to = 0; to < threads; ++to) {
        if (to == from) continue;
        const double next = load[to] + weight[vertex];
        if (next > limit && next > load[from]) continue;
        double delta = 0;
        for (int edge : incident[vertex]) {
          if (countAt(edge, from) == 1) delta -= edgeWeight[edge];
          if (countAt(edge, to) == 0) delta += edgeWeight[edge];
        }
        if (delta < bestDelta) {
          bestDelta = delta;
          bestBlock = to;
        }
      }
      if (bestBlock == from) continue;
      for (int edge : incident[vertex]) {
        int& fromCount = countAt(edge, from);
        fromCount -= 1;
        countAt(edge, bestBlock) += 1;
      }
      load[from] -= weight[vertex];
      load[bestBlock] += weight[vertex];
      part[vertex] = bestBlock;
      moved = true;
    }
    if (!moved) break;
  }
  return part;
}

bool partitionWithKaHyPar(const std::vector<double>& weight,
                          const std::vector<std::vector<int>>& pins,
                          const std::vector<double>& edgeWeight,
                          int threads,
                          const std::string& directory,
                          std::vector<int>& part) {
  if (threads <= 1 || weight.size() < static_cast<size_t>(threads) || pins.empty()) return false;
  const char* forced = std::getenv("GSIM_REPCUT_PARTITIONER");
  if (forced != nullptr && std::strcmp(forced, "internal") == 0) return false;
  const char* configured = std::getenv("GSIM_KAHYPAR");
  const char* fallback = "/home/piper/tools/bin/mtkahypar";
  const char* binary = configured != nullptr && configured[0] != '\0' &&
                       access(configured, X_OK) == 0 ? configured :
                       access(fallback, X_OK) == 0 ? fallback : nullptr;
  if (binary == nullptr) return false;
  if (mkdir(directory.c_str(), 0755) != 0 && errno != EEXIST) return false;

  double peak = 1;
  double total = 0;
  for (double value : weight) {
    peak = std::max(peak, value);
    total += value;
  }
  for (double value : edgeWeight) peak = std::max(peak, value);
  const double scale = total > 1e9 ? 1e9 / total : std::min(1000.0, 1e6 / peak);
  const std::string graph = directory + "/intersection.hgr";
  FILE* file = std::fopen(graph.c_str(), "w");
  if (file == nullptr) return false;
  fprintf(file, "%zu %zu 11\n", pins.size(), weight.size());
  for (size_t edge = 0; edge < pins.size(); ++edge) {
    fprintf(file, "%d", scaledWeight(edgeWeight[edge], scale));
    for (int pin : pins[edge]) fprintf(file, " %d", pin + 1);
    fputc('\n', file);
  }
  for (double value : weight) fprintf(file, "%d\n", scaledWeight(value, scale));
  std::fclose(file);

  DIR* handle = opendir(directory.c_str());
  if (handle != nullptr) {
    while (dirent* entry = readdir(handle)) {
      const std::string name = entry->d_name;
      if (name.size() >= 8 && name.compare(name.size() - 8, 8, ".KaHyPar") == 0) {
        std::remove((directory + "/" + name).c_str());
      }
    }
    closedir(handle);
  }

  const char* preset = std::getenv("GSIM_KAHYPAR_PRESET");
  if (preset == nullptr || preset[0] == '\0') preset = "deterministic";
  int workers = 16;
  if (const char* text = std::getenv("GSIM_KAHYPAR_THREADS")) {
    const int value = std::atoi(text);
    if (value > 0 && value <= 256) workers = value;
  }
  const std::string blocks = std::to_string(threads);
  const std::string workerText = std::to_string(workers);
  fprintf(stderr, "[repcut] kahypar k=%d vertices=%zu hyperedges=%zu preset=%s\n",
          threads, weight.size(), pins.size(), preset);
  fflush(stderr);
  char* argv[] = {
      const_cast<char*>(binary), const_cast<char*>("-h"), const_cast<char*>(graph.c_str()),
      const_cast<char*>("--preset-type"), const_cast<char*>(preset),
      const_cast<char*>("-k"), const_cast<char*>(blocks.c_str()),
      const_cast<char*>("-e"), const_cast<char*>("0.03"),
      const_cast<char*>("-o"), const_cast<char*>("km1"),
      const_cast<char*>("-w"), const_cast<char*>("--partition-output-folder"),
      const_cast<char*>(directory.c_str()), const_cast<char*>("-q"),
      const_cast<char*>("-t"), const_cast<char*>(workerText.c_str()),
      nullptr};
  pid_t pid = 0;
  if (posix_spawn(&pid, binary, nullptr, nullptr, argv, environ) != 0) return false;
  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;

  std::string result;
  handle = opendir(directory.c_str());
  if (handle == nullptr) return false;
  while (dirent* entry = readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.size() >= 8 && name.compare(name.size() - 8, 8, ".KaHyPar") == 0) {
      result = directory + "/" + name;
      break;
    }
  }
  closedir(handle);
  file = result.empty() ? nullptr : std::fopen(result.c_str(), "r");
  if (file == nullptr) return false;
  part.assign(weight.size(), 0);
  for (size_t vertex = 0; vertex < weight.size(); ++vertex) {
    int block = -1;
    if (fscanf(file, "%d", &block) != 1 || block < 0 || block >= threads) {
      std::fclose(file);
      return false;
    }
    part[vertex] = block;
  }
  std::fclose(file);
  return true;
}

}  // namespace

bool repCutSnapshotMemory(Node* memory) {
  return memory != nullptr && snapshotMems.count(memory) != 0;
}

int repCutMemoryLogPart(Node* access) {
  if (!globalConfig.RepCutMode || access == nullptr) return -1;
  if (access->type != NODE_READER && access->type != NODE_READWRITER &&
      access->type != NODE_WRITER) return -1;
  Node* memory = access->parent;
  auto found = snapshotWriterParts.find(memory);
  if (memory == nullptr || found == snapshotWriterParts.end() || snapshotMems.count(memory) == 0)
    return -1;
  const int part = access->rcPart;
  for (int writer : found->second) if (writer == part) return part;
  return -1;
}

const std::vector<int>& repCutSnapshotWriterParts(Node* memory) {
  static const std::vector<int> empty;
  auto found = snapshotWriterParts.find(memory);
  if (found == snapshotWriterParts.end()) return empty;
  return found->second;
}

const std::vector<Node*>& repCutSnapshotMemories() { return snapshotOrder; }

int repCutPartOf(SuperNode* super) { return partOf(super); }

bool repCutCommitOnly(SuperNode* super) { return commitOnlySuper(super); }

bool repCutMergeOk(SuperNode* a, SuperNode* b) {
  if (!globalConfig.RepCutMode) return true;
  const int left = partOf(a);
  const int right = partOf(b);
  if (left == -2 || right == -2) return false;
  if (left >= 0 && right >= 0 && left != right) return false;
  if (commitOnlySuper(a) != commitOnlySuper(b)) return false;
  // Serial reset cones stay in individual supers so reset groups can refresh
  // precisely the required cone, in cascade order.
  for (Node* node : a->member) if (node->rcResetSource) return false;
  for (Node* node : b->member) if (node->rcResetSource) return false;
  return true;
}

void graph::repCutSplit() {
  const int threads = repCutThreadsFromEnv();
  globalConfig.RepCutThreads = threads;
  boundaryNext.clear();
  localGraph = false;
  snapshotMems.clear();
  snapshotWriterParts.clear();
  snapshotOrder.clear();
  orderAllNodes();
  mergeResetAll();

  std::vector<Node*> nodes;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) nodes.push_back(node);
  }
  std::unordered_set<Node*> outputs(output.begin(), output.end());
  auto sideEffectFree = [](Node* node) {
    bool clean = true;
    walkAssign(node, [&](ENode* enode) {
      switch (enode->opType) {
        case OP_READ_MEM: case OP_WRITE_MEM: case OP_EXT_FUNC:
        case OP_PRINTF: case OP_ASSERT: case OP_EXIT: clean = false; break;
        default: break;
      }
    });
    return clean;
  };
  // Extmodule clock pins are not NODE_OTHERS, but clockOptimize keeps them as
  // wires. External clocks and == clocks are already constant and have no
  // side effects. Only a clock/reset expression that calls memory, DPI,
  // printf or assert is still rejected.
  auto pure = [&](Node* node) {
    if (node->type != NODE_OTHERS && node->type != NODE_OUT && !node->isClock) return false;
    return sideEffectFree(node);
  };

  // Reset cones read a consistent snapshot outside parallel evaluation. Do not
  // clone their triggers or execute reset side effects from a worker.
  std::vector<Node*> resetPending;
  for (SuperNode* reset : allReset) {
    resetPending.push_back(reset->resetNode);
    for (Node* member : reset->member) {
      std::set<Node*> rely;
      for (ExpTree* tree : member->assignTree) tree->getRelyNodes(rely);
      resetPending.insert(resetPending.end(), rely.begin(), rely.end());
    }
  }
  for (Node* node : nodes) if (node->isClock) resetPending.push_back(node);
  while (!resetPending.empty()) {
    Node* node = resetPending.back(); resetPending.pop_back();
    if (!node || node->status != VALID_NODE || node->type == NODE_REG_SRC ||
        node->type == NODE_INP || node->type == NODE_REG_RESET || node->rcResetSource) continue;
    Assert(pure(node), "RepCut reset/clock cone contains a non-pure node %s", node->name.c_str());
    node->rcResetSource = true;
    for (Node* prev : node->prev) resetPending.push_back(prev);
  }
  // Reset order must include registers written by earlier reset bodies.
  std::vector<std::set<Node*>> resetReads(allReset.size()), resetWrites(allReset.size());
  for (size_t i = 0; i < allReset.size(); ++i) {
    std::set<Node*> seen;
    std::vector<Node*> pending{allReset[i]->resetNode};
    while (!pending.empty()) {
      Node* n = pending.back(); pending.pop_back();
      if (!seen.insert(n).second) continue;
      if (!n->rcResetSource) { resetReads[i].insert(n); continue; }
      for (Node* prev : n->prev) pending.push_back(prev);
    }
    for (Node* member : allReset[i]->member) resetWrites[i].insert(member->getResetSrc());
  }
  std::vector<int> degree(allReset.size());
  std::vector<std::vector<int>> nextReset(allReset.size());
  for (size_t i = 0; i < allReset.size(); ++i) for (size_t j = 0; j < allReset.size(); ++j) {
    if (i == j || allReset[i]->superType != SUPER_ASYNC_RESET ||
        allReset[j]->superType != SUPER_ASYNC_RESET) continue;
    for (Node* n : resetReads[j]) if (resetWrites[i].count(n)) {
      nextReset[i].push_back(j); ++degree[j]; break;
    }
  }
  std::vector<SuperNode*> resetOrder;
  std::queue<int> readyReset;
  for (size_t i = 0; i < degree.size(); ++i) if (!degree[i]) readyReset.push(i);
  while (!readyReset.empty()) {
    int i = readyReset.front(); readyReset.pop(); resetOrder.push_back(allReset[i]);
    for (int j : nextReset[i]) if (!--degree[j]) readyReset.push(j);
  }
  Assert(resetOrder.size() == allReset.size(), "RepCut asynchronous reset cascade contains a cycle");
  allReset.swap(resetOrder);
  // Async side effects are emitted in the serial phase, not by genSuperEval.
  for (SuperNode* super : sortedSuper) if (super->superType == SUPER_ASYNC_RESET) super->superType = SUPER_VALID;

  auto source = [](Node* node) {
    return node->status == CONSTANT_NODE || node->type == NODE_REG_SRC ||
           node->type == NODE_INP || node->type == NODE_MEMORY || node->rcResetSource;
  };
  auto writeFirst = [](Node* node) {
    return node->type == NODE_READER && node->parent != nullptr && node->parent->extraInfo == "new";
  };
  auto oldReader = [](Node* node) {
    return node->type == NODE_READER && node->parent != nullptr && node->parent->extraInfo != "new";
  };
  // Pure cones and read-old memory ports can be cloned. Writers, write-first
  // ports, registers, extmodules and side effects stay on one partition.
  auto replicable = [&](Node* node) {
    if (node->status != VALID_NODE || source(node) || writeFirst(node)) return false;
    if (oldReader(node)) return true;
    if (node->type == NODE_WRITER || node->type == NODE_READWRITER || node->type == NODE_REG_DST ||
        node->type == NODE_SPECIAL || node->type == NODE_EXT || node->type == NODE_EXT_IN ||
        node->type == NODE_EXT_OUT) return false;
    return pure(node);
  };

  std::unordered_map<Node*, int> index;
  index.reserve(nodes.size());
  std::vector<Node*> indexed(nodes.size());
  for (Node* node : nodes) {
    Assert(index.find(node) == index.end(), "RepCut saw node %s twice", node->name.c_str());
    const int id = static_cast<int>(index.size());
    index.emplace(node, id);
    indexed[id] = node;
  }
  const int count = static_cast<int>(indexed.size());

  auto expressionRefs = [](Node* node) {
    std::unordered_set<Node*> refs;
    walkAssign(node, [&](ENode* enode) {
      if (enode->nodePtr != nullptr) refs.insert(enode->nodePtr);
    });
    return refs;
  };
  // reader -> writer keeps read-old order inside one thread. It is not a
  // value edge, so it must not merge partitions or drag the reader cone over.
  std::unordered_map<Node*, std::unordered_set<Node*>> orderPred;
  for (Node* mem : memory) {
    std::vector<Node*> readers;
    std::vector<Node*> writers;
    for (Node* port : mem->member) {
      if (port->status != VALID_NODE) continue;
      if (port->type == NODE_READER) readers.push_back(port);
      else if (port->type == NODE_WRITER || port->type == NODE_READWRITER) writers.push_back(port);
    }
    for (Node* writer : writers) {
      const std::unordered_set<Node*> refs = expressionRefs(writer);
      for (Node* reader : readers) {
        if (refs.count(reader) == 0) orderPred[writer].insert(reader);
      }
    }
  }
  auto orderingPred = [&](Node* node, Node* prev) {
    auto found = orderPred.find(node);
    return found != orderPred.end() && found->second.count(prev) != 0;
  };

  std::vector<Node*> sinks;
  for (Node* node : nodes) {
    if (node->status == VALID_NODE && !source(node) &&
        (node->type != NODE_OTHERS || outputs.count(node) || node->next.empty())) {
      sinks.push_back(node);
    }
  }
  std::sort(sinks.begin(), sinks.end(), [](Node* left, Node* right) { return left->id < right->id; });

  std::vector<int> parent(count), tag(count, -1);
  std::iota(parent.begin(), parent.end(), 0);
  auto find = [&](int id) {
    int root = id;
    while (parent[root] != root) root = parent[root];
    while (parent[id] != id) {
      const int next = parent[id];
      parent[id] = root;
      id = next;
    }
    return root;
  };
  auto unite = [&](int left, int right) {
    if (left < 0) return right;
    if (right < 0) return left;
    left = find(left);
    right = find(right);
    if (left == right) return left;
    if (left > right) std::swap(left, right);
    parent[right] = left;
    return left;
  };
  for (int id = 0; id < count; ++id) {
    Node* node = indexed[id];
    if (node->status == VALID_NODE && !source(node) && !replicable(node)) tag[id] = id;
  }
  auto glue = [&](const std::vector<Node*>& ports) {
    int owner = -1;
    for (Node* port : ports) {
      auto found = index.find(port);
      if (found == index.end() || tag[found->second] < 0) continue;
      owner = unite(owner, tag[found->second]);
    }
    if (owner < 0) return;
    for (Node* port : ports) {
      auto found = index.find(port);
      if (found == index.end() || tag[found->second] < 0) continue;
      tag[found->second] = owner;
    }
  };
  for (Node* mem : memory) {
    std::vector<Node*> ports;
    for (Node* port : mem->member) {
      if (port->status != VALID_NODE) continue;
      if (port->type == NODE_WRITER || port->type == NODE_READWRITER ||
          (mem->extraInfo == "new" && port->type == NODE_READER)) {
        ports.push_back(port);
      }
    }
    glue(ports);
  }
  for (SuperNode* super : sortedSuper) {
    if (super->superType == SUPER_EXTMOD) glue(super->member);
  }

  std::vector<char> queued(count, 0);
  std::queue<int> pendingLabels;
  for (int id = 0; id < count; ++id) {
    if (tag[id] < 0) continue;
    pendingLabels.push(id);
    queued[id] = 1;
  }
  while (!pendingLabels.empty()) {
    const int id = pendingLabels.front();
    pendingLabels.pop();
    queued[id] = 0;
    if (tag[id] < 0) continue;
    for (Node* next : indexed[id]->next) {
      if (next == nullptr || source(next) || orderingPred(next, indexed[id])) continue;
      auto found = index.find(next);
      if (found == index.end()) continue;
      const int nextId = found->second;
      const int before = tag[nextId] < 0 ? -1 : find(tag[nextId]);
      tag[nextId] = unite(tag[nextId], tag[id]);
      if (find(tag[nextId]) != before && !queued[nextId]) {
        pendingLabels.push(nextId);
        queued[nextId] = 1;
      }
    }
  }

  std::map<int, std::vector<Node*>> sinkGroups;
  for (Node* sink : sinks) {
    const int id = index.at(sink);
    const int key = tag[id] < 0 ? -(id + 1) : find(tag[id]);
    sinkGroups[key].push_back(sink);
  }
  std::vector<std::vector<Node*>> groupSinks;
  groupSinks.reserve(sinkGroups.size());
  for (auto& entry : sinkGroups) groupSinks.push_back(std::move(entry.second));
  const int groupCount = static_cast<int>(groupSinks.size());

  // Intersection hypergraph (RepCut ASPLOS'23): one cone per resource-group of
  // sinks, clusters are nodes with the same cone set. Sink groups are vertices.
  // A non-sink cluster is a hyperedge to every sink group that uses it; cutting
  // that edge is exactly the later per-partition replication.
  std::vector<int> predOffset(count + 1, 0);
  std::vector<int> predTo;
  for (int id = 0; id < count; ++id) {
    predOffset[id] = static_cast<int>(predTo.size());
    Node* node = indexed[id];
    for (Node* prev : node->prev) {
      if (prev == nullptr || source(prev) || orderingPred(node, prev)) continue;
      auto found = index.find(prev);
      if (found == index.end()) continue;
      predTo.push_back(found->second);
    }
  }
  predOffset[count] = static_cast<int>(predTo.size());

  struct Cons { int group; int tail; };
  std::vector<Cons> cons(1, Cons{-1, 0});
  std::vector<int> coneHead(count, 0);
  std::vector<int> seen(count, 0);
  std::unordered_map<int, int> split;
  std::vector<int> fringe;
  int coneStamp = 0;
  for (int group = 0; group < groupCount; ++group) {
    if (++coneStamp == 0) {
      std::fill(seen.begin(), seen.end(), 0);
      coneStamp = 1;
    }
    fringe.clear();
    split.clear();
    for (Node* sink : groupSinks[group]) {
      const int id = index.at(sink);
      if (seen[id] == coneStamp) continue;
      seen[id] = coneStamp;
      fringe.push_back(id);
    }
    for (size_t cursor = 0; cursor < fringe.size(); ++cursor) {
      const int id = fringe[cursor];
      for (int pred = predOffset[id]; pred < predOffset[id + 1]; ++pred) {
        const int previous = predTo[pred];
        if (seen[previous] == coneStamp) continue;
        seen[previous] = coneStamp;
        fringe.push_back(previous);
      }
    }
    for (int id : fringe) {
      const int old = coneHead[id];
      auto found = split.find(old);
      if (found == split.end()) {
        cons.push_back(Cons{group, old});
        found = split.emplace(old, static_cast<int>(cons.size() - 1)).first;
      }
      coneHead[id] = found->second;
    }
  }

  struct Cluster {
    int64_t eta = 0;
    bool anchored = false;
    const char* name = "";
  };
  std::unordered_map<int, Cluster> clusters;
  for (Node* node : nodes) {
    if (node->status != VALID_NODE || source(node)) continue;
    const int head = coneHead[index.at(node)];
    if (head == 0) continue;
    Cluster& cluster = clusters[head];
    cluster.eta += mtStructuralNodeCost(node);
    if (!replicable(node)) {
      cluster.anchored = true;
      cluster.name = node->name.c_str();
    }
  }

  std::vector<double> vertexWeight(groupCount, 0);
  std::vector<std::vector<int>> hedges;
  std::vector<double> hedgeWeight;
  for (const auto& entry : clusters) {
    std::vector<int> pins;
    for (int cursor = entry.first; cursor != 0; cursor = cons[cursor].tail) {
      pins.push_back(cons[cursor].group);
    }
    std::sort(pins.begin(), pins.end());
    pins.erase(std::unique(pins.begin(), pins.end()), pins.end());
    Assert(!entry.second.anchored || pins.size() <= 1,
           "RepCut resource group failed to keep %s together", entry.second.name);
    if (pins.size() <= 1) {
      if (pins.size() == 1) vertexWeight[pins[0]] += entry.second.eta;
      continue;
    }
    const double share = static_cast<double>(entry.second.eta) / pins.size();
    for (int pin : pins) vertexWeight[pin] += share;
    if (entry.second.eta <= 0) continue;
    hedges.push_back(std::move(pins));
    hedgeWeight.push_back(static_cast<double>(entry.second.eta));
  }
  for (double& value : vertexWeight) if (value <= 0) value = 1;

  const std::string kahyparDir = globalConfig.OutputDir + "/repcut-kahypar";
  std::vector<int> assigned;
  const char* solver = "km1-local";
  if (partitionWithKaHyPar(vertexWeight, hedges, hedgeWeight, threads, kahyparDir, assigned)) {
    solver = "kahypar-km1";
  } else {
    assigned = partitionKm1(vertexWeight, hedges, hedgeWeight, threads);
  }
  double km1 = 0;
  std::vector<double> blockWeight(threads, 0);
  for (int group = 0; group < groupCount; ++group) blockWeight[assigned[group]] += vertexWeight[group];
  for (size_t edge = 0; edge < hedges.size(); ++edge) {
    uint64_t blocks = 0;
    for (int pin : hedges[edge]) blocks |= uint64_t{1} << assigned[pin];
    km1 += (__builtin_popcountll(blocks) - 1) * hedgeWeight[edge];
  }
  const double average = threads > 0 ? std::accumulate(blockWeight.begin(), blockWeight.end(), 0.0) / threads : 0;
  const double heaviest = blockWeight.empty() ? 0 : *std::max_element(blockWeight.begin(), blockWeight.end());
  fprintf(stderr, "[repcut] intersection vertices=%d hyperedges=%zu solver=%s km1=%.0f balance=%.3f\n",
          groupCount, hedges.size(), solver, km1, average > 0 ? (heaviest - average) / average : 0);

  std::vector<std::vector<Node*>> roots(threads);
  for (int group = 0; group < groupCount; ++group) {
    const int block = assigned[group];
    for (Node* sink : groupSinks[group]) {
      sink->rcPart = block;
      roots[block].push_back(sink);
    }
  }

  std::vector<uint64_t> mask(count, 0);
  for (int part = 0; part < threads; ++part) {
    const uint64_t bit = uint64_t{1} << part;
    std::vector<Node*> walk = roots[part];
    for (size_t cursor = 0; cursor < walk.size(); ++cursor) {
      Node* node = walk[cursor];
      auto found = index.find(node);
      if (found == index.end() || (mask[found->second] & bit) != 0) continue;
      mask[found->second] |= bit;
      for (Node* prev : node->prev) {
        if (prev == nullptr || source(prev) || orderingPred(node, prev)) continue;
        walk.push_back(prev);
      }
    }
  }

  std::unordered_map<Node*, std::unordered_map<int, Node*>> version;
  size_t replicated = 0;
  std::vector<size_t> originalCount(threads), replicaCount(threads);
  std::vector<size_t> sharedInter(threads), exclusiveInter(threads);
  std::vector<double> work(threads);
  for (Node* n : nodes) {
    uint64_t parts = mask[index.at(n)];
    if (parts && n->status == VALID_NODE && n->type == NODE_OTHERS && replicable(n)) {
      if (__builtin_popcountll(parts) == 1) {
        ++exclusiveInter[__builtin_ctzll(parts)];
      } else {
        for (int part = 0; part < threads; ++part) {
          if (parts & (uint64_t{1} << part)) ++sharedInter[part];
        }
      }
    }
    if (!parts) { if (n->rcPart < 0) n->rcPart = 0; continue; }
    Assert(replicable(n) || __builtin_popcountll(parts)==1,
           "RepCut resource group failed to keep %s together",n->name.c_str());
    const int canonical = __builtin_ctzll(parts);
    n->rcPart = canonical; ++originalCount[canonical]; work[canonical] += mtStructuralNodeCost(n);
    if (__builtin_popcountll(parts) == 1) continue;
    version[n][canonical] = n;
    for (int part = canonical+1; part < threads; ++part) if (parts&(uint64_t{1}<<part)) {
      Node* copy = n->dup(n->type,n->name+"$RC$"+std::to_string(part));
      copy->status = n->status; copy->rcPart = part; copy->parent = n->parent;
      copy->usedBit = n->usedBit;
      for (ExpTree* tree : n->assignTree) copy->assignTree.push_back(tree->dup());
      copy->constructSuperNode(); sortedSuper.push_back(copy->super); allNodes.push_back(copy);
      version[n][part] = copy; ++replicated; ++replicaCount[part]; work[part] += mtStructuralNodeCost(n);
    }
  }
  for (Node* mem : memory) {
    if (mem == nullptr || mem->extraInfo == "new") continue;
    std::vector<int> writerParts;
    for (Node* port : mem->member) {
      if (port->status != VALID_NODE || port->rcPart < 0) continue;
      if (port->type != NODE_WRITER && port->type != NODE_READWRITER) continue;
      writerParts.push_back(port->rcPart);
    }
    std::sort(writerParts.begin(), writerParts.end());
    writerParts.erase(std::unique(writerParts.begin(), writerParts.end()), writerParts.end());
    if (writerParts.empty()) continue;
    bool cross = false;
    auto conflicts = [&](int part) {
      if (part < 0) return;
      for (int writer : writerParts) if (writer != part) cross = true;
    };
    for (Node* port : mem->member) {
      if (port->status != VALID_NODE) continue;
      if (port->type != NODE_READER && port->type != NODE_READWRITER) continue;
      conflicts(port->rcPart);
      auto found = version.find(port);
      if (found == version.end()) continue;
      for (const auto& copy : found->second) conflicts(copy.first);
    }
    if (!cross) continue;
    // Readers outside a writer partition must observe the pre-commit array.
    // Each writer partition keeps a private log and flushes it with its own
    // register commit. Same-address writes from different ports are undefined,
    // so those logs do not need another barrier.
    snapshotMems.insert(mem);
    snapshotWriterParts[mem] = std::move(writerParts);
    snapshotOrder.push_back(mem);
  }
  std::vector<size_t> regdstCount(threads), regdstBits(threads);
  for (Node* reg : regsrc) {
    reg->rcCommitOnly = true;
    Node* dst = reg->getDst();
    const int part = dst && dst->rcPart >= 0 ? dst->rcPart : 0;
    reg->rcPart = part;
    if (reg->status == DEAD_NODE || part < 0 || part >= threads) continue;
    ++regdstCount[part];
    const int bits = dst && dst->width > 0 ? dst->width : (reg->width > 0 ? reg->width : 0);
    regdstBits[part] += static_cast<size_t>(bits);
  }
  for (Node* n : nodes) {
    for (ExpTree* tree : n->assignTree) retargetTree(tree,n->rcPart,version);
  }
  for (const auto& entry : version) for (const auto& v : entry.second) {
    if (v.second == entry.first) continue;
    for (ExpTree* tree : v.second->assignTree) retargetTree(tree,v.first,version);
  }
  reconnectAll(); resort();
  FILE* report = std::fopen((globalConfig.OutputDir+"/repcut-partitions.csv").c_str(),"w");
  Assert(report,"cannot write RepCut partition report");
  fprintf(report,"worker,sinks,original_nodes,replica_nodes,structural_cost,regdst_count,regdst_bits,shared_intermediates,exclusive_intermediates\n");
  int nonempty = 0;
  for (int part = 0; part < threads; ++part) {
    nonempty += !roots[part].empty();
    fprintf(report,"%d,%zu,%zu,%zu,%.0f,%zu,%zu,%zu,%zu\n",part,roots[part].size(),originalCount[part],replicaCount[part],work[part],regdstCount[part],regdstBits[part],sharedInter[part],exclusiveInter[part]);
    fprintf(stderr,"[repcut] partition=%d sinks=%zu original=%zu replicas=%zu cost=%.0f regdst=%zu regdst_bits=%zu shared_intermediates=%zu exclusive_intermediates=%zu\n",part,roots[part].size(),originalCount[part],replicaCount[part],work[part],regdstCount[part],regdstBits[part],sharedInter[part],exclusiveInter[part]);
  }
  fclose(report);
  fprintf(stderr,"[repcut] threads=%d nonempty=%d sinks=%zu resource-groups=%zu replicated=%zu deferred-memories=%zu\n",
          threads,nonempty,sinks.size(),static_cast<size_t>(groupCount),replicated,snapshotOrder.size());
  localGraph = true;
}

void graph::finalizeRepCutParts() {
  if (!globalConfig.RepCutMode) return;
  for (SuperNode* super : sortedSuper) {
    int part = partOf(super);
    Assert(part != -2, "RepCut merged nodes from different partitions into super %d", super->id);
    if (part < 0) part = 0;
    for (Node* node : super->member) {
      if (node->rcPart < 0) node->rcPart = part;
    }
  }
}


void repCutNoteBoundary(Node* from, Node* to) {
  std::vector<Node*>& nexts = boundaryNext[from];
  if (std::find(nexts.begin(), nexts.end(), to) == nexts.end()) nexts.push_back(to);
}

const std::vector<Node*>& repCutBoundaryNext(Node* node) {
  static const std::vector<Node*> empty;
  auto found = boundaryNext.find(node);
  if (found == boundaryNext.end()) return empty;
  return found->second;
}


bool repCutLocalGraph() { return globalConfig.RepCutMode && localGraph; }

void repCutPruneEdges(const std::vector<SuperNode*>& supers) {
  auto boundary = [](Node* left, Node* right) {
    return left->rcPart >= 0 && right->rcPart >= 0 &&
           (left->rcPart != right->rcPart || left->rcResetSource != right->rcResetSource);
  };
  std::unordered_map<Node*, std::unordered_set<Node*>> uses;
  auto expressionUses = [&](Node* node, Node* target) {
    auto found = uses.find(node);
    if (found == uses.end()) {
      std::unordered_set<Node*> refs;
      walkAssign(node, [&](ENode* enode) {
        if (enode->nodePtr != nullptr) refs.insert(enode->nodePtr);
      });
      found = uses.emplace(node, std::move(refs)).first;
    }
    return found->second.count(target) != 0;
  };
  // A snapshotted read-old port has no value dependence on the writer. Drop
  // the ordering edge instead of activating the writer from another worker.
  auto snapshotOrder = [&](Node* from, Node* to) {
    if (from == nullptr || to == nullptr || from->type != NODE_READER) return false;
    if (to->type != NODE_WRITER && to->type != NODE_READWRITER) return false;
    if (from->parent == nullptr || from->parent != to->parent) return false;
    if (!repCutSnapshotMemory(from->parent)) return false;
    return !expressionUses(to, from);
  };
  auto cutEdge = [&](Node* from, Node* to) {
    if (!snapshotOrder(from, to)) {
      Assert(from->type == NODE_REG_SRC || from->type == NODE_INP || from->rcResetSource ||
             from->status == CONSTANT_NODE,
             "RepCut has an unsynchronized compute edge %s -> %s", from->name.c_str(), to->name.c_str());
      repCutNoteBoundary(from, to);
    }
    from->eraseNext(to);
    to->erasePrev(from);
  };
  for (SuperNode* super : supers) for (Node* node : super->member) {
    for (Node* prev : std::vector<Node*>(node->prev.begin(), node->prev.end())) if (boundary(prev, node)) {
      cutEdge(prev, node);
    }
    for (Node* next : std::vector<Node*>(node->next.begin(), node->next.end())) if (boundary(node, next)) {
      cutEdge(node, next);
    }
    for (Node* prev : std::vector<Node*>(node->depPrev.begin(), node->depPrev.end())) if (boundary(prev, node)) {
      prev->eraseDepNext(node); node->eraseDepPrev(prev);
    }
    for (Node* next : std::vector<Node*>(node->depNext.begin(), node->depNext.end())) if (boundary(node, next)) {
      node->eraseDepNext(next); next->eraseDepPrev(node);
    }
  }
}
