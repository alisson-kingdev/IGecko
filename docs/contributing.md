# Contributing

## Como contribuir

1. Abra um issue descrevendo o bug ou funcionalidade
2. Faça um fork do repositório
3. Crie uma branch descritiva:
   - `feature/<nome-da-funcionalidade>`
   - `fix/<descricao-do-bug>`
4. Faça commits pequenos e granulares
5. Envie um pull request para `main`

## Padrões de código

- **C++20/23**: Concepts, Ranges, `std::format`, `std::print`
- **RAII**: sem `delete` manual, smart pointers obrigatórios
- **Visitor Pattern**: usado em AST e interpretadores
- **I/O**: `std::format`/`std::print` preferido sobre `std::cout`/`printf`
- **Performance**: zero-overhead abstractions, evitar heap em hot paths
- Comentários explicam *por que* (intent), não *o que* (código)

## Estrutura de pastas

```
src/
├── cli/          — entrypoint, argumentos
├── lexer/        — tokenização
├── parser/       — AST recursivo descendente
├── ast/          — definições da AST
├── interpreter/  — backend tree-walking
│   ├── environment/
│   ├── module/
│   └── runtime/
├── compiler/     — compilador AST → bytecode
├── vm/           — stack-based VM
└── utils/        — logger e utilitários
```

## Boas práticas

- Execute `./scripts/test.sh` antes de submeter
- Execute `./scripts/tidy.sh` para análise estática
- Execute `./scripts/format.sh` para formatação
- Atualize `docs/` se adicionar ou alterar funcionalidades
- Teste com ambos os backends: `--backend=tree` e `--backend=vm`

## Sugestões de contribuição

- Implementar opcodes faltantes no backend VM (break, template strings, object literals)
- String interning para a VM
- Garbage collector (mark-and-sweep)
- Melhorar cobertura de testes para o backend VM
- Suporte a tipos nativos mais robustos no módulo JSON
- Testes de integração para resolução de dependências circulares
