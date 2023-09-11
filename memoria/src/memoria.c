#include "memoria.h"

void sighandler(int s)
{
	finalizar_memoria();
	exit(0);
}

int main(int argc, char **argv)
{

	signal(SIGINT, sighandler);

	logger_memoria_info = log_create("./cfg/memoria.log", "MEMORIA", true, LOG_LEVEL_INFO);
	cargar_configuracion(argv[1]);

	server_memoria = iniciar_servidor(logger_memoria_info, config_valores_memoria.ip_escucha, config_valores_memoria.puerto_escucha);
	log_info(logger_memoria_info, "Servidor MEMORIA Iniciado");

	atender_clientes_memoria();
	// sleep(5);
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

void atender_clientes_memoria()
{

	socket_cpu = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel
	manejo_conexion_cpu((void *)&socket_cpu);
	socket_fs = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel
	manejo_conexion_filesystem((void *)&socket_fs);
	socket_kernel = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel
	manejo_conexion_kernel((void *)&socket_kernel);
}

int atender_cliente_cpu()
{

	socket_cpu = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_cpu != -1)
	{
		// log_info(logger_memoria_info, "Se conecto un cliente "); // TODO - BORRA LOG
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_cpu, (void *)&socket_cpu);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes... Finalizando servidor \n"); // log para fallo de comunicaciones
		finalizar_memoria();
	}
	return 0;
}

int atender_cliente_kernel()
{

	socket_kernel = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_kernel != -1)
	{
		// log_info(logger_memoria_info, "Se conecto un cliente "); // TODO - BORRA LOG
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_kernel, (void *)&socket_kernel);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes... Finalizando servidor \n"); // log para fallo de comunicaciones
		finalizar_memoria();
	}
	return 0;
}

int atender_cliente_fs()
{

	socket_fs = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_fs != -1)
	{
		// log_info(logger_memoria_info, "Se conecto un cliente "); // TODO - BORRA LOG
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_filesystem, (void *)&socket_fs);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes... Finalizando servidor \n"); // log para fallo de comunicaciones
		finalizar_memoria();
	}
	return 0;
}

void *manejo_conexion_cpu(void *arg)
{

	int socket_cpu_int = *(int *)arg;
	// while (1)
	//{

	op_code codigo_operacion = recibir_operacion(socket_cpu_int);

	switch (codigo_operacion)
	{
	case HANDSHAKE_CPU:
		log_info(logger_memoria_info, "Handshake exitosa con CPU, se conecto un CPU");
		enviar_mensaje("Handshake exitoso con Memoria", socket_cpu_int);
		send_page_size(config_valores_memoria.tamanio_pagina, socket_cpu_int);
		break;
	default:
		log_error(logger_memoria_info, "Fallo la comunicacion. Abortando \n");
		close(socket_cpu_int);
		break;
	}
	//}
}

void *manejo_conexion_kernel(void *arg)
{

	int socket_kernel_int = *(int *)arg;
	// while (1)
	//{

	op_code codigo_operacion = recibir_operacion(socket_kernel_int);

	switch (codigo_operacion)
	{
	case HANDSHAKE_KERNEL:
		log_info(logger_memoria_info, "Handshake exitoso con KERNEL, se conecto un KERNEL");
		enviar_mensaje("Handshake exitoso con Memoria", socket_kernel_int);

		t_proceso_memoria *proceso_nuevo = malloc(sizeof(t_proceso_memoria));
		proceso_nuevo->pid = 0;
		proceso_nuevo->tamano = 16;
		proceso_nuevo->path = string_new();
		string_append(&(proceso_nuevo->path), "./cfg/pseudocodigo");

		iniciar_proceso_path(proceso_nuevo);
		break;
	case INICIALIZAR_PROCESO:
		// recibir y deserializar para instanciar el t_ini_proceso;
		// inicializo t_ini_proceso para probar
		/*
		t_ini_proceso *proceso_nuevo = malloc(sizeof(t_ini_proceso));
		proceso_nuevo->pid = 0;
		proceso_nuevo->tamano = 16;
		proceso_nuevo->path = string_new();
		string_append(&(proceso_nuevo->path), "./cfg/pseudocodigo");

		iniciar_proceso_path(proceso_nuevo);
*/
		break;
	default:
		log_error(logger_memoria_info, "Fallo la comunicacion. Abortando \n");
		close(socket_kernel_int);
		break;
	}
	//}
}

void *manejo_conexion_filesystem(void *arg)
{

	int socket_fs_int = *(int *)arg;
	// while (1)
	//{

	op_code codigo_operacion = recibir_operacion(socket_fs_int);

	switch (codigo_operacion)
	{
	case HANDSHAKE_FILESYSTEM:
		log_info(logger_memoria_info, "Handshake exitosa con FILESYSTEM, se conecto un FS");
		enviar_mensaje("Handshake exitoso con Memoria", socket_fs_int);
		break;
	default:
		log_error(logger_memoria_info, "Fallo la comunicacion. Abortando \n");
		close(socket_fs_int);
		;
		break;
	}
	//}
}

void send_page_size(uint32_t tam_pagina, int socket)
{
	t_paquete *paquete_tam_pagina = crear_paquete_con_codigo_de_operacion(TAMANO_PAGINA);
	paquete_tam_pagina->buffer->size += sizeof(uint32_t);
	paquete_tam_pagina->buffer->stream = realloc(paquete_tam_pagina->buffer->stream, paquete_tam_pagina->buffer->size);
	memcpy(paquete_tam_pagina->buffer->stream, &(tam_pagina), sizeof(uint32_t));
	enviar_paquete(paquete_tam_pagina, socket);
	eliminar_paquete(paquete_tam_pagina);
}

