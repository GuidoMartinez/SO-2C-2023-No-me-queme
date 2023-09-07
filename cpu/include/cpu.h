#ifndef CPU_H
#define CPU_H

#include "shared_utils.h"


t_log *cpu_logger_info; 
t_config* config;

//registros
uint32_t _AX, _BX, _CX, _DX;

//conexiones
int socket_memoria, servidor_cpu_dispatch, 
servidor_cpu_interrupt, conexion_kernel_dispatch, 
conexion_kernel_interrupt;
op_code codigo_operacion;

char** instrucciones;

//t_PCB contexto;

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
void cargar_servidor(int* servidor, char* puerto_escucha, int* conexion, op_code handshake, char* nombre_servidor);
void ejecutar_ciclo_instrucciones(char** contextos);

//funciones de instrucciones
void fetch(int IP);
void decode(char* instruccion);
bool check_interrupt();

void dividirCadena(char* cadena, char** cadena_dividida);
uint32_t str_to_uint32(char* cadena);
#endif