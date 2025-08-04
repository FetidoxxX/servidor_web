#include "servidor_web.h"

// Función auxiliar para cargar el contenido de un archivo en una estructura Pagina.
// Esta función ahora recibe la ruta completa y la utiliza directamente.
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

    if (fread(pagina->contenido, 1, tam, archivo) != tam) {
        perror("Error leyendo el archivo");
        free(pagina->contenido);
        fclose(archivo);
        return -1;
    }
    pagina->contenido[tam] = '\0';
    pagina->tamano = tam;
    
    // Almacenar solo el nombre del archivo, no la ruta completa
    const char *nombre_archivo = strrchr(ruta_archivo, '/');
    if (nombre_archivo == NULL) {
        nombre_archivo = ruta_archivo;
    } else {
        nombre_archivo++;
    }
    pagina->nombre_archivo = strdup(nombre_archivo);

    fclose(archivo);
    return 0;
}

// Inicializa el buffer de páginas
void inicializar_buffer(BufferPaginas *buffer) {
    buffer->num_paginas = 0;
    pthread_mutex_init(&buffer->mutex, NULL);

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(PAGINAS_DIR)) != NULL) {
        // [BUFFER] Iniciando la carga de páginas al buffer
        printf("[BUFFER] Cargando páginas iniciales en el buffer de memoria...\n");
        while ((ent = readdir(dir)) != NULL) {
            // Asegurarse de no sobrepasar el límite del buffer
            if (buffer->num_paginas >= MAX_PAGINAS) {
                break;
            }
            
            if (ent->d_type == DT_REG) { // Asegurarse de que es un archivo regular
                char ruta_completa[512];
                snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", PAGINAS_DIR, ent->d_name);
                Pagina nueva_pagina;
                if (cargar_archivo_en_pagina(ruta_completa, &nueva_pagina) == 0) {
                    time(&nueva_pagina.ultimo_acceso);
                    buffer->paginas[buffer->num_paginas] = nueva_pagina;
                    buffer->num_paginas++;
                    // [BUFFER] Página cargada exitosamente en el buffer
                    printf("[BUFFER] Página cargada: %s\n", nueva_pagina.nombre_archivo);
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error abriendo el directorio de paginas");
    }
}

// Obtiene una página del buffer o la carga si no está.
// La función ahora usa la ruta completa pasada por el hilo trabajador.
char *obtener_pagina(BufferPaginas *buffer, const char *ruta, size_t *tam_out) {
    // 1. Quitar el '/' inicial de la ruta y obtener el nombre del archivo.
    const char *nombre_solicitado = (ruta[0] == '/') ? ruta + 1 : ruta;
    const char *extension = strrchr(nombre_solicitado, '.');

    // ** Lógica para manejar imágenes **
    // Las imágenes se sirven directamente desde el disco sin usar el buffer de caché.
    if (extension && (strcmp(extension, ".png") == 0 || strcmp(extension, ".jpg") == 0 || strcmp(extension, ".jpeg") == 0 || strcmp(extension, ".gif") == 0)) {
        Pagina imagen_temp;
        char ruta_imagen[512];

        // Se verifica si la ruta ya tiene el prefijo de la carpeta de imagenes
        if (strncmp(nombre_solicitado, IMAGENES_DIR, strlen(IMAGENES_DIR)) != 0) {
            // Si no, se construye la ruta correcta
            snprintf(ruta_imagen, sizeof(ruta_imagen), "%s%s", IMAGENES_DIR, nombre_solicitado);
        } else {
            // Si ya lo tiene, se usa la ruta tal cual
            snprintf(ruta_imagen, sizeof(ruta_imagen), "%s", nombre_solicitado);
        }

        if (cargar_archivo_en_pagina(ruta_imagen, &imagen_temp) == 0) {
            *tam_out = imagen_temp.tamano;
            // [BUFFER] Imágen cargada directamente desde el disco, no se usa el buffer
            printf("[BUFFER] Sirviendo imagen '%s' directamente del disco.\n", nombre_solicitado);
            return imagen_temp.contenido;
        }
        // Si no se encuentra, devuelve NULL.
        return NULL;
    }
    
    pthread_mutex_lock(&buffer->mutex);
    
    char *contenido = NULL;
    int indice_encontrado = -1;
    char ruta_completa[512];
    
    // [BUFFER] Buscando la página en el caché...
    // 2. Buscar la página en el búfer.
    for (int i = 0; i < buffer->num_paginas; i++) {
        if (strcmp(buffer->paginas[i].nombre_archivo, nombre_solicitado) == 0) {
            // Página encontrada en el búfer. Se actualiza el tiempo de acceso.
            contenido = buffer->paginas[i].contenido;
            *tam_out = buffer->paginas[i].tamano;
            time(&buffer->paginas[i].ultimo_acceso);
            indice_encontrado = i;
            // [BUFFER] Página '%s' encontrada en el caché.
            printf("[BUFFER] Página '%s' encontrada en el caché.\n", nombre_solicitado);
            break;
        }
    }
    
    // 3. Si la página no está en el búfer, cargarla y manejar el caché.
    if (indice_encontrado == -1) {
        // [BUFFER] Página '%s' no encontrada en el caché. Intentando cargarla desde el disco...
        printf("[BUFFER] Página '%s' no encontrada en el caché. Intentando cargarla desde el disco...\n", nombre_solicitado);
        Pagina nueva_pagina;
        snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", PAGINAS_DIR, nombre_solicitado);
        
        if (cargar_archivo_en_pagina(ruta_completa, &nueva_pagina) != 0) {
            pthread_mutex_unlock(&buffer->mutex);
            return NULL;
        }

        time(&nueva_pagina.ultimo_acceso);
        
        // 4. Verificar si hay espacio en el búfer.
        if (buffer->num_paginas < MAX_PAGINAS) {
            buffer->paginas[buffer->num_paginas] = nueva_pagina;
            contenido = nueva_pagina.contenido;
            *tam_out = nueva_pagina.tamano;
            buffer->num_paginas++;
            // [BUFFER] Nueva página '%s' añadida al buffer.
            printf("[BUFFER] Nueva página '%s' añadida al buffer.\n", nueva_pagina.nombre_archivo);
        } else {
            // 5. Aplicar el algoritmo LRU si el búfer está lleno.
            // [BUFFER] Buffer lleno. Aplicando algoritmo LRU.
            printf("[BUFFER] Buffer lleno. Aplicando algoritmo LRU para reemplazar una página.\n");
            int indice_lru = 0;
            time_t menor_tiempo = buffer->paginas[0].ultimo_acceso;
            for (int i = 1; i < buffer->num_paginas; i++) {
                if (buffer->paginas[i].ultimo_acceso < menor_tiempo) {
                    menor_tiempo = buffer->paginas[i].ultimo_acceso;
                    indice_lru = i;
                }
            }
            
            // 6. Reemplazar la página menos usada.
            printf("[BUFFER] Reemplazando '%s' con '%s'\n", 
                   buffer->paginas[indice_lru].nombre_archivo, nueva_pagina.nombre_archivo);
            free(buffer->paginas[indice_lru].nombre_archivo);
            free(buffer->paginas[indice_lru].contenido);
            buffer->paginas[indice_lru] = nueva_pagina;
            contenido = nueva_pagina.contenido;
            *tam_out = nueva_pagina.tamano;
        }
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
    if (buffer->num_paginas == 0) {
        printf("  (Buffer vacío)\n");
    } else {
        for (int i = 0; i < buffer->num_paginas; i++) {
            printf("  [%d] Nombre: %s, Ultimo Acceso: %lld\n",
                   i, buffer->paginas[i].nombre_archivo, (long long)buffer->paginas[i].ultimo_acceso);
        }
    }
    pthread_mutex_unlock(&buffer->mutex);
    printf("-------------------------------------------\n");
}

// Función para liberar toda la memoria usada por el buffer de páginas
void liberar_buffer(BufferPaginas *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    printf("\n[BUFFER] Liberando memoria del buffer de páginas...\n");
    for (int i = 0; i < buffer->num_paginas; i++) {
        free(buffer->paginas[i].nombre_archivo);
        free(buffer->paginas[i].contenido);
    }
    buffer->num_paginas = 0;
    pthread_mutex_unlock(&buffer->mutex);
    pthread_mutex_destroy(&buffer->mutex); // Destruir el mutex
}