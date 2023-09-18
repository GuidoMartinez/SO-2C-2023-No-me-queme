#ifndef MEMORIA_H
#define MEMORIA_H

#include "shared_utils.h"

t_log* logger_memoria_info;
t_config* config;
t_list *procesos_totales,*tablas_de_paginas_en_memoria;

int server_memoria, socket_fs, socket_cpu, socket_kernel;
int tamanio_memoria;
void* memoria_usuario;


 typedef struct  { // Configuracion de la memoria
    char* puerto_escucha;
    char* ip_escucha; 
    char* ip_filesystem;
    char* puerto_filesystem;
    uint32_t tamanio_memoria;   
    uint32_t tamanio_pagina;
    uint32_t retardo_respuesta;
    char* algoritmo_reemplazo; 
    char* path_instrucciones;
} arch_config_memoria;

arch_config_memoria config_valores_memoria;


void cargar_configuracion(char*);
void inicializar_memoria();
void atender_clientes_memoria();
void* manejo_conexiones(void*);

int atender_cliente_cpu();
int atender_cliente_fs();
int atender_cliente_kernel();
void* manejo_conexion_cpu(void* );
void* manejo_conexion_filesystem(void* );
void* manejo_conexion_kernel(void* );
void send_page_size(uint32_t,int);

t_proceso_memoria *iniciar_proceso_path(t_proceso_memoria*); 
t_list *parsear_instrucciones(char *);
char *leer_archivo(char *);
t_instruccion *armar_estructura_instruccion(nombre_instruccion, char* parametro1, char* parametro2);

int paginas__necesarias_proceso(uint32_t,uint32_t);

t_proceso_memoria *obtener_proceso_pid(uint32_t);
t_instruccion *obtener_instrccion_pc(t_proceso_memoria*, uint32_t);
t_instruccion* obtener_instruccion_pid_pc(uint32_t, uint32_t);


void cargar_proceso_prueba();

void finalizar_memoria();

#endif