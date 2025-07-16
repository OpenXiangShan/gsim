%{
  #include <iostream>
  #include "common.h"
  #include "syntax.hh"
  #include "Parser.h"

int p_stoi(const char* str);
//   int  yylex (yy::parser::value_type* yylval);
static char* emptyStr = NULL;
%}

%language "c++"
%require "3.2"

%define api.parser.class {Syntax}
%define api.namespace {Parser}
%define parse.error verbose
%parse-param {Lexical* scanner}

%code requires
{
    namespace Parser {
        class Lexical;
    } // namespace calc
}

%code
{
    #define synlineno() scanner->lineno()
    #define yylex(x) scanner->lex(x)
    // #define yylex(x) scanner->lex_debug(x)
}

%union {
  const char* name;
  const char* strVal;
  const char* typeGround;
  const char* typeAggregate;
  const char* typeOP;
  const char* typeRUW;
  int         intVal;
  PNode*      pnode;
  PList*      plist;
}

/* token */
%token <name> ID
%token <strVal> INT RINT String
%token DoubleLeft "<<"
%token DoubleRight ">>"
%token <typeGround> Clock Reset IntType anaType FixedType AsyReset Probe ProbeType
/* %token <intVal> IntVal */

%token <typeOP> E2OP E1OP E1I1OP E1I2OP  
%token <typeRUW> Ruw
%token <strVal> Info
%token Flip Mux Validif Invalidate Mem SMem CMem Wire Reg RegReset Inst Of Node Attach
%token When Else Stop Printf Skip Input Output Assert
%token Module Extmodule Defname Parameter Intmodule Intrinsic Circuit Connect Public
%token Define Const
%token Firrtl Version INDENT DEDENT
%token RightArrow "=>"
%token Leftarrow "<-"
%token DataType Depth ReadLatency WriteLatency ReadUnderwrite Reader Writer Readwriter Write Read Infer Rdwr Mport
/* internal node */
%type <intVal> width
%type <plist> cir_mods fields
%type <pnode> module extmodule ports statements port type statement when_else param exprs params
%type <pnode> chirrtl_memory chirrtl_memory_datatype chirrtl_memory_port
%type <pnode> reference expr primop_2expr primop_1expr primop_1expr1int primop_1expr2int
%type <pnode> field type_aggregate type_ground circuit
%type <strVal> info ALLID ext_defname
/* %token <pnode> */

%nonassoc LOWER_THAN_ELSE
%nonassoc Else


%%
/* remove version */
circuit: version Circuit ALLID ':' annotations info INDENT cir_mods DEDENT { scanner->root = newNode(P_CIRCUIT, synlineno(), $6, $3); scanner->list = $8; (void)yynerrs_;}
  | INDENT cir_mods DEDENT { scanner->list = $2; }
  ;
ALLID: ID {$$ = $1; }
    | Inst { $$ = "inst"; }
    | Printf { $$ = "printf"; }
    | Assert { $$ = "assert"; }
    | Mem { $$ = "mem"; }
    | Of { $$ = "of"; }
    | Reg { $$ = "reg"; }
    | Input { $$ = "input"; }
    | Output { $$ = "output"; }
    | Invalidate { $$ = "invalidate"; }
    | Mux { $$ = "mux"; }
    | Stop { $$ = "stop"; }
    | Depth {$$ = "depth"; }
    | Skip {$$ = "skip"; }
    | Write {$$ = "write"; }
    | Writer {$$ = "writer"; }
    | Read {$$ = "read"; }
    | Reader {$$ = "reader"; }
    | Version {$$ = "version"; }
    | Probe {$$ = "probe"; }
    | Module { $$ = "module"; }
    | Const { $$ = "const"; }
    ;
/* Fileinfo communicates Chisel source file and line/column info */
/* linecol: INT ':' INT    { $$ = malloc(strlen($1) + strlen($2) + 2); strcpy($$, $1); str$1 + ":" + $3}
    ; */
info:               { $$ = NULL;}
    | Info          { $$ = $1;}
    ;
/* type definition */
width:                { $$ = -1; } /* infered width */
    | '<' INT '>'     { $$ = p_stoi($2); }
    ;
binary_point:
    | "<<" INT ">>"   { TODO(); }
    ;
type_ground: Clock              { $$ = new PNode(P_Clock, synlineno()); }
    | Reset                     { $$ = new PNode(P_RESET, synlineno()); $$->setWidth(1); $$->setSign(0);}
    | AsyReset                  { $$ = new PNode(P_ASYRESET, synlineno()); $$->setWidth(1); $$->setSign(0);}
    | IntType width             { $$ = newNode(P_INT_TYPE, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); }
    | ProbeType '<' IntType '>' { $$ = newNode(P_INT_TYPE, synlineno(), $3, 0); $$->setWidth(-1); $$->setSign($3[0] == 'S'); }
    | ProbeType '<' IntType width '>' { $$ = newNode(P_INT_TYPE, synlineno(), $3, 0); $$->setWidth($4); $$->setSign($3[0] == 'S'); }
    | ProbeType '<' IntType '<' INT ">>" { $$ = newNode(P_INT_TYPE, synlineno(), $3, 0); $$->setWidth(p_stoi($5)); $$->setSign($3[0] == 'S'); }
    | anaType width             { TODO(); }
    | FixedType width binary_point  { TODO(); }
    ;
