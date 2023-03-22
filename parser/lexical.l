%{
  int curr_indent = 0;
  static int TAB_WIDTH = 2;
  std::stack<int>indentLevels;
  #define YY_USER_INIT { \
    indentLevels.push(0);
    BEGIN(indent);
  }
%}

%option c++
/* Integer literal definitions */
digit_bin   0|1
digit_oct   [0-7]
digit_dec   [0-9]
digit_hex   [0-9A-Fa-f]
/* Integer */
int         \"b-?{digit_bin}+\"|\"o-?{digit_oct}+\"|\"h-?{digit_hex}+\"|-?{digit_dec}+
/* Identifiers define legal FIRRTL or Verilog names */
letter      [A-Fa-f]
id          (_|{letter})(_|{letter}|{digit_dec})*
string      [0-9A-Za-z]+
/* Fileinfo communicates Chisel source file and line/column info */
linecol     {digit_dec}+:{digit_dec}+
info        @\[({string} {linecol})*\]

/* %x initial */
%x indent
%s normal

%%


<indent>" "   { curr_indent ++; }
<indent>\t    { curr_indent = (curr_indent + TAB_WIDTH) & ~ (TAB_WIDTH - 1); }
<indent>\n    { curr_indent = 0; }
<indent>.     { yyless(0);
                if(curr_indent > indentLevels.top()) {
                  BEGIN(normal); 
                  indentLevels.push(curr_indent);
                  return INDENT;
                } else if(curr_indent < indentLevels.top()) {
                  /* may need to return multiple DEDENT */
                  indentLevels.pop();
                  return DEDENT;
                } else {
                  BEGIN(normal);
                }
              }
<indent><<EOF>> { if(indentLevels.top() != 0) {
                    indentLevels.pop();
                    return DEDENT;
                  } else {
                    yyterminate();
                  }
                }

<normal>"<<"    { return DoubleLeft; }
<normal>">>"    { return DoubleRight; }
<normal>Clock|Reset|AsyncReset  { yylval.typeGround = std::string(yytext); return Clock; }
<normal>UInt|SInt               { yylval.typeGround = std::string(yytext); return IntType; }
<normal>Analog                  { yylval.typeGround = std::string(yytext); return anaType; }
<normal>Fixed                   { yylval.typeGround = std::string(yytext); return FixedType; }
<normal>flip                    { return Flip;}
<normal>add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat { yylval.typeOP = std::string(yytext); return E2OP; }
<normal>asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr { yylval.typeOP = std::string(yytext); return E1OP; }
<normal>pad|shl|shr|head|tail   { yylval.typeOP = std::string(yytext); return E1I1OP; }
<normal>bits                    { yylval.typeOP = std::string(yytext); return E1I2OP; }
<normal>mux                     { return Mux; }
<normal>validif                 { return Validif; }
<normal>old|new|undefined       { yylval.typeRUW = std::string(yytext); return Ruw; }
<normal>mem                     { return Mem; }
<normal>[.,:=@%<>()\[\]{}""]    { return yytext[0]; }
<normal>{int}                   { yylval.strVal = std::string(yytext); return INT; }
<normal>"=>"                    { return RightArrow; }
<normal>"<="                    { return LeftArrow; }
<normal>"<-"                    { return Leftarrow; }
<normal>data-type               { return DataType; }
<normal>depth                   { return Depth; }
<normal>read-latency            { return ReadLatency; }
<normal>write-latency           { return WriteLatency; }
<normal>read-under-write        { return ReadUnderwrite; }
<normal>reader                  { return Reader; }
<normal>writer                  { return Writer; }
<normal>readwriter              { return Readwriter; }
<normal>wire                    { return Wire; }
<normal>reg                     { return Reg; }
<normal>with                    { return With; }
<normal>reset                   { return Reset;} /* Reset & reset */
<normal>inst                    { return Inst; }
<normal>of                      { return Of; }
<normal>node                    { return Node; }
<normal>is                      { return Is; }
<normal>invalid                 { return Invalid; }
<normal>attach                  { return Attach; }
<normal>when                    { return When; }
<normal>else                    { return Else; }
<normal>stop                    { return Stop; }
<normal>printf                  { return Printf; }
<normal>skip                    { return Skip; }
<normal>input                   { return Input; }
<normal>output                  { return Output; }
<normal>module                  { return Module; }
<normal>extmodule               { return Extmodule; }
<normal>defname                 { return Defname; }
<normal>parameter               { return Parameter; }
<normal>intmodule               { return Intmodule; }
<normal>intrinsic               { return Intrinsic; }
<normal>class                   { return Class; }
<normal>target                  { return Target; }
<normal>FIRRTL                  { return Firrtl; }
<normal>version                 { return Version; }
<normal>circuit                 { return Circuit; }
<normal>{string}                { yylval.strVal = std::string(yytext); return String; }
<normal>[ \t]                   { }
<normal>\n                      { curr_indent = 0; BEGIN(indent); }
<normal>.                       { std::cout << "Mysterious character: " << yytext << std::endl; }

%%
