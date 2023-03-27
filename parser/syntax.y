%{
  #include <iostream>
  #include "common.h"
  #include "PNode.h"
  #include "syntax.hh"
  #include "Parser.h"
  PNode* root;

  int p_stoi(char* str);
//   int  yylex (yy::parser::value_type* yylval);
  PNode* newNode(int type, char* info, char* name, int num, ...);
  PNode* newNode(int type, char* info, PList* plist);
  char* emptyStr = NULL;
%}

%language "c++"
%require "3.2"

%define api.parser.class {Syntax}
%define api.namespace {Parser}
%parse-param {Lexical* scanner}

%code requires
{
    namespace Parser {
        class Lexical;
    } // namespace calc
}

%code
{
    #define yylex(x) scanner->lex(x)
    /* #define yylex(x) scanner->lex_debug(x) */
}

%union {
  char*       name;
  char*       strVal;
  char*       typeGround;
  char*       typeAggregate;
  char*       typeOP;
  char*       typeRUW;
  int         intVal;
  PNode*      pnode;
  PList*      plist;
}

/* token */
%token <name> ID
%token <strVal> INT String
%token DoubleLeft "<<"
%token DoubleRight ">>"
%token <typeGround> Clock IntType anaType FixedType
/* %token <intVal> IntVal */

%token <typeOP> E2OP E1OP E1I1OP E1I2OP  
%token <typeRUW> Ruw
%token <strVal> Info
%token Flip Mux Validif Invalid Mem Wire Reg WithReset Inst Of Node Is Attach
%token When Else Stop Printf Skip Input Output
%token Module Extmodule Defname Parameter Intmodule Intrinsic Circuit
%token Class Target Firrtl Version INDENT DEDENT
%token RightArrow "=>"
%token LeftArrow "<="
%token Leftarrow "<-"
%token DataType Depth ReadLatency WriteLatency ReadUnderwrite Reader Writer Readwriter
/* internal node */
%type <intVal> width
%type <plist> cir_mods mem_compulsory mem_optional fields
%type <pnode> module ports statements port type statement when_else memory
%type <pnode> mem_reader mem_writer mem_readwriter
%type <pnode> mem_datatype mem_depth mem_rlatency mem_wlatency mem_ruw
%type <pnode> reference expr primop_2expr primop_1expr primop_1expr1int primop_1expr2int
%type <pnode> field type_aggregate type_ground circuit
%type <strVal> info
/* %token <pnode> */

%nonassoc LOWER_THAN_ELSE
%nonassoc Else


%%
/* remove version */
circuit: Circuit ID ':' annotations info INDENT cir_mods DEDENT { $$ = newNode(P_CIRCUIT, $5, $7); root = $$; }
	;
/* Fileinfo communicates Chisel source file and line/column info */
/* linecol: INT ':' INT    { $$ = malloc(strlen($1) + strlen($2) + 2); strcpy($$, $1); str$1 + ":" + $3}
    ; */
info:               { $$ = NULL;}
    | Info          { $$ = $1;}
    ;
/* type definition */
width:                { $$ = 0; } /* infered width */
    | '<' INT '>'     { $$ = p_stoi($2); }
    ;
binary_point:
    | "<<" INT ">>"   { TODO(); }
    ;
type_ground: Clock    { $$ = new PNode(P_Clock); }
    | IntType width   { $$ = newNode(P_INT_TYPE, $1, 0); $$->setWidth($2); }
    | anaType width   { TODO(); }
    | FixedType width binary_point  { TODO(); }
    ;
fields:                 { $$ = new PList(); }
    | fields ',' field  { $$ = $1; $$->append($3); }
    | field             { $$ = new PList($1); }
    ;
type_aggregate: '{' fields '}'  { $$ = new PNode(P_AG_FIELDS); $$->appendChildList($2); }
    | type '[' INT ']'          { $$ = newNode(P_AG_TYPE, emptyStr, 1, $1); $$->appendExtraInfo($3); }
    ;
field: ID ':' type { $$ = newNode(P_FIELD, $1, 1, $3); }
    | Flip ID ':' type  { $$ = newNode(P_FLIP_FIELD, $2, 1, $4); }
    ;
type: type_ground  { $$ = $1; }
    | type_aggregate { $$ = $1; }
    ;
/* primitive operations */
primop_2expr: E2OP '(' expr ',' expr ')' { $$ = newNode(P_2EXPR, $1, 2, $3, $5); }
    ;
primop_1expr: E1OP '(' expr ')' { $$ = newNode(P_1EXPR, $1, 1, $3); }
    ;
primop_1expr1int: E1I1OP '(' expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, $1, 1, $3); $$->appendExtraInfo($5); }
    ;
primop_1expr2int: E1I2OP '(' expr ',' INT ',' INT ')' { $$ = newNode(P_1EXPR2INT, $1, 1, $3); $$->appendExtraInfo($5); $$->appendExtraInfo($7); }
    ;
/* expression definitions */
exprs:
    | exprs expr    { TODO(); }
    ;
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, $1, 0); $$->setWidth($2);}
    | IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, $1, 0); $$->setWidth($2); $$->appendExtraInfo($4);}
    | reference { $$ = $1; }
    | Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, NULL, 3, $3, $5, $7); }
    | Validif '(' expr ',' expr ')' { TODO(); }
    | primop_2expr  { $$ = $1; }
    | primop_1expr  { $$ = $1; }
    | primop_1expr1int  { $$ = $1; }
    | primop_1expr2int  { $$ = $1; }
    ;
