#include "servidor_web.h"

pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;

//función para incluir la página solicitada en el log
void registrar_conexion(const char *ip, int puerto, const char *pagina_solicitada) {
    FILE *archivo = fopen(LOG_ARCHIVO, "a");
    if (!archivo) {
        perror("Error al abrir archivo de log");
        return;
    }

    time_t ahora = time(NULL);
    char *timestamp = ctime(&ahora);
    timestamp[strlen(timestamp) - 1] = '\0';

    pthread_mutex_lock(&mutex_log);
    fprintf(archivo, "[%s] Conexión desde %s:%d, página solicitada: %s\n", 
            timestamp, ip, puerto, pagina_solicitada);
    pthread_mutex_unlock(&mutex_log);

    fclose(archivo);
}
