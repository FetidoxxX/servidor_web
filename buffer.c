#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>

#define PAGINAS_DIR "paginas/"
#define MAX_PAGINAS 3 // Tamaño del buffer modificado a 3

// --- Definiciones de estructuras y funciones compartidas ---
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

// -----------------------------------------------------------

// Función auxiliar para cargar el contenido de un archivo en una estructura Pagina.
static int cargar_archivo_en_pagina(const char *ruta_archivo, Pagina *pagina) {
    FILE *archivo = fopen(ruta_archivo, "rb");
    if (!archivo) {
        printf("Error: No se pudo abrir el archivo %s\n", ruta_archivo);
        return -1;
    }

    fseek(archivo, 0, SEEK_END);
    size_t tam = ftell(archivo);
    fseek(archivo, 0, SEEK_SET);

    // Reservar un byte extra para el caracter nulo de terminación
    pagina->contenido = malloc(tam + 1);
    if (!pagina->contenido) {
        perror("Error reservando memoria para el contenido del archivo");
        fclose(archivo);
        return -1;
    }

    fread(pagina->contenido, 1, tam, archivo);
    fclose(archivo);
    
    // Terminar la cadena con el caracter nulo
    pagina->contenido[tam] = '\0';
    
    pagina->tamano = tam;
    pagina->nombre_archivo = strdup(ruta_archivo + strlen(PAGINAS_DIR));
    pagina->ultimo_acceso = time(NULL);

    return 0;
}

// Inicializa el buffer de páginas al inicio del programa.
void inicializar_buffer(BufferPaginas *buffer) {
    printf("Cargando páginas iniciales en el buffer de memoria...\n");
    pthread_mutex_init(&buffer->mutex, NULL);

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(PAGINAS_DIR)) != NULL) {
        buffer->num_paginas = 0;
        while ((ent = readdir(dir)) != NULL && buffer->num_paginas < MAX_PAGINAS) {
            if (ent->d_type == DT_REG) {
                char ruta_completa[1024];
                snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", PAGINAS_DIR, ent->d_name);
                if (cargar_archivo_en_pagina(ruta_completa, &buffer->paginas[buffer->num_paginas]) == 0) {
                    printf(" - Página cargada: %s\n", buffer->paginas[buffer->num_paginas].nombre_archivo);
                    buffer->num_paginas++;
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error al abrir el directorio de páginas");
        exit(EXIT_FAILURE);
    }
}

// Obtiene una página del buffer o la carga desde el disco si no está presente.
char *obtener_pagina(BufferPaginas *buffer, const char *ruta, size_t *tam_out) {
    pthread_mutex_lock(&buffer->mutex);

    // 1. Buscar la página en el buffer
    char *contenido = NULL;
    int indice_encontrado = -1;
    for (int i = 0; i < buffer->num_paginas; i++) {
        if (strcmp(buffer->paginas[i].nombre_archivo, ruta) == 0) {
            contenido = buffer->paginas[i].contenido;
            *tam_out = buffer->paginas[i].tamano;
            indice_encontrado = i;
            break;
        }
    }

    // 2. Si la página se encontró, actualizar su tiempo de acceso y devolverla.
    if (indice_encontrado != -1) {
        buffer->paginas[indice_encontrado].ultimo_acceso = time(NULL);
        pthread_mutex_unlock(&buffer->mutex);
        return contenido;
    }

    // 3. Si la página no se encontró, cargarla desde el disco.
    char ruta_completa[1024];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", PAGINAS_DIR, ruta);

    Pagina nueva_pagina;
    if (cargar_archivo_en_pagina(ruta_completa, &nueva_pagina) != 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }
    
    // 4. Si el buffer no está lleno, añadir la nueva página.
    if (buffer->num_paginas < MAX_PAGINAS) {
        buffer->paginas[buffer->num_paginas] = nueva_pagina;
        buffer->num_paginas++;
        contenido = nueva_pagina.contenido;
        *tam_out = nueva_pagina.tamano;
    } else {
        // 5. Si el buffer está lleno, encontrar la página menos usada (LRU).
        int indice_lru = 0;
        long long menor_tiempo = buffer->paginas[0].ultimo_acceso;
        for (int i = 1; i < buffer->num_paginas; i++) {
            if (buffer->paginas[i].ultimo_acceso < menor_tiempo) {
                menor_tiempo = buffer->paginas[i].ultimo_acceso;
                indice_lru = i;
            }
        }
        
        // 6. Liberar la memoria de la página menos usada y reemplazarla.
        printf("Buffer lleno. Reemplazando '%s' con '%s'\n", 
               buffer->paginas[indice_lru].nombre_archivo, nueva_pagina.nombre_archivo);
        free(buffer->paginas[indice_lru].nombre_archivo);
        free(buffer->paginas[indice_lru].contenido);
        buffer->paginas[indice_lru] = nueva_pagina;
        contenido = nueva_pagina.contenido;
        *tam_out = nueva_pagina.tamano;
    }

    pthread_mutex_unlock(&buffer->mutex);
    return contenido;
}

// Nueva función para imprimir el estado del búfer de páginas
void imprimir_buffer(BufferPaginas *buffer) {
    time_t tiempo_actual;
    time(&tiempo_actual);
    printf("\n--- Estado del Buffer de Paginas (%lld) ---\n", (long long)tiempo_actual);
    pthread_mutex_lock(&buffer->mutex);
    for (int i = 0; i < buffer->num_paginas; i++) {
        printf("  [%d] Nombre: %s, Ultimo Acceso: %lld\n", i, buffer->paginas[i].nombre_archivo, buffer->paginas[i].ultimo_acceso);
    }
    pthread_mutex_unlock(&buffer->mutex);
    printf("-------------------------------------------\n\n");
    fflush(stdout); // Asegura que la salida se imprima inmediatamente
}