reference: ID  { $$ = newNode(P_REF, $1, 0); }
    | reference '.' ID  { $$ = $1; $1->appendChild(newNode(P_REF_DOT, $3, 0)); }
    | reference '[' INT ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX, $3, 0)); }
    | reference '[' expr ']' { $$ = $1; $1->appendChild($3); }
    ;
/* Memory */
mem_datatype: DataType "=>" type { $$ = newNode(P_DATATYPE, NULL, 1, $3); }
    ;
mem_depth: Depth "=>" INT   { $$ = newNode(P_DEPTH, $3, 0); }
    ;
mem_rlatency: ReadLatency "=>" INT  { $$ = newNode(P_RLATENCT, $3, 0); }
    ;
mem_wlatency: WriteLatency "=>" INT { $$ = newNode(P_WLATENCT, $3, 0); }
    ;
mem_ruw: ReadUnderwrite "=>" Ruw { $$ = newNode(P_RUW, $3, 0); }
    ;
mem_compulsory: mem_datatype mem_depth mem_rlatency mem_wlatency mem_ruw { $$ = new PList(); $$->append(5, $1, $2, $3, $4, $5); }
    ;
mem_reader: { $$ = NULL; }
    | Reader "=>" ID    { $$ = newNode(P_READER, $3, 0);}
    ;
mem_writer: { $$ = NULL; }
    | Writer "=>" ID    { $$ = newNode(P_WRITER, $3, 0);}
    ;
mem_readwriter: { $$ = NULL; }
    | Readwriter "=>" ID    { $$ = newNode(P_READWRITER, $3, 0);}
    ;
mem_optional: mem_reader mem_writer mem_readwriter { $$ = new PList(); $$->append(3, $1, $2, $3); }
    ;
memory: Mem ID ':' info INDENT mem_compulsory mem_optional DEDENT { $$ = newNode(P_MEMORY, $4, $2, 0); $$->appendChildList($6); $$->appendChildList($7); }
    ;
/* statements */
references:
    | references reference { TODO(); }
    ;
statements: { $$ = new PNode(P_STATEMENTS); }
    | statements statement { $$ =  $1; $1->appendChild($2); }
    ;
when_else:  %prec LOWER_THAN_ELSE { $$ = NULL; }
    | Else ':' INDENT statements DEDENT { $$ = newNode(P_ELSE, NULL, NULL, 1, $4); }
    ;
statement: Wire ID ':' type info    { $$ = newNode(P_WIRE_DEF, $5, $2, 1, $4); }
    | Reg ID ':' type expr '(' WithReset '(' expr ',' expr ')' '}' ')' info { $$ = newNode(P_REG_DEF, $15, $2, 4, $4, $5, $9, $11); }
    | memory    { $$ = $1;}
    | Inst ID Of ID info    { $$ = newNode(P_INST, $5, $2, 0); $$->appendExtraInfo($4); }
    | Node ID '=' expr info { $$ = newNode(P_NODE, $5, $2, 1, $4); }
    | reference "<=" expr info  { $$ = newNode(P_CONNECT, $4, NULL, 2, $1, $3); }
    | reference "<-" expr info  { TODO(); }
    | reference Is Invalid info { TODO(); }
    | Attach '(' references ')' info { TODO(); }
    | When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $4, NULL, 3, $2, $6, $8); } /* expected newline before statement */
    | Stop '(' expr ',' expr ',' INT ')' info   { TODO(); }
    | Printf '(' expr ',' expr ',' String exprs ')' ':' ID info { TODO(); }
    | Printf '(' expr ',' expr ',' String exprs ')' info    { TODO(); }
    | Skip info { $$ = NULL; }
    ;
/* module definitions */
port: Input ID ':' type info    { $$ = newNode(P_INPUT, $5, $2, 1, $4); }
    | Output ID ':' type info   { $$ = newNode(P_OUTPUT, $5, $2, 1, $4); }
    ;
ports:  { $$ = new PNode(P_PORTS); }
    | ports port    { $$ = $1; $$->appendChild($2); }
    ;
module: Module ID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, $4, $2, 2, $6, $7); }
    ;
ext_defname:
    | Defname '=' ID            { TODO(); }
    ;
params: 
    | params param              { TODO(); }
    ;
param: Parameter ID '=' String  { TODO(); }
    | Parameter ID '=' INT      { TODO(); }
    ;
extmodule: Extmodule ID ':' info INDENT ports ext_defname params DEDENT  { TODO(); }
    ;
intmodule: Intmodule ID ':' info INDENT ports Intrinsic '=' ID params DEDENT	{ TODO(); }
		;
/* in-line anotations */
jsons:
		| jsons json    { TODO(); }
		;
json: '"' Class '"' ':' String '"' Target '"' ':' String    { TODO(); }
		;
json_array: '[' jsons ']'   { TODO(); }
		;
annotations: 
		| '%' '[' json_array ']' { TODO(); }
		;
/* version definition */
/*
version: Firrtl Version INT '.' INT '.' INT { TODO(); }
		;
*/
cir_mods:                       { $$ = new PList(); }
		| module cir_mods       { $$ = $2; $$->append($1); }
		| extmodule cir_mods    { TODO(); }
		| intmodule cir_mods    { TODO(); }
		;

%%

void Parser::Syntax::error(const std::string& msg) {
    std::cerr << msg << '\n';
}
