#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "shared_utils.h"
#include <dirent.h>
#include "commons/bitarray.h"
#include <stdint.h>
#define UINT32_MAX 4294967295 // Define UINT32_MAX si no está definido

int socket_memoria_op, server_filesystem, socket_kernel,socket_memoria_swap;

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
    char *nombre_archivo;
    char *ruta_archivo;
    uint32_t id;
    uint32_t tamanio_archivo;
    uint32_t bloque_inicial;
} fcb_t;

typedef struct
{
    t_list *lista_fcb;
} fcb_list_t;

typedef enum
{
    TAMANIO_ARCHIVO,
    BLOQUE_INICIAL
} fcb_prop_t;

typedef struct
{
    uint32_t id_bloque;
    uint32_t offset;
    uint32_t tamanio;
} offset_fcb_t;

typedef struct {
    int id_bloque;
    bloque_t bloque;
} bloque_con_id_t;

void sighandler(int);

int atender_kernel();
void inicializar_swap();
void *leer_bloque_swap(uint32_t );
int escribir_bloque_swap(uint32_t , void *);
uint32_t obtener_primer_bloque_libre_swap();
void liberar_bloques_swap(t_list *);
t_list *lista_bloques_swap_reservados(int);
bloque_con_id_t recibir_escritura_swap(int);
void *manejo_conexion_memoria_swap(void *);

void cargar_configuracion(char *);
void finalizar_filesystem();

int obtener_cantidad_de_bloques(int);
uint32_t obtener_primer_bloque_libre();
int crear_archivo_bloques(char *);
void *leer_bloque(uint32_t);

int crear_fat(char *);

uint32_t posicion_en_fat(uint32_t);
void modificar_en_fat(uint32_t , uint32_t);

fcb_t *obtener_fcb_desde_archivo(char *);
void inicializar_fcb_list(char *);

int crear_fcb(char *);

uint32_t conseguir_id_fcb(char *);
uint32_t buscar_fcb(char *);
uint32_t buscar_fcb_id(int);
fcb_t *inicializar_fcb();
fcb_t *_get_fcb_id(int);
fcb_t *_get_fcb(char *);
uint32_t valor_para_fcb(fcb_t *, fcb_prop_t);
uint32_t valor_fcb(int, fcb_prop_t);
int _modificar_fcb(fcb_t *, fcb_prop_t, uint32_t);
int modificar_fcb(int, fcb_prop_t, uint32_t);

void asignar_bloques(int, int);
void desasignar_bloques(int, int);

int truncar_fcb(char *, uint32_t);

void escribir_bloque(uint32_t, void *);

void realizar_f_write(t_instruccion_fs *);
void realizar_f_read(t_instruccion_fs *);

void borrar_fcb(int);

void destroy_instruccion_file(t_instruccion_fs *);

t_instruccion_fs *deserializar_instruccion_file(int);
void *comunicacion_kernel();
void *hilo_f_open(void *);
void *hilo_f_create(void *);
void *hilo_f_close(void *);
void *hilo_f_truncate(void *);
void *hilo_f_write(void *);
void *hilo_f_read(void *);

t_log *filesystem_logger_info;
t_config *config;
//uint32_t fcb_id;
arch_config config_valores_filesystem;
int formatear;
void *memoria_file_system;
int tam_memoria_file_system;
int tamanio_fat;
t_list *lista_fcb;
int cant_bloques;
int tamanio_bloque;
int server_filesystem;
int socket_kernel;
int socket_memoria;
char *nombre_archivo;
char *path_fcb;
char *path_bloques;
char *path_fat;
int retardo_acceso_bloque;
int bloques_swap;
uint32_t *swap;
bloque_t bloque_fs;
bloque_t *bloques;
pthread_mutex_t mutex_fat;

#endif