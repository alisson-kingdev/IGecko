# Language Specification

Este documento descreve a sintaxe atual e os recursos disponíveis na linguagem Gecko.

## Tipos primitivos
- `number` — valores numéricos com ponto flutuante (`double`).
- `string` — strings delimitadas por aspas duplas.
- `bool` — `true` ou `false`.
- `null` — valor nulo.

## Literais
```go
123
3.14
"hello"
true
false
null
```

## Identificadores
Nomes válidos começam com letra ou underscore e podem conter letras, dígitos ou underscore.

## Operadores
### Aritméticos
- `+`, `-`, `*`, `/`, `%`

### Comparação
- `==`, `!=`, `<`, `<=`, `>`, `>=`

### Lógicos
- `&&`, `||`, `!`

### Atribuição
- `=` para atribuição de variáveis.

## Estruturas de controle
### `if` / `else`
```go
if (x > 0) {
    print("positive");
} else {
    print("non-positive");
}
```

### `while`
```go
while (i < 10) {
    print(i);
    i = i + 1;
}
```

### `for`
```go
for (var i = 0; i < 10; i = i + 1) {
    print(i);
}
```

### `break`
```go
while (true) {
    break;
}
```

## Funções
### Definição
```go
func add(a, b) {
    return a + b;
}
```

### Chamada
```go
print(add(2, 3));
```

## Classes e métodos
A linguagem oferece suporte completo a classes com construtores, métodos e herança:
```js
class Point {
    constructor(x, y) {
        this.x = x;
        this.y = y;
    }

    distance() {
        return this.x * this.x + this.y * this.y;
    }
}
```

## `this`
`this` só é válido dentro de métodos de classe.

## Importação de módulos
Sintaxe suportada:
```go
import "module";
import { foo, bar } from "module";
import { foo as baz } from "module";
import * from "module";
import math from "math";
```

### Comportamento do `import "module"`
Se o módulo importado definir um `package` (ex: `package utils;`), ele será automaticamente disponibilizado sob esse namespace no escopo atual, além de importar todos os nomes exportados diretamente.

## Exportação
Nomes em um módulo são privados por padrão se houver ao menos um comando `export`. Se não houver nenhum `export`, todos os nomes são públicos (comportamento legado).
```ts
export { foo, bar };
```

## Tratamento de `package`
O comando `package` define a identidade do módulo e é usado para namespacing automático em importações.
```go
package math;
```

## Tratamento de exceções
```go
try {
    print(divide(1, 0));
} catch (error) {
    print(error);
}
```

## Comentários
A linguagem suporta comentários de linha (`//`) e comentários de bloco (`/* ... */`) no lexer.

## Módulo JSON
Gecko inclui um módulo nativo `json` para manipulação de JSON:
```go
import json from "json";

var data = json.parse('{"name":"Gecko","version":1.5}');
print(data.name);                // "Gecko"
print(json.stringify(data));     // serializado
print(json.prettify(data, 2));   // indentado
json.write("/tmp/data.json", data);
var loaded = json.read("/tmp/data.json");
```

### API
| Função | Descrição |
|--------|-----------|
| `json.parse(str)` | Parseia string JSON → Gecko value (object, array, string, number, bool, null) |
| `json.stringify(val)` | Serializa Gecko value → string JSON compacta |
| `json.prettify(val, indent)` | Serializa com indentação (indent opcional, padrão 2) |
| `json.read(path)` | Lê arquivo JSON → Gecko value |
| `json.write(path, val)` | Serializa e escreve Gecko value → arquivo JSON |

## Módulo HTTP
Gecko oferece suporte completo a HTTP cliente e servidor via módulo nativo `http`.

### Cliente HTTP
```go
import http from "http";

var res = http.get("https://api.example.com/data");
print("Status: " + res.status);
print("Body: " + res.body);

http.post("https://api.example.com/submit", "payload");
http.put("https://api.example.com/update", "data");
http.delete("https://api.example.com/resource");

// Requisição genérica com headers
var res = http.request("POST", "https://api.example.com/data",
                       ["Content-Type: application/json"], '{"key":"val"}');
```

### Servidor HTTP (Express-like)
```go
import http from "http";

var app = http.createServer();

// Rotas
app.get("/", func(req, res) {
  res.send("<h1>Hello World!</h1>");
});

app.get("/api/data", func(req, res) {
  res.json({ name: "Gecko", version: 1.5 });
});

app.post("/api/submit", func(req, res) {
  var body = json.parse(req.body);
  res.status(201);
  res.json({ received: body });
});

// Inicia o servidor (bloqueante)
app.listen(8080);
```

#### Objeto `req`
| Campo | Tipo | Descrição |
|-------|------|-----------|
| `req.method` | string | Método HTTP (GET, POST, etc.) |
| `req.url` | string | Caminho da requisição |
| `req.body` | string | Corpo da requisição |
| `req.headers` | object | Cabeçalhos (chave → valor) |
| `req.query` | string | Query string |
| `req.remoteAddress` | string | IP:porta do cliente |

#### Objeto `res`
| Método | Descrição |
|--------|-----------|
| `res.send(body)` | Envia resposta como HTML/texto |
| `res.json(data)` | Envia resposta como JSON |
| `res.setHeader(name, value)` | Define cabeçalho de resposta |
| `res.status(code)` | Define código HTTP de resposta |
| `res.redirect(url, status?)` | Redireciona (status opcional, padrão 302) |
| `res.type(mime)` | Define Content-Type |

#### Rotas registráveis
`app.get()`, `app.post()`, `app.put()`, `app.delete()`, `app.patch()`, `app.options()`, `app.all()` (todos os métodos), `app.use()` (middleware global).

#### Servidor de arquivos estáticos
```go
http.serveStatic(8080, "./public");
```

## Observações
- Propriedades e atribuição de campos em `ClassInstance` têm suporte completo com sintaxe `this.campo = valor`.
- Tipagem dinâmica: o tipo das variáveis é inferido em tempo de execução.
- O servidor HTTP é bloqueante: `app.listen()` não retorna até que o servidor seja interrompido (Ctrl+C).
- Dois backends de execução disponíveis via flag `--backend=tree` (interpretação direta da AST) ou `--backend=vm` (compilação para bytecode). Ambos implementam a mesma especificação linguística.
