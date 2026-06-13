#ifndef VM_DISASSEMBLER_H
#define VM_DISASSEMBLER_H

#include <string>

struct Chunk;

void disassembleChunk(Chunk* chunk, const std::string& name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif
