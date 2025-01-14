/* Copyright (c) 2023 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2023/08/16.
//

#include "sql/optimizer/logical_plan_generator.h"

#include "sql/operator/logical_operator.h"
#include "sql/operator/calc_logical_operator.h"
#include "sql/operator/project_logical_operator.h"
#include "sql/operator/predicate_logical_operator.h"
#include "sql/operator/table_get_logical_operator.h"
#include "sql/operator/insert_logical_operator.h"
#include "sql/operator/update_logical_operator.h"
#include "sql/operator/delete_logical_operator.h"
#include "sql/operator/join_logical_operator.h"
#include "sql/operator/project_logical_operator.h"
#include "sql/operator/explain_logical_operator.h"
#include "sql/operator/aggr_logical_operator.h"
#include "sql/operator/order_logical_operator.h"

#include "sql/stmt/stmt.h"
#include "sql/stmt/calc_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/explain_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "sql/stmt/join_stmt.h"

using namespace std;

RC LogicalPlanGenerator::create(Stmt *stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  RC rc = RC::SUCCESS;
  switch (stmt->type()) {
    case StmtType::CALC: {
      CalcStmt *calc_stmt = static_cast<CalcStmt *>(stmt);
      rc = create_plan(calc_stmt, logical_operator);
    } break;

    case StmtType::SELECT: {
      SelectStmt *select_stmt = static_cast<SelectStmt *>(stmt);
      rc = create_plan(select_stmt, logical_operator);
    } break;

    case StmtType::INSERT: {
      InsertStmt *insert_stmt = static_cast<InsertStmt *>(stmt);
      rc = create_plan(insert_stmt, logical_operator);
    } break;

    case StmtType::UPDATE: {
      UpdateStmt *insert_stmt = static_cast<UpdateStmt *>(stmt);
      rc = create_plan(insert_stmt, logical_operator);
    } break;

    case StmtType::DELETE: {
      DeleteStmt *delete_stmt = static_cast<DeleteStmt *>(stmt);
      rc = create_plan(delete_stmt, logical_operator);
    } break;

    case StmtType::EXPLAIN: {
      ExplainStmt *explain_stmt = static_cast<ExplainStmt *>(stmt);
      rc = create_plan(explain_stmt, logical_operator);
    } break;
    default: {
      rc = RC::UNIMPLENMENT;
    }
  }
  return rc;
}

RC LogicalPlanGenerator::create_plan(CalcStmt *calc_stmt, std::unique_ptr<LogicalOperator> &logical_operator)
{
  logical_operator.reset(new CalcLogicalOperator(std::move(calc_stmt->expressions())));
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(
    SelectStmt *select_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  const std::vector<Table *> &tables = select_stmt->tables();// select的所有表
  std::vector<Expression*> &query_exprs = select_stmt->query_exprs();// select的所有表达式
  std::vector<Expression*> aggr_exprs;// select的聚合表达式
  std::vector<Field> query_fields;// select的字段
  std::vector<JoinStmt*> &join_stmts = select_stmt->join_stmts();
  std::vector<GroupStmt*> &group_stmts = select_stmt->groups();
  FilterStmt * filter_stmt = select_stmt->filter_stmt();// select的查询条件
  int index=0;// 内连接使用的循环变量
  bool is_inner_join = select_stmt->join_stmts().size() > 0;// 是否为内连接

  // 总的读取表算子
  unique_ptr<LogicalOperator> table_oper(nullptr);
  // 遍历所有表
  for (Table *table : tables) {
    std::vector<Field> fields;
    for (Expression *expr : query_exprs) {
      switch (expr->type()) {
        case ExprType::FIELD : {
          FieldExpr *field_expr = static_cast<FieldExpr*>(expr);
          if (0 == strcmp(field_expr->field().table_name(), table->name())) {
            fields.push_back(field_expr->field());
            query_fields.push_back(field_expr->field());
          }
        } break;
        case ExprType::AGGREGATION : {
          AggregationExpr *aggr_expr = static_cast<AggregationExpr*>(expr);
          if (0 == strcmp(aggr_expr->field().table_name(), table->name())) {
            fields.push_back(aggr_expr->field());
            aggr_exprs.push_back(expr);
          }
        } break;
        default : {
          return RC::INTERNAL;
        }
      }
    }

    // 获取表数据的算子
    unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(table, fields, true/*readonly*/));

    // 连接表
    if (is_inner_join) {
      // 内连接
      if (table_oper == nullptr) {
        table_oper = std::move(table_get_oper);
      } else {
        unique_ptr<LogicalOperator> join_oper(new JoinLogicalOperator);
        join_oper->add_child(std::move(table_oper));
        join_oper->add_child(std::move(table_get_oper));
        unique_ptr<LogicalOperator> predicate_oper;
        FilterStmt *filter = join_stmts[index-1]->join_condition();
        RC rc = create_plan(filter, predicate_oper);
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to create predicate logical plan. rc=%s", strrc(rc));
          return rc;
        }
        predicate_oper->add_child(std::move(join_oper));

        table_oper = std::move(predicate_oper);
      }
    } else {
      // 自然连接
      if (table_oper == nullptr) {
        table_oper = std::move(table_get_oper);
      } else {
        JoinLogicalOperator *join_oper = new JoinLogicalOperator;
        join_oper->add_child(std::move(table_oper));
        join_oper->add_child(std::move(table_get_oper));
        table_oper = unique_ptr<LogicalOperator>(join_oper);
      }
    }
    index++;
  }

  // 检查group by中select的合法性
  if(!group_stmts.empty()){
    for(Field field:query_fields){
      bool contains=false;
      for(auto stmt:group_stmts){
        Field group_field= stmt->group_unit()->field();
        if(0 == strcmp(field.table_name(), group_field.table_name()) && 0 == strcmp(field.field_name(), group_field.field_name())){
          contains=true;
          break;
        }
      }
      if(!contains){
          LOG_WARN("Selected field [%s.%s] must in group by fields.",field.table_name(),field.field_name());
          return RC::GROUP_BY_SELECT_INVALID;
      }
    }
  }

  // 过滤算子
  unique_ptr<LogicalOperator> predicate_oper;
  RC rc = create_plan(select_stmt->filter_stmt(), predicate_oper);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create predicate logical plan. rc=%s", strrc(rc));
    return rc;
  }

  // 聚合算子
  unique_ptr<LogicalOperator> aggr_oper(aggr_exprs.size() != 0 ? new AggregationLogicalOperator(aggr_exprs,query_fields,select_stmt->groups()):nullptr);

  // 排序算子
  unique_ptr<LogicalOperator> order_by_oper(!select_stmt->orders().empty()?new OrderLogicalOperator(select_stmt->orders()):nullptr);

  // 投影算子
  unique_ptr<LogicalOperator> project_oper(new ProjectLogicalOperator(query_exprs));

  // 连接所有算子，跳过为nullptr的算子
  std::vector<unique_ptr<LogicalOperator>> stack;
  stack.push_back(std::move(table_oper));
  stack.push_back(std::move(predicate_oper));
  stack.push_back(std::move(aggr_oper));
  stack.push_back(std::move(order_by_oper));
  stack.push_back(std::move(project_oper));
  for (int i = 0; i < stack.size() ; i++) {
    if (stack[i] == nullptr) continue;
    for (int j = i+1; j < stack.size(); j++) {
      if (stack[j] != nullptr) {
        stack[j]->add_child(std::move(stack[i]));
        break;
      }
    }
  }

  logical_operator.swap(stack[stack.size()-1]);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(
    FilterStmt *filter_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  std::vector<unique_ptr<Expression>> cmp_exprs;
  const std::vector<FilterUnit *> &filter_units = filter_stmt->filter_units();
  for (const FilterUnit *filter_unit : filter_units) {
    const FilterObj &filter_obj_left = filter_unit->left();
    const FilterObj &filter_obj_right = filter_unit->right();

    unique_ptr<Expression> left(filter_obj_left.is_attr
                                         ? static_cast<Expression *>(new FieldExpr(filter_obj_left.field))
                                         : static_cast<Expression *>(new ValueExpr(filter_obj_left.value)));

    unique_ptr<Expression> right(filter_obj_right.is_attr
                                          ? static_cast<Expression *>(new FieldExpr(filter_obj_right.field))
                                          : static_cast<Expression *>(new ValueExpr(filter_obj_right.value)));

    ComparisonExpr *cmp_expr = new ComparisonExpr(filter_unit->comp(), std::move(left), std::move(right));
    cmp_exprs.emplace_back(cmp_expr);
  }

  unique_ptr<PredicateLogicalOperator> predicate_oper;
  if (!cmp_exprs.empty()) {
    unique_ptr<ConjunctionExpr> conjunction_expr(new ConjunctionExpr(ConjunctionExpr::Type::AND, cmp_exprs));
    predicate_oper = unique_ptr<PredicateLogicalOperator>(new PredicateLogicalOperator(std::move(conjunction_expr)));
  }

  logical_operator = std::move(predicate_oper);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(
    InsertStmt *insert_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Table *table = insert_stmt->table();
  vector<RawTuple> tuples(insert_stmt->tuples());

  InsertLogicalOperator *insert_operator = new InsertLogicalOperator(table, tuples);
  logical_operator.reset(insert_operator);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(
    UpdateStmt *update_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Table *table = update_stmt->table();
  std::string field_name = update_stmt->field_name();
  Value *value = update_stmt->value();
  FilterStmt *filter_stmt = update_stmt->filter_stmt();
  std::vector<Field> fields;
  // 获得表的所有字段
  for (int i = table->table_meta().sys_field_num(); i < table->table_meta().field_num(); i++) {
    const FieldMeta *field_meta = table->table_meta().field(i);
    fields.push_back(Field(table, field_meta));
  }
  // 创建获取表数据算子，false表示可修改数据
  unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(table, fields, false));
  // 根据filter_stmt创建过滤算子
  unique_ptr<LogicalOperator> predicate_oper;
  RC rc = create_plan(filter_stmt, predicate_oper);
  if (rc != RC::SUCCESS) {
    return rc;
  }
  // 创建更新算子
  unique_ptr<LogicalOperator> update_oper(new UpdateLogicalOperator(table, value, field_name));
  // 连接各个算子
  if (predicate_oper) {
    predicate_oper->add_child(std::move(table_get_oper));
    update_oper->add_child(std::move(predicate_oper));
  } else {
    update_oper->add_child(std::move(table_get_oper));
  }
  logical_operator = std::move(update_oper);
  return RC::SUCCESS;
}

RC LogicalPlanGenerator::create_plan(
    DeleteStmt *delete_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Table *table = delete_stmt->table();
  FilterStmt *filter_stmt = delete_stmt->filter_stmt();
  std::vector<Field> fields;
  for (int i = table->table_meta().sys_field_num(); i < table->table_meta().field_num(); i++) {
    const FieldMeta *field_meta = table->table_meta().field(i);
    fields.push_back(Field(table, field_meta));
  }
  unique_ptr<LogicalOperator> table_get_oper(new TableGetLogicalOperator(table, fields, false/*readonly*/));

  unique_ptr<LogicalOperator> predicate_oper;
  RC rc = create_plan(filter_stmt, predicate_oper);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  unique_ptr<LogicalOperator> delete_oper(new DeleteLogicalOperator(table));

  if (predicate_oper) {
    predicate_oper->add_child(std::move(table_get_oper));
    delete_oper->add_child(std::move(predicate_oper));
  } else {
    delete_oper->add_child(std::move(table_get_oper));
  }

  logical_operator = std::move(delete_oper);
  return rc;
}

RC LogicalPlanGenerator::create_plan(
    ExplainStmt *explain_stmt, unique_ptr<LogicalOperator> &logical_operator)
{
  Stmt *child_stmt = explain_stmt->child();
  unique_ptr<LogicalOperator> child_oper;
  RC rc = create(child_stmt, child_oper);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create explain's child operator. rc=%s", strrc(rc));
    return rc;
  }

  logical_operator = unique_ptr<LogicalOperator>(new ExplainLogicalOperator);
  logical_operator->add_child(std::move(child_oper));
  return rc;
}
