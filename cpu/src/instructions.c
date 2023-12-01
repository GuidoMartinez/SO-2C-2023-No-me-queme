#include "instructions.h"
#include "cpu.h"

// Asigna al registro el valor pasado como parámetro.
void _set(char *registro, char* valor)
{
    *(get_registry(registro)) = str_to_uint32(valor);
    contexto_actual->codigo_ultima_instru = SET;
}

// Suma al registro el valor pasado como parámetro.
void _sum(char *registro_destino, char *registro_origen)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino);
    uint32_t *origen = malloc(sizeof(uint32_t));
    origen = get_registry(registro_origen);

    *(destino) = *(destino) + *(origen);

    contexto_actual->codigo_ultima_instru = SUM;
    //free(destino);
    //free(origen);
}

// Resta al registro el valor pasado como parámetro.
void _sub(char *registro_destino, char *registro_origen)
{
    uint32_t *destino = malloc(sizeof(uint32_t));
    destino = get_registry(registro_destino);
    uint32_t *origen = malloc(sizeof(uint32_t));
    origen = get_registry(registro_origen);

    *(destino) = *(destino) - *(origen);

    contexto_actual->codigo_ultima_instru = SUB;
    //free(destino);
    //free(origen);
}

// Si valor del registro != cero, actualiza el IP al número de instrucción pasada por parámetro.
void _jnz(char *registro, char* instruccion)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);
    if (*(regis) != 0)
    {
        contexto_actual->program_counter = str_to_uint32(instruccion);
        contexto_actual->codigo_ultima_instru = JNZ;
    }
    else
    {
        //log_warning(cpu_logger_info, "El registro %s es igual a cero, no se actualiza el IP", registro);
    }

    //free(regis);
}

// Syscall bloqueante. Devuelve el contexto_actual de Ejecución actualizado al Kernel
// junto a la cantidad de segundos que va a bloquearse el proceso.
void _sleep()
{
    contexto_actual->codigo_ultima_instru = SLEEP;
    contexto_actual->motivo_desalojado = SYSCALL;
}

// Esta instrucción solicita al Kernel que se asigne una instancia del recurso indicado por parámetro.
void _wait()
{
    //solicitar al kernel asignar recurso
    contexto_actual->codigo_ultima_instru = WAIT;
    contexto_actual->motivo_desalojado = SYSCALL;
}

//  Esta instrucción solicita al Kernel que se libere una instancia del recurso indicado por parámetro.
void _signal()
{
    //solicitar al kernel liberar recurso
    contexto_actual->codigo_ultima_instru = SIGNAL;
    contexto_actual->motivo_desalojado = SYSCALL;
}

// Lee el valor de memoria correspondiente a la Dirección Lógica y lo almacena en el Registro.
void _mov_in(char *registro, char* direc_logica)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);

    uint32_t valor = obtener_valor_dir(str_to_uint32(direc_logica));
    if(valor != -1) {
        *(regis) = valor;
    }

    contexto_actual->codigo_ultima_instru = MOV_IN;
    //free(regis);
}

// Lee el valor del Registro y lo escribe en la dirección
// física de memoria obtenida a partir de la Dirección Lógica.
void _mov_out(char* direc_logica, char *registro)
{
    uint32_t *regis = malloc(sizeof(uint32_t));
    regis = get_registry(registro);

    escribir_memoria(str_to_uint32(direc_logica), *(regis));

    //TODO: solo cambiar si no es page fault
    contexto_actual->codigo_ultima_instru = MOV_OUT;
    //free(regis);
}

// Solicita al kernel que abra el archivo pasado por parámetro con el modo de apertura indicado.
void _f_open(char *nombre_archivo, char *modo_apertura)
{
    contexto_actual->codigo_ultima_instru = F_OPEN;
}

// Solicita al kernel que cierre el archivo pasado por parámetro.
void _f_close(char *nombre_archivo)
{
    contexto_actual->codigo_ultima_instru = F_CLOSE;
}

// Solicita al kernel actualizar el puntero del archivo a la posición pasada por parámetro.
void _f_seek(char *nombre_archivo, char* posicion)
{
    contexto_actual->codigo_ultima_instru = F_SEEK;
}

// Solicita al Kernel que se lea del archivo indicado y
// se escriba en la dirección física de Memoria la información leída
void _f_read(char *nombre_archivo, char* direc_logica)
{
    traducir_dl_fs(direc_logica);

    contexto_actual->codigo_ultima_instru = F_READ;
}

// Solicita al Kernel que se escriba en el archivo indicado l
// la información que es obtenida a partir de la dirección física de Memoria.
void _f_write(char *nombre_archivo, char* direc_logica)
{
    traducir_dl_fs(direc_logica);

    contexto_actual->codigo_ultima_instru = F_WRITE;
}

//  Solicita al Kernel que se modifique el tamaño del archivo al indicado por parámetro.
void _f_truncate(char *nombre_archivo, char* tamanio)
{
    contexto_actual->codigo_ultima_instru = F_TRUNCATE;
}

// representa la syscall de finalización del proceso.
// Se deberá devolver el contexto_actual de ejecución actualizado al kernel
void __exit()
{
    contexto_actual->codigo_ultima_instru = EXIT;
}

void traducir_dl_fs(char* dl){
    int df = traducir_dl(str_to_uint32(dl));
    if(df == -1){
        log_error(cpu_logger_info, "Page fault: %s", dl);
    }else{
        char* df_string = string_itoa(df);
        contexto_actual->instruccion_ejecutada->longitud_parametro2 = strlen(df_string) + 1;
        contexto_actual->instruccion_ejecutada->parametro2 = strdup(df_string);
    }
}

uint32_t *get_registry(char *registro)
{
    if (strcmp(registro, "AX") == 0)
        return &(contexto_actual->registros->ax);
    else if (strcmp(registro, "BX") == 0)
        return &(contexto_actual->registros->bx);
    else if (strcmp(registro, "CX") == 0)
        return &(contexto_actual->registros->cx);
    else if (strcmp(registro, "DX") == 0)
        return &(contexto_actual->registros->dx);
    else
    {
        log_error(cpu_logger_info, "No se reconoce el registro %s", registro);
        return NULL;
    }
}