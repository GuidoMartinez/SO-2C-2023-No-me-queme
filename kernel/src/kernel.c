#include "kernel.h"

void sighandler(int s)
{
    finalizar_kernel();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    if (argc < 1)
    {
        // para ejecutar el kernel es: ./kernel ./cfg/kernel.config
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }

    kernel_logger_info = log_create("./cfg/kernel.log", "KERNEL", true, LOG_LEVEL_INFO);

    cargar_configuracion(argv[1]);

    // CONEXION CON MEMORIA
    conexion_memoria = crear_conexion(config_valores_kernel.ip_memoria, config_valores_kernel.puerto_memoria);

    t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_KERNEL);
    enviar_paquete(paquete_handshake, conexion_memoria);
    eliminar_paquete(paquete_handshake);
    // Recibo handshake de memoria
    op_code recibir_cod_op = recibir_operacion(conexion_memoria);
    if (recibir_cod_op == MENSAJE)
    {
        char *mensaje = recibir_mensaje(conexion_memoria, kernel_logger_info);
        free(mensaje);
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }

    // CONEXION CON CPU - DISPATCH
    conexion_cpu_dispatch = crear_conexion(config_valores_kernel.ip_cpu, config_valores_kernel.puerto_cpu_dispatch);

    paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_CPU_DISPATCH);
    enviar_paquete(paquete_handshake, conexion_cpu_dispatch);
    eliminar_paquete(paquete_handshake);
    // Recibo handshake de cpu dispatch
    recibir_cod_op = recibir_operacion(conexion_cpu_dispatch);
    if (recibir_cod_op == MENSAJE)
    {
        char *mensaje = recibir_mensaje(conexion_cpu_dispatch, kernel_logger_info);
        free(mensaje);
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta del cpu para DISPATCH.");
    }

    // CONEXION CON CPU - INTERRUPT
    conexion_cpu_interrupt = crear_conexion(config_valores_kernel.ip_cpu, config_valores_kernel.puerto_cpu_interrupt);

    paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_CPU_INTERRUPT);
    enviar_paquete(paquete_handshake, conexion_cpu_interrupt);
    eliminar_paquete(paquete_handshake);
    // Recibo handshake de cpu interrupt
    recibir_cod_op = recibir_operacion(conexion_cpu_interrupt);
    if (recibir_cod_op == MENSAJE)
    {
        char *mensaje = recibir_mensaje(conexion_cpu_interrupt, kernel_logger_info);
        free(mensaje);
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta del cpu para INTERRUPT");
    }

    // conexion FILESYSTEM
    conexion_filesystem = crear_conexion(config_valores_kernel.ip_filesystem, config_valores_kernel.puerto_filesystem);

    paquete_handshake = crear_paquete_con_codigo_de_operacion(HANDSHAKE_KERNEL);
    enviar_paquete(paquete_handshake, conexion_filesystem);
    eliminar_paquete(paquete_handshake);
    // Recibo handshake de filesystem
    recibir_cod_op = recibir_operacion(conexion_filesystem);
    if (recibir_cod_op == MENSAJE)
    {
        char *mensaje = recibir_mensaje(conexion_filesystem, kernel_logger_info);
        free(mensaje);
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta del FILESYSTEM.");
    }

    log_warning(kernel_logger_info, "me conecte OK A TODOS LADOS, NO TENGO NADA QUE HACER");
}

void cargar_configuracion(char *path)
{

    t_config *config = config_create(path); // Leo el archivo de configuracion

    if (config == NULL)
    {
        perror("Archivo de configuracion de kernel no encontrado \n");
        abort();
    }

    config_valores_kernel.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    config_valores_kernel.ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
    config_valores_kernel.ip_cpu = config_get_string_value(config, "IP_CPU");
    config_valores_kernel.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    config_valores_kernel.puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    config_valores_kernel.puerto_cpu_interrupt = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    config_valores_kernel.puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");
    config_valores_kernel.quantum = config_get_int_value(config, "QUANTUM");
    config_valores_kernel.algoritmo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    config_valores_kernel.grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION_INI");
    config_valores_kernel.recursos = config_get_array_value(config, "RECURSOS");
    config_valores_kernel.instancias_recursos = config_get_array_value(config, "INSTANCIAS_RECURSOS");
}

void finalizar_kernel()
{ // Chequear todo lo que deberia finalizarse al tirar Control + C
    log_info(kernel_logger_info, "Finalizando el modulo Kernel");
    log_destroy(kernel_logger_info);
    close(conexion_cpu_dispatch);
    close(conexion_cpu_interrupt);
    close(conexion_memoria);
    close(conexion_filesystem);
}

void iniciar_proceso(){
    generador_de_id = 0;

}

void finalizar_proceso(){
    
}

void iniciar_planificacion(){
    
}

void detener_planificacion(){

}

t_pcb* pcb_create() {
	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->archivos_abiertos = list_create();
	pcb->pid = generador_de_id;
    generador_de_id++;
	pcb->program_counter = 0;
    pcb->estado = NEW;

	return pcb;
}