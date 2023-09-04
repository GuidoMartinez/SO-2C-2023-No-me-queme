#ifndef CPU_H
#define CPU_H

#include "shared_utils.h"


t_log *cpu_logger_info; 
t_config* config;

int socket_memoria, servidor_cpu_dispatch, servidor_cpu_interrupt, conexion_kernel_dispatch, conexion_kernel_interrupt;
op_code codigo_operacion;

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

#endif