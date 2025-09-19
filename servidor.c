#include <dirent.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORTA_WEB 8080
#define TAMANHO_BUFFER 8192
#define TAMANHO_BACKLOG 10

// Protótipos
int criar_socket_escuta(int porta);
void tratar_conexao(int cliente_fd);
void enviar_listagem_diretorio(int cliente_fd, const char *caminho_req,
                               const char *caminho_dir);
void enviar_arquivo(int cliente_fd, const char *caminho_arquivo);
void enviar_resposta_http(int cliente_fd, const char *status,
                          const char *content_type, const char *corpo);
void enviar_erro(int cliente_fd, int status_code, const char *status_msg);
const char *get_mime_type(const char *nome_arquivo);

int main(void) {
  int servidor_fd = criar_socket_escuta(PORTA_WEB);
  printf("Servidor web iniciado na porta %d. Abra http://127.0.0.1:%d no seu "
         "navegador.\n",
         PORTA_WEB, PORTA_WEB);

  while (1) {
    struct sockaddr_in endereco_cliente;
    socklen_t tamanho_addr_cliente = sizeof(endereco_cliente);
    int cliente_fd = accept(servidor_fd, (struct sockaddr *)&endereco_cliente,
                            &tamanho_addr_cliente);

    if (cliente_fd < 0) {
      perror("Erro no accept()");
      continue;
    }

    tratar_conexao(cliente_fd);

    close(cliente_fd);
  }

  return 0;
}

void tratar_conexao(int cliente_fd) {
  char buffer[TAMANHO_BUFFER] = {0};
  read(cliente_fd, buffer, TAMANHO_BUFFER - 1);

  char metodo[16], caminho[256];
  sscanf(buffer, "%s %s", metodo, caminho);

  if (strcmp(metodo, "GET") != 0) {
    enviar_erro(cliente_fd, 501, "Not Implemented");
    return;
  }
  // Endpoint de saúde para o script de verificação
  if (strcmp(caminho, "/healthz") == 0) {
    enviar_resposta_http(cliente_fd, "200 OK", "text/plain", "ok");
    return;
  }

  // Botão "Desconectar" — retorna 200 e finaliza
  if (strcmp(caminho, "/disconnect") == 0) {
    enviar_resposta_http(cliente_fd, "200 OK", "text/html",
                         "<html><body><h1>Desconectado</h1>"
                         "<p>Voce foi desconectado. Pode fechar esta aba.</p>"
                         "</body></html>");
    return;
  }

  // Prevenção simples contra acesso a diretórios pais (Path Traversal)
  if (strstr(caminho, "..") != NULL) {
    enviar_erro(cliente_fd, 403, "Forbidden");
    return;
  }

  char caminho_fs[256];
  if (strcmp(caminho, "/") == 0) {
    strcpy(caminho_fs, ".");
  } else {
    strcpy(caminho_fs, caminho + 1);
  }

  struct stat info_caminho;
  if (stat(caminho_fs, &info_caminho) != 0) {
    enviar_erro(cliente_fd, 404, "Not Found");
  } else if (S_ISDIR(info_caminho.st_mode)) {
    enviar_listagem_diretorio(cliente_fd, caminho, caminho_fs);
  } else if (S_ISREG(info_caminho.st_mode)) {
    enviar_arquivo(cliente_fd, caminho_fs);
  } else {
    enviar_erro(cliente_fd, 403, "Forbidden");
  }
}

