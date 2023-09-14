#include "cpu.h"
#include "instructions.h"

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

    iniciar_conexiones();
    // TODO: Recibir PCBs de los programas a ejecutar por DISPATCH
    // if (pcb_actual != null)ejecutar_ciclo_instrucciones();

    return EXIT_SUCCESS;
}

void iniciar_conexiones()
{
    conectar_memoria();
    cargar_servidor(&servidor_cpu_dispatch, config_valores_cpu.puerto_escucha_dispatch, &conexion_kernel_dispatch, HANDSHAKE_CPU_DISPATCH, "DISPATCH");
    cargar_servidor(&servidor_cpu_interrupt, config_valores_cpu.puerto_escucha_interrupt, &conexion_kernel_interrupt, HANDSHAKE_CPU_INTERRUPT, "INTERRUPT");
}

void conectar_memoria()
{
    socket_memoria = crear_conexion(config_valores_cpu.ip_memoria, config_valores_cpu.puerto_memoria);
    t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_CPU);
    enviar_paquete(paquete_handshake, socket_memoria);
    eliminar_paquete(paquete_handshake);

    // TODO: recibir informacion para traducir direcciones de memoria
    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == MENSAJE)
    {
        char *mensaje = recibir_mensaje(socket_memoria, cpu_logger_info);
        free(mensaje);
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
    char mensaje_a_enviar[100];
    if (codigo_operacion == handshake)
    {
        sprintf(mensaje_a_enviar, "Handshake para %s existoso con CPU", nombre_servidor);
        log_info(cpu_logger_info, "Handshake exitoso con KERNEL para la conexion %s", nombre_servidor);
        enviar_mensaje(mensaje_a_enviar, *(conexion));
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

void ejecutar_ciclo_instrucciones()
{
    /*
        t_instruccion instruccion = fetch(pcb_actual->contexto_ejecucion.IP);
        decode(instruccion);
        if(check_interrupt()) interrumpir;
    */
    // devolver contexto de ejecucion al kernel al final por dispatch
}

// TODO: pide a memoria la siguiente instruccion a ejecutar
// Recibe por parametro la primer posicion de las instrucciones
t_instruccion fetch(int IP)
{
    t_instruccion ins;
    ins.codigo = 0;
    return ins;
}

// Ejecuta instrucciones
void decode(t_instruccion instruccion)
{
    switch(instruccion.codigo){
        case SET:
            _set(instruccion.parametro1, instruccion.parametro2);
            break;
        //revisar
        case SUM:
            _sum(instruccion.parametro1, instruccion.parametro2);
            break;
        case SUB:
            _sub(instruccion.parametro1, instruccion.parametro2);
            break;
        case JNZ:
            _jnz(instruccion.parametro1, instruccion.parametro2);
            break;
        case SLEEP:
            _sleep(instruccion.parametro1);
            break;
        case WAIT:
            _wait(instruccion.parametro1);
            break;
        case SIGNAL:
            _signal(instruccion.parametro1);
            break;
        case MOV_IN:
            _mov_in(instruccion.parametro1, instruccion.parametro2);
            break;
        case MOV_OUT:
            _mov_out(instruccion.parametro1, instruccion.parametro2);
            break;
        case F_OPEN:
            _f_open(instruccion.parametro1, instruccion.parametro2);
            break;
        case F_CLOSE:
            _f_close(instruccion.parametro1);
            break;
        case F_SEEK:
            _f_seek(instruccion.parametro1, instruccion.parametro2);
        case F_READ:
            _f_read(instruccion.parametro1, instruccion.parametro2);
            break;
        case F_WRITE:
            _f_write(instruccion.parametro1, instruccion.parametro2);
            break;
        case F_TRUNCATE:
            _f_truncate(instruccion.parametro1, instruccion.parametro2);
            break;
        case EXIT:
            __exit();
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

// TODO: recibir por interrupt interrupciones
bool check_interrupt()
{
    return false;
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
