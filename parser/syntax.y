%{
  #include <iostream>
  #include "common.h"
  #include "PNode.h"
  PNode* root;

  int p_stoi(char* str);
%}

%language "c++"

%union {
  std::string name;
  std::string strVal;
  std::string typeGround;
  std::string typeAggregate;
  std::string typeOP;
  std::string typeRUW;
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
%token Flip Mux Validif Invalid Mem Wire Reg With Reset Inst Of Node Is Attach
%token When Else Stop Printf Skip Input Output
%token Module Extmodule Defname Parameter Intmodule Intrinsic Circuit
%token Class Target Firrtl Version
%token RightArrow "=>"
%token LeftArrow "<="
%token Leftarrow "<-"
%token DataType Depth ReadLatency WriteLatency ReadUnderwrite Reader Writer Readwriter
/* internal node */
%type <intVal> width
%type <plist> cir_mods statements mem_compulsory mem_optional fields
%type <pnode> module ports port type statement when_else memory
%type <pnode> mem_reader mem_writer mem_readwriter
%type <pnode> mem_datatype mem_depth mem_rlatency mem_wlatency mem_ruw
%type <pnode> reference expr primop_2expr primop_1expr primop_1expr1int primop_1expr2int
%type <pnode> field type_aggregate type_ground circuit
%type <strVal> info linecol
/* %token <pnode> */

%nonassoc LOWER_THAN_ELSE
%nonassoc Else

%%
/* remove version */
circuit: Circuit ID ':' annotations info cir_mods { $$ = newNode(P_CIRCUIT, $6, NULL); $$ = ; root = $$; }
	;
/* Fileinfo communicates Chisel source file and line/column info */
linecol: INT ':' INT    { $$ = $1 + ":" + $3}
    ;
info:               { $$ = std::string("");}
    | '@' '[' ']'   { $$ = std::string("");}
    | '@' '[' String linecol ']'    { $$ = $3 + $4}
    ;
/* type definition */
width:                { $$ = 0; } /* infered width */
    | '<' INT '>'     { $$ = p_stoi($2); }
    ;
binary_point: 
    | "<<" INT ">>"   { TODO(); }
    ;
type_ground: Clock    { $$ = new Node(P_Clock); }
    | IntType width   { $$ = newNode(P_INT_TYPE, "", $1, 0); $$.setWidth($2) }
    | anaType width   { TODO(); }
    | FixedType width binary_point  { TODO(); }
    ;
fields:                 { $$ = new PList(); }
    | field ',' fields  { $$ = $3; $$.append($1); }
    | field             { $$ = new Plist($1); }
    ;
type_aggregate: '{' fields '}'  { $$ = new PNode(P_AG_FIELDS); $$.appendChildList($2); }
    | type '[' INT ']'          { $$ = newNode(P_AG_TYPE, "", "", 1, $1); $$.appendExtraInfo($3); }
    ;
field: ID ':' type { $$ = newNode(P_FIELD, "", $1, 1, $3); }
    | Flip ID ':' type  { $$ = newNode(P_FLIP_FIELD, "", $2, 1, $4); }
    ;
type: type_ground  { $$ = $1; }
    | type_aggregate { $$ = $1; }
    ;
/* primitive operations */
primop_2expr: E2OP '(' expr ',' expr ')' { $$ = newNode(P_2EXPR, "", $1, 2, $3, $5); }
    ;
primop_1expr: E1OP '(' expr ')' { $$ = newNode(P_1EXPR, "", $1, 1, $3); }
    ;
primop_1expr1int: E1I1OP '(' expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, "", 1, $3); $$.appendExtraInfo($5); }
    ;
primop_1expr2int: E1I2OP '(' expr ',' INT ',' INT ')' { $$ = newNode(P_1EXPR2INT, "", 1, $3); $$.appendExtraInfo($5); $$.appendExtraInfo($7); }
    ;
/* expression definitions */
exprs:
    | expr exprs    { TODO(); }
    ;
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, "", $1, 0); $$.setWidth($2);}
    | IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, "", $1, 0); $$.setWidth($2); $$.appendExtraInfo($4);}
    | reference { $$ = $1; }
    | Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, "", "", 3, $3, $5, $7); }
    | Validif '(' expr ',' expr ')' { TODO(); }
    | primop_2expr  { $$ = $1; }
    | primop_1expr  { $$ = $1; }
    | primop_1expr1int  { $$ = $1; }
    | primop_1expr2int  { $$ = $1; }
    ;
