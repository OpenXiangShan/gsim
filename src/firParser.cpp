#include "common.h"
#include "syntax.hh"
#include "Parser.h"
#include <future>
#include <mutex>
#include <condition_variable>

#define MODULES_PER_TASK 5

typedef struct TaskRecord {
  off_t offset;
  size_t len;
  int lineno;
  int id;
} TaskRecord;

static std::vector<TaskRecord> *taskQueue;
static std::mutex m;
static std::condition_variable cv;
static PList **lists;
static PNode *globalRoot;

static const char* moduleHeaderKeywords[] = {
  "module ",
  "public module ",
  "private module ",
  "protected module ",
  "extmodule ",
  "public extmodule ",
  "private extmodule ",
  "protected extmodule ",
  "intmodule ",
  "public intmodule ",
  "private intmodule ",
  "protected intmodule "
};

static inline bool startsWithKeyword(const char* str, const char* keyword) {
  size_t len = strlen(keyword);
  return strncmp(str, keyword, len) == 0;
}

static char* findNextModuleMarker(char* start) {
  if (!start) return nullptr;
  char* cursor = start;
  while (true) {
    char* newline = strchr(cursor, '\n');
    if (!newline) return nullptr;
    char* after = newline + 1;
    if (after[0] == '\0') return nullptr;
    if (after[0] == ' ' && after[1] == ' ') {
      char* tokenStart = after + 2;
      for (const char* keyword : moduleHeaderKeywords) {
        if (startsWithKeyword(tokenStart, keyword)) {
          return newline;
        }
      }
    }
    cursor = newline + 1;
  }
}

void parseFunc(char *strbuf, int tid) {
  while (true) {
    std::unique_lock lk(m);
    cv.wait(lk, []{ return !taskQueue->empty(); });
    auto e = taskQueue->back();
    taskQueue->pop_back();
    lk.unlock();
    cv.notify_one();

    if (e.id == -1) { return; }
    //Log("e.offset = %ld, e.lineno = %d", e.offset, e.lineno);
    //for (int i = 0; i < 100; i ++) { putchar(strbuf[e.offset + i]); } putchar('\n');

    std::istringstream *streamBuf = new std::istringstream(strbuf + e.offset);
    Parser::Lexical *lexical = new Parser::Lexical(*streamBuf, std::cout);
    Parser::Syntax *syntax = new Parser::Syntax(lexical);
    lexical->set_lineno(e.lineno);
    syntax->parse();

    lists[e.id] = lexical->list;
    if (e.id == 0) {
      assert(lexical->root != NULL);
      globalRoot = lexical->root;
    }

    delete syntax;
    delete lexical;
    delete streamBuf;
  }
}

PNode* parseFIR(char *strbuf) {
  taskQueue = new std::vector<TaskRecord>;
  std::future<void> *threads = new std::future<void> [NR_THREAD];

  // create tasks
  char *prev = strbuf;
  int next_lineno = 1;
  int id = 0;
  int modules = 0;

  std::vector<char*> moduleMarkers;
  char* marker = findNextModuleMarker(prev);
  while (marker) {
    moduleMarkers.push_back(marker);
    marker = findNextModuleMarker(marker + 1);
  }
  modules = moduleMarkers.size();

  if (moduleMarkers.empty()) {
    taskQueue->push_back(TaskRecord{prev - strbuf, strlen(prev) + 1, next_lineno, id});
    id ++;
  } else {
    size_t idx = 0;
    while (idx < moduleMarkers.size()) {
      int prev_lineno = next_lineno;
      size_t nextIdx = idx + MODULES_PER_TASK;
      bool isEnd = nextIdx >= moduleMarkers.size();
      char* split = nullptr;
      if (!isEnd) {
        split = moduleMarkers[nextIdx];
        assert(split[0] == '\n');
        split[0] = '\0';
        for (char *q = prev; (q = strchr(q, '\n')) != NULL; next_lineno ++, q ++);
        next_lineno ++; // '\0' is overwritten originally from '\n', so count it
      }
      taskQueue->push_back(TaskRecord{prev - strbuf, strlen(prev) + 1, prev_lineno, id});
      id ++;
      if (isEnd) break;
      prev = split + 1;
      idx = nextIdx;
    }
  }
  printf("[Parser] using %d threads to parse %d modules with %d tasks\n",
      NR_THREAD, modules, id);
  lists = new PList* [id];
  for (int i = 0; i < NR_THREAD; i ++) {
    taskQueue->push_back(TaskRecord{0, 0, -1, -1}); // exit flags
  }

  // sort to handle the largest module first
  std::sort(taskQueue->begin(), taskQueue->end(),
      [](TaskRecord &a, TaskRecord &b){ return a.len < b.len; });

  // create threads
  for (int i = 0; i < NR_THREAD; i ++) {
    threads[i] = async(std::launch::async, parseFunc, strbuf, i);
  }

  for (int i = 0; i < NR_THREAD; i ++) {
    printf("[Parser] waiting for thread %d/%d...\n", i, NR_THREAD - 1);
    threads[i].get();
  }
  assert(taskQueue->empty());

  printf("[Parser] merging lists...\n");
  TIMER_START(MergeList);
  for (int i = 1; i < id; i ++) {
    lists[0]->concat(lists[i]);
  }
  globalRoot->child.assign(lists[0]->siblings.begin(), lists[0]->siblings.end());
  TIMER_END(MergeList);

  delete [] lists;
  delete [] threads;
  delete taskQueue;
  return globalRoot;
}
