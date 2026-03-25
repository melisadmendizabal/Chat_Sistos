# Sistema de Chat Cliente-Servidor en C

Este proyecto implementa un sistema de chat basado en el modelo cliente-servidor utilizando sockets en C y concurrencia mediante hilos. El sistema permite la comunicación entre múltiples usuarios en tiempo real, incluyendo mensajes globales, mensajes privados y consultas de información.

El sistema está compuesto por:

- Un servidor que gestiona múltiples clientes simultáneamente
- Un cliente que actúa como interfaz de usuario
- Un protocolo de comunicación estructurado
- Un script en Python para pruebas del protocolo


## Arquitectura

El sistema sigue el modelo cliente-servidor, donde:

El servidor administra conexiones, usuarios y mensajes
Los clientes envían comandos y reciben respuestas
La comunicación se realiza mediante TCP (SOCK_STREAM)

Cada cliente se maneja en un hilo independiente en el servidor, permitiendo múltiples conexiones simultáneas.

## Protocolo de Comunicación

La comunicación se basa en una estructura fija llamada ChatPacket de 1024 bytes.

Estructura:
command → tipo de mensaje
payload_len → longitud del mensaje
sender → usuario emisor
target → usuario destino
payload → contenido del mensaje

Este diseño permite una comunicación eficiente y estructurada entre cliente y servidor

## Compilación
El proyecto incluye un Makefile para compilar el servidor:

```
make
```

Esto generará el ejecutable:

```
./servidor
```

Para limpiar archivos compilados:
```
make clean
```


## Ejecución
### Servidor

```
./servidor <puerto>
```


### Cliente

```
gcc -o cliente cliente.c -lpthread
./cliente <username> <IP_servidor> <puerto>
```

## Funcionalidades del Servidor
- Registro de usuarios con validación de duplicados
- Manejo de múltiples clientes con hilos (pthread)
- Broadcast de mensajes
- Mensajes privados
- Listado de usuarios activos
- Consulta de información (IP y estado)
- Cambio de estado del usuario
- Detección de inactividad automática
- Notificación de desconexión


## Funcionalidades del Cliente
El cliente utiliza multihilos, permitiendo enviar comandos y recibir mensajes simultáneamente.
- /broadcast <mensaje>	Enviar mensaje a todos
- /msg <usuario> <mensaje>	Enviar mensaje privado
- /list	Ver usuarios conectados
- /info <usuario>	Ver información de usuario
- /status <estado>	Cambiar estado (ACTIVE, BUSY, INACTIVE)
- /help	Mostrar ayuda
- /exit	Salir


### Concurrencia

El sistema utiliza hilos para manejar múltiples clientes simultáneamente, escuchar mensajes sin bloquear la ejecución y monitorear la inactividad de usuarios
Se emplea un mutex (pthread_mutex) para proteger el acceso a la lista de clientes y evitar condiciones de carrera.

### Manejo de Inactividad

El servidor incluye un hilo adicional que revisa periódicamente la actividad de los usuarios, luego cambia el estado a INACTIVE si no hay actividad en cierto tiempo y notifica al usuario sobre el cambio de estado


## Limitaciones
No incluye cifrado de datos
No hay autenticación avanzada de usuarios
El protocolo depende de tamaños fijos
No maneja reconexión automática

