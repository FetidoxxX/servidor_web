// ====================================================================
// ARCHIVO: servidor_web.h
// DESCRIPCIÓN: Archivo de encabezado que centraliza las definiciones
//              compartidas para todos los módulos del servidor.
// ====================================================================

#ifndef SERVIDOR_WEB_H
#define SERVIDOR_WEB_H

// Inclusiones de librerías estándar necesarias
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

// --- Constantes globales para el servidor ---
#define PUERTO_DEFECTO 8000           // Puerto por defecto para la escucha del servidor
#define IP_DEFECTO "0.0.0.0"          // IP por defecto para escuchar en todas las interfaces
#define MAX_CONEXIONES 10             // Número máximo de conexiones pendientes en la cola de listen
#define MAX_PAGINAS 3                 // Tamaño del buffer de páginas (modificado a 3)
#define PAGINAS_DIR "paginas/"        // Directorio donde se almacenan los archivos HTML
#define IMAGENES_DIR "imagenes/"      // Directorio para los archivos de imagen
#define LOG_ARCHIVO "log_conexiones.txt" // Archivo donde se registran las conexiones
#define TAMANO_BUFFER 4096            // Tamaño del buffer para las peticiones HTTP

// --- Estructuras de datos compartidas ---

// Estructura que representa una página en el buffer
typedef struct {
    char *nombre_archivo;
    char *contenido;
    size_t tamano;
    time_t ultimo_acceso; // timestamp del último acceso
} Pagina;

// Estructura que representa el buffer de páginas en memoria
typedef struct {
    Pagina paginas[MAX_PAGINAS];
    int num_paginas;
    pthread_mutex_t mutex; // Mutex para proteger el acceso concurrente al buffer
} BufferPaginas;

// Estructura para pasar argumentos al hilo despachador
typedef struct {
    int servidor_fd;
    BufferPaginas *buffer_paginas;
} ServidorArgs;

// Estructura para pasar argumentos al hilo trabajador
typedef struct {
    int cliente_fd;
    BufferPaginas *buffer_paginas;
} TrabajadorArgs;

// --- Variables globales (declaraciones) ---
extern volatile sig_atomic_t servidor_corriendo;
extern int servidor_fd_global;
extern BufferPaginas buffer_global;
extern pthread_mutex_t mutex_log;

// --- Declaraciones de funciones compartidas ---
void inicializar_buffer(BufferPaginas *buffer);
char *obtener_pagina(BufferPaginas *buffer, const char *ruta, size_t *tam_out);
void imprimir_buffer(BufferPaginas *buffer);
void registrar_conexion(const char *ip, int puerto, const char *pagina_solicitada);
void *hilo_despachador(void *arg);
void *hilo_trabajador(void *arg);
void cerrar_servidor(int signum);
void liberar_buffer(BufferPaginas *buffer);
#endif // SERVIDOR_WEB_H