fields:                 { $$ = new PList(); }
    | fields ',' field  { $$ = $1; $$->append($3); }
    | field             { $$ = new PList($1); }
    ;
type_aggregate: '{' fields '}'  { $$ = new PNode(P_AG_FIELDS, synlineno()); $$->appendChildList($2); }
    | type '[' INT ']'          { $$ = newNode(P_AG_ARRAY, synlineno(), emptyStr, 1, $1); $$->appendExtraInfo($3); }
    ;
field: ALLID ':' type { $$ = newNode(P_FIELD, synlineno(), $1, 1, $3); }
    | Flip ALLID ':' type  { $$ = newNode(P_FLIP_FIELD, synlineno(), $2, 1, $4); }
    ;
type: type_ground  { $$ = $1; }
    | Const type_ground  { $$ = $2; }
    | type_aggregate { $$ = $1; }
    | Const type_aggregate { $$ = $2; }
    ;
/* primitive operations */
primop_2expr: E2OP expr ',' expr ')' { $$ = newNode(P_2EXPR, synlineno(), $1, 2, $2, $4); }
    ;
primop_1expr: E1OP expr ')' { $$ = newNode(P_1EXPR, synlineno(), $1, 1, $2); }
    ;
primop_1expr1int: E1I1OP expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); }
    ;
primop_1expr2int: E1I2OP expr ',' INT ',' INT ')' { $$ = newNode(P_1EXPR2INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); $$->appendExtraInfo($6); }
    ;
/* expression definitions */
exprs:                  { $$ = new PNode(P_EXPRS, synlineno());}
    | exprs ',' expr    { $$ = $1; $$->appendChild($3); }
    ;
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S');}
    | IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
    | IntType width '(' RINT ')'{ $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
    | reference { $$ = $1; }
    | Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, synlineno(), NULL, 3, $3, $5, $7); }
    | Validif '(' expr ',' expr ')' { $$ = $5; }
    | primop_2expr  { $$ = $1; }
    | primop_1expr  { $$ = $1; }
    | primop_1expr1int  { $$ = $1; }
    | primop_1expr2int  { $$ = $1; }
    ;
reference: ALLID  { $$ = newNode(P_REF, synlineno(), $1, 0); }
    | reference '.' ALLID  { $$ = $1; $1->appendChild(newNode(P_REF_DOT, synlineno(), $3, 0)); }
    | reference '[' INT ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_INT, synlineno(), $3, 0)); }
    | reference '[' expr ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_EXPR, synlineno(), NULL, 1, $3)); }
    ;

/* CHIRRTL Memory */
chirrtl_memory_datatype: type { $$ = newNode(P_DATATYPE, synlineno(), NULL, 1, $1); }
                       ;

chirrtl_memory : SMem ALLID ':' chirrtl_memory_datatype Ruw info            { $$ = newNode(P_SEQ_MEMORY , synlineno(), /*info*/$6, /*name*/$2, 1, /* DataType */ $4); $$->appendExtraInfo($5); }
               | SMem ALLID ':' chirrtl_memory_datatype info                { $$ = newNode(P_SEQ_MEMORY , synlineno(), /*info*/$5, /*name*/$2, 1, /* DataType */ $4); }
               | CMem ALLID ':' chirrtl_memory_datatype info                { $$ = newNode(P_COMB_MEMORY, synlineno(), /*info*/$5, /*name*/$2, 1, /* DataType */ $4); }
               ;

chirrtl_memory_port: Write Mport ALLID '=' ALLID '[' expr ']' ',' expr info { $$ = newNode(P_WRITE, synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7, /* clock */ $10); $$->appendExtraInfo(/* MemName */$5); }
                   | Read  Mport ALLID '=' ALLID '[' expr ']' ',' expr info { $$ = newNode(P_READ , synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7, /* clock */ $10); $$->appendExtraInfo(/* MemName */$5); }
                   | Infer Mport ALLID '=' ALLID '[' expr ']' ',' expr info { $$ = newNode(P_INFER, synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7, /* clock */ $10); $$->appendExtraInfo(/* MemName */$5); }
                   | Rdwr Mport ALLID '=' ALLID '[' expr ']' ',' expr info { $$ = newNode(P_READWRITER, synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7, /* clock */ $10); $$->appendExtraInfo(/* MemName */$5); }
                   ;

/* statements */
references:
    | references reference { TODO(); }
    ;
statements: { $$ = new PNode(P_STATEMENTS, synlineno()); }
    | statements statement { $$ =  $1; $1->appendChild($2); }
    ;
