// =======================================================
// ARCHIVO: hilo_trabajador.c
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
#include <time.h>

#define TAMANO_BUFFER 4096
#define MAX_PAGINAS 3 // Tamaño del buffer modificado a 3
#define PAGINAS_DIR "paginas/"
// Se define el nuevo directorio para las imágenes
#define IMAGENES_DIR "imagenes/"

// --- Definiciones de estructuras y funciones compartidas para la compilación ---
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
    int cliente_fd;
    BufferPaginas *buffer_paginas;
} TrabajadorArgs;

// Declara la variable global
extern BufferPaginas buffer_global;

// Declaraciones de funciones
char *obtener_pagina(BufferPaginas *buffer, const char *ruta, size_t *tam_out);
void registrar_conexion(const char *ip, int puerto, const char *pagina_solicitada);
void imprimir_buffer(BufferPaginas *buffer); // Declaración de la nueva función

// -----------------------------------------------------------------------------------

void *hilo_trabajador(void *arg) {
    TrabajadorArgs *args = (TrabajadorArgs *)arg;
    int cliente_fd = args->cliente_fd;
    BufferPaginas *buffer_global = args->buffer_paginas;
    char buffer_peticion[TAMANO_BUFFER];
    
    struct sockaddr_in direccion_cliente;
    socklen_t longitud_cliente = sizeof(direccion_cliente);
    getpeername(cliente_fd, (struct sockaddr *)&direccion_cliente, &longitud_cliente);
    char *ip_cliente = inet_ntoa(direccion_cliente.sin_addr);
    int puerto_cliente = ntohs(direccion_cliente.sin_port);

    // [TRABAJADOR] Hilo iniciado para atender a 127.0.0.1:49874
    printf("[TRABAJADOR %p] Hilo iniciado para atender a %s:%d\n", (void*)pthread_self(), ip_cliente, puerto_cliente);

    ssize_t bytes_recibidos = recv(cliente_fd, buffer_peticion, sizeof(buffer_peticion) - 1, 0);
    if (bytes_recibidos > 0) {
        buffer_peticion[bytes_recibidos] = '\0';
        char *linea_peticion = strtok(buffer_peticion, "\n");
        char metodo[10], ruta[256];
        sscanf(linea_peticion, "%s %s", metodo, ruta);

        // [TRABAJADOR] Petición para '/index.html' recibida desde 127.0.0.1:49874
        printf("[TRABAJADOR %p] Petición para '%s' recibida desde %s:%d\n", (void*)pthread_self(), ruta, ip_cliente, puerto_cliente);

        size_t tam_contenido = 0;
        char *contenido = obtener_pagina(buffer_global, ruta, &tam_contenido);

        if (contenido) {
            const char *extension = strrchr(ruta, '.');
            char *content_type = "text/html";

            if (extension) {
                if (strcmp(extension, ".css") == 0)
                    content_type = "text/css";
                else if (strcmp(extension, ".js") == 0)
                    content_type = "application/javascript";
                else if (strcmp(extension, ".png") == 0)
                    content_type = "image/png";
                else if (strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0)
                    content_type = "image/jpeg";
                else if (strcmp(extension, ".gif") == 0)
                    content_type = "image/gif";
            }

            char encabezado[512];
            snprintf(encabezado, sizeof(encabezado),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     content_type, tam_contenido);

            send(cliente_fd, encabezado, strlen(encabezado), 0);
            send(cliente_fd, contenido, tam_contenido, 0);

            // [TRABAJADOR] Recurso 'index.html' servido con éxito. Estado 200 OK.
            printf("[TRABAJADOR %p] Recurso '%s' servido con éxito. Estado 200 OK.\n", (void*)pthread_self(), ruta);

            // --- Lógica para registrar solo páginas HTML ---
            if (extension && (strcmp(extension, ".html") == 0)) {
                registrar_conexion(ip_cliente, puerto_cliente, ruta);
            }

            // Se imprime el estado del buffer después de servir la página
            imprimir_buffer(buffer_global);

        } else {
            printf("[TRABAJADOR %p] Recurso '%s' NO encontrado. Sirviendo error 404.\n", (void*)pthread_self(), ruta);
            char *error_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
            send(cliente_fd, error_404, strlen(error_404), 0);
        }
    }

    close(cliente_fd);
    free(args);
    // [TRABAJADOR] Hilo finalizado. Conexión con 127.0.0.1:49874 cerrada.
    printf("[TRABAJADOR %p] Hilo finalizado. Conexión con %s:%d cerrada.\n", (void*)pthread_self(), ip_cliente, puerto_cliente);
    return NULL;
}
