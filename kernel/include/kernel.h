#ifndef KERNEL_H
#define KERNEL_H

#include "shared_utils.h"

t_log *kernel_logger_info;
t_config *config;

pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_listos_para_ready;

t_list* lista_ready;
t_list* cola_listos_para_ready;

int generador_de_id;
int conexion_cpu_dispatch, conexion_cpu_interrupt, conexion_memoria, conexion_filesystem;

typedef struct // archivo de configuracion kernel
{
    char *ip_memoria;
    char *puerto_memoria;
    char *ip_filesystem;
    char *puerto_filesystem;
    char *ip_cpu;
    char *puerto_cpu_dispatch;
    char *puerto_cpu_interrupt;
    char *algoritmo_planificacion;
    int quantum;
    int grado_multiprogramacion;
    char **recursos;
    char **instancias_recursos;

} arch_config;
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
    int prioridad;
    t_registros* registros;
    estado_proceso estado;
    motivo_exit motivo_exit;
    motivo_block motivo_block;
    t_list* archivos_abiertos; 
} t_pcb;

typedef enum{
	FIFO,
	RR,
    PRIORIDADES
} t_algoritmo;

t_algoritmo ALGORITMO_PLANIFICACION;

arch_config config_valores_kernel;

void finalizar_kernel();
void sighandler(int);
void cargar_configuracion(char *);
void iniciar_proceso();
void finalizar_proceso();
void iniciar_planificacion();
void detener_planificacion();
void cambiar_estado(t_pcb *, estado_proceso );
void procesar_cambio_estado(t_pcb* , estado_proceso );	
void planificar_largo_plazo();
void planificar_corto_plazo() ;
char *estado_to_string(estado_proceso );
void safe_pcb_add(t_list*, t_pcb*, pthread_mutex_t*);
void pcb_create();
t_pcb* safe_pcb_remove(t_list* , pthread_mutex_t* );
t_pcb* elegir_pcb_segun_algoritmo();

#endif