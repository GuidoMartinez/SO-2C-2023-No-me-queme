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
    TAMANO_PAGINA
} op_code;

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
    uint32_t lineas;
} t_paquete;

typedef enum{
	NEW,
	READY,
	EXEC,
	BLOCK,
	FINISH_EXIT,
	FINISH_ERROR,
} estado_proceso;

typedef enum{
	SUCCESS,
	SEG_FAULT,
	OUT_OF_MEMORY,
	RECURSO_INEXISTENTE,
}motivo_exit;

typedef enum{
	RECURSO_BLOCK,
    ARCHIVO_BLOCK
}motivo_block;

typedef struct{
	
} t_registros;

typedef struct{
    int pid;
    int program_counter;
    t_registros* registros;
    int numero_marco;
    int nro_pf;
}t_contexto_ejecucion;
typedef struct{
    int pid;
    int prioridad;
    int tamanio;
    t_contexto_ejecucion* contexto_ejecucion;
    estado_proceso estado;
    motivo_exit motivo_exit;
    motivo_block motivo_block;
    t_list* archivos_abiertos; 
} t_pcb;

void enviar_mensaje(char*, int);
void* serializar_paquete(t_paquete*, int);
void crear_buffer(t_paquete*);
t_paquete* crear_paquete(void);
t_paquete *crear_paquete_con_codigo_de_operacion(uint32_t);
void agregar_a_paquete(t_paquete*, void*, int);
void enviar_paquete(t_paquete*, int);
void eliminar_paquete(t_paquete*);

int crear_conexion(char*, char*);
int iniciar_servidor(t_log*, char*, char*);
int esperar_cliente(int, t_log*);
int crear_socket_escucha(char*);

int recibir_operacion(int);
void* recibir_buffer(int*, int);
void* recibir_mensaje(int, t_log*);
t_list* recibir_paquete(int);

char* mi_funcion_compartida(); // TODO - BORRAR - PARA CHEQUEAR FUNCIONAMIENTO OK DE LAS SHARED

#endif