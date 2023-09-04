#include "memoria.h"

void sighandler(int s)
{
    finalizar_memoria();
    exit(0);
}

int main(int argc, char **argv)
{

    signal(SIGINT, sighandler);
   /* if (argc < 1)
    {
        // para ejecutar la memoria es, es: ./memoria ./cfg/memoria.cfg
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }*/

    logger_memoria_info = log_create("./cfg/memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);
    cargar_configuracion(argv[1]);

    server_memoria = iniciar_servidor(logger_memoria_info, config_valores_memoria.ip_escucha, config_valores_memoria.puerto_escucha);
    log_info(logger_memoria_info, "Servidor MEMORIA Iniciado");
    
	while(atender_clientes_memoria(server_memoria));
	finalizar_memoria();
	return EXIT_SUCCESS;    
}



void cargar_configuracion(char *path)
{

    config = config_create(path); // Leo el archivo de configuracion

    if (config == NULL)
    {
        perror("Archivo de configuracion de memoria no encontrado \n");
        abort();
    }
    config_valores_memoria.puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");
    config_valores_memoria.ip_escucha = config_get_string_value(config, "IP_ESCUCHA");
    config_valores_memoria.ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
    config_valores_memoria.puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");
    config_valores_memoria.tamanio_memoria = config_get_int_value(config, "TAM_MEMORIA");
    config_valores_memoria.tamanio_pagina = config_get_int_value(config, "TAM_PAGINA");
    config_valores_memoria.path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");
    config_valores_memoria.retardo_respuesta = config_get_int_value(config, "RETARDO_RESPUESTA");
    config_valores_memoria.algoritmo_reemplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");
}

int atender_clientes_memoria(){

	int socket_cliente = esperar_cliente(server_memoria,logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if(socket_cliente != -1){
		//log_info(logger_memoria_info, "Se conecto un cliente "); // TODO - BORRA LOG
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexiones, (void*)&socket_cliente);
		pthread_detach(hilo_cliente);
		return 1;
	}else {
		log_error(logger_memoria_info, "Error al escuchar clientes... Finalizando servidor \n"); // log para fallo de comunicaciones
	}
	return 0;
}


void* manejo_conexiones(void* arg)
{   
    int sock_clientes = *(int*)arg;
	while(1) {

	op_code codigo_operacion = recibir_operacion(sock_clientes);

	switch(codigo_operacion){
		case HANDSHAKE_CPU:
			log_info(logger_memoria_info,"Handshake exitosa con CPU, se conecto un CPU");
			enviar_mensaje("Handshake exitoso con Memoria",sock_clientes);
			break;
		case HANDSHAKE_FILESYSTEM:
			log_info(logger_memoria_info,"Handshake exitosa con FILESYSTEM, se conecto un FS");
			enviar_mensaje("Handshake exitoso con Memoria",sock_clientes);
			break;
		case HANDSHAKE_KERNEL:
			log_info(logger_memoria_info,"Handshake exitoso con KERNEL, se conecto un KERNEL");
			enviar_mensaje("Handshake exitoso con Memoria",sock_clientes);
            break;               
		default:
            //log_error(logger_memoria_info, "Fallo la comunicacion. Abortando \n");
            //finalizar_memoria();
			break;
	}

	}
}

void finalizar_memoria()
{
    log_destroy(logger_memoria_info);
    close(server_memoria);
    config_destroy(config);
}