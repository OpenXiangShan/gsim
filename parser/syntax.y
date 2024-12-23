%{
  #include <iostream>
  #include "common.h"
  #include "PNode.h"
  #include "syntax.hh"
  #include "Parser.h"
  PNode* root;

int p_stoi(const char* str);
//   int  yylex (yy::parser::value_type* yylval);
  char* emptyStr = NULL;
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
%token DataType Depth ReadLatency WriteLatency ReadUnderwrite Reader Writer Readwriter Write Read Infer Mport
/* internal node */
%type <intVal> width
%type <plist> cir_mods mem_compulsory mem_optional fields params
%type <plist> mem_reader mem_writer mem_readwriter
%type <pnode> module extmodule ports statements port type statement when_else memory param exprs
%type <pnode> mem_datatype mem_depth mem_rlatency mem_wlatency mem_ruw
%type <pnode> chirrtl_memory chirrtl_memory_datatype chirrtl_memory_ruw chirrtl_memory_port
%type <pnode> reference expr primop_2expr primop_1expr primop_1expr1int primop_1expr2int
%type <pnode> field type_aggregate type_ground circuit
%type <strVal> info ALLID
/* %token <pnode> */

%nonassoc LOWER_THAN_ELSE
%nonassoc Else


%%
/* remove version */
circuit: version Circuit ALLID ':' annotations info INDENT cir_mods DEDENT { $$ = newNode(P_CIRCUIT, synlineno(), $6, $3, $8); root = $$; (void)yynerrs_;}
	;
ALLID: ID {$$ = $1; }
    | Inst { $$ = strdup("inst"); }
    | Printf { $$ = strdup("printf"); }
    | Assert { $$ = strdup("assert"); }
    | Mem { $$ = strdup("mem"); }
    | Of { $$ = strdup("of"); }
    | Reg { $$ = strdup("reg"); }
    | Input { $$ = strdup("input"); }
    | Output { $$ = strdup("output"); }
    | Invalidate { $$ = strdup("invalidate"); }
    | Mux { $$ = strdup("mux"); }
    | Stop { $$ = strdup("stop"); }
    | Depth {$$ = strdup("depth"); }
    | Skip {$$ = strdup("skip"); }
    | Write {$$ = strdup("write"); }
    | Read {$$ = strdup("read"); }
    | Version {$$ = strdup("version"); }
    | Probe {$$ = strdup("probe"); }
    | Module { $$ = strdup("module"); }
    | Const { $$ = strdup("const"); }
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
    | Reset                     { $$ = new PNode(P_INT_TYPE, synlineno()); $$->setWidth(1); $$->setSign(0);}
    | AsyReset                  { $$ = new PNode(P_ASYRESET, synlineno()); $$->setWidth(1); $$->setSign(0);}
    | IntType width             { $$ = newNode(P_INT_TYPE, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); }
    | ProbeType '<' IntType '>' { $$ = newNode(P_INT_TYPE, synlineno(), $3, 0); $$->setSign($3[0] == 'S'); }
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
/* Memory */
mem_datatype: DataType "=>" type { $$ = newNode(P_DATATYPE, synlineno(), NULL, 1, $3); }
    ;
mem_depth: Depth "=>" INT   { $$ = newNode(P_DEPTH, synlineno(), $3, 0); }
    ;
mem_rlatency: ReadLatency "=>" INT  { $$ = newNode(P_RLATENCT, synlineno(), $3, 0); }
    ;
mem_wlatency: WriteLatency "=>" INT { $$ = newNode(P_WLATENCT, synlineno(), $3, 0); }
    ;
mem_ruw: ReadUnderwrite "=>" Ruw { $$ = newNode(P_RUW, synlineno(), $3, 0); }
    ;
mem_compulsory: mem_datatype mem_depth mem_rlatency mem_wlatency { $$ = new PList(); $$->append(4, $1, $2, $3, $4); }
    ;
mem_reader: { $$ = new PList(); }
    | mem_reader Reader "=>" ALLID  { $$ = $1; $$->append(newNode(P_READER, synlineno(), $4, 0));}
    ;
mem_writer: { $$ = new PList(); }
    | mem_writer Writer "=>" ALLID    { $$ = $1; $$->append(newNode(P_WRITER, synlineno(), $4, 0));}
    ;
mem_readwriter: { $$ = new PList(); }
    | mem_readwriter Readwriter INT ALLID  { $$ = $1; $$->append(newNode(P_READWRITER, synlineno(), $4, 0));}
    ;
mem_optional: mem_reader mem_writer mem_readwriter { $$ = $1; $$->concat($2); $$->concat($3); }
    ;
memory: Mem ALLID ':' info INDENT mem_compulsory mem_optional mem_ruw DEDENT { $$ = newNode(P_MEMORY, synlineno(), $4, $2, 0); $$->appendChildList($6); $$->appendChild($8); $$->appendChildList($7); }
    ;

/* CHIRRTL Memory */
chirrtl_memory_datatype: type { $$ = newNode(P_DATATYPE, synlineno(), NULL, 1, $1); }
                       ;
chirrtl_memory_ruw: Ruw { $$ = newNode(P_RUW, synlineno(), $1, 0); }
                  ;

chirrtl_memory : SMem ALLID ':' chirrtl_memory_datatype ',' chirrtl_memory_ruw info { $$ = newNode(P_SEQ_MEMORY , synlineno(), /*info*/$7, /*name*/$2, 2, /* DataType */ $4, /* Ruw */ $6); }
               | SMem ALLID ':' chirrtl_memory_datatype info                        { $$ = newNode(P_SEQ_MEMORY , synlineno(), /*info*/$5, /*name*/$2, 1, /* DataType */ $4); }
               | CMem ALLID ':' chirrtl_memory_datatype info                        { $$ = newNode(P_COMB_MEMORY, synlineno(), /*info*/$5, /*name*/$2, 1, /* DataType */ $4              ); }
               ;

chirrtl_memory_port: Write Mport ALLID '=' ALLID '[' expr ']' ',' ALLID info { $$ = newNode(P_WRITE, synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7); $$->appendExtraInfo(/* MemName */$5); $$->appendExtraInfo(/* clock */$10); }
                   | Read  Mport ALLID '=' ALLID '[' expr ']' ',' ALLID info { $$ = newNode(P_READ , synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7); $$->appendExtraInfo(/* MemName */$5); $$->appendExtraInfo(/* clock */$10); }
                   | Infer Mport ALLID '=' ALLID '[' expr ']' ',' ALLID info { $$ = newNode(P_INFER, synlineno(), /* Info */ $11, /* Name */ $3, 2, /* Addr */ $7); $$->appendExtraInfo(/* MemName */$5); $$->appendExtraInfo(/* clock */$10); }
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
    | memory    { $$ = $1;}
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
ext_defname:                       {  }
    | Defname '=' ALLID            {  }
    ;
params:                            { $$ = new PList(); }
    | params param                 { $$ = $1; $$->append($2); }
    ;
param: Parameter ALLID '=' String  { TODO(); }
    | Parameter ALLID '=' INT      { TODO(); }
    ;
extmodule: Extmodule ALLID ':' info INDENT ports ext_defname params DEDENT  { $$ = newNode(P_EXTMOD, synlineno(), $4, $2, 1, $6); $$->appendChildList($8);}
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
