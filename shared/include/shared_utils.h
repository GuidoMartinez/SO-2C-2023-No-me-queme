#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/string.h>
#include <commons/config.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "pthread.h"
#include "semaphore.h"
#include <commons/collections/queue.h>

typedef enum
{
    MENSAJE,
    PAQUETE,
    HANDSHAKE_CPU_DISPATCH,
    HANDSHAKE_CPU_INTERRUPT,
    HANDSHAKE_FILESYSTEM,
    HANDSHAKE_KERNEL,
    HANDSHAKE_MEMORIA,
    HANDSHAKE_CPU,
    INSTRUCCION,
    TAMANO_PAGINA,
    INICIALIZAR_PROCESO,
    PROCESO_INICIALIZADO,
    FINALIZAR_PROCESO,
    PEDIDO_INSTRUCCION,
    CONTEXTO,
    INTERRUPCION,
    F_READ_FS,
    F_WRITE_FS,
    INICIO_SWAP,
    LISTA_BLOQUES_SWAP,
    SWAP_A_LIBERAR,
    SWAP_LIBERADA,
    LEER_BLOQUE,
    VALOR_BLOQUE,
    ESCRIBIR_BLOQUE,
    ESCRITURA_BLOQUE_OK,
    MOV_IN_CPU,
    MOV_OUT_CPU,
    INSTRUCCION_MEMORIA_OK,
    MARCO,
    PAGE_FAULT_KERNEL,
    MARCO_PAGE_FAULT,
    PAGINA_CARGADA,
    HANDSHAKE_FILESYSTEM_OPS
} op_code;

typedef enum
{
    SET,
    SUM,
    SUB,
    JNZ,
    SLEEP,
    WAIT,
    SIGNAL,
    MOV_IN,
    MOV_OUT,
    F_OPEN,
    F_CREATE,
    F_CLOSE,
    F_SEEK,
    F_READ,
    F_WRITE,
    F_TRUNCATE,
    EXIT,
    PRINT_FILE_DATA
} nombre_instruccion;

typedef struct
{
    nombre_instruccion codigo;
    uint32_t pid;
    char *parametro1;
    uint32_t longitud_parametro1;
    char *parametro2;
    uint32_t longitud_parametro2;
} t_instruccion;

typedef struct
{
    nombre_instruccion estado;
    uint32_t pid;
    uint32_t param1_length;
    char *param1;
    uint32_t param2_length;
    char *param2;
    uint32_t puntero;
} t_instruccion_fs;

typedef struct
{
    int size;
    void *stream;
} t_buffer;

typedef struct
{
    op_code codigo_operacion;
    t_buffer *buffer;
    uint32_t lineas;
} t_paquete;

typedef enum
{
    NEW,
    READY,
    EXEC,
    BLOCK,
    FINISH_EXIT,
    FINISH_ERROR,
} estado_proceso;

typedef enum
{
    SUCCESS,
    SEG_FAULT,
    OUT_OF_MEMORY,
    RECURSO_INEXISTENTE,
} motivo_exit;

typedef enum
{
    RECURSO_BLOCK,
    ARCHIVO_BLOCK,
    SLEEP_BN,
    OP_FILESYSTEM
} motivo_block;

typedef enum
{
    INTERRUPT_FIN_QUANTUM,
    INTERRUPT_FIN_PROCESO,
    INTERRUPT_NUEVO_PROCESO,
    SYSCALL,
    PAGE_FAULT,
    PAGE_FAULT_RESUELTO
} motivo_desalojo;

typedef struct
{
    uint32_t ax;
    uint32_t bx;
    uint32_t cx;
    uint32_t dx;
} t_registros;

typedef struct
{
    char *nombre;
    uint32_t cantidad;
    t_queue *colabloqueado;
} recurso_instancia;
typedef struct
{
    int pid;
    int program_counter;
    t_registros *registros;
    int numero_marco;
    int nro_pf;
    t_instruccion *instruccion_ejecutada;
    nombre_instruccion codigo_ultima_instru;
    motivo_desalojo motivo_desalojado;
} t_contexto_ejecucion;
typedef struct
{
    int pid;
    int prioridad;
    int tamanio;
    t_contexto_ejecucion *contexto_ejecucion;
    estado_proceso estado;
    motivo_exit motivo_exit;
    motivo_block motivo_block;
    t_list *archivos_abiertos;
    time_t tiempo_ingreso_ready;
    char *recurso_instruccion;
    t_list *recursos_asignados;
} t_pcb;

