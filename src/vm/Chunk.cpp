#include "Chunk.h"

void Chunk::write(uint8_t byte, int line)
{
  bytecode.push_back(byte);
  lines.push_back(line);
}

int Chunk::addConstant(Value value)
{
  constants.push_back(value);
  return static_cast<int>(constants.size()) - 1;
}

void Chunk::writeConstant(Value value, int line)
{
  int index = addConstant(value);
  if (index < 256) {
    write(OP_CONSTANT, line);
    write(static_cast<uint8_t>(index), line);
  }
}

void Chunk::writeJump(uint8_t op, int line)
{
  write(op, line);
  write(0xff, line);
  write(0xff, line);
}

int Chunk::count() const
{
  return static_cast<int>(bytecode.size());
}