t_list *parsear_instrucciones(char *path)
{
	t_list *instrucciones = list_create();
	char *pseudo_codigo_leido = leer_archivo(path);
	char **split_instrucciones = string_split(pseudo_codigo_leido, "\n");
	int indice_split = 0;
	while (split_instrucciones[indice_split] != NULL)
	{
		char **palabras = string_split(split_instrucciones[indice_split], " ");
		if (string_equals_ignore_case(palabras[0], "SET"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SET, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "ADD"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SET, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "SUB"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SET, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "MOV_IN"))
		{
			list_add(instrucciones, armar_estructura_instruccion(MOV_IN, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "MOV_OUT"))
		{
			list_add(instrucciones, armar_estructura_instruccion(MOV_OUT, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "SLEEP"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SLEEP, palabras[1], ""));
		}
		else if (string_equals_ignore_case(palabras[0], "WAIT"))
		{
			list_add(instrucciones, armar_estructura_instruccion(WAIT, palabras[1], ""));
		}
		else if (string_equals_ignore_case(palabras[0], "SIGNAL"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SIGNAL, palabras[1], ""));
		}
		else if (string_equals_ignore_case(palabras[0], "F_OPEN"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_OPEN, palabras[1], ""));
		}
		else if (string_equals_ignore_case(palabras[0], "F_TRUNCATE"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_TRUNCATE, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "F_SEEK"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_SEEK, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "F_WRITE"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_WRITE, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "F_READ"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_READ, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "F_CLOSE"))
		{
			list_add(instrucciones, armar_estructura_instruccion(F_CLOSE, palabras[1], ""));
		}
		else if (string_equals_ignore_case(palabras[0], "EXIT"))
		{
			list_add(instrucciones, armar_estructura_instruccion(EXIT, "",""));
		}
		indice_split++;
		string_iterate_lines(palabras, (void (*)(char *))free);
		free(palabras);
	}

	free(pseudo_codigo_leido);
	string_iterate_lines(split_instrucciones, (void (*)(char *))free);
	free(split_instrucciones);
	return instrucciones;
}

t_proceso_memoria *iniciar_proceso_path(t_proceso_memoria* proceso_nuevo) 
{
	// TODO - CHEQUEAR SI CORRESPONDE INICIALIZAR LAS LISTAS ACA TAMBIEN
	proceso_nuevo->bloques_swap = list_create();
	proceso_nuevo->tabla_paginas = list_create();
	proceso_nuevo->instrucciones = parsear_instrucciones(proceso_nuevo->path);
	log_warning(logger_memoria_info,"Instrucciones parseadas OK para el proceso %d",proceso_nuevo->pid);
	return proceso_nuevo;
}


char *leer_archivo(char *unPath) {
    char instrucciones[100];
    strcpy(instrucciones, unPath);

    FILE *archivo = fopen(instrucciones, "r");

    if (archivo == NULL)
    {
        perror("Error al abrir el archivo.\n");
    }
    fseek(archivo, 0, SEEK_END);         // mover el archivo al final
    int cant_elementos = ftell(archivo); // cantidad total de elementos que tiene el archivo
    rewind(archivo);                        //mover archivo al inicio del txt

    char *cadena = calloc(cant_elementos+1, sizeof(char)); //arreglo dinamico de caracteres para almacenar en cadena el contenido del archivo
    if (cadena == NULL)
    {
        perror("Error en la reserva de memoria \n");
        fclose(archivo);
        return NULL;
    }
    int cant_elementos_leidos = fread(cadena, sizeof(char), cant_elementos, archivo);
    if (cant_elementos_leidos != cant_elementos)
    {
        perror("Error leyendo el archivo.\n");
        fclose(archivo);
        free(cadena);
        return NULL;
    }

    fclose(archivo);
    log_info(logger_memoria_info,"Archivo de instrucciones leido.");
    return cadena;

}


t_instruccion *armar_estructura_instruccion(nombre_instruccion id, char* parametro1, char* parametro2) {
	t_instruccion *estructura = (t_instruccion *)malloc(sizeof(t_instruccion));
	estructura->codigo = id;
	estructura->parametro1 = (parametro1[0] != '\0') ? strdup(parametro1) : parametro1;
    estructura->parametro2 = (parametro2[0] != '\0') ? strdup(parametro2) : parametro2;
    estructura->longitud_parametro1 = strlen(estructura->parametro1) + 1;
    estructura->longitud_parametro2 = strlen(estructura->parametro2) + 1;
	printf("%s - %s - %s \n",obtener_nombre_instruccion(estructura->codigo),estructura->parametro1,estructura->parametro2);
	
	return estructura;
}



int paginas__necesarias_proceso(uint32_t tamanio_proc, uint32_t tamanio_pag)
{
	double entero = ((double)tamanio_proc) / (double)tamanio_pag;
	if ((tamanio_proc % tamanio_pag) > 0)
	{
		entero++;
	}
	return (int)ceil(entero);
}

void finalizar_memoria()
{
	log_destroy(logger_memoria_info);
	close(server_memoria);
	close(socket_cpu);
	close(socket_kernel);
	close(socket_fs);
	config_destroy(config);
}