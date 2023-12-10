
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "common/log/log.h"
#include "common/lang/string.h"
#include "sql/parser/parse_defs.h"
#include "sql/parser/yacc_sql.hpp"
#include "sql/parser/lex_sql.h"
#include "sql/expr/expression.h"

using namespace std;

string token_name(const char *sql_string, YYLTYPE *llocp)
{
  return string(sql_string + llocp->first_column, llocp->last_column - llocp->first_column + 1);
}

int yyerror(YYLTYPE *llocp, const char *sql_string, ParsedSqlResult *sql_result, yyscan_t scanner, const char *msg)
{
  std::unique_ptr<ParsedSqlNode> error_sql_node = std::make_unique<ParsedSqlNode>(SCF_ERROR);
  error_sql_node->error.error_msg = msg;
  error_sql_node->error.line = llocp->first_line;
  error_sql_node->error.column = llocp->first_column;
  sql_result->add_sql_node(std::move(error_sql_node));
  return 0;
}

ArithmeticExpr *create_arithmetic_expression(ArithmeticExpr::Type type,
                                             Expression *left,
                                             Expression *right,
                                             const char *sql_string,
                                             YYLTYPE *llocp)
{
  ArithmeticExpr *expr = new ArithmeticExpr(type, left, right);
  expr->set_name(token_name(sql_string, llocp));
  return expr;
}

%}

%define api.pure full
%define parse.error verbose
/** 启用位置标识 **/
%locations
%lex-param { yyscan_t scanner }
/** 这些定义了在yyparse函数中的参数 **/
%parse-param { const char * sql_string }
%parse-param { ParsedSqlResult * sql_result }
%parse-param { void * scanner }

//标识tokens
%token  SEMICOLON
        CREATE
        DROP
        TABLE
        TABLES
        INDEX
        CALC
        SELECT
        SHOW
        SYNC
        INSERT
        DELETE
        UPDATE
        LBRACE
        RBRACE
        COMMA
        TRX_BEGIN
        TRX_COMMIT
        TRX_ROLLBACK
        INT_T
        STRING_T
        FLOAT_T
        DATE_T
        HELP
        EXIT
        DOT //QUOTE
        INTO
        VALUES
        FROM
        WHERE
        AND
        SET
        ON
        LOAD
        DATA
        INFILE
        EXPLAIN
        EQ
        LT
        GT
        LE
        GE
        NE
        MAX
        MIN
        COUNT
        AVG
        SUM
        UNIQUE
        IS_T
        NOT
        NULL_T
        NULLABLE
        INNER
        JOIN
        ORDER
        BY
        ASC_T
        DESC_T
        GROUP

/** union 中定义各种数据类型，真实生成的代码也是union类型，所以不能有非POD类型的数据 **/
%union {
  ParsedSqlNode *                   sql_node;
  ConditionSqlNode *                condition;
  Value *                           value;
  enum CompOp                       comp;
  RelAttrSqlNode *                  rel_attr;
  std::vector<AttrInfoSqlNode> *    attr_infos;
  AttrInfoSqlNode *                 attr_info;
  Expression *                      expression;
  std::vector<Expression *> *       expression_list;
  std::vector<Value> *              value_list;
  std::vector<ConditionSqlNode> *   condition_list;
  std::vector<RelAttrSqlNode> *     rel_attr_list;
  std::vector<std::string> *        relation_list;
  char *                            string;
  int                               number;
  float                             floats;
  date                              dates;
  AggrFuncType                      aggr_func_type;
  AggrFuncSqlNode*                  aggr_func_node;
  SelectExprSqlNode*                select_expr_node;
  std::vector<SelectExprSqlNode> *  s_expr_node_list;
  RawTuple *                        raw_tuple;
  std::vector<RawTuple> *           raw_tuple_list;
  JoinSqlNode *                     join_node;  
  std::vector<JoinSqlNode>*         join_list;
  OrderType                         order_type;
  OrderSqlNode *                    order_node;
  std::vector<OrderSqlNode> *       order_node_list;
  GroupSqlNode *                    group_node;
  std::vector<GroupSqlNode> *       group_node_list;
}

%token <number> NUMBER
%token <floats> FLOAT
%token <dates> DATE
%token <string> ID
%token <string> SSS
//非终结符

