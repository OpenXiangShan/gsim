/**
 * @file opFuncs.h
 * @brief operation functions
 */

#ifndef PARSER_H
#define PARSER_H

#if !defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

#include <stack>
#include "PNode.h"

namespace Parser {

class Lexical : public yyFlexLexer {
public:
  PNode* root = NULL;
  Lexical(std::istream& arg_yyin, std::ostream& arg_yyout) : Lexical(&arg_yyin, &arg_yyout) {}
  Lexical(std::istream* arg_yyin = nullptr, std::ostream* arg_yyout = nullptr) :
    yyFlexLexer(arg_yyin, arg_yyout) {
    indentLevels.push(0);
  }
  int lex(Syntax::semantic_type* yylval);
  int lex_debug(Syntax::semantic_type* yylval);
protected:
  int curr_indent = 0;
  int angle_num = 0;
  int square_num = 0;
  int bracket_num = 0;
  int parenthesis_num = 0;
  std::stack<int>indentLevels;
};

}  // namespace Parser

#endif
