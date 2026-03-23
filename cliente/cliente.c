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
 * FUNCIÓN: main
 * ================================================================
 * Aquí empieza el programa.
 */
int main(int argc, char *argv[])
{
    /* ── Paso 0: Verificar argumentos ── */
    /*
     * argc es la cantidad de argumentos que recibió el programa.
     * argv[0] = nombre del programa ("./cliente")
     * argv[1] = username
     * argv[2] = IP del servidor
     * argv[3] = puerto
     * 
     * Si no recibimos exactamente 3 argumentos (más el nombre = 4), error.
     */
    if (argc != 4) {
        printf("Uso: %s <username> <IP_servidor> <puerto>\n", argv[0]);
        printf("Ejemplo: %s alice 127.0.0.1 5000\n", argv[0]);
        exit(1); /* Salimos con código de error */
    }

    /* Guardamos los argumentos en variables con nombres claros */
    const char *username   = argv[1];
    const char *ip_servidor = argv[2];
    int         puerto     = atoi(argv[3]); /* atoi convierte texto a número */

    printf("=== Cliente de Chat ===\n");
    printf("Usuario  : %s\n", username);
    printf("Servidor : %s:%d\n", ip_servidor, puerto);
    printf("\n");


    /* ── Paso 1: Crear el socket ── */
    /*
     * Un socket es como crear un "teléfono" antes de llamar a alguien.
     * 
     * AF_INET    = usamos IPv4 (direcciones tipo 192.168.x.x)
     * SOCK_STREAM = usamos TCP (conexión confiable, los datos llegan en orden)
     * 0          = protocolo por defecto (TCP en este caso)
     * 
     * Retorna un número entero (el "file descriptor" del socket).
     * Si retorna -1, algo salió mal.
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error al crear el socket"); /* perror imprime el error del sistema */
        exit(1);
    }
    printf("[OK] Socket creado.\n");


    /* ── Paso 2: Preparar la dirección del servidor ── */
    /*
     * Necesitamos decirle al socket A DÓNDE conectarse.
     * Usamos una estructura llamada sockaddr_in para eso.
     */
    struct sockaddr_in direccion_servidor;
    memset(&direccion_servidor, 0, sizeof(direccion_servidor)); /* limpiar */

    direccion_servidor.sin_family = AF_INET;  /* IPv4 */
    direccion_servidor.sin_port   = htons(puerto);
    /*
     * htons = "host to network short"
     * Los números en la red viajan en un orden de bytes específico
     * (big-endian). htons convierte el número del formato de tu
     * computadora al formato de red. Siempre hay que usarlo con puertos.
     */

    /* inet_pton convierte el string "192.168.1.10" al formato binario */
    int resultado = inet_pton(AF_INET, ip_servidor, &direccion_servidor.sin_addr);
    if (resultado <= 0) {
        printf("Error: IP '%s' no es válida.\n", ip_servidor);
        close(sockfd);
        exit(1);
    }


    /* ── Paso 3: Conectarse al servidor ── */
    /*
     * connect() intenta establecer la conexión TCP con el servidor.
     * Es como "marcar el número" en el teléfono.
     * 
     * Si el servidor no está corriendo, esto fallará.
     */
    printf("[...] Conectando a %s:%d...\n", ip_servidor, puerto);

    if (connect(sockfd, (struct sockaddr *)&direccion_servidor,
                sizeof(direccion_servidor)) == -1) {
        perror("Error al conectar con el servidor");
        close(sockfd);
        exit(1);
    }

    printf("[OK] ¡Conectado al servidor!\n\n");


    /* ── Paso 4: Registrarse ── */
    /*
     * Ahora que estamos conectados, el servidor no sabe quiénes somos.
     * Tenemos que mandarle un paquete CMD_REGISTER con nuestro username.
     * 
     * Según el protocolo:
     *   sender  = nuestro username
     *   target  = vacío
     *   payload = nuestro username (lo manda dos veces por diseño del protocolo)
     */
    printf("[...] Registrando usuario '%s'...\n", username);

    ChatPacket pkt_registro;
    crear_paquete(&pkt_registro, CMD_REGISTER, username, "", username);

    /*
     * send() manda los bytes del paquete por el socket.
     * sizeof(pkt_registro) = 1024 (siempre mandamos 1024 bytes completos)
     */
    ssize_t bytes_enviados = send(sockfd, &pkt_registro, sizeof(pkt_registro), 0);
    if (bytes_enviados == -1) {
        perror("Error al enviar el registro");
        close(sockfd);
        exit(1);
    }
    printf("[OK] Paquete de registro enviado (%zd bytes).\n", bytes_enviados);


    /* ── Paso 5: Esperar respuesta del servidor ── */
    /*
     * El servidor nos va a responder con CMD_OK o CMD_ERROR.
     * Usamos recv() para recibir el paquete de respuesta.
     * 
     * MSG_WAITALL le dice a recv que espere hasta tener los 1024 bytes completos.
     */
    ChatPacket pkt_respuesta;
    memset(&pkt_respuesta, 0, sizeof(pkt_respuesta));

    printf("[...] Esperando respuesta del servidor...\n");

    ssize_t bytes_recibidos = recv(sockfd, &pkt_respuesta, sizeof(pkt_respuesta), MSG_WAITALL);

    if (bytes_recibidos <= 0) {
        /* 0 significa que el servidor cerró la conexión */
        /* -1 significa error de red */
        printf("Error: el servidor cerró la conexión o hubo un error de red.\n");
        close(sockfd);
        exit(1);
    }

    /* Ahora revisamos qué nos respondió el servidor */
    if (pkt_respuesta.command == CMD_OK) {
        printf("\n¡¡ Registro exitoso !!\n");
        printf("Servidor dice: %s\n", pkt_respuesta.payload);
        printf("\nYa estás conectado como '%s'. ¡Listo para chatear!\n", username);

    } else if (pkt_respuesta.command == CMD_ERROR) {
        printf("\nError de registro: %s\n", pkt_respuesta.payload);
        printf("Cierra el programa e intenta con otro username.\n");
        close(sockfd);
        exit(1);

    } else {
        /* El servidor mandó algo inesperado */
        printf("Respuesta inesperada del servidor (command=%d)\n", pkt_respuesta.command);
        close(sockfd);
        exit(1);
    }


    /* ── Por ahora el programa termina aquí ── */
    /*
     * En la siguiente etapa, en lugar de terminar, aquí arrancaremos:
     *   - Un thread que reciba mensajes del servidor
     *   - Un loop que lea lo que escribe el usuario
     */
    printf("\n(Por ahora el programa termina aquí. ¡Siguiente paso: chatear!)\n");

    close(sockfd); /* Cerramos el socket limpiamente */
    return 0;
}