/** type 定义了各种解析后的结果输出的是什么类型。类型对应了 union 中的定义的成员变量名称 **/
%type <number>              type
%type <condition>           condition
%type <value>               value
%type <number>              number
%type <comp>                comp_op
%type <rel_attr>            rel_attr
%type <attr_infos>          attr_def_list
%type <attr_info>           attr_def
%type <value_list>          value_list
%type <condition_list>      where
%type <condition_list>      condition_list
%type <rel_attr_list>       select_attr
%type <relation_list>       rel_list
%type <rel_attr_list>       attr_list
%type <expression>          expression
%type <expression_list>     expression_list
%type <sql_node>            calc_stmt
%type <sql_node>            select_stmt
%type <sql_node>            insert_stmt
%type <sql_node>            update_stmt
%type <sql_node>            delete_stmt
%type <sql_node>            create_table_stmt
%type <sql_node>            drop_table_stmt
%type <sql_node>            show_tables_stmt
%type <sql_node>            desc_table_stmt
%type <sql_node>            create_index_stmt
%type <sql_node>            drop_index_stmt
%type <sql_node>            sync_stmt
%type <sql_node>            begin_stmt
%type <sql_node>            commit_stmt
%type <sql_node>            rollback_stmt
%type <sql_node>            load_data_stmt
%type <sql_node>            explain_stmt
%type <sql_node>            set_variable_stmt
%type <sql_node>            help_stmt
%type <sql_node>            exit_stmt
%type <sql_node>            command_wrapper
// commands should be a list but I use a single command instead
%type <sql_node>            commands
%type <aggr_func_type>      aggr_func_type
%type <aggr_func_node>      aggr_func
%type <select_expr_node>    select_expr
%type <s_expr_node_list>    select_expr_list
%type <s_expr_node_list>    select_exprs
%type <raw_tuple>           raw_tuple
%type <raw_tuple_list>      raw_tuple_list
%type <join_node>           join_node
%type <join_list>           join_list
%type <order_type>          order_type
%type <order_node>          order_node
%type <order_node_list>     order_node_list
%type <order_node_list>     order
%type <group_node>          group_node
%type <group_node_list>     group_node_list
%type <group_node_list>     group

%left '+' '-'
%left '*' '/'
%nonassoc UMINUS
%%

commands: command_wrapper opt_semicolon  //commands or sqls. parser starts here.
  {
    std::unique_ptr<ParsedSqlNode> sql_node = std::unique_ptr<ParsedSqlNode>($1);
    sql_result->add_sql_node(std::move(sql_node));
  }
  ;

command_wrapper:
    calc_stmt
  | select_stmt
  | insert_stmt
  | update_stmt
  | delete_stmt
  | create_table_stmt
  | drop_table_stmt
  | show_tables_stmt
  | desc_table_stmt
  | create_index_stmt
  | drop_index_stmt
  | sync_stmt
  | begin_stmt
  | commit_stmt
  | rollback_stmt
  | load_data_stmt
  | explain_stmt
  | set_variable_stmt
  | help_stmt
  | exit_stmt
    ;

exit_stmt:      
    EXIT {
      (void)yynerrs;  // 这么写为了消除yynerrs未使用的告警。如果你有更好的方法欢迎提PR
      $$ = new ParsedSqlNode(SCF_EXIT);
    };

help_stmt:
    HELP {
      $$ = new ParsedSqlNode(SCF_HELP);
    };

sync_stmt:
    SYNC {
      $$ = new ParsedSqlNode(SCF_SYNC);
    }
    ;

begin_stmt:
    TRX_BEGIN  {
      $$ = new ParsedSqlNode(SCF_BEGIN);
    }
    ;

commit_stmt:
    TRX_COMMIT {
      $$ = new ParsedSqlNode(SCF_COMMIT);
    }
    ;

rollback_stmt:
    TRX_ROLLBACK  {
      $$ = new ParsedSqlNode(SCF_ROLLBACK);
    }
    ;

drop_table_stmt:    /*drop table 语句的语法解析树*/
    DROP TABLE ID {
      $$ = new ParsedSqlNode(SCF_DROP_TABLE);
      $$->drop_table.relation_name = $3;
      free($3);
    };

