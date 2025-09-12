#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORTA_CONTROLE 1025
#define PORTA_DADOS 1026
#define TAMANHO_BUFFER 4096
#define TAMANHO_BACKLOG 5

// Protótipos das funções
int criar_e_definir_socket_inet(struct sockaddr_in *endereco, in_addr_t ip,
                                in_port_t porta);
int interpretar_executar_comando(char *comando, int cliente_fd_controle,
                                 int dados_fd);
int mandar_arquivo(int client_fd, const char *filepath, const char *status_line,
                   const char *extra_headers);
int send_all(int fd, const void *buf, size_t len);

int main(int argc, char *argv[]) {
  char *path = "index.html";

  // Variaveis
  int controle_fd, dados_fd, cliente_fd;
  struct sockaddr_in endereco_controle, endereco_dados;
  socklen_t tamanho_addr_cliente = sizeof(struct sockaddr_in);
  char buffer[TAMANHO_BUFFER];

  // Cria e configura o socket de CONTROLE
  controle_fd = criar_e_definir_socket_inet(&endereco_controle, INADDR_ANY,
                                            htons(PORTA_CONTROLE));
  if (controle_fd < 0)
    exit(-1);
  printf("Servidor aguardando conexões na porta de controle %d...\n",
         PORTA_CONTROLE);

  // Cria e configura o socket de DADOS
  dados_fd = criar_e_definir_socket_inet(&endereco_dados, INADDR_ANY,
                                         htons(PORTA_DADOS));
  if (dados_fd < 0)
    exit(-1);
  printf("Servidor pronto para transferir dados na porta %d...\n", PORTA_DADOS);

  while (1) {

    cliente_fd = accept(controle_fd, (struct sockaddr *)&endereco_controle,
                        &tamanho_addr_cliente);

    if (cliente_fd < 0) {
      perror("Erro no accept() de controle");
      continue; // Continua para a próxima iteração ao invés de sair
    }

    printf("\nCliente conectado!\n");

    char req[2048];
    ssize_t n = recv(cliente_fd, req, sizeof(req) - 1, 0);
    req[n] = '\0';

    char method[8], path[1024], version[16];
    if (sscanf(req, "%7s %1023s %15s", method, path, version) != 3) {
      const char bad[] =
          "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";
      send_all(cliente_fd, bad, sizeof(bad) - 1);
      close(cliente_fd);
      continue;
    }

    if (strcmp(method, "GET") != 0) {
      const char only_get[] =
          "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n";
      send_all(cliente_fd, only_get, sizeof(only_get) - 1);
      close(cliente_fd);
      continue;
    }

    if (strcmp(path, "/") == 0) {
      if (mandar_arquivo(cliente_fd, "./www/index.html", "HTTP/1.1 200 OK",
                         "Content-Type: text/html\r\n") < 0) {
        const char nf[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
        send_all(cliente_fd, nf, sizeof(nf) - 1);
      }
    } else if (strcmp(path, "/app.js") == 0) {
      if (mandar_arquivo(cliente_fd, "./www/app.js", "HTTP/1.1 200 OK",
                         "Content-Type: application/javascript\r\n") < 0) {
        const char nf[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
        send_all(cliente_fd, nf, sizeof(nf) - 1);
      }
    } else {
      const char nf[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
      send_all(cliente_fd, nf, sizeof(nf) - 1);
    }
    // Loop para tratar os comandos do cliente conectado
    while (1) {
      memset(buffer, 0, TAMANHO_BUFFER); // Limpa o buffer
      int bytes_lidos = read(cliente_fd, buffer, TAMANHO_BUFFER - 1);

      if (bytes_lidos <= 0) {
        printf("Cliente desconectado.\n");
        break; // Sai do loop se o cliente fechar a conexão ou houver erro
      }

      // Remove a quebra de linha do final do comando, se houver
      buffer[strcspn(buffer, "\r\n")] = 0;

      printf("Comando recebido: '%s'\n", buffer);

      // Se o comando for para encerrar, a função retornará 0
      if (interpretar_executar_comando(buffer, cliente_fd, dados_fd) == 0) {
        break; // Sai do loop do cliente
      }
    }

    close(cliente_fd); // Fecha a conexão com o cliente atual
    printf("Aguardando novo cliente...\n");
  }

  close(controle_fd);
  close(dados_fd);
  return 0;
}

/*
    interpretar_executar_comando(comando, cliente_fd_controle, dados_fd)
    - Interpreta o comando recebido do cliente.
    - Executa a ação correspondente.
    - Usa cliente_fd_controle para enviar respostas de texto/status.
    - Usa dados_fd para aceitar conexões e transferir arquivos.
    - Retorna 0 se o comando for 'encerrar', e 1 para os outros casos.
*/
int interpretar_executar_comando(char *comando, int cliente_fd_controle,
                                 int dados_fd) {

  // --- COMANDO: listar ---
  if (strncmp("listar", comando, 6) == 0) {
    DIR *d;
    struct dirent *dir;
    char lista_arquivos[TAMANHO_BUFFER] = {0};

    d = opendir(".");
    if (d) {
      while ((dir = readdir(d)) != NULL) {
        // Evita concatenar o diretório atual "." e o pai ".."
        if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
          strncat(lista_arquivos, dir->d_name,
                  TAMANHO_BUFFER - strlen(lista_arquivos) - 1);
          strncat(lista_arquivos, "\n",
                  TAMANHO_BUFFER - strlen(lista_arquivos) - 1);
        }
      }
      closedir(d);
      send(cliente_fd_controle, lista_arquivos, strlen(lista_arquivos), 0);
    } else {
      char *msg_erro = "Erro ao listar diretório no servidor.";
      send(cliente_fd_controle, msg_erro, strlen(msg_erro), 0);
    }
  }
  // --- COMANDO: mudar-dir <nome_do_diretorio> ---
  else if (strncmp("mudar-dir", comando, 9) == 0) {
    char sub_dir[256];
    sscanf(comando, "mudar-dir %s", sub_dir);

    // Medida de segurança simples: não permite voltar para diretórios pais
    // (evita sair da pasta raiz do servidor)
    if (strstr(sub_dir, "..") != NULL || sub_dir[0] == '/') {
      char *msg_erro = "Operação não permitida: caminho inválido.";
      send(cliente_fd_controle, msg_erro, strlen(msg_erro), 0);
    } else {
      if (chdir(sub_dir) == 0) {
        char msg_sucesso[512];
        getcwd(msg_sucesso,
               sizeof(msg_sucesso)); // Pega o diretório atual para confirmar
        send(cliente_fd_controle, msg_sucesso, strlen(msg_sucesso), 0);
      } else {
        char msg_erro[512];
        snprintf(msg_erro, sizeof(msg_erro), "Erro ao mudar de diretório: %s",
                 strerror(errno));
        send(cliente_fd_controle, msg_erro, strlen(msg_erro), 0);
      }
    }
  }
  // --- COMANDO: enviar <nome_arquivo> (Servidor ENVIA para o cliente) ---
  else if (strncmp("enviar", comando, 6) == 0) {
    char nome_arquivo[256];
    sscanf(comando, "enviar %s", nome_arquivo);

    FILE *arquivo = fopen(nome_arquivo, "rb");
    if (arquivo == NULL) {
      send(cliente_fd_controle, "ERRO: Arquivo não encontrado no servidor.",
           strlen("ERRO: Arquivo não encontrado no servidor."), 0);
    } else {
      send(cliente_fd_controle, "OK", strlen("OK"),
           0); // Avisa ao cliente que o arquivo existe e a transferência vai
               // começar

      struct sockaddr_in endereco_dados_cliente;
      socklen_t tamanho_addr = sizeof(endereco_dados_cliente);
      int dados_cliente_fd = accept(
          dados_fd, (struct sockaddr *)&endereco_dados_cliente, &tamanho_addr);

      if (dados_cliente_fd < 0) {
        perror("Erro no accept() de dados");
        fclose(arquivo);
        return 1;
      }

      char buffer_dados[TAMANHO_BUFFER];
      size_t bytes_lidos;
      while ((bytes_lidos = fread(buffer_dados, 1, TAMANHO_BUFFER, arquivo)) >
             0) {
        send(dados_cliente_fd, buffer_dados, bytes_lidos, 0);
      }

      printf("Arquivo '%s' enviado com sucesso.\n", nome_arquivo);
      fclose(arquivo);
      close(dados_cliente_fd);
    }
  }
  // --- COMANDO: receber <nome_arquivo> (Servidor RECEBE do cliente) ---
  else if (strncmp("receber", comando, 7) == 0) {
    char nome_arquivo[256];
    sscanf(comando, "receber %s", nome_arquivo);

    // Avisa ao cliente que está pronto para receber
    send(cliente_fd_controle, "OK", strlen("OK"), 0);

    struct sockaddr_in endereco_dados_cliente;
    socklen_t tamanho_addr = sizeof(endereco_dados_cliente);
    int dados_cliente_fd = accept(
        dados_fd, (struct sockaddr *)&endereco_dados_cliente, &tamanho_addr);

    if (dados_cliente_fd < 0) {
      perror("Erro no accept() de dados");
      return 1;
    }

    FILE *arquivo = fopen(nome_arquivo, "wb");
    if (arquivo == NULL) {
      perror("Erro ao criar arquivo no servidor");
      close(dados_cliente_fd);
      return 1;
    }

    char buffer_dados[TAMANHO_BUFFER];
    int bytes_recebidos;
    while ((bytes_recebidos =
                recv(dados_cliente_fd, buffer_dados, TAMANHO_BUFFER, 0)) > 0) {
      fwrite(buffer_dados, 1, bytes_recebidos, arquivo);
    }

    printf("Arquivo '%s' recebido com sucesso.\n", nome_arquivo);
    fclose(arquivo);
    close(dados_cliente_fd);
  }
  // --- COMANDO: encerrar ---
  else if (strncmp("encerrar", comando, 8) == 0) {
    send(cliente_fd_controle, "Conexão encerrada.",
         strlen("Conexão encerrada."), 0);
    return 0; // Sinaliza para o loop principal fechar a conexão
  }
  // --- COMANDO DESCONHECIDO ---
  else {
    char *msg = "Comando desconhecido.";
    send(cliente_fd_controle, msg, strlen(msg), 0);
  }

  return 1; // Mantém a conexão ativa
}

/*
    criar_e_definir_socket_inet(endereco, ip, porta)
    - Função auxiliar para criar, configurar e colocar um socket em modo de
   escuta.
    - Retorna o descritor de arquivo do socket ou -1 em caso de erro.
*/
int criar_e_definir_socket_inet(struct sockaddr_in *endereco, in_addr_t ip,
                                in_port_t porta) {

  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("Erro ao criar o socket");
    return -1;
  }

  // Permite reutilizar o endereço local rapidamente (útil para testes)
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt(SO_REUSEADDR) falhou");
  }

  endereco->sin_family = AF_INET;
  endereco->sin_addr.s_addr = ip;
  endereco->sin_port = porta;
  memset(endereco->sin_zero, 0, sizeof(endereco->sin_zero));

  if (bind(fd, (struct sockaddr *)endereco, sizeof(*endereco)) < 0) {
    char msg_erro[100];
    sprintf(msg_erro, "Erro no bind() para a porta %d", ntohs(porta));
    perror(msg_erro);
    return -1;
  }

  if (listen(fd, TAMANHO_BACKLOG) < 0) {
    perror("Erro no listen()");
    return -1;
  }

  return fd;
}

int send_all(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, p + sent, len - sent, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (n == 0)
      break;
    sent += (size_t)n;
  }
  return 0;
}

int mandar_arquivo(int cliente_fd, const char *filepath,
                   const char *status_line, const char *extra_headers) {
  int f = open(filepath, O_RDONLY);
  if (f < 0)
    return -1;

  struct stat st;
  if (fstat(f, &st) < 0) {
    close(f);
    return -1;
  }

  char hdr[512];
  int n = snprintf(hdr, sizeof(hdr),
                   "%s\r\n"
                   "Content-Length: %lld\r\n"
                   "Connection: close\r\n"
                   "%s"
                   "\r\n",
                   status_line, (long long)st.st_size,
                   (extra_headers ? extra_headers : ""));
  if (send_all(cliente_fd, hdr, (size_t)n) < 0) {
    close(f);
    return -1;
  }

  char buf[8192];
  ssize_t r;
  while ((r = read(f, buf, sizeof(buf))) > 0) {
    if (send_all(cliente_fd, buf, (size_t)r) < 0) {
      close(f);
      return -1;
    }
  }
  close(f);
  return 0;
}
