%{
  #include <stack>
  #include <string>
  #include "common.h"
  #include "PNode.h"
  #include "syntax.hh"
  #include "Parser.h"
  int curr_indent = 0;
  int angle_num = 0;
  int square_num = 0;
  int bracket_num = 0;
  int parenthesis_num = 0;
  static int TAB_WIDTH = 2;
  std::stack<int>indentLevels;
  #define YY_USER_INIT { \
    indentLevels.push(0); \
    BEGIN(indent); \
  }
  typedef Parser::Syntax::token token;
  #define YY_DECL int Parser::Lexical::lex(Parser::Syntax::semantic_type* yylval)
  int Parser::Lexical::lex_debug(Parser::Syntax::semantic_type* yylval) {
    int ret = lex(yylval);
    std::cout << "(" << lineno() << ", " << ret << ") " << yytext << std::endl;
    return ret;
  }
%}

%option c++ noyywrap yylineno nodefault

/* Integer literal definitions */
digit_bin   0|1
digit_oct   [0-7]
digit_dec   [0-9]
digit_hex   [0-9A-Fa-f]
/* Integer */
int         -?{digit_dec}+
/* Radix-specified Integer Literals */
rint        -?0b{digit_bin}+|-?0o{digit_oct}+|-?0d{digit_dec}+|-?0h{digit_hex}+
int_dec     -?{digit_dec}+
/* Identifiers define legal FIRRTL or Verilog names */
letter      [A-Za-z]
id          (_|{letter})(_|{letter}|{digit_dec})*
string      \"((\\\")|[!|0-9A-Za-z %\\,:=_^.&~\t()-{}$'])*\"
info        [0-9A-Za-z.{}:,/ _-]+
/* Fileinfo communicates Chisel source file and line/column info */

/* %x initial */
%x indent
%x info
%x dotId
%s normal

%%


<indent>" "   { curr_indent ++; }
<indent>\t    { curr_indent = (curr_indent + TAB_WIDTH) & ~ (TAB_WIDTH - 1); }
<indent>\n    { curr_indent = 0; }
<indent>.     { yyless(0);
                if(curr_indent > indentLevels.top()) {
                  BEGIN(normal); 
                  indentLevels.push(curr_indent);
                  return token::INDENT;
                } else if(curr_indent < indentLevels.top()) {
                  /* may need to return multiple DEDENT */
                  indentLevels.pop();
                  return token::DEDENT;
                } else {
                  BEGIN(normal);
                }
              }
<indent><<EOF>> { if(indentLevels.top() != 0) {
                    indentLevels.pop();
                    return token::DEDENT;
                  } else {
                    yyterminate();
                  }
                }

<info>"["       { }
<info>{info}* { yylval->strVal = strdup(yytext); return token::Info; }
<info>"]"       { BEGIN(normal); }

<dotId>{int_dec}|{id} { yylval->name = strdup(yytext); BEGIN(normal); return token::ID; }

