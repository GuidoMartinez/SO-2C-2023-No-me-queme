#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "shared_utils.h"

//t_pcb pcb_actual;

//instrucciones

void _set(char* registro, char* valor);
void _sum(char* registro_destino, char* registro_origen);
void _sub(char* registro_destino, char* registro_origen);
void _jnz(char* registro, char* instruccion);
void _sleep();
void _wait();
void _signal();
void _mov_in(char* registro, char* direc_logica);
void _mov_out(char* direc_logica, char* registro);
void _f_open(char* nombre_archivo, char* modo_apertura);
void _f_close(char* nombre_archivo);
void _f_seek(char* nombre_archivo, char* posicion);
void _f_read(char* nombre_archivo, char* direc_logica);
void _f_write(char* nombre_archivo, char* direc_logica);
void _f_truncate(char* nombre_archivo, char* tamanio);
void __exit();

uint32_t *get_registry(char* registry);

void traducir_dl_fs(char*);

#endif
