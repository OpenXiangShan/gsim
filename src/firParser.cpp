#include "common.h"
#include "syntax.hh"
#include "Parser.h"

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

  std::istringstream *streamBuf = new std::istringstream(strbuf);
  Parser::Lexical *lexical = new Parser::Lexical(*streamBuf, std::cout);
  Parser::Syntax *syntax = new Parser::Syntax(lexical);
  syntax->parse();

  std::istringstream *streamBuf2 = new std::istringstream(p);
  Parser::Lexical *lexical2 = new Parser::Lexical(*streamBuf2, std::cout);
  Parser::Syntax *syntax2 = new Parser::Syntax(lexical2);
  lexical2->set_lineno(next_lineno);
  syntax2->parse();

  PNode* globalRoot = lexical->root;
  lexical->list->concat(lexical2->list);
  globalRoot->child.assign(lexical->list->siblings.begin(), lexical->list->siblings.end());

  delete syntax;
  delete lexical;
  delete streamBuf;
  delete syntax2;
  delete lexical2;
  delete streamBuf2;

  return globalRoot;
}
