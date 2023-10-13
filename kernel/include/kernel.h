#ifndef KERNEL_H
#define KERNEL_H
#include "shared_utils.h"

t_log *kernel_logger_info;t_config *config;

pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_listos_para_ready;
pthread_mutex_t mutex_cola_block;
pthread_mutex_t mutex_cola_exec;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t leer_grado;
pthread_mutex_t mutex_generador_pid;
sem_t sem_multiprog;
sem_t sem_exit;
sem_t sem_listos_ready;
sem_t sem_ready;
sem_t sem_exec;
sem_t sem_detener;
sem_t sem_blocked_w;

t_list* recursosKernel; 
t_list* lista_ready;
t_list* cola_block ;
t_list* cola_listos_para_ready;
t_list* cola_exec;
t_list* cola_exit;
t_list* lista_global;

int generador_de_id=0;
int grado_multiprogramacion_ini;
int conexion_cpu_dispatch, conexion_cpu_interrupt, conexion_memoria, conexion_filesystem;
int pid_nuevo;
bool frenado=0;
op_code codigo_operacion;
recurso_instancia*  recursoProceso;
recurso_instancia*  recurso_signal;

int actual_sleep = 0;

typedef struct // archivo de configuracion kernel
{char *ip_memoria;char *puerto_memoria;char *ip_filesystem;char *puerto_filesystem;char *ip_cpu;char *puerto_cpu_dispatch;char *puerto_cpu_interrupt;char *algoritmo_planificacion;int quantum;int grado_multiprogramacion;char **recursos;char **instancias_recursos;
} arch_config;

typedef struct
{
    int g_multiprog_ini;
} sem_t1;

sem_t1 sem;

typedef enum{FIFO,RR,PRIORIDADES} t_algoritmo;
t_algoritmo ALGORITMO_PLANIFICACION;
arch_config config_valores_kernel;
recurso_instancia* buscarRecursoW(t_list* , char* );
void finalizar_kernel();
void sighandler(int);
void cargar_configuracion(char *);
void iniciar_proceso(char* , int , int );
void finalizar_proceso(int);
void iniciar_planificacion();
void detener_planificacion();
void cambiar_estado(t_pcb *, estado_proceso );
void procesar_cambio_estado(t_pcb* , estado_proceso );
void planificar_largo_plazo();
void planificar_corto_plazo() ;
char *estado_to_string(estado_proceso );
void safe_pcb_add(t_list*, t_pcb*, pthread_mutex_t*);
void pcb_create(int,int,int);
t_pcb* safe_pcb_remove(t_list* , pthread_mutex_t* );
t_pcb* elegir_pcb_segun_algoritmo();
void ready_pcb(void);
void block(void);
void exec_pcb(void);
void exit_pcb(void);
void quantum_interrupter(void);
void sleeper(void);
void set_pcb_ready(t_pcb *) ;
void prceso_admitido(t_pcb* );
void pcb_destroy(t_pcb* );
char* motivo_exit_to_string(motivo_exit );
t_pcb *buscarProceso(int );
void crear_proceso_memoria(int,int,char*,int);
void serializar_pedido_proceso_nuevo(t_paquete*, int, int,char*);
t_pcb* obtener_pcb_RR();
#endif