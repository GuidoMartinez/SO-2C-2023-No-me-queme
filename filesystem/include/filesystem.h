#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "shared_utils.h"
#include <dirent.h>
#include "commons/bitarray.h"
#include <stdint.h>
#define UINT32_MAX 4294967295 // Define UINT32_MAX si no está definido


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
	NOMBRE_ARCHIVO,
    TAMANIO_ARCHIVO,
	BLOQUE_INICIAL
} fcb_prop_t;

typedef struct
{
	uint32_t id_bloque;
	uint32_t offset;
	uint32_t tamanio;
} offset_fcb_t;

typedef struct
{
    uint32_t next_block;
    uint32_t tamanio;
    uint32_t id;
    bool utilizado;
} bloque;



void finalizar_filesystem();
void sighandler(int);
void cargar_configuracion(char *);
void inicializar_datos_memoria(int , void *);
off_t obtener_primer_bloque_libre(bloque *);
bool bloque_esta_ocupado(bloque *, int, int);
int crear_fat(int, char *, t_log *);
void leerBloque(bloque *, int);
void escribirBloque(bloque *, int);
void guardarFAT(const char *, bloque *, int);
bloque * cargarFAT(const char *);
void liberarMemoriaFAT();
void inicializarFATDesdeDirectorio(char *, t_log *);
int crearFCB(char *, t_log *, bloque *);
int crearFAT(char *, t_log *);
uint32_t buscar_fcb(char *);
uint32_t buscar_fcb_id(int);
fcb_t *inicializar_fcb();
fcb_t *_get_fcb_id(int);
fcb_t *_get_fcb(char *);
uint32_t valor_para_fcb(fcb_t *, fcb_prop_t);
uint32_t valor_fcb(int, fcb_prop_t);
void asignar_bloques(int, int, bloque *);
void desasignar_bloques(int, int, bloque *);
void realizar_f_read(t_instruccion_fs *);
int borrar_fcb(int);
t_paquete *crear_paquete_con_respuesta(t_resp_file*);
void comunicacion_kernel();

t_log *filesystem_logger_info;
t_config *config;
uint32_t fcb_id;
arch_config config_valores_filesystem;
int formatear;
void* memoria_file_system;
int tam_memoria_file_system;
int tamanio_fat;
t_list *lista_fcb;
int cant_bloques;
int tamanio_bloque;
bloque * fat;
void *memoria_file_system;
int server_filesystem;
int socket_kernel;
int socket_memoria;
char *nombre_archivo;
char * path_fcb;
int retardo_acceso_bloque;
int bloques_swap;
bloque *swap;

#endif