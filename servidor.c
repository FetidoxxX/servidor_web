#include "servidor_web.h"

//variables globales
volatile sig_atomic_t servidor_corriendo = 1;
int servidor_fd_global;
BufferPaginas buffer_global;

int main(int argc, char *argv[]) {
    int puerto_escucha = PUERTO_DEFECTO;
    const char *ip_escucha = IP_DEFECTO;
    
    //argumento puerto.
    if (argc > 1) {
        puerto_escucha = atoi(argv[1]);
        if (puerto_escucha <= 0) {
            fprintf(stderr, "[SERVIDOR] Uso: %s [puerto] [ip]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    //argumento IP.
    if (argc > 2) {
        ip_escucha = argv[2];
    }
    
    printf("[SERVIDOR] Iniciando servidor web...\n");

    // Configurar la señal SIGINT para un cierre limpio.
    struct sigaction sa;
    sa.sa_handler = cerrar_servidor;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    //Creacion del socket
    servidor_fd_global = socket(AF_INET, SOCK_STREAM, 0);
    if (servidor_fd_global < 0) {
        perror("[SERVIDOR] Error creando el socket");
        exit(EXIT_FAILURE);
    }
    
    // se Configura el socket para la reutilización de la dirección del puerto
    int optval = 1;
    if (setsockopt(servidor_fd_global, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("[SERVIDOR] Error al configurar setsockopt");
        close(servidor_fd_global);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in servidor_addr;
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(puerto_escucha);
    
    int pton_result = inet_pton(AF_INET, ip_escucha, &servidor_addr.sin_addr);
    if (pton_result <= 0) {
        if (pton_result == 0) {
            fprintf(stderr, "[SERVIDOR] Dirección IP inválida: La cadena '%s' no es una dirección IP válida.\n", ip_escucha);
        } else {
            perror("[SERVIDOR] Error en inet_pton");
        }
        close(servidor_fd_global);
        exit(EXIT_FAILURE);
    }
    
    if (bind(servidor_fd_global, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("[SERVIDOR] Error en bind");
        close(servidor_fd_global);
        exit(EXIT_FAILURE);
    }
    
    if (listen(servidor_fd_global, MAX_CONEXIONES) < 0) {
        perror("[SERVIDOR] Error en listen");
        close(servidor_fd_global);
        exit(EXIT_FAILURE);
    }
    
    printf("[SERVIDOR] Servidor escuchando en %s:%d...\n", ip_escucha, puerto_escucha);
    printf("[SERVIDOR] Esperando conexiones...\n");
    
    inicializar_buffer(&buffer_global);
    
    ServidorArgs args;
    args.servidor_fd = servidor_fd_global;
    args.buffer_paginas = &buffer_global;
    
    pthread_t despachador;
    if (pthread_create(&despachador, NULL, hilo_despachador, &args) != 0) {
        perror("[SERVIDOR] Error creando hilo despachador");
        // Los hilos se cierran cuando el proceso termina.
        close(servidor_fd_global);
        exit(EXIT_FAILURE);
    }
    
    while(servidor_corriendo) {
        sleep(1);
    }
    printf("[SERVIDOR] Servidor apagado limpiamente.\n");
    return 0;
}

// funcion de señales para cerrar el servidor
void cerrar_servidor(int signum) {
    printf("\n[SERVIDOR] Señal de terminación recibida. Cerrando el servidor...\n");
    servidor_corriendo = 0;
    //Liberar la memoria del buffer de páginas
    liberar_buffer(&buffer_global);
    //Usar shutdown() para desbloquear accept() de forma segura
    if (servidor_fd_global != -1) {
        shutdown(servidor_fd_global, SHUT_RDWR);
        close(servidor_fd_global);
    }
}