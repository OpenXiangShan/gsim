#include "common.h"
#include "syntax.hh"
#include "Parser.h"
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

typedef struct TaskRecord {
  off_t offset;
  int lineno;
  int id;
} TaskRecord;

static std::queue<TaskRecord> *taskQueue;
static std::mutex m;
static std::condition_variable cv;
static PList **lists;
static PNode *globalRoot;

void parseFunc(char *strbuf, int tid) {
  while (true) {
    std::unique_lock lk(m);
    cv.wait(lk, []{ return taskQueue->size() > 0; });
    auto e = taskQueue->front();
    taskQueue->pop();
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
  taskQueue = new std::queue<TaskRecord>;
  std::future<void> *threads = new std::future<void> [NR_THREAD];

  // create tasks
  char *prev = strbuf;
  char *next = strstr(prev, "\n  module ") + 1;
  int next_lineno = 1;
  int id = 0;
  while (true) {
    int prev_lineno = next_lineno;
    next = strstr(next, "\n  module ");
    bool isEnd = (next == NULL);
    if (!isEnd) {
      assert(next[0] == '\n');
      next[0] = '\0';
      for (char *q = prev; (q = strchr(q, '\n')) != NULL; next_lineno ++, q ++);
      next_lineno ++; // '\0' is overwritten originally from '\n', so count it
    }
    taskQueue->push(TaskRecord(prev - strbuf, prev_lineno, id));
    id ++;
    if (isEnd) break;
    next ++;
    prev = next;
  }

  printf("[Parser] using %d threads to parse %d modules...\n", NR_THREAD, id);
  lists = new PList* [id];
  for (int i = 0; i < NR_THREAD; i ++) {
    taskQueue->push(TaskRecord(0, 0, -1)); // exit flags
  }

  // create threads
  for (int i = 0; i < NR_THREAD; i ++) {
    threads[i] = async(std::launch::async, parseFunc, strbuf, i);
  }

  for (int i = 0; i < NR_THREAD; i ++) {
    printf("[Parser] waiting for thread %d/%d...\n", i, NR_THREAD - 1);
    threads[i].get();
  }
  assert(taskQueue->size() == 0);

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
