# Architecture

Gecko é organizado em camadas que transformam texto fonte em execução, com dois backends alternativos.

## Fluxo geral

```
Fonte (.gk)
  → Lexer (tokenização)
  → Parser (AST)
  → Interpreter (tree-walking)   ← backend tree (padrão)
  → Compiler (bytecode) + Vm     ← backend vm (--backend=vm)
```

## Componentes principais

### `src/cli/main.cpp`

Entrypoint que:
- Faz parsing dos argumentos de linha de comando (`--backend`, `--help`)
- Lê o arquivo fonte
- Instancia o backend escolhido (Interpreter ou Vm)
- Trata exceções de alto nível com stack trace

```cpp
if (backend == "vm") {
    Vm vm;
    vm.interpret(source, filename);
} else {
    Interpreter interpreter;
    interpreter.interpret(statements, filename);
}
```

### `src/lexer/Lexer.cpp`

Produz um vetor de `Token` a partir do texto fonte. Suporta strings, números, identificadores, operadores e comentários `//` e `/* */`.

### `src/parser/Parser.cpp`

Parser recursivo descendente que implementa cada regra da gramática como um método. Constrói a AST compartilhada via `std::shared_ptr`.

### `src/ast/Nodes.h`

Define a AST com classes para cada expressão e statement, seguindo o padrão Visitor para separar travessia de execução.

### `src/interpreter/` — Backend Tree-Walking

- `Interpreter.cpp` — avalia a AST diretamente, nó por nó
- `environment/Environment.cpp` — escopos aninhados com herança
- `module/ModuleLoader.cpp` — resolução e cache de módulos com validação de caminho seguro
- `module/HttpModule.cpp` — cliente HTTP + servidor Express-like com sanitização de headers
- `module/JsonModule.cpp` — parse/serialize JSON via nlohmann/json
- `runtime/Runtime.cpp` — `UserFunction`, `UserClass`, `ClassInstance`, `ReturnException`

### `src/compiler/Compiler.h/.cpp` — Backend Bytecode

Compila a AST em bytecode linear. Gerencia:
- Escopos e resolução de variáveis locais/globais/upvalues
- Compilação de funções com clausuras
- Definição de classes e métodos
- Emissão de operadores, estruturas de controle e chamadas

Utiliza `CompilerState` para rastrear escopo, upvalues e contexto de loop.

### `src/vm/` — Backend Bytecode

- `Chunk.h/.cpp` — contêiner de bytecode com constant pool e line info
- `Value.h/.cpp` — tagged union (nil, bool, number, Obj*) com type-safe accessors
- `Object.h/.cpp` — heap objects: string, function, closure, native, class, instance, array, bound method
- `Disassembler.h/.cpp` — debug: desmonta bytecode para stdout
- `Vm.h/.cpp` — stack-based executor com 42+ opcodes, call frames, globals, natives

### `src/utils/Logger.cpp`

Mantém pilha de frames e imprime stack trace no estilo Node.js:

```
Traceback (most recent call last):
  at divide (examples/error_test.gk:3:12)
  at calculate (examples/error_test.gk:7:5)
RuntimeError: Division by zero at line 4
```

## Build system

CMake com gerador Ninja. Padrão C++20.

- `CMakeLists.txt` inclui `src/lexer/`, `src/parser/`, `src/ast/`, `src/interpreter/`, `src/compiler/`, `src/vm/`, `src/utils/`, `src/cli/`
- Executável: `gecko`
- Build em `build/`
- LTO (Link-Time Optimization) habilitado para release
- Sanitizers (ASan, UBSan, TSan) configuráveis via CMake

## Segurança

- **Sandbox de módulos**: `isPathSafe()` valida que imports estão dentro da raiz do projeto ou diretórios permitidos
- **Hardening de HTTP**: `isValidHeaderKey()` e `sanitizeHeaderValue()` previnem header injection
- **CI**: CodeQL análise estática + sanitizers em todos os PRs
