/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/07/05.
//

#include "sql/expr/expression.h"
#include "sql/expr/tuple.h"

using namespace std;

RC FieldExpr::get_value(const Tuple &tuple, Value &value) const
{
  return tuple.find_cell(TupleCellSpec(table_name(), field_name()), value);
}

RC ValueExpr::get_value(const Tuple &tuple, Value &value) const
{
  value = value_;
  return RC::SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////
CastExpr::CastExpr(unique_ptr<Expression> child, AttrType cast_type)
    : child_(std::move(child)), cast_type_(cast_type)
{}

CastExpr::~CastExpr()
{}

RC CastExpr::cast(const Value &value, Value &cast_value) const
{
  RC rc = RC::SUCCESS;
  if (this->value_type() == value.attr_type()) {
    cast_value = value;
    return rc;
  }

  switch (cast_type_) {
    case BOOLEANS: {
      bool val = value.get_boolean();
      cast_value.set_boolean(val);
    } break;
    default: {
      rc = RC::INTERNAL;
      LOG_WARN("unsupported convert from type %d to %d", child_->value_type(), cast_type_);
    }
  }
  return rc;
}

RC CastExpr::get_value(const Tuple &tuple, Value &cell) const
{
  RC rc = child_->get_value(tuple, cell);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(cell, cell);
}

RC CastExpr::try_get_value(Value &value) const
{
  RC rc = child_->try_get_value(value);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  return cast(value, value);
}

////////////////////////////////////////////////////////////////////////////////

ComparisonExpr::ComparisonExpr(CompOp comp, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : comp_(comp), left_(std::move(left)), right_(std::move(right))
{}

ComparisonExpr::~ComparisonExpr()
{}

RC ComparisonExpr::compare_value(const Value &left, const Value &right, bool &result) const
{
  RC rc = RC::SUCCESS;
  result = false;
  // 如果右值为NULL，则运算符只能为IS或IS_NOT
  if(right.attr_type()==NULLS){
    if(comp_==IS){
      result = left.attr_type()==NULLS;
    }else if(comp_==IS_NOT){
      result = left.attr_type()!=NULLS;
    }else{
      // 其他一律返回false
      result = false;
    }
  } else {
    int cmp_result = left.compare(right);
    switch (comp_) {
      case EQUAL_TO: {
        result = (0 == cmp_result);
      } break;
      case LESS_EQUAL: {
        result = (cmp_result <= 0);
      } break;
      case NOT_EQUAL: {
        result = (cmp_result != 0);
      } break;
      case LESS_THAN: {
        result = (cmp_result < 0);
      } break;
      case GREAT_EQUAL: {
        result = (cmp_result >= 0);
      } break;
      case GREAT_THAN: {
        result = (cmp_result > 0);
      } break;
      default: {
        LOG_WARN("unsupported comparison. %d", comp_);
        rc = RC::INTERNAL;
      } break;
    }
  }

  return rc;
}

RC ComparisonExpr::try_get_value(Value &cell) const
{
  if (left_->type() == ExprType::VALUE && right_->type() == ExprType::VALUE) {
    ValueExpr *left_value_expr = static_cast<ValueExpr *>(left_.get());
    ValueExpr *right_value_expr = static_cast<ValueExpr *>(right_.get());
    const Value &left_cell = left_value_expr->get_value();
    const Value &right_cell = right_value_expr->get_value();

    bool value = false;
    RC rc = compare_value(left_cell, right_cell, value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to compare tuple cells. rc=%s", strrc(rc));
    } else {
      cell.set_boolean(value);
    }
    return rc;
  }

  return RC::INVALID_ARGUMENT;
}

RC ComparisonExpr::get_value(const Tuple &tuple, Value &value) const
{
  Value left_value;
  Value right_value;

  RC rc = left_->get_value(tuple, left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_value(tuple, right_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }

  bool bool_value = false;
  rc = compare_value(left_value, right_value, bool_value);
  if (rc == RC::SUCCESS) {
    value.set_boolean(bool_value);
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
ConjunctionExpr::ConjunctionExpr(Type type, vector<unique_ptr<Expression>> &children)
    : conjunction_type_(type), children_(std::move(children))
{}

RC ConjunctionExpr::get_value(const Tuple &tuple, Value &value) const
{
  RC rc = RC::SUCCESS;
  if (children_.empty()) {
    value.set_boolean(true);
    return rc;
  }

  Value tmp_value;
  for (const unique_ptr<Expression> &expr : children_) {
    rc = expr->get_value(tuple, tmp_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value by child expression. rc=%s", strrc(rc));
      return rc;
    }
    bool bool_value = tmp_value.get_boolean();
    if ((conjunction_type_ == Type::AND && !bool_value) || (conjunction_type_ == Type::OR && bool_value)) {
      value.set_boolean(bool_value);
      return rc;
    }
  }

  bool default_value = (conjunction_type_ == Type::AND);
  value.set_boolean(default_value);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////

ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, Expression *left, Expression *right)
    : arithmetic_type_(type), left_(left), right_(right)
{}
ArithmeticExpr::ArithmeticExpr(ArithmeticExpr::Type type, unique_ptr<Expression> left, unique_ptr<Expression> right)
    : arithmetic_type_(type), left_(std::move(left)), right_(std::move(right))
{}

AttrType ArithmeticExpr::value_type() const
{
  if (!right_) {
    return left_->value_type();
  }

  if (left_->value_type() == AttrType::INTS &&
      right_->value_type() == AttrType::INTS &&
      arithmetic_type_ != Type::DIV) {
    return AttrType::INTS;
  }
  
  return AttrType::FLOATS;
}

RC ArithmeticExpr::calc_value(const Value &left_value, const Value &right_value, Value &value) const
{
  RC rc = RC::SUCCESS;

  const AttrType target_type = value_type();

  switch (arithmetic_type_) {
    case Type::ADD: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() + right_value.get_int());
      } else {
        value.set_float(left_value.get_float() + right_value.get_float());
      }
    } break;

    case Type::SUB: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() - right_value.get_int());
      } else {
        value.set_float(left_value.get_float() - right_value.get_float());
      }
    } break;

    case Type::MUL: {
      if (target_type == AttrType::INTS) {
        value.set_int(left_value.get_int() * right_value.get_int());
      } else {
        value.set_float(left_value.get_float() * right_value.get_float());
      }
    } break;

    case Type::DIV: {
      if (target_type == AttrType::INTS) {
        if (right_value.get_int() == 0) {
          // NOTE: 设置为整数最大值是不正确的。通常的做法是设置为NULL，但是当前的miniob没有NULL概念，所以这里设置为整数最大值。
          value.set_int(numeric_limits<int>::max());
        } else {
          value.set_int(left_value.get_int() / right_value.get_int());
        }
      } else {
        if (right_value.get_float() > -EPSILON && right_value.get_float() < EPSILON) {
          // NOTE: 设置为浮点数最大值是不正确的。通常的做法是设置为NULL，但是当前的miniob没有NULL概念，所以这里设置为浮点数最大值。
          value.set_float(numeric_limits<float>::max());
        } else {
          value.set_float(left_value.get_float() / right_value.get_float());
        }
      }
    } break;

    case Type::NEGATIVE: {
      if (target_type == AttrType::INTS) {
        value.set_int(-left_value.get_int());
      } else {
        value.set_float(-left_value.get_float());
      }
    } break;

    default: {
      rc = RC::INTERNAL;
      LOG_WARN("unsupported arithmetic type. %d", arithmetic_type_);
    } break;
  }
  return rc;
}

