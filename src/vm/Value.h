#ifndef VM_VALUE_H
#define VM_VALUE_H

#include <cmath>
#include <cstdint>
#include <string>

struct Obj;

enum class ValueType : uint8_t
{
  NIL,
  BOOL,
  NUMBER,
  OBJ
};

struct Value
{
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as;

  Value() : type(ValueType::NIL) { as.number = 0; }
  explicit Value(bool b) : type(ValueType::BOOL) { as.boolean = b; }
  explicit Value(double n) : type(ValueType::NUMBER) { as.number = n; }
  explicit Value(Obj* o) : type(ValueType::OBJ) { as.obj = o; }

  bool isNil() const { return type == ValueType::NIL; }
  bool isBool() const { return type == ValueType::BOOL; }
  bool isNumber() const { return type == ValueType::NUMBER; }
  bool isObj() const { return type == ValueType::OBJ; }

  bool asBool() const { return as.boolean; }
  double asNumber() const { return as.number; }
  Obj* asObj() const { return as.obj; }

  bool isTruthy() const
  {
    if (isNil()) return false;
    if (isBool()) return as.boolean;
    return true;
  }

  bool equals(const Value& other) const;

  std::string toString() const;
  std::string typeString() const;
};

#define NIL_VAL() Value()
#define BOOL_VAL(b) Value(b)
#define NUMBER_VAL(n) Value(n)
#define OBJ_VAL(o) Value(o)

#endif