show_tables_stmt:
    SHOW TABLES {
      $$ = new ParsedSqlNode(SCF_SHOW_TABLES);
    }
    ;

desc_table_stmt:
    DESC_T ID  {
      $$ = new ParsedSqlNode(SCF_DESC_TABLE);
      $$->desc_table.relation_name = $2;
      free($2);
    }
    ;

create_index_stmt:    /*create index 语句的语法解析树*/
    CREATE INDEX ID ON ID LBRACE ID RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.index_name = $3;
      create_index.relation_name = $5;
      create_index.attribute_name = $7;
      create_index.unique=false;
      free($3);
      free($5);
      free($7);
    }
    | CREATE UNIQUE INDEX ID ON ID LBRACE ID RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_INDEX);
      CreateIndexSqlNode &create_index = $$->create_index;
      create_index.index_name = $4;
      create_index.relation_name = $6;
      create_index.attribute_name = $8;
      create_index.unique=true;
      free($4);
      free($6);
      free($8);
    }
    ;

drop_index_stmt:      /*drop index 语句的语法解析树*/
    DROP INDEX ID ON ID
    {
      $$ = new ParsedSqlNode(SCF_DROP_INDEX);
      $$->drop_index.index_name = $3;
      $$->drop_index.relation_name = $5;
      free($3);
      free($5);
    }
    ;
create_table_stmt:    /*create table 语句的语法解析树*/
    CREATE TABLE ID LBRACE attr_def attr_def_list RBRACE
    {
      $$ = new ParsedSqlNode(SCF_CREATE_TABLE);
      CreateTableSqlNode &create_table = $$->create_table;
      create_table.relation_name = $3;
      free($3);

      std::vector<AttrInfoSqlNode> *src_attrs = $6;

      if (src_attrs != nullptr) {
        create_table.attr_infos.swap(*src_attrs);
      }
      create_table.attr_infos.emplace_back(*$5);
      std::reverse(create_table.attr_infos.begin(), create_table.attr_infos.end());
      delete $5;
    }
    ;
attr_def_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA attr_def attr_def_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<AttrInfoSqlNode>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
    
attr_def:
    ID type LBRACE number RBRACE
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable=false;
      free($1);
    }
    | ID type
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable=false;
      free($1);
    }
    | ID type LBRACE number RBRACE NOT NULL_T
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable=false;
      free($1);
    }
    | ID type NOT NULL_T
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable=false;
      free($1);
    }
    | ID type LBRACE number RBRACE NULLABLE
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = $4;
      $$->nullable=true;
      free($1);
    }
    | ID type NULLABLE
    {
      $$ = new AttrInfoSqlNode;
      $$->type = (AttrType)$2;
      $$->name = $1;
      $$->length = 4;
      $$->nullable=true;
      free($1);
    }
    ;

number:
    NUMBER {$$ = $1;}
    ;
type:
    INT_T      { $$=INTS; }
    | STRING_T { $$=CHARS; }
    | FLOAT_T  { $$=FLOATS; }
    | DATE_T   { $$=DATES; }
    ;
insert_stmt:        /*insert   语句的语法解析树*/
    INSERT INTO ID VALUES raw_tuple raw_tuple_list
    {
      $$ = new ParsedSqlNode(SCF_INSERT);
      $$->insertion.relation_name = $3;
      if ($6 != nullptr) {
        $$->insertion.tuples.swap(*$6);
      }
      $$->insertion.tuples.emplace_back(*$5);
      std::reverse($$->insertion.tuples.begin(), $$->insertion.tuples.end());
      free($3);
    }
    ;
raw_tuple_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA raw_tuple raw_tuple_list  { 
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<RawTuple>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
raw_tuple:
    LBRACE value value_list RBRACE {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new RawTuple;
      }
      $$->emplace_back(*$2);
      std::reverse($$->begin(), $$->end());
      delete $2;
    }
value_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA value value_list  { 
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<Value>;
      }
      $$->emplace_back(*$2);
      delete $2;
    }
    ;
value:
    NUMBER {
      $$ = new Value((int)$1);
      @$ = @1;
    }
    |FLOAT {
      $$ = new Value((float)$1);
      @$ = @1;
    }
    |DATE {
      $$ = new Value((date)$1);
    }
    |NULL_T {
      $$ = new Value(NULLS);
    }
    |SSS {
      char *tmp = common::substr($1,1,strlen($1)-2);
      $$ = new Value(tmp);
      free(tmp);
    }
    ;
    
