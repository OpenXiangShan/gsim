#include "common.h"
#include "syntax.hh"
#include "Parser.h"
#include <future>

static PList* parseFunc(char *strbuf, PNode **globalRoot, int lineno) {
  std::istringstream *streamBuf = new std::istringstream(strbuf);
  Parser::Lexical *lexical = new Parser::Lexical(*streamBuf, std::cout);
  Parser::Syntax *syntax = new Parser::Syntax(lexical);
  lexical->set_lineno(lineno);
  syntax->parse();
  PList *list = lexical->list;
  if (globalRoot) {
    assert(lexical->root != NULL);
    *globalRoot = lexical->root;
  }
  delete syntax;
  delete lexical;
  delete streamBuf;
  return list;
}

PNode* parseFIR(char *strbuf) {
  char *p = strbuf;
  for (int i = 0; i < 150; i ++) {
    p = strstr(p, "\n  module ");
    p ++;
  }
  p[-1] = '\0';

  int next_lineno = 1;
  for (char *q = strbuf; (q = strchr(q, '\n')) != NULL; next_lineno ++, q ++);
  next_lineno ++; // '\0' is overwritten originally from '\n', so count it

  PNode* globalRoot = NULL;
  std::future<PList *> result[2];
  result[0] = async(std::launch::async, parseFunc, strbuf, &globalRoot, 1);
  result[1] = async(std::launch::async, parseFunc, p, nullptr, next_lineno);
  PList *list0 = result[0].get();
  PList *list1 = result[1].get();

  list0->concat(list1);
  globalRoot->child.assign(list0->siblings.begin(), list0->siblings.end());
  return globalRoot;
}
