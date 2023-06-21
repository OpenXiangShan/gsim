/**
 * @file Parser.h
 * @brief Declaration of Lexical class.
 */

#ifndef PARSER_H
#define PARSER_H

#if !defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

namespace Parser {

class Lexical : public yyFlexLexer {
 public:
  Lexical(std::istream& arg_yyin, std::ostream& arg_yyout) : yyFlexLexer(arg_yyin, arg_yyout) {}
  Lexical(std::istream* arg_yyin = nullptr, std::ostream* arg_yyout = nullptr)
      : yyFlexLexer(arg_yyin, arg_yyout) {}
  int lex(Syntax::semantic_type* yylval);
  int lex_debug(Syntax::semantic_type* yylval);
};

}  // namespace Parser

#endif
