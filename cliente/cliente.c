/*
 * cliente.c
 * 
 * Por ahora este programa solo hace DOS cosas:
 *   1. Conectarse al servidor
 *   2. Registrarse con un username
 *
 * Cómo compilar:
 *   gcc -o cliente cliente.c -lpthread
 *
 * Cómo ejecutar:
 *   ./cliente <username> <IP_servidor> <puerto>
 *
 * Ejemplo:
 *   ./cliente alice 127.0.0.1 5000
 */

/* ── Librerías necesarias ── */
#include <stdio.h>      /* printf, perror */
#include <stdlib.h>     /* exit */
#include <string.h>     /* memset, strncpy, strlen */
#include <unistd.h>     /* close */

/* Librerías de red (sockets) */
#include <sys/socket.h> /* socket, connect, send, recv */
#include <arpa/inet.h>  /* inet_pton, htons — para convertir IPs y puertos */

#include "protocolo.h"  /* Nuestro struct ChatPacket y los CMD_* */

/* ================================================================
 * VARIABLE GLOBAL: el socket
 * ================================================================
 * Tanto el hilo principal como el hilo receptor necesitan usar
 * el mismo socket. Lo hacemos global para que ambos lo vean.
 *
 * "volatile" le dice al compilador que esta variable puede cambiar
 * en cualquier momento (desde otro hilo), no la cachee.
 */
volatile int sockfd_global = -1;

/* ================================================================
 * FUNCIÓN: crear_paquete
 * ================================================================
 * Esta función "llena" un ChatPacket con los datos que le pasemos.
 * Es un ayudante para no repetir código cada vez que queramos
 * armar un paquete.
 *
 * Parámetros:
 *   pkt      — puntero al paquete que vamos a llenar
 *   command  — qué tipo de mensaje es (CMD_REGISTER, CMD_BROADCAST, etc.)
 *   sender   — quién manda el mensaje (tu username)
 *   target   — a quién va (vacío si es para todos o no aplica)
 *   payload  — el contenido del mensaje
 */
void crear_paquete(ChatPacket *pkt, uint8_t command,
                   const char *sender, const char *target, const char *payload)
{
    /* Primero ponemos TODO en cero (limpiamos basura de memoria) */
    memset(pkt, 0, sizeof(ChatPacket));

    /* Llenamos cada campo */
    pkt->command = command;

    /* strncpy copia la cadena pero con límite para no pasarnos del tamaño */
    /* El -1 es para siempre dejar espacio para el '\0' (fin de cadena) */
    if (sender)  strncpy(pkt->sender,  sender,  sizeof(pkt->sender)  - 1);
    if (target)  strncpy(pkt->target,  target,  sizeof(pkt->target)  - 1);
    if (payload) strncpy(pkt->payload, payload, sizeof(pkt->payload) - 1);

    /* Guardamos cuántos bytes válidos tiene el payload */
    pkt->payload_len = payload ? (uint16_t)strlen(payload) : 0;
}


/* ================================================================
 * FUNCIÓN: hilo_receptor
 * ================================================================
 * Corre en su PROPIO HILO. Solo escucha mensajes del servidor
 * e imprime lo que llega. Nunca escribe al servidor.
 */
