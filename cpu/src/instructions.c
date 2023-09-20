#include "instructions.h"

// Asigna al registro el valor pasado como parámetro.
void _set(char *registro, char* valor, t_contexto_ejecucion *contexto)
{
    *(get_registry(registro, contexto)) = str_to_uint32(valor);
    contexto->codigo_ultima_instru = SET;
}

// Suma al registro el valor pasado como parámetro.
void _sum(char *registro_destino, char *registro_origen, t_contexto_ejecucion *contexto)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino, contexto);
    uint32_t *origen = malloc(sizeof(uint32_t));
    destino = get_registry(registro_origen, contexto);

    *(destino) = *(destino) + *(origen);

    contexto->codigo_ultima_instru = SUM;
    free(destino);
    free(origen);
}

// Resta al registro el valor pasado como parámetro.
void _sub(char *registro_destino, char *registro_origen, t_contexto_ejecucion *contexto)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino, contexto);
    uint32_t *origen = malloc(sizeof(uint32_t));
    destino = get_registry(registro_origen, contexto);

    *(destino) = *(destino) - *(origen);

    contexto->codigo_ultima_instru = SUB;
    free(destino);
    free(origen);
}

// Si valor del registro != cero, actualiza el IP al número de instrucción pasada por parámetro.
void _jnz(char *registro, char* instruccion, t_contexto_ejecucion *contexto)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro, contexto);
    if (*(regis) != 0)
    {
        contexto->program_counter = str_to_uint32(instruccion);
        contexto->codigo_ultima_instru = JNZ;
    }
    else
    {
        //log_warning(cpu_logger_info, "El registro %s es igual a cero, no se actualiza el IP", registro);
    }
}

// Syscall bloqueante. Devuelve el Contexto de Ejecución actualizado al Kernel
// junto a la cantidad de segundos que va a bloquearse el proceso.
void _sleep(char* tiempo, t_contexto_ejecucion *contexto)
{
    //devolver contexto al kernel
    contexto->codigo_ultima_instru = SLEEP;
}

// Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
void _wait(char *recurso, t_contexto_ejecucion *contexto)
{
    //solicitar al kernel asignar recurso y que actualice su contexto
    contexto->codigo_ultima_instru = WAIT;
}

//  Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
void _signal(char *recurso, t_contexto_ejecucion *contexto)
{
    //solicitar al kernel asignar recurso y que actualice su contexto
    contexto->codigo_ultima_instru = SIGNAL;
}

// Lee el valor de memoria correspondiente a la Dirección Lógica y lo almacena en el Registro.
void _mov_in(char *registro, char* direc_logica, t_contexto_ejecucion *contexto)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro, contexto);

    // obtener valor desde memoria
    //*(regis) = valor;
    contexto->codigo_ultima_instru = MOV_IN;
    free(regis);
}

// Lee el valor del Registro y lo escribe en la dirección
// física de memoria obtenida a partir de la Dirección Lógica.
void _mov_out(char* direc_logica, char *registro, t_contexto_ejecucion *contexto)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro, contexto);

    // escribir en memoria el contenido del registro
    contexto->codigo_ultima_instru = MOV_OUT;
    free(regis);
}

// Solicita al kernel que abra el archivo pasado por parámetro con el modo de apertura indicado.
void _f_open(char *nombre_archivo, char *modo_apertura, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_OPEN;
}

// Solicita al kernel que cierre el archivo pasado por parámetro.
void _f_close(char *nombre_archivo, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_CLOSE;
}

// Solicita al kernel actualizar el puntero del archivo a la posición pasada por parámetro.
void _f_seek(char *nombre_archivo, char* posicion, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_SEEK;
}

// Solicita al Kernel que se lea del archivo indicado y
// se escriba en la dirección física de Memoria la información leída
void _f_read(char *nombre_archivo, char* direc_logica, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_READ;
}

// Solicita al Kernel que se escriba en el archivo indicado l
// la información que es obtenida a partir de la dirección física de Memoria.
void _f_write(char *nombre_archivo, char* direc_logica, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_WRITE;
}

//  Solicita al Kernel que se modifique el tamaño del archivo al indicado por parámetro.
void _f_truncate(char *nombre_archivo, char* tamanio, t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = F_TRUNCATE;
}

// representa la syscall de finalización del proceso.
// Se deberá devolver el contexto de ejecución actualizado al kernel
void __exit(t_contexto_ejecucion *contexto)
{
    contexto->codigo_ultima_instru = EXIT;
}

uint32_t *get_registry(char *registro, t_contexto_ejecucion *contexto)
{
    if (strcmp(registro, "AX") == 0)
        return &(contexto->registros->ax);
    else if (strcmp(registro, "BX") == 0)
        return &(contexto->registros->bx);
    else if (strcmp(registro, "CX") == 0)
        return &(contexto->registros->cx);
    else if (strcmp(registro, "DX") == 0)
        return &(contexto->registros->dx);
    else
    {
        //log_error(cpu_logger_info, "No se reconoce el registro %s", registro);
        return NULL;
    }
}