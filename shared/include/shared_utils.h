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
    INSTRUCCION_PEDIDA
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
    EXIT
} nombre_instruccion;

typedef struct
{
    nombre_instruccion codigo;
    char *parametro1;
    uint32_t longitud_parametro1;
    char *parametro2;
    uint32_t longitud_parametro2;
} t_instruccion;

typedef struct
{
    nombre_instruccion codigo;
    char *parametro1;
    uint32_t long_parametro1;
    uint32_t direc_fisica;
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
    SLEEP_BN
} motivo_block;

typedef struct
{

} t_registros;

typedef struct
{
    int pid;
    int program_counter;
    t_registros *registros;
    int numero_marco;
    int nro_pf;
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
    uint32_t pid;
    uint32_t tamano;
    char *path;
    uint32_t longitud_path;
    t_list* instrucciones;
    t_list* bloques_swap;
    t_list* tabla_paginas;

} t_proceso_memoria;

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
t_list *recibir_paquete(int);

void realizar_handshake(int, op_code, t_log *);

const char *obtener_nombre_instruccion(nombre_instruccion);

uint32_t str_to_uint32(char *str);

#endif