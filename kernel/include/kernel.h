#ifndef KERNEL_H
#define KERNEL_H

#include "shared_utils.h"

extern t_log *kernel_logger_info;
extern t_config *config;

extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_listos_para_ready;
extern pthread_mutex_t mutex_cola_block;
extern pthread_mutex_t mutex_cola_exec;
extern pthread_mutex_t mutex_cola_exit;
extern pthread_mutex_t leer_grado;
extern pthread_mutex_t mutex_generador_pid;
extern pthread_mutex_t mutex_tabla_archivos;

extern sem_t sem_multiprog;
extern sem_t sem_exit;
extern sem_t sem_listos_ready;
extern sem_t sem_ready;
extern sem_t sem_exec;
extern sem_t sem_detener;
extern sem_t sem_blocked_w;
extern sem_t sem_detener_sleep;
extern sem_t sem_hilo_FS;
extern sem_t reanudaar_exec;

extern t_list *recursos_kernel;
extern t_list *lista_ready;
extern t_list *cola_block;
extern t_list *cola_listos_para_ready;
extern t_list *cola_exec;
extern t_list *cola_exit;
extern t_list *lista_global;
extern t_list *cola_blocked_recurso;

extern t_list *lista_ready_detenidos;
extern t_list *lista_block_detenidos;
extern t_list *lista_exec_detenidos;
extern t_list *lista_exit_detenidos;
extern t_list *lista_archivos_abiertos;
extern t_list *lista_global_archivos;

extern t_pcb *proceso_aux;
extern t_pcb *proceso_en_ejecucion;

extern pthread_t hiloFS;

extern int generador_de_id;
extern int grado_multiprogramacion_ini;
extern int conexion_cpu_dispatch, conexion_cpu_interrupt, conexion_memoria, conexion_filesystem;
extern int pid_nuevo;
extern bool frenado;
extern op_code codigo_operacion;
extern recurso_instancia *recurso_proceso;
extern recurso_instancia *recurso_signal;
extern t_contexto_ejecucion *ultimo_contexto_ejecucion;

typedef struct
{
    t_pcb *pcb;
    int tiempo;
} args_sleep;

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

typedef struct
{
    int g_multiprog_ini;
} sem_t1;

extern sem_t1 sem;
extern t_pcb *pcbelegido;
extern t_algoritmo ALGORITMO_PLANIFICACION;
extern arch_config config_valores_kernel;

t_list *cargar_recursos_totales();
t_archivo_abierto_proceso *crear_archivo_proceso(char *, t_pcb *);
recurso_instancia *buscar_recurso(t_list *, char *);
void finalizar_kernel();
void sighandler(int);
void cargar_configuracion(char *);
void iniciar_proceso(char *, int, int);
void finalizar_proceso(int);
void iniciar_planificacion();
void detener_planificacion();
void quantum_interrupter(void);
bool ocupa_recursos(t_list *);
bool tiene_recurso(t_list *, char *);
char *motivo_exit_to_string(motivo_exit);
t_pcb *buscarProceso(int);
void crear_proceso_memoria(int, int, char *, int);
void serializar_pedido_proceso_nuevo(t_paquete *, int, int, char *);

void liberar_recursos(t_pcb*);
void finalizar_proceso_en_ejecucion();
t_archivo_abierto_proceso *buscar_archivo_proceso(t_list *, char *);
bool archivo_existe(t_list *, char *);
t_archivo_global *crear_archivo_global(char *, char);
int verif_crear_recurso_file(t_archivo_global *);
t_archivo_global *buscarArchivoGlobal(t_list *, char *);
void exec_block_fs () ;

void open_file(char*, char);
void fs_interaction();

void* recibir_op_FS();
void* manejar_pf();
void chequear_archivo_fs(int , char *, int);
void serializar_pedido_archivo_fs(t_paquete *, int ,char *);
void truncate_archivo_fs(int , int, int);
void serializar_truncate_archivo_fs(t_paquete *, int , int );
t_instruccion_fs* inicializar_instruccion_fs(t_instruccion* , uint32_t );
t_resp_file esperar_respuesta_file();
t_resp_file recibir_operacion_fs(int);
t_resp_file* obtener_nombre_resp_file(t_resp_file);
void enviarInstruccionFS(int, t_instruccion_fs*);
t_paquete* crear_paquete_con_codigo_de_operacion_fs(t_instruccion_fs* inst_fs);
t_pcb *buscarProcesoBloqueado(int );

#endif