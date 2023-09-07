#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "shared_utils.h"

//registros
//uint32_t _AX, _BX, _CX, _DX;

//instrucciones

void _set(char* registro, uint32_t valor);
void _sum(char* registro_destino, char* registro_origen);
void _sub(char* registro_destino, char* registro_origen);
void _jnz(char* registro, uint32_t instruccion);
void _sleep(uint32_t tiempo);
void _wait(char * recurso);
void _signal(char* recurso);
void _mov_in(char* registro, uint32_t direc_logica);
void _mov_out(uint32_t direc_logica, char* registro);
void _f_open(char* nombre_archivo, char* modo_apertura);
void _f_close(char* nombre_archivo);
void _f_seek(char* nombre_archivo, uint32_t posicion);
void _f_read(char* nombre_archivo, uint32_t direc_logica);
void _f_write(char* nombre_archivo, uint32_t direc_logica);
void _f_truncate(char* nombre_archivo, uint32_t tamanio);
void __exit();

uint32_t *get_registry(char* registry);

#endif
