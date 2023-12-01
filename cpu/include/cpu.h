#ifndef CPU_H
#define CPU_H

#include "shared_utils.h"
#include <pthread.h>


extern t_log *cpu_logger_info; 
extern t_config* config;

extern uint32_t tamano_pagina;
extern bool page_fault;

//interrupciones
extern bool interrupciones[3];
extern bool descartar_instruccion;

//conexiones
extern int socket_memoria, servidor_cpu_dispatch, 
servidor_cpu_interrupt, conexion_kernel_dispatch, 
conexion_kernel_interrupt;
extern op_code codigo_operacion;

extern pthread_t hiloInterrupt;
extern pthread_mutex_t mutex_interrupt;

extern t_contexto_ejecucion *contexto_actual;

typedef struct  // archivo de configuracion cpu
{
   char* ip_memoria;
   char* ip_escucha;
   char* puerto_memoria;
   char* puerto_escucha_dispatch;
   char* puerto_escucha_interrupt;
} arch_config;

extern arch_config config_valores_cpu;

void finalizar_cpu();
void sighandler(int);
void cargar_configuracion(char*);
void iniciar_conexiones();

void conectar_memoria();
void receive_page_size(int);

void cargar_servidor(int* servidor, char* puerto_escucha, int* conexion, op_code handshake, char* nombre_servidor);

//interrupciones
void* recibir_interrupt(void* arg);
bool descartar_interrupcion(int);
bool hay_interrupciones();
void obtener_motivo_desalojo();
void limpiar_interrupciones();

//funciones de instrucciones
void ejecutar_ciclo_instruccion();
t_instruccion* fetch(int,int);
void decode(t_instruccion* instruccion);
bool es_syscall();


int traducir_dl(uint32_t );
void enviar_mov_out(int, uint32_t);
uint32_t obtener_valor_dir(uint32_t);

void dividirCadena(char* cadena, char** cadena_dividida);

#endif