reference: ID  { $$ = newNode(P_REF, "", $1, 0); }
    | reference '.' ID  { $$ = $1; $1.appendChild(newNode(P_REF_DOT, "", $3, 0)); }
    | reference '[' INT ']' { $$ = $1; $1.appendChild(newNode(P_REF_IDX, "", $3, 0)); }
    | reference '[' expr ']' { $$ = $1; $1.appendChild($3); }
    ;
/* Memory */
mem_datatype: DataType "=>" type { $$ = newNode(P_DATATYPE, "", "", 1, $3); }
    ;
mem_depth: Depth "=>" INT   { $$ = newNode(P_DEPTH, "", $3, 0); }
    ;
mem_rlatency: ReadLatency "=>" INT  { $$ = newNode(P_RLATENCT, "", $3, 0); }
    ;
mem_wlatency: WriteLatency "=>" INT { $$ = newNode(P_WLATENCT, "", $3, 0); }
    ;
mem_ruw: ReadUnderwrite "=>" Ruw { $$ = newNode(P_RUW, "", $3, 0); }
    ;
mem_compulsory: mem_datatype mem_depth mem_rlatency mem_wlatency mem_ruw { $$ = new PList(); $$.append(5, $1, $2, $3, $4, $5); }
    ;
mem_reader: { $$ = NULL; }
    | Reader "=>" ID    { $$ = newNode(P_READER, "", $3, 0);}
    ;
mem_writer: { $$ = NULL; }
    | Writer "=>" ID    { $$ = newNode(P_WRITER, "", $3, 0);}
    ;
mem_readwriter: { $$ = NULL; }
    | Readwriter "=>" ID    { $$ = newNode(P_READWRITER, "", $3, 0);}
    ;
mem_optional: mem_reader mem_writer mem_readwriter { $$ = new PList(); $$.append(3, $1, $2, $3); }
    ;
memory: Mem ID ':' info mem_compulsory mem_optional { $$ = newNode(P_MEMORY, $4, $2); $$.appendChildList($5); $$.appendChildList($6); }
    ;
/* statements */
references:
    | reference references  { TODO(); }
    ;
statements: { $$ = new PList(); }
    | statement statements { $$ =  $2; $$.appendChild($1); }
    ;
when_else:  %prec LOWER_THAN_ELSE { $$ = NULL; }
    | Else ':' statements { $$ = newNode(P_ELSE, "", $3); }
    ;
statement: Wire ID ':' type info    { $$ = newNode(P_WIRE_DEF, $5, $2, 1, $4); }
    | Reg ID ':' type expr '(' With ':' '{' Reset "=>" '(' expr ',' expr ')' '}' ')' info { $$ = newNode(P_REG_DEF, $19, $2, 4, $4, $5, $13, $15); }
    | memory    { $$ = $1;}
    | Inst ID Of ID info    { $$ = newNode(P_INST, $5, $2, 0); $$.appendExtraInfo($4); }
    | Node ID '=' expr info { $$ = newNode(P_NODE, $5, $2, 1, $4); }
    | reference "<=" expr info  { $$ = newNode(P_CONNECT, $4, "", 2, $1, $3); }
    | reference "<-" expr info  { TODO(); }
    | reference Is Invalid info { TODO(); }
    | Attach '(' references ')' info { TODO(); }
    | When expr ':' info statements when_else   { $$ = newNode(P_WHEN, $4, "", 3, $2, $5, $6)} /* expected newline before statement */
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
    | port ports    { $$ = $2; $$.appendChild($1); }
    ;
module: Module ID ':' info ports statements { $$ = newNode(P_MOD, $4, $2, 2, $5, $6); }
    ;
ext_defname:
    | Defname '=' ID            { TODO(); }
    ;
params: 
    | param params              { TODO(); }
    ;
param: Parameter ID '=' String  { TODO(); }
    | Parameter ID '=' INT      { TODO(); }
    ;
extmodule: Extmodule ID ':' info ports ext_defname params   { TODO(); }
    ;
intmodule: Intmodule ID ':' info ports Intrinsic '=' ID params 	{ TODO(); }
		;
/* in-line anotations */
jsons:
		| json jsons    { TODO(); }
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
		| module cir_mods       { $$ = $2; $$.push_back($1); }
		| extmodule cir_mods    { TODO(); }
		| intmodule cir_mods    { TODO(); }
		;
