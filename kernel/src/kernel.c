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

    planificar_largo_plazo();
    planificar_corto_plazo();
    
}

void detener_planificacion(){

}

void safe_pcb_add(t_list* list, t_pcb* pcb, pthread_mutex_t* mutex){
	pthread_mutex_lock(mutex);
	list_add(list, pcb);
	pthread_mutex_unlock(mutex);
}

t_pcb* safe_pcb_remove(t_list* list, pthread_mutex_t* mutex){
	t_pcb* pcb;
	pthread_mutex_lock(mutex);
	pcb = list_remove(list, 0);
	pthread_mutex_unlock(mutex);
	return pcb;
}
void pcb_create() {
	t_pcb *pcb = malloc(sizeof(t_pcb));
	pcb->archivos_abiertos = list_create();
	pcb->pid = generador_de_id;
    generador_de_id++;
	pcb->program_counter = 0;
    pcb->estado = NEW;
    safe_pcb_add(cola_listos_para_ready, pcb, &mutex_cola_listos_para_ready);
	
}

/*t_pcb* elegir_pcb_segun_algoritmo(){
	switch (ALGORITMO_PLANIFICACION) {
	case FIFO:
		return safe_pcb_remove(lista_ready, &mutex_cola_ready);
	case RR:
		return algo;
    case PRIORIDADES:
    return    algo; 
	default:
		exit(1);
	}
}*/
char *estado_to_string(estado_proceso estado)
{
    switch (estado)
    {
    case NEW:
        return "NEW";
        break;
    case READY:
        return "READY";
        break;
    case BLOCK:
        return "BLOCK";
        break;
    case EXEC:
        return "EXEC";
        break;
    case FINISH_EXIT:
        return "EXIT";
        break;
    case FINISH_ERROR:
        return "EXIT_ERROR";
        break;
    default:
        return "UNKNOWN";
        break;
    }
}
void cambiar_estado(t_pcb *pcb, estado_proceso nuevo_estado) {
	if(pcb->estado != nuevo_estado){
		char *nuevo_estado_string = strdup(estado_to_string(nuevo_estado));
		char *estado_anterior_string = strdup(estado_to_string(pcb->estado));
		pcb->estado = nuevo_estado;
		free(estado_anterior_string);
		free(nuevo_estado_string);
	}
}

void procesar_cambio_estado(t_pcb* pcb, estado_proceso estado_nuevo){

	switch(estado_nuevo){
	case READY:

		break;
	case FINISH_EXIT:
	
		break;
	case FINISH_ERROR:
	
		break;

    case BLOCK:

    break;
	default:
		
		break;
	}
}

void planificar_largo_plazo() {
	//pthread_t hilo_ready;
	//pthread_t hilo_exit;
	//pthread_t hilo_block;
	
}

void planificar_corto_plazo() {
	//pthread_t hilo_corto_plazo;
	
}

void asignar_algoritmo(char *algoritmo) {
	if (strcmp(algoritmo, "FIFO") == 0) {
		ALGORITMO_PLANIFICACION = FIFO;
	} else if (strcmp(algoritmo, "RR") == 0) {
		ALGORITMO_PLANIFICACION = RR;
	}else{
		ALGORITMO_PLANIFICACION = PRIORIDADES;
	}
}

void set_pcb_ready(t_pcb *pcb) {
	pthread_mutex_lock(&mutex_cola_ready);
	cambiar_estado(pcb, READY);
	list_add(lista_ready, pcb);
	pthread_mutex_unlock(&mutex_cola_ready);
}

