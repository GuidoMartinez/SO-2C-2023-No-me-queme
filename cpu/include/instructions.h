#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include "shared_utils.h"

//instrucciones

void _set(u_int32_t registro, u_int32_t valor);
void _sum(u_int32_t registro_destino, u_int32_t registro_origen);
void _sub(u_int32_t registro_destino, u_int32_t registro_origen);
void _jnz(u_int32_t registro, u_int32_t instruccion);
void _sleep(u_int32_t tiempo);
void _wait(char * recurso);
void _signal(char* recurso);
void _mov_in(u_int32_t registro, u_int32_t direc_logica);
void _mov_out(u_int32_t direc_logica, u_int32_t registro);
void _f_open(char* nombre_archivo, char* modo_apertura);
void _f_close(char* nombre_archivo);
void _f_seek(char* nombre_archivo, u_int32_t posicion);
void _f_read(char* nombre_archivo, u_int32_t direc_logica);
void _f_write(char* nombre_archivo, u_int32_t direc_logica);
void _f_truncate(char* nombre_archivo, u_int32_t tamanio);
void __exit();

#endif
