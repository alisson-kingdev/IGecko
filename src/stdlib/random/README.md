# random

Módulo do Gecko para geração de números aleatórios.

## Funções

- `setSeed(seed: number)` - Define semente para gerador.
- `next(): number` - Próximo número pseudo-aleatório.
- `random(): number` - Número aleatório entre 0 e 1.
- `randint(min: number, max: number): number` - Inteiro aleatório no intervalo.

## Exemplo

```gecko
import { random, randint } from "random";

print(random()); // 0.123...
print(randint(1, 10)); // 5
```