delete_stmt:    /*  delete 语句的语法解析树*/
    DELETE FROM ID where 
    {
      $$ = new ParsedSqlNode(SCF_DELETE);
      $$->deletion.relation_name = $3;
      if ($4 != nullptr) {
        $$->deletion.conditions.swap(*$4);
        delete $4;
      }
      free($3);
    }
    ;
update_stmt:      /*  update 语句的语法解析树*/
    UPDATE ID SET ID EQ value where 
    {
      $$ = new ParsedSqlNode(SCF_UPDATE);
      $$->update.relation_name = $2;
      $$->update.attribute_name = $4;
      $$->update.value = *$6;
      if ($7 != nullptr) {
        $$->update.conditions.swap(*$7);
        delete $7;
      }
      free($2);
      free($4);
    }
    ;
select_stmt:        /*  select 语句的语法解析树*/
    SELECT select_exprs FROM ID rel_list where group order
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.select_exprs.swap(*$2);
        delete $2;
      }
      if ($5 != nullptr) {
        $$->selection.relations.swap(*$5);
        delete $5;
      }
      $$->selection.relations.push_back($4);
      std::reverse($$->selection.relations.begin(), $$->selection.relations.end());
      
      if ($6 != nullptr) {
        $$->selection.conditions.swap(*$6);
        delete $6;
      }
      if ($7 != nullptr) {
        $$->selection.groups.swap(*$7);
        delete $7;
      }
      if ($8 != nullptr) {
        $$->selection.orders.swap(*$8);
        delete $8;
      }
      free($4);
    }
    | SELECT select_exprs FROM ID join_node join_list where group order
    {
      $$ = new ParsedSqlNode(SCF_SELECT);
      if ($2 != nullptr) {
        $$->selection.select_exprs.swap(*$2);
        delete $2;
      }
      if ($6 != nullptr) {
        $$->selection.joins.swap(*$6);
        delete $6;
      }
      $$->selection.joins.emplace_back(*$5);
      std::reverse($$->selection.joins.begin(), $$->selection.joins.end());
      $$->selection.relations.push_back($4);
      if ($7 != nullptr) {
        $$->selection.conditions.swap(*$7);
        delete $7;
      }
      if ($8 != nullptr) {
        $$->selection.groups.swap(*$8);
        delete $8;
      }
      if ($9 != nullptr) {
        $$->selection.orders.swap(*$9);
        delete $9;
      }
      free($4);
    }
    ;
order:
    /* empty */
    {
      $$ = nullptr;
    }
    | ORDER BY order_node_list
    {
      $$ = $3;
      std::reverse($$->begin(), $$->end());
    }
