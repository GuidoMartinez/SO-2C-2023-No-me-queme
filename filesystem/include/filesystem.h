#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "shared_utils.h"

t_log *filesystem_logger_info;
t_config *config;

int socket_memoria,server_filesystem, socket_kernel;

typedef struct // archivo de configuracion filesystem
{
    char *ip_memoria;
    char *puerto_memoria;
    char *ip_escucha;
    char *puerto_escucha;
    char *path_fat;
    char *path_bloques;
    char *path_fcb;
    int cant_bloques_total;
    int cant_bloques_swap;
    int tam_bloque;
    int retardo_acceso_bloque;
    int retardo_acceso_fat;
} arch_config;

arch_config config_valores_filesystem;

void finalizar_filesystem();
void sighandler(int);
void cargar_configuracion(char *);

#endif