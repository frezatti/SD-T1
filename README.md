# Servidor Web Simples em C

Este projeto implementa um servidor web minimalista em C, capaz de:

- Atender requisições HTTP GET
- Servir arquivos estáticos do diretório atual
- Listar o conteúdo de diretórios como páginas HTML
- Expor um endpoint de saúde (`/healthz`)
- Oferecer um “botão” de desconexão via rota `/disconnect`
- Retornar códigos e mensagens de erro básicas (404, 403, 501)

É útil para estudos de redes, sockets e protocolo HTTP, bem como para inspecionar rapidamente arquivos de um diretório via navegador.

## Funcionalidades

- Roteamento básico:
  - `/` ou `/<subcaminho>`: serve diretórios e arquivos a partir do diretório atual.
  - `/healthz`: retorna `200 OK` com corpo `ok` (para verificações).
  - `/disconnect`: retorna uma página HTML simples de “Desconectado”.
- Listagem de diretórios: gera uma página HTML com links para cada item.
- Prevenção simples de path traversal: bloqueia caminhos contendo `..`.
- Tipos MIME básicos: html, css, js, txt, jpg/jpeg, png, gif, com fallback para `application/octet-stream`.
- Cabeçalhos HTTP mínimos (HTTP/1.0), incluindo `Content-Length` e `Connection: close`.

## Requisitos

- Sistema Unix-like (Linux/macOS) com:
  - Compilador C (ex.: `gcc` ou `clang`)
  - Bibliotecas de rede padrão (sockets POSIX)

## Compilação

Use `gcc`

## Execução

```bash
./servidor
```

Saída esperada:

```
Servidor web iniciado na porta 8080. Abra http://127.0.0.1:8080 no seu navegador.
```

Em seguida, abra no navegador:

- Página inicial/listagem: http://127.0.0.1:8080/
- Health check: http://127.0.0.1:8080/healthz
- Desconectar: http://127.0.0.1:8080/disconnect

Por padrão, o servidor escuta na porta 8080 (constante `PORTA_WEB`).

## Como funciona

- `main`:
  - Cria o socket de escuta na porta 8080.
  - Aceita conexões em loop.
  - Para cada conexão, chama `tratar_conexao` e fecha o descritor do cliente.

- `tratar_conexao`:
  - Lê a primeira linha da requisição.
  - Suporta apenas método `GET`; outros retornam `501 Not Implemented`.
  - Impede `..` no caminho (bloqueio simples de path traversal).
  - Mapeia `/` para o diretório atual (`.`) e remove a barra inicial dos demais caminhos.
  - Usa `stat` para decidir se é diretório (`enviar_listagem_diretorio`) ou arquivo (`enviar_arquivo`); senão, erro.

- `enviar_listagem_diretorio`:
  - Emite um HTML com:
    - Título, toolbar com botão “Desconectar” e um banner de status.
    - Lista de arquivos/dirs do caminho solicitado.
    - Script JS que:
      - Faz ping periódico em `/healthz` a cada 5s.
      - Mostra banner “Servidor nao conectado” se falhar.
      - No botão “Desconectar”, chama `/disconnect`, mostra “Desconectado” e desativa links.

- `enviar_arquivo`:
  - Lê e envia o arquivo com `Content-Type` determinado por `get_mime_type`.
  - Inclui `Content-Length` e fecha a conexão ao final.

- `enviar_resposta_http` / `enviar_erro`:
  - Montam respostas HTTP/1.0 simples com cabeçalhos básicos.

- `criar_socket_escuta`:
  - Cria socket TCP, configura `SO_REUSEADDR`, faz `bind` e `listen`.

- `get_mime_type`:
  - Faz detecção simples por substring da extensão.

## Segurança e limitações

- Path traversal: há uma checagem simples para `..`, mas não é robusta contra todos os casos (ex.: codificação de URL, symlinks). Para fins didáticos apenas.
- HTTP/1.0: sem suporte a persistência de conexão, chunked encoding, ou cabeçalhos avançados.
- Concurrency: trata uma conexão por vez no loop principal (não usa threads/fork). Em produção, considerar paralelismo.
- Parsing de HTTP: parsing mínimo; não valida todos os cabeçalhos, nem suporta métodos além de GET.
- Tamanho fixo de buffers: entradas muito grandes podem ser truncadas.
- MIME types: detecção simplista baseada em substring; idealmente usar comparação por sufixo/extension exata.

## Personalizações

- Porta: altere `#define PORTA_WEB 8080`.
- Backlog: ajuste `TAMANHO_BACKLOG`.
- Buffer: ajuste `TAMANHO_BUFFER` para transferências maiores.
- Tipos MIME: amplie `get_mime_type` com novas extensões.
- Estilo/UX: edite o HTML/CSS/JS gerado em `enviar_listagem_diretorio`.

## Exemplos de uso

- Servir rapidamente um diretório de arquivos estáticos durante o desenvolvimento.
- Explorar conteúdo de uma pasta local via navegador.
- Estudar sockets TCP e respostas HTTP na prática.

## Solução de problemas

- “Erro no bind()”: verifique se a porta 8080 não está em uso ou rode com outra porta.
- Navegador não carrega:
  - Confirme execução no terminal.
  - Tente `curl http://127.0.0.1:8080/` para inspecionar a resposta.
- Arquivo não abre:
  - Verifique permissões de leitura.
  - Cheque o caminho mapeado: a URL `/subpasta/arquivo.txt` corresponde ao arquivo `./subpasta/arquivo.txt`.

## Licença

Este código é para fins educacionais. Adapte uma licença conforme sua necessidade (por exemplo, MIT).
