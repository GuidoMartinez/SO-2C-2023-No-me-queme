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
    uint32_t tamanio_memoria;   
    uint32_t tamanio_pagina;
    uint32_t retardo_respuesta;
    char* algoritmo_reemplazo; 
    char* path_instrucciones;
} arch_config_memoria;

arch_config_memoria config_valores_memoria;


void cargar_configuracion(char*);
void atender_clientes_memoria();
void* manejo_conexiones(void*);

int atender_cliente_cpu();
int atender_cliente_fs();
int atender_cliente_kernel();
void* manejo_conexion_cpu(void* );
void* manejo_conexion_filesystem(void* );
void* manejo_conexion_kernel(void* );
void send_page_size(uint32_t,int);

void finalizar_memoria();

#endif