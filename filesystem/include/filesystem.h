#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "shared_utils.h"
#include <dirent.h>
#include "commons/bitarray.h"


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

typedef struct
{
	char* nombre_archivo;
    char* ruta_archivo;
    uint32_t id;
	uint32_t tamanio_archivo;
	uint32_t bloque_inicial;
} fcb_t;

typedef struct
{	
    t_list* lista_fcb;
} fcb_list_t;

typedef enum
{
	TAMANIO_ARCHIVO,
	BLOQUE_INICIAL
} fcb_prop_t;

void finalizar_filesystem();
void sighandler(int);
void cargar_configuracion(char *);

t_log *filesystem_logger_info;
t_config *config;
fcb_list_t* lista_global_fcb;
uint32_t fcb_id;
arch_config config_valores_filesystem;
int formatear;
t_bitarray* bitarray;

#endif