void *hilo_receptor(void *arg)
{
    (void)arg; /* Silencia el warning de parámetro no usado */
 
    ChatPacket pkt;
 
    while (1) {
        memset(&pkt, 0, sizeof(pkt));
 
        /*
         * recv() se queda esperando hasta que lleguen 1024 bytes.
         * Si retorna 0 o negativo, el servidor cerró la conexión.
         */
        ssize_t n = recv(sockfd_global, &pkt, sizeof(pkt), MSG_WAITALL);
 
        if (n <= 0) {
            printf("\n[!] Conexión con el servidor perdida.\n");
            exit(1);
        }
 
        /* Procesar según el tipo de mensaje */
        switch (pkt.command) {
 
            case CMD_MSG:
                /* Mensaje de chat (broadcast o privado) */
                if (strcmp(pkt.target, "ALL") == 0) {
                    printf("\n[Todos] %s: %s\n", pkt.sender, pkt.payload);
                } else {
                    printf("\n[Privado de %s]: %s\n", pkt.sender, pkt.payload);
                }
                break;
 
            case CMD_OK:
                printf("\n[OK] %s\n", pkt.payload);
                break;
 
            case CMD_ERROR:
                printf("\n[Error] %s\n", pkt.payload);
                break;
 
            case CMD_USER_LIST:
                /*
                 * Respuesta a /list.
                 * Formato del payload: "alice,ACTIVE;bob,BUSY;carlos,INACTIVE"
                 * Separamos por ";" e imprimimos cada entrada.
                 */
                printf("\n--- Usuarios conectados ---\n");
                char lista_copia[957];
                strncpy(lista_copia, pkt.payload, sizeof(lista_copia) - 1);
                char *entrada = strtok(lista_copia, ";");
                while (entrada != NULL) {
                    printf("  - %s\n", entrada);
                    entrada = strtok(NULL, ";");
                }
                printf("---------------------------\n");
                break;
 
            case CMD_USER_INFO:
                /* Respuesta a /info. Payload = "IP,STATUS" */
                printf("\n--- Info de usuario ---\n");
                printf("  %s\n", pkt.payload);
                printf("-----------------------\n");
                break;
 
            case CMD_DISCONNECTED:
                /* El servidor avisa que alguien se fue */
                printf("\n[!] '%s' se ha desconectado.\n", pkt.payload);
                break;
 
            default:
                printf("\n[?] Mensaje desconocido (cmd=%d)\n", pkt.command);
                break;
        }
 
        /* Reimprimir el prompt para que el usuario sepa que puede escribir */
        printf("> ");
        fflush(stdout);
    }
 
    return NULL;
}
 
/* ================================================================
 * FUNCIÓN: mostrar_ayuda
 * ================================================================
 */
void mostrar_ayuda()
{
    printf("\n=== Comandos disponibles ===\n");
    printf("  /broadcast <mensaje>              Enviar a todos\n");
    printf("  /msg <usuario> <mensaje>          Mensaje privado\n");
    printf("  /list                             Ver usuarios conectados\n");
    printf("  /info <usuario>                   Ver info de un usuario\n");
    printf("  /status <ACTIVE|BUSY|INACTIVE>    Cambiar tu estado\n");
    printf("  /help                             Ver esta ayuda\n");
    printf("  /exit                             Salir\n");
    printf("============================\n\n");
}
 


/* ================================================================
 * FUNCIÓN: main
 * ================================================================
 * Aquí empieza el programa.
 */
