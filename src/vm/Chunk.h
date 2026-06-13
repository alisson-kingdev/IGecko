#ifndef VM_CHUNK_H
#define VM_CHUNK_H

#include <cstdint>
#include <string>
#include <vector>

#include "Value.h"

enum OpCode : uint8_t
{
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_GET_SUPER,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_SUPER_INVOKE,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD,
  OP_DUP,
  OP_ARRAY,
  OP_OBJECT,
  OP_OBJECT_BUILD,
  OP_INDEX_GET,
  OP_INDEX_SET,
  OP_PUSH_NULL,
  OP_DEFINE_GLOBAL,
};

struct Chunk
{
  std::vector<uint8_t> bytecode;
  std::vector<Value> constants;
  std::vector<int> lines;

  void write(uint8_t byte, int line);
  void writeConstant(Value value, int line);
  void writeJump(uint8_t op, int line);
  int addConstant(Value value);
  int count() const;
};

#endif
