# Automation Scripts

O IGecko utiliza um conjunto de scripts para automatizar tarefas comuns de desenvolvimento, build e manutenção. Todos os scripts localizam automaticamente a raiz do projeto.

## Lista de Scripts

| Script | Descrição | Uso |
| :--- | :--- | :--- |
| `build.sh` | Compila o projeto usando CMake/Ninja | `./scripts/build.sh` |
| `coverage.sh` | Gera relatórios de cobertura de testes | `./scripts/coverage.sh` |
| `format.sh` | Formata o código C++ seguindo `.clang-format` | `./scripts/format.sh` |
| `install-in-termux.sh` | Instala Gecko/GPM globalmente no Termux | `./scripts/install-in-termux.sh` |
| `test.sh` | Executa todos os testes e exemplos | `./scripts/test.sh` |
| `tidy.sh` | Executa análise estática (`clang-tidy`) | `./scripts/tidy.sh` |

## Detalhes

### `build.sh`
Configura e compila o projeto no diretório `build/` usando o gerador Ninja. Utiliza paralelismo automático baseado no número de núcleos da CPU.

### `coverage.sh`
Configura o build para instrumentação de cobertura, roda a suíte de testes (`ctest`) e utiliza `gcovr` para gerar `coverage.html`. Exige que o `gcovr` esteja instalado no sistema.

### `format.sh`
Aplica automaticamente as regras de formatação definidas no arquivo `.clang-format` da raiz do projeto. Garante a conformidade do código C++20.

### `install-in-termux.sh`
Script interativo para usuários do Termux.
- Verifica e instala dependências automaticamente (toilet, figlet, ncurses-utils).
- Compila o projeto.
- Instala os binários `gecko` e `gpm` em `/data/data/com.termux/files/usr/bin/` para acesso global.

### `test.sh`
Executor de testes de integração.
- Itera sobre todos os arquivos `.gk` nos diretórios `examples/` e `examples/useLibs/`.
- Suporta marcação `// @expected-fail` para testes que devem falhar intencionalmente.
- Exibe resumo de `Passed`, `Expected Fails` e `Failed`.

### `tidy.sh`
Verificador de qualidade de código. Executa `clang-tidy` sobre todos os arquivos `.cpp` do projeto usando as configurações definidas em `.clang-tidy`.
