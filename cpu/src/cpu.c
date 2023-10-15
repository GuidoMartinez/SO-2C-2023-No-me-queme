#include "cpu.h"
#include "instructions.h"

t_contexto_ejecucion *contexto_actual;

void sighandler(int s)
{
    finalizar_cpu();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    if (argc < 1)
    {
        // para ejecutar el cpu es: ./cpu ./cfg/cpu.config
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }
    cpu_logger_info = log_create("./cfg/cpu.log", "CPU", true, LOG_LEVEL_INFO);
    cargar_configuracion(argv[1]);

    pthread_mutex_init(&mutex_interrupt, NULL);

    iniciar_conexiones();
    // cambiar a uno para recibir la operacion de kernel
    while (1)
    {

        codigo_operacion = recibir_operacion(conexion_kernel_dispatch);

        switch (codigo_operacion)
        {
        case CONTEXTO:
            contexto_actual = recibir_contexto(conexion_kernel_dispatch);

            while (contexto_actual->codigo_ultima_instru != EXIT && !interrumpir){
                ejecutar_ciclo_instruccion();
            }
            if(interrumpir) log_info(cpu_logger_info,"interrumpi un proceso");
            log_info(cpu_logger_info, "Ultima instruccion: %s", obtener_nombre_instruccion(contexto_actual->codigo_ultima_instru));
            log_info(cpu_logger_info, "PC actual: %d", contexto_actual->program_counter);
            enviar_contexto(conexion_kernel_dispatch, contexto_actual);

            pthread_mutex_lock(&mutex_interrupt);
            interrumpir = false;
            pthread_mutex_unlock(&mutex_interrupt);

            break;
        default:
            log_error(cpu_logger_info, "El codigo que me llego es %d",codigo_operacion);
            log_warning(cpu_logger_info, "Operacion desconocida \n");
            abort();
            break;
        }
    }

    return EXIT_SUCCESS;
}

void iniciar_conexiones()
{
    conectar_memoria();
    cargar_servidor(&servidor_cpu_dispatch, config_valores_cpu.puerto_escucha_dispatch, &conexion_kernel_dispatch, HANDSHAKE_CPU_DISPATCH, "DISPATCH");
    cargar_servidor(&servidor_cpu_interrupt, config_valores_cpu.puerto_escucha_interrupt, &conexion_kernel_interrupt, HANDSHAKE_CPU_INTERRUPT, "INTERRUPT");
    pthread_create(&hiloInterrupt, NULL, recibir_interrupcion, NULL);
    pthread_detach(hiloInterrupt);
}

void *recibir_interrupcion(void *arg)
{
    codigo_operacion = recibir_operacion(conexion_kernel_interrupt);
    switch (codigo_operacion)
    {
    case FIN_QUANTUM:
        log_info(cpu_logger_info, "Recibo fin de quantum");
        if (!descartar_instruccion) {
            pthread_mutex_lock(&mutex_interrupt);
            interrumpir = true;
            contexto_actual->motivo_desalojado = INTERRUPT_FIN_QUANTUM;
            pthread_mutex_unlock(&mutex_interrupt);
        }
        break;
    case FIN_PROCESO:
        log_info(cpu_logger_info, "Recibo fin de proceso");
        if (!descartar_instruccion) {
            pthread_mutex_lock(&mutex_interrupt);
            interrumpir = true;
            contexto_actual->motivo_desalojado = INTERRUPT_FIN_PROCESO;
            pthread_mutex_unlock(&mutex_interrupt);
        }
        break;
    default:
        log_warning(cpu_logger_info, "Operacion desconocida \n");
        break;
    }

    return NULL;
}

void descartar_interrupcion(){
    descartar_instruccion = true; //TODO - Logica de descartar interrupciones
}