order_node_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | order_node {
      $$ = new std::vector<OrderSqlNode>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | order_node COMMA order_node_list {
      $$ = $3;
      $$->emplace_back(*$1);
      delete $1;
    }
order_node:
    rel_attr order_type
    {
      $$ = new OrderSqlNode;
      $$->type=$2;
      $$->attribute=*$1;
      free($1);
    }
group:
    /* empty */
    {
      $$ = nullptr;
    }
    | GROUP BY group_node_list
    {
      $$ = $3;
      std::reverse($$->begin(), $$->end());
    }
group_node_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | group_node {
      $$ = new std::vector<GroupSqlNode>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | group_node COMMA group_node_list {
      $$ = $3;
      $$->emplace_back(*$1);
      delete $1;
    }
group_node:
    rel_attr
    {
      $$ = $1;
    }
order_type:
    /* empty */
    {
      $$ = ASC;
    }
    | ASC_T {
      $$ = ASC;
    }
    | DESC_T {
      $$ = DESC;
    }
    ;
join_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | join_node join_list  { 
      if ($2 != nullptr) {
        $$ = $2;
      } else {
        $$ = new std::vector<JoinSqlNode>;
      }
      $$->emplace_back(*$1);
      delete $1;
    }
    ;
join_node:
    INNER JOIN ID ON condition_list
    {
      $$ = new JoinSqlNode;
      if ($5 != nullptr) {
        $$->conditions.swap(*$5);
      }
      $$->relation = $3;
      free($3);
      delete $5;
    }
calc_stmt:
    CALC expression_list
    {
      $$ = new ParsedSqlNode(SCF_CALC);
      std::reverse($2->begin(), $2->end());
      $$->calc.expressions.swap(*$2);
      delete $2;
    }
    ;

expression_list:
    expression
    {
      $$ = new std::vector<Expression*>;
      $$->emplace_back($1);
    }
    | expression COMMA expression_list
    {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<Expression *>;
      }
      $$->emplace_back($1);
    }
    ;
expression:
    expression '+' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::ADD, $1, $3, sql_string, &@$);
    }
    | expression '-' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::SUB, $1, $3, sql_string, &@$);
    }
    | expression '*' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::MUL, $1, $3, sql_string, &@$);
    }
    | expression '/' expression {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::DIV, $1, $3, sql_string, &@$);
    }
    | LBRACE expression RBRACE {
      $$ = $2;
      $$->set_name(token_name(sql_string, &@$));
    }
    | '-' expression %prec UMINUS {
      $$ = create_arithmetic_expression(ArithmeticExpr::Type::NEGATIVE, $2, nullptr, sql_string, &@$);
    }
    | value {
      $$ = new ValueExpr(*$1);
      $$->set_name(token_name(sql_string, &@$));
      delete $1;
    }
    ;

select_exprs:
    '*' {
      $$ = new std::vector<SelectExprSqlNode>;
      SelectExprSqlNode expr;
      expr.type = REL_ATTR_SELECT_T;
      expr.attribute = new RelAttrSqlNode;
      expr.attribute->relation_name  = "";
      expr.attribute->attribute_name = "*";
      $$->emplace_back(expr);
    }
    | select_expr select_expr_list {
      if ($2 != nullptr) {
        $$ = $2;
      } else {
        $$ = new std::vector<SelectExprSqlNode>;
      }
      $$->emplace_back(*$1);
      delete $1;
    }
    ;

select_expr:
    rel_attr {
      $$ = new SelectExprSqlNode;
      $$->type = REL_ATTR_SELECT_T;
      $$->attribute = $1;
    }
    | aggr_func {
      $$ = new SelectExprSqlNode;
      $$->type = AGGR_FUNC_SELECT_T;
      $$->aggrfunc = $1;
    }
    ;
  
select_expr_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA select_expr select_expr_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<SelectExprSqlNode>;
      }

      $$->emplace_back(*$2);
      delete $2;
    }
    ;

aggr_func:
    aggr_func_type LBRACE select_attr RBRACE {
      $$ = new AggrFuncSqlNode;
      $$->type = $1;

      if ($3 != nullptr) {
        $$->attributes.swap(*$3);
        delete $3;
      }
    }
    ;

aggr_func_type:
    MAX {
      $$ = MAX_AGGR_T;
    }
    | MIN {
      $$ = MIN_AGGR_T;
    }
    | COUNT {
      $$ = COUNT_AGGR_T;
    }
    | AVG {
      $$ = AVG_AGGR_T;
    }
    | SUM {
      $$ = SUM_AGGR_T;
    }
    ;

select_attr:
    '*' {
      $$ = new std::vector<RelAttrSqlNode>;
      RelAttrSqlNode attr;
      attr.relation_name  = "";
      attr.attribute_name = "*";
      $$->emplace_back(attr);
    }
    | '*' COMMA rel_attr attr_list {
      if ($4 != nullptr) {
        $$ = $4;
      } else {
        $$ = new std::vector<RelAttrSqlNode>;
      }
      $$->emplace_back(*$3);
      RelAttrSqlNode attr;
      attr.relation_name  = "";
      attr.attribute_name = "*";
      $$->emplace_back(attr);
    }
    | rel_attr attr_list {
      if ($2 != nullptr) {
        $$ = $2;
      } else {
        $$ = new std::vector<RelAttrSqlNode>;
      }
      $$->emplace_back(*$1);
      delete $1;
    }
    | /* empty */ {
      $$ = new std::vector<RelAttrSqlNode>;
      RelAttrSqlNode attr;
      attr.relation_name  = "";
      attr.attribute_name = "";
      $$->emplace_back(attr);
    }
    ;

