# Getting Started

## Requisitos

- CMake 3.16+
- Compilador C++20 (GCC 11+, Clang 14+, Apple Clang 15+)
- Linux, macOS ou Termux (Android)

## Compilar

```bash
git clone https://github.com/TheKingKoders/gecko.git
cd gecko
cmake -B build -S .
cmake --build build
```

O binário estará em `build/gecko`.

## Executar

### Backend tree-walking (padrão)
```bash
./build/gecko examples/00_hello.gk
```

### Backend VM bytecode
```bash
./build/gecko --backend=vm examples/00_hello.gk
```

O backend tree-walking é mais adequado para debug e desenvolvimento. O backend VM compila a AST para bytecode antes da execução, oferecendo melhor performance e rastreabilidade.

## Exemplo rápido

Crie `hello.gk`:
```go
package main;

func main() {
    print("Hello, Gecko!");
}

main();
```

Execute:
```bash
./build/gecko hello.gk
```

## Scripts auxiliares

| Comando | Descrição |
|---------|-----------|
| `./scripts/build.sh` | Compila com CMake/Ninja |
| `./scripts/test.sh` | Executa exemplos como testes de regressão |
| `./scripts/tidy.sh` | Análise estática com clang-tidy |
| `./scripts/format.sh` | Formata código com clang-format |

## CI/CD

- **CodeQL**: análise estática em todo PR
- **Sanitizers**: ASan, UBSan, TSan ativos em CI
- **Testes**: `./scripts/test.sh` obrigatório antes de submeter PR