when_else:  %prec LOWER_THAN_ELSE { $$ = new PNode(P_STATEMENTS, synlineno()); }
    | Else ':' INDENT statements DEDENT { $$ = $4; }
    ;
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
    | Reg      ALLID ':' type ',' expr info  { $$ = newNode(P_REG_DEF, $4->lineno, /* info */$7 , /* name */$2, /* num */2, /* Type */$4, /* Clock */$6); }
    | RegReset ALLID ':' type ',' expr ',' expr ',' expr info { $$ = newNode(P_REG_RESET_DEF, $4->lineno, /* info */$11, /* name */$2, /* num */4, /* Type */ $4, /* Clock */$6, /* Reset Cond */ $8 , /* Reset Val*/$10); }
    | chirrtl_memory      { $$ = $1; }
    | chirrtl_memory_port { $$ = $1; }
    | Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
    | Node ALLID '=' expr info { $$ = newNode(P_NODE, synlineno(), $5, $2, 1, $4); }
    | Connect reference ',' expr info { $$ = newNode(P_CONNECT, $2->lineno, $5, NULL, 2, $2, $4); }
    | Connect reference ',' Read '(' expr ')' info { $$ = newNode(P_CONNECT, $2->lineno, $8, NULL, 2, $2, $6); }
    | reference "<-" expr info  { $$ = newNode(P_PAR_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
    | Invalidate reference info { $$ = newNode(P_INVALID, synlineno(), nullptr, 0); $$->setWidth(1); $$ = newNode(P_CONNECT, $2->lineno, $3, NULL, 2, $2, $$); }
    | Define reference '=' Probe '(' expr ')' info { $$ = newNode(P_CONNECT, synlineno(), $8, NULL, 2, $2, $6); }
    | Define reference '=' expr info { $$ = newNode(P_CONNECT, synlineno(), $5, NULL, 2, $2, $4); }
    | Attach '(' references ')' info { TODO(); }
    | When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); } /* expected newline before statement */
    | Stop '(' expr ',' expr ',' INT ')' info   { $$ = newNode(P_STOP, synlineno(), $9, NULL, 2, $3, $5); $$->appendExtraInfo($7); }
    | Stop '(' expr ',' expr ',' INT ')' ':' ALLID info   { $$ = newNode(P_STOP, synlineno(), $11, $10, 2, $3, $5); $$->appendExtraInfo($7); }
    | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Skip info { $$ = NULL; }
    ;
/* module definitions */
port: Input ALLID ':' type info    { $$ = newNode(P_INPUT, synlineno(), $5, $2, 1, $4); }
    | Output ALLID ':' type info   { $$ = newNode(P_OUTPUT, synlineno(), $5, $2, 1, $4); }
    ;
ports:  { $$ = new PNode(P_PORTS); }
    | ports port    { $$ = $1; $$->appendChild($2); }
    ;
opt_public:   {}
    | Public  {}
    ;
module: opt_public Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $5, $3, 2, $7, $8); }
    ;
ext_defname:                       { $$ = ""; }
    | Defname '=' ALLID            { $$ = $3; }
    ;
params:                            { $$ = new PNode(P_PARAMS); }
    | params param                 { $$ = $1; $$->appendChild($2); }
    ;
param: Parameter ALLID '=' String  { $$ = newNode(P_PARAM_STR, synlineno(), $2, 0); $$->appendExtraInfo($4); }
    | Parameter ALLID '=' INT      { $$ = newNode(P_PARAM_INT, synlineno(), $2, 0); $$->appendExtraInfo($4); }
    ;
extmodule: Extmodule ALLID ':' info INDENT ports ext_defname params DEDENT  { $$ = newNode(P_EXTMOD, synlineno(), $4, $2, 2, $6, $8); $$->appendExtraInfo($7); }
    ;
intmodule: Intmodule ALLID ':' info INDENT ports Intrinsic '=' ALLID params DEDENT	{ TODO(); }
		;
/* in-line anotations */
member:
      String ':' String {}
    | String ':' ALLID {}
    | String ':' INT {}
    | String ':' json {}
    | String ':' json_array {}
    ;
members:
    | member             {}
    | member ',' members {}
    ;
Strings: String             {}
    | String ',' Strings     {}
    ;
json: '{' '}'  {}
    | '{' INDENT members DEDENT '}' {}
    | '[' INDENT Strings DEDENT ']' {}
    ;
jsons: json            {}
     | json ',' jsons  {}
     ;
json_array: '[' INDENT jsons DEDENT ']' { }
		;
annotations: 
		| '%' '[' json_array ']' { }
		;
/* version definition */
version: Firrtl Version INT '.' INT '.' INT { }
		;
cir_mods:                       { $$ = new PList(); }
		| cir_mods module       { $$ = $1; $$->append($2); }
		| cir_mods extmodule    { $$ = $1; $$->append($2); }
		| cir_mods intmodule    { TODO(); } // TODO
		;

%%

void Parser::Syntax::error(const std::string& msg) {
    auto Line = scanner->lineno();
    auto UnexpectedToken = std::string(scanner->YYText());
    std::cerr << "Error at line " << Line << ": " << msg 
          << " (unexpected token: '" << UnexpectedToken << "')." << std::endl;
    exit(EXIT_FAILURE);
}