rel_attr:
    ID {
      $$ = new RelAttrSqlNode;
      $$->attribute_name = $1;
      free($1);
    }
    | ID DOT ID {
      $$ = new RelAttrSqlNode;
      $$->relation_name  = $1;
      $$->attribute_name = $3;
      free($1);
      free($3);
    }
    ;

attr_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA rel_attr attr_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<RelAttrSqlNode>;
      }

      $$->emplace_back(*$2);
      delete $2;
    }
    ;

rel_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | COMMA ID rel_list {
      if ($3 != nullptr) {
        $$ = $3;
      } else {
        $$ = new std::vector<std::string>;
      }

      $$->push_back($2);
      free($2);
    }
    ;
where:
    /* empty */
    {
      $$ = nullptr;
    }
    | WHERE condition_list {
      $$ = $2;  
    }
    ;
condition_list:
    /* empty */
    {
      $$ = nullptr;
    }
    | condition {
      $$ = new std::vector<ConditionSqlNode>;
      $$->emplace_back(*$1);
      delete $1;
    }
    | condition AND condition_list {
      $$ = $3;
      $$->emplace_back(*$1);
      delete $1;
    }
    ;
condition:
    rel_attr comp_op value
    {
      $$ = new ConditionSqlNode;
      $$->left_is_attr = 1;
      $$->left_attr = *$1;
      $$->right_is_attr = 0;
      $$->right_value = *$3;
      $$->comp = $2;

      delete $1;
      delete $3;
    }
    | value comp_op value 
    {
      $$ = new ConditionSqlNode;
      $$->left_is_attr = 0;
      $$->left_value = *$1;
      $$->right_is_attr = 0;
      $$->right_value = *$3;
      $$->comp = $2;

      delete $1;
      delete $3;
    }
    | rel_attr comp_op rel_attr
    {
      $$ = new ConditionSqlNode;
      $$->left_is_attr = 1;
      $$->left_attr = *$1;
      $$->right_is_attr = 1;
      $$->right_attr = *$3;
      $$->comp = $2;

      delete $1;
      delete $3;
    }
    | value comp_op rel_attr
    {
      $$ = new ConditionSqlNode;
      $$->left_is_attr = 0;
      $$->left_value = *$1;
      $$->right_is_attr = 1;
      $$->right_attr = *$3;
      $$->comp = $2;

      delete $1;
      delete $3;
    }
    ;

comp_op:
      EQ { $$ = EQUAL_TO; }
    | LT { $$ = LESS_THAN; }
    | GT { $$ = GREAT_THAN; }
    | LE { $$ = LESS_EQUAL; }
    | GE { $$ = GREAT_EQUAL; }
    | NE { $$ = NOT_EQUAL; }
    | IS_T { $$ = IS; }
    | IS_T NOT { $$ = IS_NOT; }
    ;

load_data_stmt:
    LOAD DATA INFILE SSS INTO TABLE ID 
    {
      char *tmp_file_name = common::substr($4, 1, strlen($4) - 2);
      
      $$ = new ParsedSqlNode(SCF_LOAD_DATA);
      $$->load_data.relation_name = $7;
      $$->load_data.file_name = tmp_file_name;
      free($7);
      free(tmp_file_name);
    }
    ;

explain_stmt:
    EXPLAIN command_wrapper
    {
      $$ = new ParsedSqlNode(SCF_EXPLAIN);
      $$->explain.sql_node = std::unique_ptr<ParsedSqlNode>($2);
    }
    ;

set_variable_stmt:
    SET ID EQ value
    {
      $$ = new ParsedSqlNode(SCF_SET_VARIABLE);
      $$->set_variable.name  = $2;
      $$->set_variable.value = *$4;
      free($2);
      delete $4;
    }
    ;

opt_semicolon: /*empty*/
    | SEMICOLON
    ;
%%
//_____________________________________________________________________
extern void scan_string(const char *str, yyscan_t scanner);

int sql_parse(const char *s, ParsedSqlResult *sql_result) {
  yyscan_t scanner;
  yylex_init(&scanner);
  scan_string(s, scanner);
  int result = yyparse(s, sql_result, scanner);
  yylex_destroy(scanner);
  return result;
}
