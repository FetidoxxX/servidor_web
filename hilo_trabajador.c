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
// Declaración de la nueva función para imprimir el estado del buffer
void imprimir_buffer(BufferPaginas *buffer);

// ---------------------------------------------------------------------

void *hilo_trabajador(void *arg) {
    TrabajadorArgs *args = (TrabajadorArgs *)arg;
    int cliente_fd = args->cliente_fd;
    
    unsigned long id_hilo = (unsigned long)pthread_self();
    
    printf("[TRABAJADOR %lu] Hilo iniciado para atender la conexión.\n", id_hilo);

    char buffer_peticion[TAMANO_BUFFER];
    ssize_t bytes_recibidos = recv(cliente_fd, buffer_peticion, sizeof(buffer_peticion) - 1, 0);

    if (bytes_recibidos > 0) {
        buffer_peticion[bytes_recibidos] = '\0';

        char metodo[10], ruta[256], protocolo[20];
        sscanf(buffer_peticion, "%s %s %s", metodo, ruta, protocolo);
        
        char ruta_final[512];
        if (strcmp(ruta, "/") == 0) {
            strcpy(ruta_final, "index.html");
        } else {
            strcpy(ruta_final, ruta + 1);
        }

        struct sockaddr_in direccion_cliente;
        socklen_t longitud_cliente = sizeof(direccion_cliente);
        getpeername(cliente_fd, (struct sockaddr*)&direccion_cliente, &longitud_cliente);
        char cliente_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(direccion_cliente.sin_addr), cliente_ip, INET_ADDRSTRLEN);
        
        printf("[TRABAJADOR %lu] Petición recibida para el recurso '%s'. Buscando en el buffer...\n", id_hilo, ruta_final);
        registrar_conexion(cliente_ip, ntohs(direccion_cliente.sin_port), ruta_final);
        
        size_t tam_contenido = 0;
        char *contenido = obtener_pagina(&buffer_global, ruta_final, &tam_contenido);
        
        if (contenido) {
            printf("[TRABAJADOR %lu] Recurso '%s' encontrado. Sirviendo la página (%zu bytes)...\n", id_hilo, ruta_final, tam_contenido);

            const char *extension = strrchr(ruta_final, '.');
            const char *content_type = "application/octet-stream"; // Tipo por defecto

            if (extension) {
                if (strcmp(extension, ".html") == 0 || strcmp(extension, ".htm") == 0)
                    content_type = "text/html";
                else if (strcmp(extension, ".css") == 0)
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

            // Se imprime el estado del buffer después de servir la página
            imprimir_buffer(&buffer_global);

        } else {
            printf("[TRABAJADOR %lu] Recurso '%s' NO encontrado. Sirviendo error 404.\n", id_hilo, ruta_final);
            char *error_404 = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
            send(cliente_fd, error_404, strlen(error_404), 0);
        }
    }
    
    close(cliente_fd);
    free(args);
    printf("[TRABAJADOR %lu] Conexión cerrada. Recursos del hilo liberados.\n", id_hilo);

    return NULL;
}
