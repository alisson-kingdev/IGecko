#ifndef VM_OBJECT_H
#define VM_OBJECT_H

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <functional>

#include "Chunk.h"
#include "Value.h"

enum class ObjType : uint8_t
{
  STRING,
  FUNCTION,
  CLOSURE,
  NATIVE,
  CLASS,
  INSTANCE,
  BOUND_METHOD,
  ARRAY
};

struct Obj
{
  ObjType type;

  explicit Obj(ObjType t) : type(t) {}
  virtual ~Obj() = default;
};

struct ObjString : Obj
{
  std::string str;

  explicit ObjString(std::string s) : Obj(ObjType::STRING), str(std::move(s)) {}
};

struct ObjFunction : Obj
{
  std::string name;
  int arity;
  Chunk chunk;
  int upvalueCount;

  ObjFunction()
      : Obj(ObjType::FUNCTION), arity(0), upvalueCount(0)
  {}
};

struct ObjClosure : Obj
{
  ObjFunction* function;
  std::vector<Value> captured;

  explicit ObjClosure(ObjFunction* fn)
      : Obj(ObjType::CLOSURE), function(fn)
  {
    captured.resize(fn->upvalueCount, NIL_VAL());
  }
};

using NativeFn = std::function<Value(const std::vector<Value>&)>;

struct ObjNative : Obj
{
  NativeFn function;
  std::string name;

  ObjNative(NativeFn fn, std::string n)
      : Obj(ObjType::NATIVE), function(fn), name(std::move(n))
  {}
};

struct ObjClass : Obj
{
  std::string name;
  std::map<std::string, ObjClosure*> methods;

  explicit ObjClass(std::string n)
      : Obj(ObjType::CLASS), name(std::move(n))
  {}
};

struct ObjInstance : Obj
{
  ObjClass* klass;
  std::map<std::string, Value> fields;

  explicit ObjInstance(ObjClass* k)
      : Obj(ObjType::INSTANCE), klass(k)
  {}
};

struct ObjBoundMethod : Obj
{
  Value receiver;
  ObjClosure* method;

  ObjBoundMethod(Value recv, ObjClosure* m)
      : Obj(ObjType::BOUND_METHOD), receiver(recv), method(m)
  {}
};

struct ObjArray : Obj
{
  std::vector<Value> elements;

  ObjArray() : Obj(ObjType::ARRAY) {}
  explicit ObjArray(std::vector<Value> elems)
      : Obj(ObjType::ARRAY), elements(std::move(elems))
  {}
};

ObjString* copyString(const std::string& s);
void printObject(Value value);

#endif