RC ArithmeticExpr::get_value(const Tuple &tuple, Value &value) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  rc = left_->get_value(tuple, left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }
  rc = right_->get_value(tuple, right_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
    return rc;
  }
  return calc_value(left_value, right_value, value);
}

RC ArithmeticExpr::try_get_value(Value &value) const
{
  RC rc = RC::SUCCESS;

  Value left_value;
  Value right_value;

  rc = left_->try_get_value(left_value);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to get value of left expression. rc=%s", strrc(rc));
    return rc;
  }

  if (right_) {
    rc = right_->try_get_value(right_value);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to get value of right expression. rc=%s", strrc(rc));
      return rc;
    }
  }

  return calc_value(left_value, right_value, value);
}

AggregationExpr::AggregationExpr(Field field, AggrFuncType aggr_type)
{
  aggr_type_ = aggr_type;
  field_ = field;
  field_expr_ = new FieldExpr(field_);

  switch (aggr_type_)
  {
  case MAX_AGGR_T: {
    attr_type_ = field_.attr_type();
    aggr_func_ = &AggregationExpr::max_aggr_func;
  } break;
  case MIN_AGGR_T: {
    attr_type_ = field_.attr_type();
    aggr_func_ = &AggregationExpr::min_aggr_func;
  } break;
  case SUM_AGGR_T: {
    attr_type_ = field_.attr_type();
    aggr_func_ = &AggregationExpr::sum_aggr_func;
  } break;
  case AVG_AGGR_T: {
    attr_type_ = FLOATS;
    aggr_func_ = &AggregationExpr::avg_aggr_func;
  } break;
  case COUNT_AGGR_T: {
    attr_type_ = INTS;
    aggr_func_ = &AggregationExpr::count_aggr_func;
  } break;
  default:
    break;
  }
}

