%{
    
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

%%
"<<"    { return DoubleLeft; }
">>"    { return DoubleRight; }
Clock|Reset|AsyncReset  { yylval.typeGround = std::string(yytext); return Clock; }
UInt|SInt               { yylval.typeGround = std::string(yytext); return IntType; }
Analog                  { yylval.typeGround = std::string(yytext); return anaType; }
Fixed                   { yylval.typeGround = std::string(yytext); return FixedType; }
flip                    { return Flip;}
add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat { yylval.typeOP = std::string(yytext); return E2OP; }
asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr { yylval.typeOP = std::string(yytext); return E1OP; }
pad|shl|shr|head|tail   { yylval.typeOP = std::string(yytext); return E1I1OP; }
bits                    { yylval.typeOP = std::string(yytext); return E1I2OP; }
mux                     { return Mux; }
validif                 { return Validif; }
old|new|undefined       { yylval.typeRUW = std::string(yytext); return Ruw; }
mem                     { return Mem; }
[.,:=@%<>()\[\]{}""]    { return yytext[0]; }
{int}                   { yylval.strVal = std::string(yytext); return INT; }
"=>"                    { return RightArrow; }
"<="                    { return LeftArrow; }
"<-"                    { return Leftarrow; }
data-type               { return DataType; }
depth                   { return Depth; }
read-latency            { return ReadLatency; }
write-latency           { return WriteLatency; }
read-under-write        { return ReadUnderwrite; }
reader                  { return Reader; }
writer                  { return Writer; }
readwriter              { return Readwriter; }
wire                    { return Wire; }
reg                     { return Reg; }
with                    { return With; }
reset                   { return Reset;} /* Reset & reset */
inst                    { return Inst; }
of                      { return Of; }
node                    { return Node; }
is                      { return Is; }
invalid                 { return Invalid; }
attach                  { return Attach; }
when                    { return When; }
else                    { return Else; }
stop                    { return Stop; }
printf                  { return Printf; }
skip                    { return Skip; }
input                   { return Input; }
output                  { return Output; }
module                  { return Module; }
extmodule               { return Extmodule; }
defname                 { return Defname; }
parameter               { return Parameter; }
intmodule               { return Intmodule; }
intrinsic               { return Intrinsic; }
class                   { return Class; }
target                  { return Target; }
FIRRTL                  { return Firrtl; }
version                 { return Version; }
circuit                 { return Circuit; }
{string}                { yylval.strVal = std::string(yytext); return String; }
[ \n\t]                 { }
.                       { std::cout << "Mysterious character: " << yytext << std::endl; }

%%
