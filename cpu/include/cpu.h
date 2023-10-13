#ifndef CPU_H
#define CPU_H

#include "shared_utils.h"
#include <pthread.h>


t_log *cpu_logger_info; 
t_config* config;

uint32_t tamano_pagina;
bool interrumpir = false;
bool descartar_instruccion = false;
bool page_fault = false;

//conexiones
int socket_memoria, servidor_cpu_dispatch, 
servidor_cpu_interrupt, conexion_kernel_dispatch, 
conexion_kernel_interrupt;
op_code codigo_operacion;

pthread_t hiloInterrupt;
pthread_mutex_t mutex_interrupt;

typedef struct  // archivo de configuracion cpu
{
   char* ip_memoria;
   char* ip_escucha;
   char* puerto_memoria;
   char* puerto_escucha_dispatch;
   char* puerto_escucha_interrupt;
} arch_config;

arch_config config_valores_cpu;

void finalizar_cpu();
void sighandler(int);
void cargar_configuracion(char*);
void iniciar_conexiones();

void conectar_memoria();
void receive_page_size(int);

void cargar_servidor(int* servidor, char* puerto_escucha, int* conexion, op_code handshake, char* nombre_servidor);

void* recibir_interrupcion(void* arg);
void descartar_interrupcion();


//funciones de instrucciones
void ejecutar_ciclo_instruccion();
t_instruccion* fetch(int,int);
void decode(t_instruccion* instruccion);

void dividirCadena(char* cadena, char** cadena_dividida);

#endif