#include "Disassembler.h"
#include "Chunk.h"
#include "Object.h"
#include "Value.h"

#include <format>
#include <iostream>

static int constantInstruction(const std::string& name, Chunk* chunk, int offset)
{
  uint8_t index = chunk->bytecode[offset + 1];
  std::cout << std::format("{:<16} {:>4} '{}'\n", name, index, chunk->constants[index].toString());
  return offset + 2;
}

static int simpleInstruction(const std::string& name, int offset)
{
  std::cout << name << "\n";
  return offset + 1;
}

static int jumpInstruction(const std::string& name, int sign, Chunk* chunk, int offset)
{
  uint16_t jump = static_cast<uint16_t>((chunk->bytecode[offset + 1] << 8) | chunk->bytecode[offset + 2]);
  std::cout << std::format("{:<16} {:>4} -> {}\n", name, jump, offset + 3 + sign * jump);
  return offset + 3;
}

static int byteInstruction(const std::string& name, Chunk* chunk, int offset)
{
  uint8_t slot = chunk->bytecode[offset + 1];
  std::cout << std::format("{:<16} {:>4}\n", name, slot);
  return offset + 2;
}

static int invokeInstruction(const std::string& name, Chunk* chunk, int offset)
{
  uint8_t method = chunk->bytecode[offset + 1];
  uint8_t argCount = chunk->bytecode[offset + 2];
  std::cout << std::format("{:<16} ({:>2} args) {:>4} '{}'\n",
                           name, argCount, method, chunk->constants[method].toString());
  return offset + 3;
}

void disassembleChunk(Chunk* chunk, const std::string& name)
{
  std::cout << std::format("== {} ==\n", name);
  for (int offset = 0; offset < chunk->count();) {
    offset = disassembleInstruction(chunk, offset);
  }
}

int disassembleInstruction(Chunk* chunk, int offset)
{
  std::cout << std::format("{:>04} ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    std::cout << "   | ";
  } else {
    std::cout << std::format("{:>4} ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->bytecode[offset];
  switch (static_cast<OpCode>(instruction)) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_GET_SUPER:
      return constantInstruction("OP_GET_SUPER", chunk, offset);
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_MODULO:
      return simpleInstruction("OP_MODULO", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_SUPER_INVOKE:
      return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t index = chunk->bytecode[offset++];
      std::cout << std::format("{:<16} {:>4} '{}'\n", "OP_CLOSURE", index, chunk->constants[index].toString());
      auto* fn = static_cast<ObjFunction*>(chunk->constants[index].asObj());
      for (int j = 0; j < fn->upvalueCount; ++j) {
        uint8_t isLocal = chunk->bytecode[offset++];
        uint8_t slot = chunk->bytecode[offset++];
        std::cout << std::format("{:>04}      |                     {} {}\n",
                                 offset - 2, isLocal ? "local" : "upvalue", slot);
      }
      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
      return constantInstruction("OP_METHOD", chunk, offset);
    case OP_OBJECT:
      return simpleInstruction("OP_OBJECT", offset);
    case OP_OBJECT_BUILD:
      return byteInstruction("OP_OBJECT_BUILD", chunk, offset);
    case OP_DUP:
      return simpleInstruction("OP_DUP", offset);
    case OP_ARRAY:
      return byteInstruction("OP_ARRAY", chunk, offset);
    case OP_INDEX_GET:
      return simpleInstruction("OP_INDEX_GET", offset);
    case OP_INDEX_SET:
      return simpleInstruction("OP_INDEX_SET", offset);
  case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_PUSH_NULL:
      return simpleInstruction("OP_PUSH_NULL", offset);
  default:
      std::cout << std::format("Unknown opcode {}\n", instruction);
      return offset + 1;
  }
}
