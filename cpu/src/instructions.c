#include "instructions.h"

// Asigna al registro el valor pasado como parámetro.
void _set(char *registro, uint32_t valor)
{
    *(get_registry(registro)) = valor;
}

// Suma al registro el valor pasado como parámetro.
void _sum(char *registro_destino, char *registro_origen)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino);
    uint32_t *origen = malloc(sizeof(uint32_t));
    destino = get_registry(registro_origen);

    *(destino) = *(destino) + *(origen);

    free(destino);
    free(origen);
}

// Resta al registro el valor pasado como parámetro.
void _sub(char *registro_destino, char *registro_origen)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino);
    uint32_t *origen = malloc(sizeof(uint32_t));
    destino = get_registry(registro_origen);

    *(destino) = *(destino) - *(origen);

    free(destino);
    free(origen);
}

// Si valor del registro != cero, actualiza el IP al número de instrucción pasada por parámetro.
void _jnz(char *registro, uint32_t instruccion)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);
    if (*(regis) != 0)
    {
        // actualizar el IP
    }
    else
    {
        //log_warning(cpu_logger_info, "El registro %s es igual a cero, no se actualiza el IP", registro);
    }
}

// Syscall bloqueante. Devuelve el Contexto de Ejecución actualizado al Kernel
// junto a la cantidad de segundos que va a bloquearse el proceso.
void _sleep(uint32_t tiempo)
{
}

// Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
void _wait(char *recurso)
{
}

//  Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
void _signal(char *recurso)
{
}

// Lee el valor de memoria correspondiente a la Dirección Lógica y lo almacena en el Registro.
void _mov_in(char *registro, uint32_t direc_logica)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);

    *(regis) = direc_logica;
    free(regis);
}

// Lee el valor del Registro y lo escribe en la dirección
// física de memoria obtenida a partir de la Dirección Lógica.
void _mov_out(uint32_t direc_logica, char *registro)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);

    // hace algo
    free(regis);
}

// Solicita al kernel que abra el archivo pasado por parámetro con el modo de apertura indicado.
void _f_open(char *nombre_archivo, char *modo_apertura)
{
}

// Solicita al kernel que cierre el archivo pasado por parámetro.
void _f_close(char *nombre_archivo)
{
}

// Solicita al kernel actualizar el puntero del archivo a la posición pasada por parámetro.
void _f_seek(char *nombre_archivo, uint32_t posicion)
{
}

// Solicita al Kernel que se lea del archivo indicado y
// se escriba en la dirección física de Memoria la información leída
void _f_read(char *nombre_archivo, uint32_t direc_logica)
{
}

// Solicita al Kernel que se escriba en el archivo indicado l
// la información que es obtenida a partir de la dirección física de Memoria.
void _f_write(char *nombre_archivo, uint32_t direc_logica)
{
}

//  Solicita al Kernel que se modifique el tamaño del archivo al indicado por parámetro.
void _f_truncate(char *nombre_archivo, uint32_t tamanio)
{
}

// representa la syscall de finalización del proceso.
// Se deberá devolver el contexto de ejecución actualizado al kernel
void __exit()
{
}

uint32_t *get_registry(char *registro)
{
    /*if (strcmp(registro, "AX") == 0)
        return &_AX;
    else if (strcmp(registro, "BX") == 0)
        return &_BX;
    else if (strcmp(registro, "CX") == 0)
        return &_CX;
    else if (strcmp(registro, "DX") == 0)
        return &_DX;
    else
    {
        //log_error(cpu_logger_info, "No se reconoce el registro %s", registro);
        return NULL;
    }
    */
    return malloc(sizeof(uint32_t));
}