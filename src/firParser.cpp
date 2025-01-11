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
  char *next = strstr(prev, "\n  module ");
  int next_lineno = 1;
  int id = 0;
  int modules = 0;
  while (true) {
    int prev_lineno = next_lineno;
    bool isEnd = false;
    for (int i = 0; i < MODULES_PER_TASK; i ++) {
      next ++;
      next = strstr(next, "\n  module ");
      modules ++;
      isEnd = (next == NULL);
      if (isEnd) break;
    }
    if (!isEnd) {
      assert(next[0] == '\n');
      next[0] = '\0';
      for (char *q = prev; (q = strchr(q, '\n')) != NULL; next_lineno ++, q ++);
      next_lineno ++; // '\0' is overwritten originally from '\n', so count it
    }
    taskQueue->push_back(TaskRecord{prev - strbuf, strlen(prev) + 1, prev_lineno, id});
    id ++;
    if (isEnd) break;
    prev = next + 1;
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