int main(int argc, char *argv[])
{
    /* Verificar argumentos */
    if (argc != 4) {
        printf("Uso: %s <username> <IP_servidor> <puerto>\n", argv[0]);
        exit(1);
    }
 
    const char *username    = argv[1];
    const char *ip_servidor = argv[2];
    int         puerto      = atoi(argv[3]);
 
    /* Crear socket */
    sockfd_global = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_global == -1) { perror("socket"); exit(1); }
 
    /* Preparar dirección del servidor */
    struct sockaddr_in dir_servidor;
    memset(&dir_servidor, 0, sizeof(dir_servidor));
    dir_servidor.sin_family = AF_INET;
    dir_servidor.sin_port   = htons(puerto);
    if (inet_pton(AF_INET, ip_servidor, &dir_servidor.sin_addr) <= 0) {
        printf("IP inválida: %s\n", ip_servidor);
        exit(1);
    }
 
    /* Conectarse */
    printf("Conectando a %s:%d...\n", ip_servidor, puerto);
    if (connect(sockfd_global, (struct sockaddr *)&dir_servidor,
                sizeof(dir_servidor)) == -1) {
        perror("connect");
        exit(1);
    }
    printf("[OK] Conectado.\n");
 
    /* Registrarse */
    ChatPacket pkt;
    crear_paquete(&pkt, CMD_REGISTER, username, "", username);
    send(sockfd_global, &pkt, sizeof(pkt), 0);
 
    memset(&pkt, 0, sizeof(pkt));
    recv(sockfd_global, &pkt, sizeof(pkt), MSG_WAITALL);
 
    if (pkt.command == CMD_OK) {
        printf("[OK] Registrado como '%s'.\n", username);
        printf("Escribe /help para ver los comandos.\n\n");
    } else {
        printf("[Error] %s\n", pkt.payload);
        close(sockfd_global);
        exit(1);
    }
 
    /* ── Crear el hilo receptor ── */
    /*
     * A partir de aquí hay DOS hilos corriendo al mismo tiempo:
     *   1. Este mismo (main) — lee el teclado
     *   2. El nuevo hilo    — escucha al servidor
     */
    pthread_t tid;
    if (pthread_create(&tid, NULL, hilo_receptor, NULL) != 0) {
        perror("pthread_create");
        close(sockfd_global);
        exit(1);
    }
    pthread_detach(tid); /* Se limpia solo al terminar */
 
 
    /* ================================================================
     * LOOP PRINCIPAL: leer comandos del teclado
     * ================================================================
     */
    char linea[1024];
 
    while (1) {
        printf("> ");
        fflush(stdout);
 
        if (fgets(linea, sizeof(linea), stdin) == NULL) break;
 
        /* Quitar el '\n' del final */
        linea[strcspn(linea, "\n")] = '\0';
 
        if (strlen(linea) == 0) continue;
 
        /* /help */
        if (strcmp(linea, "/help") == 0) {
            mostrar_ayuda();
        }
 
        /* /exit */
        else if (strcmp(linea, "/exit") == 0) {
            crear_paquete(&pkt, CMD_LOGOUT, username, "", "");
            send(sockfd_global, &pkt, sizeof(pkt), 0);
            printf("¡Hasta luego!\n");
            break;
        }
 
        /* /list */
        else if (strcmp(linea, "/list") == 0) {
            crear_paquete(&pkt, CMD_LIST, username, "", "");
            send(sockfd_global, &pkt, sizeof(pkt), 0);
        }
 
        /* /broadcast <mensaje> */
        else if (strncmp(linea, "/broadcast ", 11) == 0) {
            const char *mensaje = linea + 11;
            if (strlen(mensaje) == 0) {
                printf("Uso: /broadcast <mensaje>\n");
            } else {
                crear_paquete(&pkt, CMD_BROADCAST, username, "", mensaje);
                send(sockfd_global, &pkt, sizeof(pkt), 0);
            }
        }
 
        /* /msg <usuario> <mensaje> */
        else if (strncmp(linea, "/msg ", 5) == 0) {
            char destinatario[32] = {0};
            sscanf(linea + 5, "%31s", destinatario);
 
            int len_prefijo = 5 + strlen(destinatario) + 1;
            const char *mensaje = linea + len_prefijo;
 
            if (strlen(destinatario) == 0 || strlen(mensaje) == 0) {
                printf("Uso: /msg <usuario> <mensaje>\n");
            } else {
                crear_paquete(&pkt, CMD_DIRECT, username, destinatario, mensaje);
                send(sockfd_global, &pkt, sizeof(pkt), 0);
            }
        }
 
        /* /info <usuario> */
        else if (strncmp(linea, "/info ", 6) == 0) {
            const char *usuario_consulta = linea + 6;
            if (strlen(usuario_consulta) == 0) {
                printf("Uso: /info <usuario>\n");
            } else {
                crear_paquete(&pkt, CMD_INFO, username, usuario_consulta, "");
                send(sockfd_global, &pkt, sizeof(pkt), 0);
            }
        }
 
        /* /status <ACTIVE|BUSY|INACTIVE> */
        else if (strncmp(linea, "/status ", 8) == 0) {
            const char *nuevo_status = linea + 8;
            if (strcmp(nuevo_status, "ACTIVE")   != 0 &&
                strcmp(nuevo_status, "BUSY")      != 0 &&
                strcmp(nuevo_status, "INACTIVE")  != 0) {
                printf("Status inválido. Usa: ACTIVE, BUSY o INACTIVE\n");
            } else {
                crear_paquete(&pkt, CMD_STATUS, username, "", nuevo_status);
                send(sockfd_global, &pkt, sizeof(pkt), 0);
            }
        }
 
        /* Comando desconocido */
        else {
            printf("Comando no reconocido. Escribe /help para ver los comandos.\n");
        }
    }
 
    close(sockfd_global);
    return 0;
}