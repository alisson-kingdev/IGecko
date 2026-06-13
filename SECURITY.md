# Política de Segurança

## Versões Suportadas

Nós fornecemos atualizações de segurança ativamente para as seguintes versões do IGecko:

| Versão | Suportada          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1   | :x:                |

## Relatando uma Vulnerabilidade

Levamos a segurança do IGecko a sério. Se você acredita que encontrou uma vulnerabilidade de segurança, por favor, não a relate através de *issues* públicas no GitHub. Em vez disso, siga nosso processo de divulgação responsável:

1.  **Relatório Privado**: Envie um relatório detalhado para [alissonfarias920@gmail.com](mailto:alissonfarias920@gmail.com).
2.  **Confirmação**: Você receberá uma confirmação do recebimento do seu relatório dentro de 48 horas.
3.  **Avaliação**: Investigaremos o problema e determinaremos seu impacto.
4.  **Correção**: Se uma vulnerabilidade for confirmada, trabalharemos em uma correção e coordenaremos o lançamento.
5.  **Divulgação**: Assim que a correção for lançada, divulgaremos publicamente a vulnerabilidade com o devido crédito ao relator (se desejado).

## Melhores Práticas de Segurança para Desenvolvedores do IGecko

### 1. Módulos Nativos e Acesso ao Sistema
O IGecko fornece módulos nativos poderosos (como o módulo `http` e o `ProcessUtils`).
- **Validação de Entrada**: Sempre valide e higienize os dados fornecidos pelo usuário antes de passá-los para funções nativas (ex: URLs, caminhos de arquivos, argumentos de comando).
- **Princípio do Menor Privilégio**: Execute scripts Gecko com o mínimo de permissões de sistema necessárias.

### 2. Segurança de Rede
O módulo nativo `http` usa `httplib` com suporte a OpenSSL.
- **TLS/SSL**: Sempre use `https` para dados sensíveis.
- **Injeção de Cabeçalho**: Seja cauteloso ao construir cabeçalhos personalizados a partir da entrada do usuário.

### 3. Segurança de Memória
O IGecko é escrito em C++20 e utiliza RAII e ponteiros inteligentes para mitigar problemas comuns de segurança de memória.
- **Mudanças Cirúrgicas**: Ao contribuir para o núcleo em C++, siga estritamente o modelo de propriedade definido em `AGENTS.md`.
- **Sanitizadores**: Sempre execute o build com sanitizadores ativados durante o desenvolvimento (`-DSANITIZE_ADDRESS=ON`, `-DSANITIZE_THREAD=ON`, `-DSANITIZE_UNDEFINED=ON`).
- **Segurança de Memória**: Aplicamos uma política de zero vazamento (verificação com Valgrind recomendada para mudanças importantes).

## Ferramentas de Segurança Automatizadas e CI/CD

O IGecko integra várias ferramentas automatizadas para manter um alto nível de segurança. Os *pipelines* de CI/CD DEVEM passar por todas as varreduras de segurança antes de realizar o *merge*:

- **CodeQL**: Análise estática automatizada para identificar vulnerabilidades na base de código C++.
- **Clang-Tidy**: Aplica padrões de codificação e melhores práticas relacionadas à segurança (`bugprone`, `cppcoreguidelines`, `readability`).
- **Sanitizadores (CI)**: O CI exige sanitizadores (AddressSanitizer, UndefinedBehaviorSanitizer, ThreadSanitizer) para detectar erros de memória e comportamento indefinido.
- **Auditoria de Dependências**: Utilizamos o `FetchContent` do CMake com tags/hashes específicos para todas as dependências externas para evitar ataques à cadeia de suprimentos (*supply-chain attacks*).

## Segurança da Cadeia de Suprimentos

- **Dependências**: Utilize tags/hashes específicos do Git para todas as bibliotecas externas.
- **Revisão**: Todos os *pull requests* estão sujeitos a uma revisão de segurança rigorosa, incluindo análise SAST, antes de serem mesclados.
