#include "instructions.h"

// Asigna al registro el valor pasado como parámetro.
void _set(u_int32_t registro, u_int32_t valor){
    registro = valor;
}

// Suma al registro el valor pasado como parámetro.
void _sum(u_int32_t registro_destino, u_int32_t registro_origen){
    registro_destino = registro_destino + registro_origen;
}

// Resta al registro el valor pasado como parámetro.
void _sub(u_int32_t registro_destino, u_int32_t registro_origen){
    registro_destino = registro_destino - registro_origen;
}

// Si valor del registro != cero, actualiza el IP al número de instrucción pasada por parámetro.
void _jnz(u_int32_t registro, u_int32_t instruccion){

}

// Syscall bloqueante. Devuelve el Contexto de Ejecución actualizado al Kernel 
// junto a la cantidad de segundos que va a bloquearse el proceso.
void _sleep(u_int32_t tiempo){

}

// Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
void _wait (char * recurso){

}

//  Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
void _signal (char* recurso){

}

// Lee el valor de memoria correspondiente a la Dirección Lógica y lo almacena en el Registro.
void _mov_in (u_int32_t registro, u_int32_t direc_logica){

}

// Lee el valor del Registro y lo escribe en la dirección
// física de memoria obtenida a partir de la Dirección Lógica.
void _mov_out (u_int32_t direc_logica, u_int32_t registro){

}

//Solicita al kernel que abra el archivo pasado por parámetro con el modo de apertura indicado.
void _f_open (char* nombre_archivo, char* modo_apertura){

}

// Solicita al kernel que cierre el archivo pasado por parámetro.
void _f_close (char* nombre_archivo){

}

// Solicita al kernel actualizar el puntero del archivo a la posición pasada por parámetro.
void _f_seek (char* nombre_archivo, u_int32_t posicion){

}

// Solicita al Kernel que se lea del archivo indicado y 
// se escriba en la dirección física de Memoria la información leída
void _f_read (char* nombre_archivo, u_int32_t direc_logica){

}

//Solicita al Kernel que se escriba en el archivo indicado l
//la información que es obtenida a partir de la dirección física de Memoria.
void _f_write (char* nombre_archivo, u_int32_t direc_logica){

}

//  Solicita al Kernel que se modifique el tamaño del archivo al indicado por parámetro.
void _f_truncate (char* nombre_archivo, u_int32_t tamanio){

}

// representa la syscall de finalización del proceso.
// Se deberá devolver el contexto de ejecución actualizado al kernel
void __exit(){
    
}