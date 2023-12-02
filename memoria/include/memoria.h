#ifndef MEMORIA_H
#define MEMORIA_H

#include "shared_utils.h"

t_log *logger_memoria_info;
t_config *config;
t_list *procesos_totales, *tablas_de_paginas, *marcos;
t_list *paginas_utilizadas;
t_proceso_memoria *proceso_memoria;

op_code resp_code_fs;

int server_memoria, socket_fs, socket_cpu, socket_kernel, socket_fs_int, socket_cpu_int, socket_kernel_int;
int tamanio_memoria, indice_tabla;
int cantidad_pags;
int contador_lru = 0;
void *memoria_usuario;
t_algoritmo algoritmo_pags;

pthread_mutex_t mutex_procesos;
pthread_mutex_t mutex_contador_LRU;
pthread_mutex_t mutex_memoria_usuario;
pthread_mutex_t mutex_marcos;
pthread_mutex_t mutex_fifo;

typedef struct
{ // Configuracion de la memoria
    char *puerto_escucha;
    char *ip_escucha;
    char *ip_filesystem;
    char *puerto_filesystem;
    char *puerto_filesystem_swap;
    uint32_t tamanio_memoria;
    uint32_t tamanio_pagina;
    uint32_t retardo_respuesta;
    char *algoritmo_reemplazo;
    char *path_instrucciones;
} arch_config_memoria;

arch_config_memoria config_valores_memoria;

void cargar_configuracion(char *);
void inicializar_memoria();
void atender_clientes_memoria();
void *manejo_conexiones(void *);

int atender_cliente_cpu();
int atender_cliente_fs_archivos();
int atender_cliente_kernel();
void *manejo_conexion_cpu(void *);
void *manejo_conexion_filesystem_archivos(void *);
void *manejo_conexion_kernel(void *);
void send_page_size(uint32_t, int);

t_proceso_memoria *iniciar_proceso_path(t_proceso_memoria *);
t_list *parsear_instrucciones(char *);
char *leer_archivo(char *);
t_instruccion *armar_estructura_instruccion(nombre_instruccion, char *parametro1, char *parametro2);

int paginas_necesarias_proceso(uint32_t, uint32_t);

t_proceso_memoria *obtener_proceso_pid(uint32_t);
t_instruccion *obtener_instrccion_pc(t_proceso_memoria *, uint32_t);
t_instruccion *obtener_instruccion_pid_pc(uint32_t, uint32_t);

t_proceso_memoria *recibir_proceso_nuevo(int);
t_algoritmo obtener_algoritmo();

void actualizar_LRU(t_entrada_tabla_pag *);

void recibir_pf_kernel(int, int *, int *);
t_entrada_tabla_pag *obtenerPaginaFIFO();
t_entrada_tabla_pag *obtenerPaginaLRU();
t_entrada_tabla_pag *paginaAReemplazar();
bool son_iguales(t_entrada_tabla_pag *, t_entrada_tabla_pag *);
t_entrada_tabla_pag *obtener_entrada_menor_tiempo_lru(t_list *);
void agregar_pagina_fifo(t_entrada_tabla_pag *);

t_list *obtener_total_pags_en_memoria(t_list *);
void escribirPagEnMemoria(void *, int);
void pedido_lectura_swap(int,int, t_entrada_tabla_pag *);

double marcosTotales();
void inicializar_marcos();
t_list *obtener_marcos_pid(uint32_t);
bool mismo_pid_marco(t_marco *, int);
int asignar_marco_libre(uint32_t);
void liberar_marco_indice(int);
void liberar_marcos_proceso(uint32_t);
void recibir_pedido_marco(int *, int *, int);
int obtener_marco_pid(int, int);
t_marco *marco_desde_df(int);
void marcar_pag_modificada(int, int);
bool tiene_bit_presencia_igual_a_1(void *);
t_list *obtener_entradas_con_bit_presencia_1(t_proceso_memoria *);
t_entrada_tabla_pag *obtener_entrada_con_marco(t_list *, int);
void cambiar_bit_modificado(t_proceso_memoria *, int, int);

bool es_marco_libre(void *);
bool hay_marcos_libres();
void enviar_marco_cpu(int, int, op_code);

void inicializar_nuevo_proceso(t_proceso_memoria *);
int inicializar_estructuras_memoria_nuevo_proceso(t_proceso_memoria *);
void pedido_inicio_swap(int, int,int);
void asignar_id_bloque_swap(t_proceso_memoria *, t_list *);

void recibir_mov_out_cpu(uint32_t *, uint32_t *, int);
void escribir_memoria(uint32_t, uint32_t);

void recibir_mov_in_cpu(int *, int);
uint32_t leer_memoria(uint32_t);
void enviar_valor_mov_in_cpu(uint32_t, int);

void limpiar_swap(t_proceso_memoria *);
t_list *obtener_lista_id_bloque_swap(t_proceso_memoria *proceso);
void enviar_bloques_swap_a_liberar(t_list *, int);

void eliminar_proceso_memoria(t_proceso_memoria *);

void finalizar_memoria();

#endif