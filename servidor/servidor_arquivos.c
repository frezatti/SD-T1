#include <dirent.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORTA_CONTROLE 1025
#define PORTA_DADOS 1026
#define TAMANHO_BUFFER 2048
#define TAMANHO_BACKLOG 5

int criar_e_definir_socket_inet(struct sockaddr_in *endereco, in_addr_t ip,
                                in_port_t porta);
/*
    Interpretar_Executar_Comando(comando)
    Se for para listar, envia a lista dos nomes de arquivo dentro da pasta
   servidor Se for para mudar de diretório, pega diretório filho e muda
   diretório corrente desde que não seja servidor/.. ou comece com barra Se for
   para criar arquivo, chame o creat caso o arquivo não exista Se for para criar
   pasta, chame o creat caso a pasta não exista Se for para enviar um arquivo do
   diretório corrente do cliente para o diretório do servidor, recebe o nome do
   arquivo; Se o arquivo existir pergunte se deseja substituir. Se for receber
   do servidor, leia o arquivo e o mande pela conexão de dados Se for para
   encerrar, retorne um código para sair do loop para o cliente
*/

int interpretar_executar_comando(char *comando) {

  // Listar arquivos no diretório atual
  if (strncmp("listar", comando, 6) == 0) {

    DIR *d;
    struct dirent *dir;

    d = opendir(".");
    if (d) {
      dir = readdir(d);
      while (dir != NULL) {
        // Mandar direto para o cliente ou esperar até encher o buffer e mandar
        dir = readdir(d);
      }
      closedir(d);
    } else {
      // Mandar para o cliente que não foi possivel listar os arquivos e
      // diretórios e retorna
    }
  }
  // Mudar de diretório
  else if (strncmp("mudar-dir", comando, 10) == 0) {

    char sub_dir[256];
    sscanf(comando, "mudar-dir %s", sub_dir);

    if (sub_dir[0] == '/') {
      // Manda para o cliente que não é possivel mudar para a raiz e retorna
      // para a main
    }
  }
}

int main(int argc, char *argv[]) {

  // Variaveis
  int controle_fd, dados_fd, cliente_fd;
  struct sockaddr_in endereco_controle, endereco_dados;
  char entrada[TAMANHO_BUFFER], saida[TAMANHO_BUFFER];

  controle_fd = criar_e_definir_socket_inet(&endereco_controle, INADDR_ANY,
                                            htons(PORTA_CONTROLE));

  if (controle_fd < 0)
    exit(-1);

  dados_fd = criar_e_definir_socket_inet(&endereco_dados, INADDR_ANY,
                                         htons(PORTA_DADOS));

  if (dados_fd < 0)
    exit(-1);

  // Criar loop normal de escuta do servidor

  while (1) {

    cliente_fd = accept(controle_fd, (struct sockaddr *)&endereco_controle,
                        sizeof(endereco_controle));

    if (cliente_fd < 0) {
      printf("Erro no accept() de controle\n");
      exit(-1);
    }

    // Fazer loop para cliente mandar os comandos
  }
  return 0;
}

int criar_e_definir_socket_inet(struct sockaddr_in *endereco, in_addr_t ip,
                                in_port_t porta) {

  int fd;

  fd = socket(PF_INET, SOCK_STREAM, 0);

  if (fd < 0) {

    printf("Erro ao criar o socket para controle\n");
    exit(-1);
  }

  // Colocar informações da familia de endereços, endereço IP e porta no
  // endereço controle
  endereco->sin_family = AF_INET;
  endereco->sin_addr.s_addr = ip;
  endereco->sin_port = porta;

  memset(endereco->sin_zero, 0, sizeof(endereco->sin_zero));

  // Associar socket de controle ao endereço escolhido
  if (bind(fd, (struct sockaddr *)endereco, sizeof(*endereco)) < 0) {
    printf("Erro no bind() para o socket de controle\n");
    return -1;
  }

  if (listen(fd, TAMANHO_BACKLOG) < 0) {
    printf("Erro no listen()");
    return -1;
  }

  return fd;
}
