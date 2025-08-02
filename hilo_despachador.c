// =======================================================
// ARCHIVO: hilo_despachador.c (CORREGIDO)
// =======================================================
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#define MAX_CLIENTES 100
#define MAX_PAGINAS 3

// --- Declaraciones y definiciones necesarias para funcionar sin .h ---
extern volatile sig_atomic_t servidor_corriendo;

typedef struct {
    char *nombre_archivo;
    char *contenido;
    size_t tamano;
    long long ultimo_acceso;
} Pagina;

typedef struct {
    Pagina paginas[MAX_PAGINAS];
    int num_paginas;
    pthread_mutex_t mutex;
} BufferPaginas;

typedef struct {
    int servidor_fd;
    BufferPaginas *buffer_paginas;
} ServidorArgs;

typedef struct {
    int cliente_fd;
    BufferPaginas *buffer_paginas;
} TrabajadorArgs;

void *hilo_trabajador(void *arg);

// ---------------------------------------------------------------------

void *hilo_despachador(void *arg) {
    ServidorArgs *servidor_args = (ServidorArgs *)arg;
    int servidor_fd = servidor_args->servidor_fd;
    BufferPaginas *buffer_paginas = servidor_args->buffer_paginas;

    printf("[DESPACHADOR %p] Hilo despachador iniciado, esperando conexiones...\n", (void*)pthread_self());

    while (servidor_corriendo) {
        struct sockaddr_in direccion_cliente;
        socklen_t longitud_cliente = sizeof(direccion_cliente);
        int cliente_fd = accept(servidor_fd, (struct sockaddr *)&direccion_cliente, &longitud_cliente);

        if (cliente_fd < 0) {
            if (!servidor_corriendo || errno == EINVAL) {
                // EINVAL se puede recibir si el socket ya se ha cerrado.
                printf("[DESPACHADOR %p] El socket del servidor ha sido cerrado. Terminando el hilo despachador.\n", (void*)pthread_self());
                break;
            }
            perror("[DESPACHADOR] Error al aceptar conexion");
            continue;
        }

        pthread_t trabajador;
        TrabajadorArgs *args_trabajador = malloc(sizeof(TrabajadorArgs));
        if (!args_trabajador) {
            perror("[DESPACHADOR] Error reservando memoria para hilo trabajador");
            close(cliente_fd);
            continue;
        }
        
        args_trabajador->cliente_fd = cliente_fd;
        args_trabajador->buffer_paginas = buffer_paginas;

        printf("[DESPACHADOR %p] Petición recibida. Creando hilo trabajador para atenderla.\n", (void*)pthread_self());
        if (pthread_create(&trabajador, NULL, hilo_trabajador, args_trabajador) != 0) {
            perror("[DESPACHADOR] Error al crear hilo trabajador");
            close(cliente_fd);
            free(args_trabajador);
        } else {
            pthread_detach(trabajador);
        }
    }
    
    printf("[DESPACHADOR %p] Hilo despachador finalizado.\n", (void*)pthread_self());
    return NULL;
}