typedef struct
{
    int pid;
    int tamano;
    char *path;
    int longitud_path;
} t_ini_proceso;

typedef struct
{
    t_list *entradas_tabla;
    int cantidad_paginas;
} t_tabla_paginas;

typedef struct
{
    uint32_t pid;
    uint32_t tamano;
    char *path;
    uint32_t longitud_path;
    t_list *instrucciones;
    t_tabla_paginas *tabla_paginas;

} t_proceso_memoria;

typedef struct
{
    uint32_t id;
    t_list cola_pendientes;
} t_recurso;

typedef struct
{
    motivo_desalojo motivo_interrupcion;
    int pid;
} t_interrupcion;

typedef struct
{
    int valor;
    op_code codigo_operacion;
} t_valor_operacion;

typedef enum
{
    FIFO,
    RR,
    PRIORIDADES,
    LRU
} t_algoritmo;

typedef enum
{
    F_ERROR,
    F_OPEN_SUCCESS,
    F_CLOSE_SUCCESS,
    F_TRUNCATE_SUCCESS,
    F_WRITE_SUCCESS,
    F_SEEK_SUCCESS,
    F_READ_SUCCESS,
    F_CREATE_SUCCESS,
    FILE_DOESNT_EXISTS,
} t_resp_file;

typedef struct
{
    int indice;
    int marco;
    int bit_presencia;
    int bit_modificado;
    int id_bloque_swap;
    int tiempo_lru;

} t_entrada_tabla_pag;

typedef struct
{
    int pid;
    int num_de_marco;
} t_marco;

typedef struct
{
    char *nombreArchivo;
    uint32_t puntero;
} t_archivo_abierto_proceso;

typedef struct
{
    t_list *archivos_abiertos;
    pthread_mutex_t mutex_archivo;
} tabla_t_archivo_abierto_global;

typedef struct
{
    char *nombreArchivo;
    char lock;
    uint32_t contador;
    t_queue *colabloqueado;
} t_archivo_global;

typedef struct
{
    void *datos;
} bloque_t;

void enviar_mensaje(char *, int);
void *serializar_paquete(t_paquete *, int);
void crear_buffer(t_paquete *);
t_paquete *crear_paquete(void);
t_paquete *crear_paquete_con_codigo_de_operacion(uint32_t);
void agregar_a_paquete(t_paquete *, void *, int);
void enviar_paquete(t_paquete *, int);
void eliminar_paquete(t_paquete *);

int crear_conexion(char *, char *);
int iniciar_servidor(t_log *, char *, char *);
int esperar_cliente(int, t_log *);
int crear_socket_escucha(char *);

int recibir_operacion(int);
void *recibir_buffer(int *, int);
void *recibir_mensaje(int, t_log *);
void *recibir_mensaje_sin_log(int);
t_list *recibir_paquete(int);

void realizar_handshake(int, op_code, t_log *);
op_code recibir_handshake(int conexion, t_log *logger);

char *obtener_nombre_instruccion(nombre_instruccion);

void enviar_contexto(int, t_contexto_ejecucion *);
void serializar_contexto(t_paquete *, t_contexto_ejecucion *);
t_contexto_ejecucion *recibir_contexto(int);

void ask_instruccion_pid_pc(int, int, int);
void pedido_instruccion(uint32_t *, uint32_t *, int);
void enviar_instruccion(int, t_instruccion *);
void serializar_instruccion(t_paquete *, t_instruccion *);
void serializar_instruccion_fs(t_paquete *, t_instruccion_fs *);
t_instruccion *deserializar_instruccion(int);
t_instruccion_fs *deserializar_instruccion_fs(int);
t_instruccion *deserializar_instruccion_viejo(t_buffer *);

void enviar_pid(int, int, op_code);
void recibir_pid(int, int *);

void enviar_interrupcion(int, t_interrupcion *);
t_interrupcion *recibir_interrupcion(int);

void serializar_lista_swap(t_list *, t_paquete *); // serializa en el paquete la lista de ids de bloque a recibir en memoria desde FS
t_list *recibir_listado_id_bloques(int);

void enviar_pedido_marco(int, int, int);
int recibir_marco(int);

void enviar_op_con_int(int, op_code, int);
int recibir_int(int);
uint32_t recibir_valor_memoria(int);

uint32_t str_to_uint32(char *);
char *cod_inst_to_str(nombre_instruccion);

void liberar_instruccion(t_instruccion *);
void liberar_lista_instrucciones(t_list *);
void liberar_contexto(t_contexto_ejecucion*);

void enviar_bloque(int, bloque_t,int);

#endif