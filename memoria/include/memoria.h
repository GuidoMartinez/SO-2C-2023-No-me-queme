#ifndef MEMORIA_H
#define MEMORIA_H

#include "shared_utils.h"

t_log* logger_memoria_info;
t_config* config;


int server_memoria, socket_fs, socket_cpu, socket_kernel;

 typedef struct  { // Configuracion de la memoria
    char* puerto_escucha;
    char* ip_escucha; 
    char* ip_filesystem;
    char* puerto_filesystem;
    int tamanio_memoria;   
    int tamanio_pagina;
    int retardo_respuesta;
    char* algoritmo_reemplazo; 
    char* path_instrucciones;
} arch_config_memoria;

arch_config_memoria config_valores_memoria;


void cargar_configuracion(char*);
int atender_clientes_memoria();
void* manejo_conexiones(void*);
void finalizar_memoria();

#endif