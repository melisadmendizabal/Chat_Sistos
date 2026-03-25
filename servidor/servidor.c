#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocolo.h"


void cmd_broadcast(ChatPacket *pkt);
void cmd_direct(ChatPacket *pkt);
void cmd_list(ChatPacket *pkt, int fd);
void cmd_info(ChatPacket *pkt, int fd);
void cmd_status(ChatPacket *pkt, int fd);
void liberar_cliente(int fd);
void *monitor_inactividad(void *arg);


#define MAX_CLIENTS 100

typedef struct {
    char username[32];
    char ip[INET_ADDRSTRLEN];
    char status[16];
    int sockfd;
    int activo;
    time_t ultimo_mensaje;
} Cliente;

Cliente lista[MAX_CLIENTS];
int num_clientes = 0;
pthread_mutex_t mutex_lista = PTHREAD_MUTEX_INITIALIZER;


void enviar_paquete(int fd, uint8_t cmd, const char *sender,
                    const char *target, const char *payload) {
    ChatPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.command = cmd;
    if (sender)  strncpy(pkt.sender,  sender,  31);
    if (target)  strncpy(pkt.target,  target,  31);
    if (payload) {
        strncpy(pkt.payload, payload, 956);
        pkt.payload_len = strlen(pkt.payload);
    }
    send(fd, &pkt, sizeof(pkt), 0);
}


void *manejar_cliente(void *arg) {
    int fd = *(int*)arg;
    free(arg);

    //  REGISTRO 
    ChatPacket pkt;
    recv(fd, &pkt, sizeof(pkt), MSG_WAITALL);

    if (pkt.command != CMD_REGISTER) {
        close(fd);
        return NULL;
    }

    // Obtener IP del cliente
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(fd, (struct sockaddr*)&addr, &len);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

    // Verificar duplicados (nombre o IP)
    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo &&
           (strcmp(lista[i].username, pkt.sender) == 0 ||
            strcmp(lista[i].ip, ip) == 0)) {
            pthread_mutex_unlock(&mutex_lista);
            enviar_paquete(fd, CMD_ERROR, "SERVER", pkt.sender, "Usuario o IP ya existe");
            close(fd);
            return NULL;
        }
    }

    if (num_clientes >= MAX_CLIENTS) {
        pthread_mutex_unlock(&mutex_lista);
        enviar_paquete(fd, CMD_ERROR, "SERVER", pkt.sender, "Servidor lleno");
        close(fd);
        return NULL;
    }
    
    // Registrar cliente
    int idx = num_clientes++;
    strncpy(lista[idx].username, pkt.sender, 31);
    strncpy(lista[idx].ip, ip, INET_ADDRSTRLEN - 1);
    strncpy(lista[idx].status, STATUS_ACTIVO, 15);
    lista[idx].sockfd = fd;
    lista[idx].activo = 1;
    lista[idx].ultimo_mensaje = time(NULL);
    pthread_mutex_unlock(&mutex_lista);

    char bienvenida[64];
    snprintf(bienvenida, sizeof(bienvenida), "Bienvenido %s", pkt.sender);
    enviar_paquete(fd, CMD_OK, "SERVER", pkt.sender, bienvenida);

    printf("[+] Cliente registrado: %s desde %s\n", pkt.sender, ip);

    // /////////////// LOOP PRINCIPAL 
    while (1) {
        int bytes = recv(fd, &pkt, sizeof(pkt), MSG_WAITALL);
        if (bytes <= 0) break; // Desconexión abrupta

        // Actualizar timestamp de actividad
        pthread_mutex_lock(&mutex_lista);
        for (int i = 0; i < num_clientes; i++) {
            if (lista[i].sockfd == fd) {
                lista[i].ultimo_mensaje = time(NULL);
                if (strcmp(lista[i].status, STATUS_INACTIVO) == 0)
                    strncpy(lista[i].status, STATUS_ACTIVO, 15);
                break;
            }
        }
        pthread_mutex_unlock(&mutex_lista);

        // Despachar según comando
        switch (pkt.command) {
            case CMD_BROADCAST: cmd_broadcast(&pkt);        break;
            case CMD_DIRECT:    cmd_direct(&pkt);           break;
            case CMD_LIST:      cmd_list(&pkt, fd);         break;
            case CMD_INFO:      cmd_info(&pkt, fd);         break;
            case CMD_STATUS:    cmd_status(&pkt, fd);       break;
            case CMD_LOGOUT:
                enviar_paquete(fd, CMD_OK, "SERVER", pkt.sender, "Hasta luego");
                goto desconectar;
        }
    }

desconectar:
    // LIMPIEZA DE SESIÓN 
    liberar_cliente(fd);
    close(fd);
    return NULL;
}