<normal>"@"     { BEGIN(info); }
<normal>"<<"    { return token::DoubleLeft; }
<normal>">>"    { return token::DoubleRight; }
<normal>Clock                   { yylval->typeGround = strdup(yytext); return token::Clock; }
<normal>Reset                   { yylval->typeGround = strdup(yytext); return token::Reset; }
<normal>AsyncReset              { yylval->typeGround = strdup(yytext); return token::AsyReset; }
<normal>UInt|SInt               { yylval->typeGround = strdup(yytext); return token::IntType; }
<normal>Analog                  { yylval->typeGround = strdup(yytext); return token::anaType; }
<normal>Fixed                   { yylval->typeGround = strdup(yytext); return token::FixedType; }
<normal>flip                    { return token::Flip;}
<normal>(add|sub|mul|div|rem|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat)[(] { parenthesis_num++; yylval->typeOP = strndup(yytext, strlen(yytext)-1); return token::E2OP; }
<normal>(asUInt|asSInt|asClock|asAsyncReset|cvt|neg|not|andr|orr|xorr)[(] { parenthesis_num ++; yylval->typeOP = strndup(yytext, strlen(yytext)-1); return token::E1OP; }
<normal>(pad|shl|shr|head|tail)[(]   { parenthesis_num ++; yylval->typeOP = strndup(yytext, strlen(yytext)-1); return token::E1I1OP; }
<normal>bits[(]                 { parenthesis_num ++; yylval->typeOP = strndup(yytext, strlen(yytext)-1); return token::E1I2OP; }
<normal>mux                     { return token::Mux; }
<normal>validif                 { return token::Validif; }
<normal>old|new|undefined       { yylval->typeRUW = strdup(yytext); return token::Ruw; }
<normal>mem                     { return token::Mem; }
<normal>{int}                   { yylval->strVal = strdup(yytext); return token::INT; }
<normal>{rint}                  { yylval->strVal = strdup(yytext); return token::RINT; }
<normal>{int_dec}               { if (angle_num == 0 && square_num == 0 && parenthesis_num == 0 && bracket_num != 0) { \
                                    yylval->name = strdup(yytext); return token::ID; \
                                  } \
                                  yylval->strVal = strdup(yytext); return token::INT; }
<normal>data-type               { return token::DataType; }
<normal>depth                   { return token::Depth; }
<normal>read-latency            { return token::ReadLatency; }
<normal>write-latency           { return token::WriteLatency; }
<normal>read-under-write        { return token::ReadUnderwrite; }
<normal>reader                  { return token::Reader; }
<normal>writer                  { return token::Writer; }
<normal>readwriter              { return token::Readwriter; }
<normal>wire                    { return token::Wire; }
<normal>reg                     { return token::Reg; }
<normal>regreset                { return token::RegReset; }
<normal>inst                    { return token::Inst; }
<normal>of                      { return token::Of; }
<normal>node                    { return token::Node; }
<normal>invalidate              { return token::Invalidate; }
<normal>attach                  { return token::Attach; }
<normal>when                    { return token::When; }
<normal>else                    { return token::Else; }
<normal>stop                    { return token::Stop; }
<normal>printf                  { return token::Printf; }
<normal>assert                  { return token::Assert; }
<normal>skip                    { return token::Skip; }
<normal>input                   { return token::Input; }
<normal>output                  { return token::Output; }
<normal>module                  { return token::Module; }
<normal>extmodule               { return token::Extmodule; }
<normal>defname                 { return token::Defname; }
<normal>parameter               { return token::Parameter; }
<normal>intmodule               { return token::Intmodule; }
<normal>intrinsic               { return token::Intrinsic; }
<normal>FIRRTL                  { return token::Firrtl; }
<normal>version                 { return token::Version; }
<normal>circuit                 { return token::Circuit; }
<normal>connect                 { return token::Connect; }
<normal>public                  { return token::Public; }
<normal>{id}                    { yylval->name = strdup(yytext); return token::ID; }
<normal>{string}                { yylval->strVal = strdup(yytext); return token::String; }
<normal>"=>"                    { return token::RightArrow; }
<normal>"<-"                    { return token::Leftarrow; }
<normal>"<"                     { angle_num ++; return yytext[0];}
<normal>">"                     { angle_num --; return yytext[0];}
<normal>"["                     { square_num ++; return yytext[0];}
<normal>"]"                     { square_num --; return yytext[0];}
<normal>"{"                     { bracket_num ++; return yytext[0];}
<normal>"}"                     { bracket_num --; return yytext[0];}
<normal>"("                     { parenthesis_num ++; return yytext[0];}
<normal>")"                     { parenthesis_num --; return yytext[0];}
<normal>"."                     { if (lineno() != 1) BEGIN(dotId); return yytext[0]; }
<normal>[.,:=@%<>()\[\]{}""]    { return yytext[0]; }
<normal>[ \t]                   { }
<normal>\n                      { curr_indent = 0; BEGIN(indent); }
<normal>.                       { std::cout << "Mysterious character: " << yytext << std::endl; }

%%

int yyFlexLexer::yylex() {
    throw std::runtime_error("Invalid call to yyFlexLexer::yylex()");
}