#include "Value.h"
#include "Object.h"

#include <format>
#include <sstream>

bool Value::equals(const Value& other) const
{
  if (type != other.type) return false;
  switch (type) {
    case ValueType::NIL: return true;
    case ValueType::BOOL: return as.boolean == other.as.boolean;
    case ValueType::NUMBER: return as.number == other.as.number;
    case ValueType::OBJ: {
      if (as.obj->type == ObjType::STRING && other.as.obj->type == ObjType::STRING) {
        return static_cast<ObjString*>(as.obj)->str
            == static_cast<ObjString*>(other.as.obj)->str;
      }
      return as.obj == other.as.obj;
    }
  }
  return false;
}

std::string Value::toString() const
{
  switch (type) {
    case ValueType::NIL:
      return "null";
    case ValueType::BOOL:
      return as.boolean ? "true" : "false";
    case ValueType::NUMBER: {
      std::string s = std::format("{}", as.number);
      if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
        s += ".0";
      }
      return s;
    }
    case ValueType::OBJ: {
      switch (as.obj->type) {
        case ObjType::STRING:
          return static_cast<ObjString*>(as.obj)->str;
        case ObjType::FUNCTION:
          return "<fn " + static_cast<ObjFunction*>(as.obj)->name + ">";
        case ObjType::NATIVE:
          return "<native fn>";
        case ObjType::CLOSURE:
          return "<fn " + static_cast<ObjClosure*>(as.obj)->function->name + ">";
        case ObjType::CLASS:
          return "<class " + static_cast<ObjClass*>(as.obj)->name + ">";
        case ObjType::INSTANCE: {
          auto* inst = static_cast<ObjInstance*>(as.obj);
          if (inst->klass) {
            return "<instance of " + inst->klass->name + ">";
          }
          return "<module>";
        }
        case ObjType::BOUND_METHOD:
          return "<bound method>";
        case ObjType::ARRAY: {
          auto* arr = static_cast<ObjArray*>(as.obj);
          std::ostringstream oss;
          oss << "[";
          for (size_t i = 0; i < arr->elements.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << arr->elements[i].toString();
          }
          oss << "]";
          return oss.str();
        }
      }
      return "<obj>";
    }
  }
  return "<unknown>";
}

std::string Value::typeString() const
{
  switch (type) {
    case ValueType::NIL: return "null";
    case ValueType::BOOL: return "boolean";
    case ValueType::NUMBER: return "number";
    case ValueType::OBJ: {
      switch (as.obj->type) {
        case ObjType::STRING: return "string";
        case ObjType::FUNCTION: return "function";
        case ObjType::NATIVE: return "native";
        case ObjType::CLOSURE: return "function";
        case ObjType::CLASS: return "class";
        case ObjType::INSTANCE: return "object";
        case ObjType::BOUND_METHOD: return "method";
        case ObjType::ARRAY: return "array";
      }
      return "object";
    }
  }
  return "unknown";
}
