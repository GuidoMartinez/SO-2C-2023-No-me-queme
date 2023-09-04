#include "cpu.h"

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

    socket_memoria = crear_conexion(config_valores_cpu.ip_memoria, config_valores_cpu.puerto_memoria);
    t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_CPU);
    enviar_paquete(paquete_handshake, socket_memoria);
    eliminar_paquete(paquete_handshake);

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


    servidor_cpu_dispatch = iniciar_servidor(cpu_logger_info, config_valores_cpu.ip_escucha, config_valores_cpu.puerto_escucha_dispatch);
    servidor_cpu_interrupt = iniciar_servidor(cpu_logger_info, config_valores_cpu.ip_escucha, config_valores_cpu.puerto_escucha_interrupt);
    log_info(cpu_logger_info, "CPU listo para recibir al Kernel");


    conexion_kernel_dispatch = esperar_cliente(servidor_cpu_dispatch, cpu_logger_info);
    codigo_operacion = recibir_operacion(conexion_kernel_dispatch);
    switch (codigo_operacion)
    {
    case HANDSHAKE_CPU_DISPATCH:
        log_info(cpu_logger_info, "Handshake exitoso con KERNEL para la conexion DISPATCH");
        enviar_mensaje("Handshake para DISPATCH existoso con CPU", conexion_kernel_dispatch);
        break;
    default:
        log_error(cpu_logger_info, "Fallo la comunicacion. Abortando \n");
        // finalizar_cpu();
        break;
    }

    conexion_kernel_interrupt = esperar_cliente(servidor_cpu_interrupt, cpu_logger_info);
    codigo_operacion = recibir_operacion(conexion_kernel_interrupt);
    switch (codigo_operacion)
    {
    case HANDSHAKE_CPU_INTERRUPT:
        log_info(cpu_logger_info, "Handshake exitoso con KERNEL para la conexion INTERRUPT");
        enviar_mensaje("Handshake para INTERRUPT existoso con CPU", conexion_kernel_interrupt);
        break;
    default:
        log_error(cpu_logger_info, "Fallo la comunicacion. Abortando \n");
        // finalizar_cpu();
        break;
    }

    return EXIT_SUCCESS;
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
