#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "shared_utils.h"

//t_pcb pcb_actual;

//instrucciones

void _set(char* registro, char* valor, t_contexto_ejecucion *contexto);
void _sum(char* registro_destino, char* registro_origen, t_contexto_ejecucion *contexto);
void _sub(char* registro_destino, char* registro_origen, t_contexto_ejecucion *contexto);
void _jnz(char* registro, char* instruccion, t_contexto_ejecucion *contexto);
void _sleep(char* tiempo, t_contexto_ejecucion *contexto, int conexion_kernel);
void _wait(char * recurso, t_contexto_ejecucion *contexto, int conexion_kernel);
void _signal(char* recurso, t_contexto_ejecucion *contexto, int conexion_kernel);
void _mov_in(char* registro, char* direc_logica, t_contexto_ejecucion *contexto);
void _mov_out(char* direc_logica, char* registro, t_contexto_ejecucion *contexto);
void _f_open(char* nombre_archivo, char* modo_apertura, t_contexto_ejecucion *contexto);
void _f_close(char* nombre_archivo, t_contexto_ejecucion *contexto);
void _f_seek(char* nombre_archivo, char* posicion, t_contexto_ejecucion *contexto);
void _f_read(char* nombre_archivo, char* direc_logica, t_contexto_ejecucion *contexto);
void _f_write(char* nombre_archivo, char* direc_logica, t_contexto_ejecucion *contexto);
void _f_truncate(char* nombre_archivo, char* tamanio, t_contexto_ejecucion *contexto);
void __exit(t_contexto_ejecucion *contexto);

uint32_t *get_registry(char* registry, t_contexto_ejecucion *contexto);

#endif
