# Error Handling

Gecko possui um sistema de erros estruturado com logs e stack trace.

## Tipo de erros
- `LoggerException` — erros lexicais e de sintaxe.
- `RuntimeError` — erros de execução em tempo de runtime.
- `ReturnException` — controle interno de retorno de função.
- `BreakException` — controle interno para sair de loops.

## Logger e stack trace
O logger mantém uma pilha de frames (`StackFrame`) durante chamadas de função:
- cada função empilha seu nome e localização
- ao lançar um erro, o trace é impresso de forma reversa

Exemplo de saída:
```text
Traceback (most recent call last):
  at divide (examples/error_test.gk:3:12)
  at calculate (examples/error_test.gk:7:5)
RuntimeError: Division by zero at line 4
```

## Captura de erro no `main`
O arquivo `src/main.cpp` trata 4 tipos de exceções:
- `LoggerException`
- `RuntimeError`
- `ReturnException`
- `std::exception`

O processamento central garante que o usuário veja mensagens claras e o stack trace completo.

## Como os erros são gerados
### Léxicos
No `Lexer`, caracteres inesperados ou strings não terminadas lançam `LoggerException`:
```cpp
reportError("Unexpected character: " + std::string(1, c));
```

### Sintaxe
No `Parser`, erros de gramática lançam `LoggerException` com token e localização.

### Runtime
No `Interpreter`, operações inválidas lançam `std::runtime_error`
que são convertidas para `RuntimeError` com trace salvo.

### Exemplo de runtime
```cpp
if (r == 0.0)
{
    throw std::runtime_error("Division by zero at line " + std::to_string(op.line));
}
```

## Recomendações
- Ao desenvolver novas construções, inclua localizações de token nos erros.
- Preserve o stack trace ao propagar exceções técnicas.
- Use `Logger::captureTrace()` antes de relançar a exceção para manter o contexto.