void conectar_memoria()
{
    socket_memoria = crear_conexion(config_valores_cpu.ip_memoria, config_valores_cpu.puerto_memoria);

    realizar_handshake(socket_memoria, HANDSHAKE_CPU, cpu_logger_info);

    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(socket_memoria, cpu_logger_info);
        log_info(cpu_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(cpu_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }
    receive_page_size(socket_memoria);
    log_info(cpu_logger_info, "El tamano de pagina recibido de memoria es %d ", tamano_pagina);
}

void receive_page_size(int socket)
{
    op_code codigo_op = recibir_operacion(socket);

    if (codigo_op == TAMANO_PAGINA)
    {
        int size;
        void *buffer = recibir_buffer(&size, socket);
        memcpy(&tamano_pagina, buffer, sizeof(uint32_t));
        free(buffer);
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }
}

void cargar_servidor(int *servidor, char *puerto_escucha, int *conexion, op_code handshake, char *nombre_servidor)
{
    *(servidor) = iniciar_servidor(cpu_logger_info, config_valores_cpu.ip_escucha, puerto_escucha);
    *(conexion) = esperar_cliente(*(servidor), cpu_logger_info);
    codigo_operacion = recibir_operacion(*(conexion));
    if (codigo_operacion == handshake)
    {
        log_info(cpu_logger_info, "Handshake exitoso con KERNEL para la conexion %s", nombre_servidor);
        recibir_handshake(*(conexion) , cpu_logger_info);
        realizar_handshake(*(conexion), handshake, cpu_logger_info);
    }
    else
    {
        log_error(cpu_logger_info, "Fallo la comunicacion. Abortando \n");
        finalizar_cpu();
    }
}

void cargar_configuracion(char *path)
{

    config = config_create(path);

    if (config == NULL)
    {
        perror("Archivo de configuracion de kernel no encontrado \n");
        abort();
    }
    config_valores_cpu.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    config_valores_cpu.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    config_valores_cpu.puerto_escucha_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    config_valores_cpu.puerto_escucha_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    config_valores_cpu.ip_escucha = config_get_string_value(config, "IP_ESCUCHA");
}

void ejecutar_ciclo_instruccion()
{
    t_instruccion *instruccion = fetch(contexto_actual->pid, contexto_actual->program_counter);
    decode(instruccion);
    if(!page_fault) contexto_actual->program_counter++; // TODO -- chequear que en los casos de instruccion con memoria logica puede dar PAGE FAULT y no hay que aumentar el pc (restarlo dentro del decode en esos casos)
}

// Pide a memoria la siguiente instruccion a ejecutar
// Recibe por parametro el pid, el pc de la instruccion a pedir
t_instruccion *fetch(int pid, int pc)
{
    ask_instruccion_pid_pc(pid, pc, socket_memoria);

    op_code codigo_op = recibir_operacion(socket_memoria);

    t_instruccion *instruccion;

    if (codigo_op == INSTRUCCION)
    {
        instruccion = deserializar_instruccion(socket_memoria);
        contexto_actual->instruccion_ejecutada = instruccion;
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la instruccion de memoria.");
        abort();
    }

    return instruccion;
}

// Ejecuta instrucciones
void decode(t_instruccion *instruccion)
{
    switch (instruccion->codigo)
    {
    case SET:
        _set(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case SUM:
        _sum(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case SUB:
        _sub(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case JNZ:
        _jnz(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case SLEEP:
        _sleep(contexto_actual);
        break;
    case WAIT:
        _wait(contexto_actual);
        break;
    case SIGNAL:
        _signal(contexto_actual);
        break;
    case MOV_IN:
        _mov_in(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case MOV_OUT:
        _mov_out(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case F_OPEN:
        _f_open(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case F_CLOSE:
        _f_close(instruccion->parametro1, contexto_actual);
        break;
    case F_SEEK:
        _f_seek(instruccion->parametro1, instruccion->parametro2, contexto_actual);
    case F_READ:
        _f_read(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case F_WRITE:
        _f_write(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case F_TRUNCATE:
        _f_truncate(instruccion->parametro1, instruccion->parametro2, contexto_actual);
        break;
    case EXIT:
        __exit(contexto_actual);
        break;
    default:
        log_error(cpu_logger_info, "Instruccion no reconocida");
        break;
    }
}

void dividirCadena(char *cadena, char **palabras)
{
    const char delimitador[] = " ";
    char *token;
    int i = 0;

    // Usamos una copia de la cadena original para evitar modificar la original
    char copiaCadena[strlen(cadena) + 1];
    strcpy(copiaCadena, cadena);

    // Obtener la primera palabra
    token = strtok(copiaCadena, delimitador);

    while (token != NULL && i < 3)
    {
        strcpy(palabras[i], token);
        i++;
        // Obtener la siguiente palabra
        token = strtok(NULL, delimitador);
    }
}

void finalizar_cpu()
{
    log_info(cpu_logger_info, "Finalizando el modulo CPU");
    log_destroy(cpu_logger_info);
    config_destroy(config);
    close(socket_memoria);
    close(servidor_cpu_dispatch);
    close(servidor_cpu_interrupt);
    close(conexion_kernel_dispatch);
    close(conexion_kernel_interrupt);
}
