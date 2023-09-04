#ifndef KERNEL_H
#define KERNEL_H

#include "shared_utils.h"

t_log *kernel_logger_info;
t_config *config;

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

arch_config config_valores_kernel;

void finalizar_kernel();
void sighandler(int);
void cargar_configuracion(char *);

#endif