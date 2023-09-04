#include "filesystem.h"

void sighandler(int s)
{
    finalizar_filesystem();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    if (argc < 1)
    {
        // para ejecutar el fs es: ./filesyste ./cfg/fs.cfg
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }
    filesystem_logger_info = log_create("./cfg/filesystem.log", "FILESYSTEM", true, LOG_LEVEL_INFO);
    cargar_configuracion(argv[1]);

    socket_memoria = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_FILESYSTEM);
    enviar_paquete(paquete_handshake, socket_memoria);
    eliminar_paquete(paquete_handshake);

    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == MENSAJE)
    {
        char *mensaje = recibir_mensaje(socket_memoria, filesystem_logger_info);
        free(mensaje);
    }
    else
    {
        log_warning(filesystem_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }

    server_filesystem = iniciar_servidor(filesystem_logger_info, config_valores_filesystem.ip_escucha, config_valores_filesystem.puerto_escucha);
    socket_kernel = esperar_cliente(server_filesystem, filesystem_logger_info);
    log_info(filesystem_logger_info, "Filesystem listo para recibir al Kernel");
    op_code codigo_operacion = recibir_operacion(socket_kernel);

    switch (codigo_operacion)
    {
    case HANDSHAKE_KERNEL:
        log_info(filesystem_logger_info, "Handshake exitoso con KERNEL, se conecto un KERNEL");
        enviar_mensaje("Handshake exitoso con Filesystem", socket_kernel);
        break;
    default:
        log_error(filesystem_logger_info, "Fallo la comunicacion. Abortando \n");
        // finalizar_filesystem();
        break;
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
    config_valores_filesystem.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    config_valores_filesystem.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    config_valores_filesystem.ip_escucha = config_get_string_value(config, "IP_ESCUCHA");
    config_valores_filesystem.puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    config_valores_filesystem.path_fat = config_get_string_value(config, "PATH_FAT");
    config_valores_filesystem.path_bloques = config_get_string_value(config, "PATH_BLOQUES");
    config_valores_filesystem.path_fcb = config_get_string_value(config, "PATH_FCB");
    config_valores_filesystem.cant_bloques_total = config_get_int_value(config, "CANT_BLOQUES_TOTAL");
    config_valores_filesystem.cant_bloques_swap = config_get_int_value(config, "CANT_BLOQUES_SWAP");
    config_valores_filesystem.tam_bloque = config_get_int_value(config, "TAM_BLOQUE");
    config_valores_filesystem.retardo_acceso_bloque = config_get_int_value(config, "RETARDO_ACCESO_BLOQUE");
    config_valores_filesystem.retardo_acceso_fat = config_get_int_value(config, "RETARDO_ACCESO_FAT");
}

void finalizar_filesystem()
{
    log_info(filesystem_logger_info, "Finalizando el modulo FILESYSTEM");
    log_destroy(filesystem_logger_info);
    config_destroy(config);
    close(socket_memoria);
}