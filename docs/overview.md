# Overview

Gecko é uma linguagem de script com dois backends de execução: interpretador tree-walking e máquina virtual bytecode stack-based. Escrito em C++20, combina portabilidade com performance para aplicações embarcadas, scripting e automação.

## Objetivos

- Runtime leve e portável (Linux, macOS, Termux/Android)
- Módulos nativos de alto nível (JSON, HTTP, File I/O, async/await)
- Segurança por design: sandbox de módulos, sanitização de HTTP, sanitizers em CI
- Dois backends: tree-walking para debug/precisão, VM bytecode para performance

## Principais componentes

| Componente | Descrição |
|-----------|-----------|
| `src/cli/main.cpp` | Entrypoint, parsing de flags (`--backend`, `--help`) |
| `src/lexer/` | Tokenização do fonte |
| `src/parser/` | Parser recursivo descendente → AST |
| `src/ast/Nodes.h` | Definição da AST (Visitor Pattern) |
| `src/interpreter/` | Backend tree-walking |
| `src/compiler/` | Compilador AST → bytecode |
| `src/vm/` | Stack-based VM (Chunk, Value, Object, Disassembler, Vm) |
| `src/utils/Logger.cpp` | Logging e stack trace |

## Diferenciais

- **Dual backend**: alternância entre interpretação direta da AST e execução bytecode compilado
- **Stack trace Node.js-style**: erros de runtime com pilha de chamadas completa
- **Cache de módulos**: módulos importados são cacheados por path para evitar recarga
- **Módulo JSON**: parse, stringify, prettify, leitura e escrita de arquivos
- **Módulo HTTP**: cliente e servidor HTTP com API Express-like (rotas, middleware, JSON, estáticos)
- **Async/Await**: suporte nativo a funções assíncronas

## Limitações conhecidas

- Resolução de importações circulares entre módulos ainda não testada exaustivamente
- Servidor HTTP bloqueante: `app.listen()` não retorna até interrupção (Ctrl+C)
- VM bytecode em desenvolvimento ativo — alguns recursos da linguagem ainda em implementação
