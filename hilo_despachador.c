// =======================================================
// ARCHIVO: hilo_despachador.c
// =======================================================
#include "servidor_web.h"

void *hilo_despachador(void *arg) {
    ServidorArgs *servidor_args = (ServidorArgs *)arg;
    int servidor_fd = servidor_args->servidor_fd;
    BufferPaginas *buffer_paginas = servidor_args->buffer_paginas;

    // [CICLO DE VIDA] El hilo despachador se inicia.
    printf("[DESPACHADOR %p] Hilo despachador iniciado, esperando conexiones...\n", (void*)pthread_self());

    while (servidor_corriendo) {
        struct sockaddr_in direccion_cliente;
        socklen_t longitud_cliente = sizeof(direccion_cliente);
        int cliente_fd = accept(servidor_fd, (struct sockaddr *)&direccion_cliente, &longitud_cliente);

        if (cliente_fd < 0) {
            if (!servidor_corriendo || errno == EINVAL) {
                // EINVAL se puede recibir si el socket ya se ha cerrado.
                // [CICLO DE VIDA] El hilo despachador termina porque el servidor se ha detenido.
                printf("[DESPACHADOR %p] El socket del servidor ha sido cerrado. Terminando el hilo despachador.\n", (void*)pthread_self());
                break;
            }
            perror("[DESPACHADOR] Error al aceptar conexion");
            continue;
        }
        
        // [CICLO DE VIDA] El hilo despachador acepta una nueva conexión.
        printf("[DESPACHADOR %p] Conexión aceptada. Se creará un hilo trabajador para atenderla.\n", (void*)pthread_self());

        pthread_t trabajador;
        TrabajadorArgs *args_trabajador = malloc(sizeof(TrabajadorArgs));
        if (!args_trabajador) {
            perror("[DESPACHADOR] Error reservando memoria para hilo trabajador");
            close(cliente_fd);
            continue;
        }
        
        args_trabajador->cliente_fd = cliente_fd;
        args_trabajador->buffer_paginas = buffer_paginas;

        // [CICLO DE VIDA] El hilo despachador crea un hilo trabajador.
        printf("[DESPACHADOR %p] Creando hilo trabajador para atender la petición.\n", (void*)pthread_self());
        if (pthread_create(&trabajador, NULL, hilo_trabajador, args_trabajador) != 0) {
            perror("[DESPACHADOR] Error al crear hilo trabajador");
            close(cliente_fd);
            free(args_trabajador);
        } else {
            pthread_detach(trabajador);
        }
    }
    
    // [CICLO DE VIDA] El hilo despachador finaliza su ejecución.
    printf("[DESPACHADOR %p] Hilo despachador finalizado.\n", (void*)pthread_self());
    return NULL;
}