void cmd_broadcast(ChatPacket *pkt) {
    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo) {
            enviar_paquete(lista[i].sockfd, CMD_MSG,
                           pkt->sender, "ALL", pkt->payload);
        }
    }
    pthread_mutex_unlock(&mutex_lista);
}

void cmd_direct(ChatPacket *pkt) {
    pthread_mutex_lock(&mutex_lista);
    int encontrado = 0;
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo && strcmp(lista[i].username, pkt->target) == 0) {
            enviar_paquete(lista[i].sockfd, CMD_MSG,
                           pkt->sender, pkt->target, pkt->payload);
            encontrado = 1;
            break;
        }
    }
    // Buscar fd del remitente para notificar el error
    if (!encontrado) {
        for (int i = 0; i < num_clientes; i++) {
            if (lista[i].activo && strcmp(lista[i].username, pkt->sender) == 0) {
                enviar_paquete(lista[i].sockfd, CMD_ERROR,
                               "SERVER", pkt->sender, "Destinatario no conectado");
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex_lista);
}


void cmd_list(ChatPacket *pkt, int fd) {
    char buffer[957] = {0};
    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo) {
            char entry[64];
            snprintf(entry, sizeof(entry), "%s,%s;",
                     lista[i].username, lista[i].status);
            strncat(buffer, entry, sizeof(buffer) - strlen(buffer) - 1);
        }
    }
    pthread_mutex_unlock(&mutex_lista);
    enviar_paquete(fd, CMD_USER_LIST, "SERVER", pkt->sender, buffer);
}

void cmd_info(ChatPacket *pkt, int fd) {
    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo && strcmp(lista[i].username, pkt->target) == 0) {
            char info[64];
            snprintf(info, sizeof(info), "%s,%s", lista[i].ip, lista[i].status);
            pthread_mutex_unlock(&mutex_lista);
            enviar_paquete(fd, CMD_USER_INFO, "SERVER", pkt->sender, info);
            return;
        }
    }
    pthread_mutex_unlock(&mutex_lista);
    enviar_paquete(fd, CMD_ERROR, "SERVER", pkt->sender, "Usuario no conectado");
}


void cmd_status(ChatPacket *pkt, int fd) {
    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].activo && strcmp(lista[i].username, pkt->sender) == 0) {
            strncpy(lista[i].status, pkt->payload, 15);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_lista);
    enviar_paquete(fd, CMD_OK, "SERVER", pkt->sender, pkt->payload);
}

void liberar_cliente(int fd) {
    char username[32] = {0};

    pthread_mutex_lock(&mutex_lista);
    for (int i = 0; i < num_clientes; i++) {
        if (lista[i].sockfd == fd && lista[i].activo) {
            strncpy(username, lista[i].username, 31);
            lista[i].activo = 0;
            printf("[-] Cliente desconectado: %s\n", username);
            break;
        }
    }
    // Notificar a todos los demás
    if (strlen(username) > 0) {
        for (int i = 0; i < num_clientes; i++) {
            if (lista[i].activo) {
                enviar_paquete(lista[i].sockfd, CMD_DISCONNECTED,
                               "SERVER", "ALL", username);
            }
        }
    }
    pthread_mutex_unlock(&mutex_lista);
}

void *monitor_inactividad(void *arg) {
    while (1) {
        sleep(10); // Revisar cada 10 segundos
        time_t ahora = time(NULL);

        pthread_mutex_lock(&mutex_lista);
        for (int i = 0; i < num_clientes; i++) {
            if (lista[i].activo &&
                strcmp(lista[i].status, STATUS_INACTIVO) != 0 &&
                (ahora - lista[i].ultimo_mensaje) >= INACTIVITY_TIMEOUT) {

                strncpy(lista[i].status, STATUS_INACTIVO, 15);
                // Notificar al cliente
                enviar_paquete(lista[i].sockfd, CMD_MSG,
                               "SERVER", lista[i].username,
                               "Tu status cambió a INACTIVE");
                printf("[~] %s marcado INACTIVE por inactividad\n", lista[i].username);
            }
        }
        pthread_mutex_unlock(&mutex_lista);
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        exit(1);
    }

    int puerto = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Permite reusar el puerto si el servidor se reinicia
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(puerto);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("Servidor escuchando en puerto %d...\n", puerto);

    // Thread de inactividad
    pthread_t tid_inactividad;
    pthread_create(&tid_inactividad, NULL, monitor_inactividad, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);

        // Lanza un thread por cliente
        pthread_t tid;
        pthread_create(&tid, NULL, manejar_cliente, client_fd);
        pthread_detach(tid); // El thread se limpia solo al terminar
    }
    return 0;
}