RC AggregationExpr::get_value(const Tuple &tuple, Value &value) const { return RC::SUCCESS; }

RC AggregationExpr::try_get_value(Value &value) const { return RC::SUCCESS; }

TupleCellSpec AggregationExpr::cell_spec(bool with_table_name) const
{ 
  std::string alias;
  switch (aggr_type_)
  {
  case AggrFuncType::MAX_AGGR_T: {
    alias += "MAX(";
  } break;
  case AggrFuncType::MIN_AGGR_T: {
    alias += "MIN(";
  } break;
  case AggrFuncType::COUNT_AGGR_T: {
    alias += "COUNT(";
  } break;
  case AggrFuncType::SUM_AGGR_T: {
    alias += "SUM(";
  } break;
  case AggrFuncType::AVG_AGGR_T: {
    alias += "AVG(";
  } break;
  default:
    alias += "ERR_FUNC_TYPE(";
  }
  if (with_table_name) {
    alias += field_.table_name();
  }
  alias += field_.field_name();
  alias += ")";
  
  return TupleCellSpec(field_.table_name(), field_.field_name(), alias.c_str());
}

RC AggregationExpr::begin_aggr() 
{ 
  i_val_ = 0;
  f_val_ = 0;
  value_ = Value();
  has_record = false;
  return RC::SUCCESS;
}

RC AggregationExpr::aggr_tuple(Tuple *&tuple) 
{ 
  Value value;
  field_expr_->get_value(*tuple, value);
  // 当要聚合的值不是NULL时才进行聚合
  if(value.attr_type()!=NULLS){
    has_record=true;
    return (this->*aggr_func_)(value);
  }
  return RC::SUCCESS;
}

RC AggregationExpr::get_result(Value &value) 
{ 
  if(has_record){
    switch (aggr_type_ ) {
      case MAX_AGGR_T:
      case MIN_AGGR_T: {
        value = value_;
      } break;
      case COUNT_AGGR_T: {
        value = Value((int)i_val_);
      } break;
      case SUM_AGGR_T: {
        if (attr_type_ == AttrType::INTS)
          value = Value((int)i_val_);
        else
          value = Value((float)f_val_);
      } break;
      case AVG_AGGR_T: {
        if (i_val_ == 0) {
          value = Value((float)0);
        } else {
          value = Value((float)(f_val_ / i_val_));
        }
      } break;
      default: {
        return RC::INTERNAL;
      }
    }
  }else{
    // 没有被聚合的元组时，除了COUNT都返回NULL
    if(aggr_type_==COUNT_AGGR_T){
      value = Value((int)i_val_);
    }else{
      value=Value(NULLS);
    }
  }

  return RC::SUCCESS;
}

RC AggregationExpr::max_aggr_func(Value &value) 
{ 
  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }
  int rt = value_.compare(value);
  if (rt < 0) {
    value_ = value;
  }
  return RC::SUCCESS;
}

RC AggregationExpr::min_aggr_func(Value &value) 
{ 
  if (value_.attr_type() == AttrType::UNDEFINED) {
    value_ = value;
    return RC::SUCCESS;
  }
  int rt = value_.compare(value);
  if (rt > 0) {
    value_ = value;
  }
  return RC::SUCCESS;
}

RC AggregationExpr::sum_aggr_func(Value &value) 
{ 
  switch (attr_type_ ) {
    case INTS: {
      i_val_ += value.get_int();
    } break;
    case FLOATS: {
      f_val_ += value.get_float();
    } break;
    default: {
      return RC::INTERNAL;
    }
  }
  return RC::SUCCESS;
}

RC AggregationExpr::avg_aggr_func(Value &value) 
{ 
  switch (attr_type_ ) {
    case INTS: {
      f_val_ += (float)value.get_int();
    } break;
    case FLOATS: {
      f_val_ += value.get_float();
    } break;
    default: {
      return RC::INTERNAL;
    }
  }
  i_val_ += 1; 
  return RC::SUCCESS;
}

RC AggregationExpr::count_aggr_func(Value &value) 
{ 
  i_val_ += 1; 
  return RC::SUCCESS;
}