void enviar_listagem_diretorio(int cliente_fd, const char *caminho_req,
                               const char *caminho_dir) {
  char buffer[TAMANHO_BUFFER];

  int n = snprintf(
      buffer, sizeof(buffer),
      "HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Cache-Control: no-store\r\n"
      "Connection: close\r\n"
      "\r\n"
      "<html><head><title>Diretorio: %s</title>"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<style>"
      "body{font-family:sans-serif;}"
      "#server-status{display:none;position:fixed;top:0;left:0;right:0;"
      "background:#b00020;color:#fff;padding:10px 14px;z-index:9999;"
      "text-align:center}"
      "#toolbar{position:sticky;top:0;background:#f5f5f5;padding:8px;"
      "border-bottom:1px solid #ddd;margin-bottom:12px}"
      "button{padding:6px 10px;}"
      "</style>"
      "</head><body>"
      "<div id=\"server-status\">Servidor nao conectado</div>"
      "<div id=\"toolbar\">"
      "<button id=\"btn-disconnect\" type=\"button\">Desconectar</button>"
      "</div>"
      "<h1>Conteudo de %s</h1><ul>",
      caminho_req, caminho_req);
  write(cliente_fd, buffer, (size_t)n);

  DIR *d = opendir(caminho_dir);
  if (d) {
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
      const char *separador = (strcmp(caminho_req, "/") == 0) ? "" : "/";
      int m =
          snprintf(buffer, sizeof(buffer), "<li><a href=\"%s%s%s\">%s</a></li>",
                   caminho_req, separador, dir->d_name, dir->d_name);
      write(cliente_fd, buffer, (size_t)m);
    }
    closedir(d);
  }

  const char *tail =
      "</ul>\n"
      "<script>\n"
      "(function(){\n"
      "let disconnected=false;\n"
      "const banner=document.getElementById('server-status');\n"
      "const btn=document.getElementById('btn-disconnect');\n"
      "function "
      "show(msg){banner.textContent=msg;banner.style.display='block';}\n"
      "function hide(){banner.style.display='none';}\n"
      "async function ping(){\n"
      "  if(disconnected) return;\n"
      "  try{\n"
      "    const r=await fetch('/healthz',{cache:'no-store'});\n"
      "    if(r.ok){hide();}else{show('Servidor nao conectado');}\n"
      "  }catch(e){show('Servidor nao conectado');}\n"
      "}\n"
      "let iv=setInterval(ping,5000);\n"
      "ping();\n"
      "btn.addEventListener('click', async function(){\n"
      "  disconnected=true; clearInterval(iv);\n"
      "  show('Desconectado pelo usuario');\n"
      "  try{await "
      "fetch('/disconnect',{method:'GET',cache:'no-store'});}catch(e){}\n"
      "  /* Opcional: desativar links apos desconectar */\n"
      "  document.querySelectorAll('a').forEach(a=>{\n"
      "    a.addEventListener('click', e=>e.preventDefault());\n"
      "    a.style.pointerEvents='none';\n"
      "    a.style.opacity='0.6';\n"
      "  });\n"
      "});\n"
      "})();\n"
      "</script>\n"
      "</body></html>";

  write(cliente_fd, tail, strlen(tail));
}

void enviar_arquivo(int cliente_fd, const char *caminho_arquivo) {
  FILE *arquivo = fopen(caminho_arquivo, "rb");
  if (!arquivo) {
    enviar_erro(cliente_fd, 404, "Not Found");
    return;
  }

  fseek(arquivo, 0, SEEK_END);
  long tamanho_arquivo = ftell(arquivo);
  fseek(arquivo, 0, SEEK_SET);

  char header[512];
  snprintf(header, sizeof(header),
           "HTTP/1.0 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %ld\r\n"
           "Connection: close\r\n"
           "\r\n",
           get_mime_type(caminho_arquivo), tamanho_arquivo);

  write(cliente_fd, header, strlen(header));

  char buffer[TAMANHO_BUFFER];
  size_t bytes_lidos;
  while ((bytes_lidos = fread(buffer, 1, TAMANHO_BUFFER, arquivo)) > 0) {
    write(cliente_fd, buffer, bytes_lidos);
  }

  fclose(arquivo);
}

void enviar_resposta_http(int cliente_fd, const char *status,
                          const char *content_type, const char *corpo) {
  char resposta[TAMANHO_BUFFER];
  snprintf(resposta, sizeof(resposta),
           "HTTP/1.0 %s\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           status, content_type, strlen(corpo), corpo);
  write(cliente_fd, resposta, strlen(resposta));
}

void enviar_erro(int cliente_fd, int status_code, const char *status_msg) {
  char corpo[256];
  sprintf(corpo, "<html><body><h1>%d %s</h1></body></html>", status_code,
          status_msg);

  char status[64];
  sprintf(status, "%d %s", status_code, status_msg);

  enviar_resposta_http(cliente_fd, status, "text/html", corpo);
}

int criar_socket_escuta(int porta) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("Erro ao criar socket");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in endereco_servidor;
  memset(&endereco_servidor, 0, sizeof(endereco_servidor));
  endereco_servidor.sin_family = AF_INET;
  endereco_servidor.sin_addr.s_addr = htonl(INADDR_ANY);
  endereco_servidor.sin_port = htons(porta);

  if (bind(fd, (struct sockaddr *)&endereco_servidor,
           sizeof(endereco_servidor)) < 0) {
    perror("Erro no bind()");
    exit(EXIT_FAILURE);
  }

  if (listen(fd, TAMANHO_BACKLOG) < 0) {
    perror("Erro no listen()");
    exit(EXIT_FAILURE);
  }
  return fd;
}

const char *get_mime_type(const char *nome_arquivo) {
  if (strstr(nome_arquivo, ".html"))
    return "text/html";
  if (strstr(nome_arquivo, ".css"))
    return "text/css";
  if (strstr(nome_arquivo, ".js"))
    return "application/javascript";
  if (strstr(nome_arquivo, ".txt"))
    return "text/plain";
  if (strstr(nome_arquivo, ".jpg"))
    return "image/jpeg";
  if (strstr(nome_arquivo, ".jpeg"))
    return "image/jpeg";
  if (strstr(nome_arquivo, ".png"))
    return "image/png";
  if (strstr(nome_arquivo, ".gif"))
    return "image/gif";
  return "application/octet-stream";
}
