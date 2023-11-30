#include "memoria.h"

#define MAX_LRU 999999

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
	inicializar_memoria();

	server_memoria = iniciar_servidor(logger_memoria_info, config_valores_memoria.ip_escucha, config_valores_memoria.puerto_escucha);
	log_info(logger_memoria_info, "Servidor MEMORIA Iniciado");

	atender_clientes_memoria();
	while (1)
		;
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

void inicializar_memoria()
{

	memoria_usuario = malloc(config_valores_memoria.tamanio_memoria);
	tamanio_memoria = config_valores_memoria.tamanio_memoria;
	if (memoria_usuario == NULL)
	{
		log_error(logger_memoria_info, "No pudo reservarse el espacio de memoria contiguo, abortando modulo");
		abort();
	}
	indice_tabla = 0;
	inicializar_marcos();
	// tablas_de_paginas = list_create(); // para acceder a las tablas de paginas ingreso al proceso
	procesos_totales = list_create();
	algoritmo_pags = obtener_algoritmo();

	pthread_mutex_init(&mutex_procesos, NULL);
	pthread_mutex_init(&mutex_memoria_usuario, NULL);
	pthread_mutex_init(&mutex_marcos, NULL);
}

void atender_clientes_memoria()
{
	atender_cliente_cpu();
	atender_cliente_fs();
	atender_cliente_kernel();
}

int atender_cliente_cpu()
{

	socket_cpu = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_cpu != -1)
	{

		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_cpu, (void *)&socket_cpu);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes para CPU... Finalizando servidor \n"); // log para fallo de comunicaciones
		abort();
		// finalizar_memoria();
	}
	return 0;
}

int atender_cliente_kernel()
{

	socket_kernel = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_kernel != -1)
	{
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_kernel, (void *)&socket_kernel);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes en kernel... Finalizando servidor \n"); // log para fallo de comunicaciones
		abort();
		// finalizar_memoria();
	}
	return 0;
}

int atender_cliente_fs()
{

	socket_fs = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_fs != -1)
	{
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_filesystem, (void *)&socket_fs);
		pthread_detach(hilo_cliente);
		return 1;
	}
	else
	{
		log_error(logger_memoria_info, "Error al escuchar clientes... Finalizando servidor \n"); // log para fallo de comunicaciones
		abort();
	}
	return 0;
}

void *manejo_conexion_cpu(void *arg)
{

	socket_cpu_int = *(int *)arg;
	while (1)
	{
		op_code codigo_operacion = recibir_operacion(socket_cpu_int);

		log_info(logger_memoria_info, "Se recibio una operacion de CPU: %d", codigo_operacion);
		switch (codigo_operacion)
		{
		case HANDSHAKE_CPU:
			log_info(logger_memoria_info, "Handshake exitosa con CPU, se conecto un CPU");

			recibir_handshake(socket_cpu_int, logger_memoria_info);
			realizar_handshake(socket_cpu_int, HANDSHAKE_MEMORIA, logger_memoria_info);

			send_page_size(config_valores_memoria.tamanio_pagina, socket_cpu_int);
			break;
		case PEDIDO_INSTRUCCION:
			uint32_t pid, pc;
			pedido_instruccion(&pid, &pc, socket_cpu_int);
			t_instruccion *instruccion_pedida = obtener_instruccion_pid_pc(pid, pc);
			printf("La instruccion de PC %d para el PID %d es: %s - %s - %s \n", pc, pid, obtener_nombre_instruccion(instruccion_pedida->codigo), instruccion_pedida->parametro1, instruccion_pedida->parametro2);
			enviar_instruccion(socket_cpu_int, instruccion_pedida);
			break;
		case MARCO:
			int pid_tr, index, marco;
			recibir_pedido_marco(&pid_tr, &index, socket_cpu_int);
			marco = obtener_marco_pid(pid_tr, index);

			if (marco > -1)
			{
				enviar_marco_cpu(marco, socket_cpu_int, MARCO);
			}
			else
			{
				enviar_marco_cpu(marco, socket_cpu_int, PAGE_FAULT);
			}
			break;

		default:
			log_error(logger_memoria_info, "Fallo la comunicacion CON CPU. Abortando \n");
			// close(socket_cpu_int);
			// close(server_memoria);
			// abort();
			//  finalizar_memoria();
			break;
		}
	}
}

void *manejo_conexion_kernel(void *arg)
{

	socket_kernel_int = *(int *)arg;
	while (1)
	{

		op_code codigo_operacion = recibir_operacion(socket_kernel_int);

		log_info(logger_memoria_info, "Se recibio una operacion de KERNEL: %d", codigo_operacion);
		switch (codigo_operacion)
		{
		case HANDSHAKE_KERNEL:
			log_info(logger_memoria_info, "Handshake exitosa con KERNEL, se conecto un KERNEL");
			recibir_handshake(socket_kernel_int, logger_memoria_info);
			realizar_handshake(socket_kernel_int, HANDSHAKE_MEMORIA, logger_memoria_info);
			break;
		case INICIALIZAR_PROCESO:
			log_info(logger_memoria_info, " Inicializando estructuras para nuevo proceso");
			proceso_memoria = recibir_proceso_nuevo(socket_kernel_int);
			proceso_memoria = iniciar_proceso_path(proceso_memoria); // inicio en memoria y en swap
			log_info(logger_memoria_info, "Se creo correctamente el proceso PID [%d]", proceso_memoria->pid);

			// TODO -- RESPONDERLE AL KERNEL QUE SE CREO OK.
			break;
		case FINALIZAR_PROCESO:

			int pid_a_finalizar;
			recibir_pid(socket_kernel_int, &pid_a_finalizar);
			t_proceso_memoria *proceso_a_eliminar = obtener_proceso_pid(pid_a_finalizar);
			liberar_marcos_proceso(pid_a_finalizar); // libero marcos ocupados por el proceso

			limpiar_swap(proceso_a_eliminar);			  // listo los bloques swap y se los envio a FS para que los marque como libre
			eliminar_proceso_memoria(proceso_a_eliminar); // Libero las entradas de la tabla de pagina y lo elimino de la lista de procesos -- 

			// TODO -- RESPONDERLE al kernel que finalizo OK el proceso.
			break;
		default:
			log_error(logger_memoria_info, "Fallo la comunicacion con KERNEL. Abortando");
			finalizar_memoria();
			break;
		}
	}
	return NULL;
}

void *manejo_conexion_filesystem(void *arg)
{

	socket_fs_int = *(int *)arg;
	while (1)
	{

		op_code codigo_operacion = recibir_operacion(socket_fs_int);

		log_info(logger_memoria_info, "Se recibio una operacion de FS: %d", codigo_operacion);

		switch (codigo_operacion)
		{
		case HANDSHAKE_FILESYSTEM:
			log_info(logger_memoria_info, "Handshake exitosa con FILESYSTEM, se conecto un FILESYSTEM");
			recibir_handshake(socket_fs_int, logger_memoria_info);
			realizar_handshake(socket_fs_int, HANDSHAKE_MEMORIA, logger_memoria_info);
			break;
		default:
			log_error(logger_memoria_info, "Fallo la comunicacion. Abortando \n");
			abort();
			// finalizar_memoria();
			break;
		}
	}
	return NULL;
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
		else if (string_equals_ignore_case(palabras[0], "SUM"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SUM, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "SUB"))
		{
			list_add(instrucciones, armar_estructura_instruccion(SUB, palabras[1], palabras[2]));
		}
		else if (string_equals_ignore_case(palabras[0], "JNZ"))
		{
			list_add(instrucciones, armar_estructura_instruccion(JNZ, palabras[1], palabras[2]));
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
			list_add(instrucciones, armar_estructura_instruccion(EXIT, "", ""));
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

char *leer_archivo(char *unPath)
{
	char instrucciones[100];
	strcpy(instrucciones, unPath);

	FILE *archivo = fopen(instrucciones, "r");

	if (archivo == NULL)
	{
		perror("Error al abrir el archivo.\n");
	}
	fseek(archivo, 0, SEEK_END);		 // mover el archivo al final
	int cant_elementos = ftell(archivo); // cantidad total de elementos que tiene el archivo
	rewind(archivo);					 // mover archivo al inicio del txt

	char *cadena = calloc(cant_elementos + 1, sizeof(char)); // arreglo dinamico de caracteres para almacenar en cadena el contenido del archivo
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
	log_info(logger_memoria_info, "Archivo de instrucciones leido.");
	return cadena;
}

t_instruccion *armar_estructura_instruccion(nombre_instruccion id, char *parametro1, char *parametro2)
{
	t_instruccion *estructura = (t_instruccion *)malloc(sizeof(t_instruccion));
	estructura->codigo = id;
	estructura->parametro1 = (parametro1[0] != '\0') ? strdup(parametro1) : parametro1;
	estructura->parametro2 = (parametro2[0] != '\0') ? strdup(parametro2) : parametro2;
	estructura->longitud_parametro1 = strlen(estructura->parametro1) + 1;
	estructura->longitud_parametro2 = strlen(estructura->parametro2) + 1;
	printf("%s - %s - %s \n", obtener_nombre_instruccion(estructura->codigo), estructura->parametro1, estructura->parametro2);

	return estructura;
}

t_proceso_memoria *iniciar_proceso_path(t_proceso_memoria *proceso_nuevo)
{
	proceso_nuevo->instrucciones = parsear_instrucciones(proceso_nuevo->path);
	log_info(logger_memoria_info, "Instrucciones parseadas OK para el proceso %d", proceso_nuevo->pid);
	list_add(procesos_totales, proceso_nuevo);
	inicializar_nuevo_proceso(proceso_nuevo);

	return proceso_nuevo;
}

t_proceso_memoria *obtener_proceso_pid(uint32_t pid_pedido)
{
	bool _proceso_id(void *elemento)
	{
		return ((t_proceso_memoria *)elemento)->pid == pid_pedido;
	}
	t_proceso_memoria *proceso_elegido;
	proceso_elegido = list_find(procesos_totales, _proceso_id);
	return proceso_elegido;
}

// INSTRUCCIONES PROCESO

t_instruccion *obtener_instrccion_pc(t_proceso_memoria *proceso, uint32_t pc_pedido)
{
	return list_get(proceso->instrucciones, pc_pedido);
}

t_instruccion *obtener_instruccion_pid_pc(uint32_t pid_pedido, uint32_t pc_pedido)
{
	log_error(logger_memoria_info, "Voy a buscar la instruccion de PID %d con PC %d", pid_pedido, pc_pedido);
	t_proceso_memoria *proceso = obtener_proceso_pid(pid_pedido);
	sleep(config_valores_memoria.retardo_respuesta / 1000);
	return obtener_instrccion_pc(proceso, pc_pedido);
}

// MARCOS

t_list *obtener_marcos_pid(uint32_t pid_pedido)
{

	pthread_mutex_lock(&mutex_marcos);
	t_list *marcos_proceso = (t_list *)list_filter(marcos, mismo_pid_marco);
	pthread_mutex_unlock(&mutex_marcos);
	return marcos_proceso;
}

bool mismo_pid_marco(t_marco *marco, int pid)
{
	return marco->pid == pid;
}

bool es_marco_libre(void *elemento)
{
	t_marco *marco = (t_marco *)elemento;
	return marco->pid == -1;
}

bool hay_marcos_libres()
{
	return list_count_satisfying(marcos, es_marco_libre) > 0;
}

int asignar_marco_libre(uint32_t pid_pedido)
{
	bool _marco_libre(void *elemento)
	{
		return ((t_marco *)elemento)->pid == -1;
	}
	t_marco *marco_libre;
	pthread_mutex_lock(&mutex_marcos);
	marco_libre = list_find(marcos, _marco_libre);
	marco_libre->pid = pid_pedido;
	pthread_mutex_unlock(&mutex_marcos);

	return marco_libre->num_de_marco;
}

void liberar_marco_indice(int marco_a_liberar)
{

	pthread_mutex_lock(&mutex_marcos);
	t_marco *marco_ocupado = list_get(marcos, marco_a_liberar);
	marco_ocupado->pid = -1;
	pthread_mutex_unlock(&mutex_marcos);
}

void liberar_marcos_proceso(uint32_t pid_a_liberar)
{
	log_info(logger_memoria_info, "Liberando los marcos ocupados por el PID [%d]", pid_a_liberar);

	t_list *marcos_proceso = obtener_marcos_pid(pid_a_liberar);
	for (int i = 0; i < list_size(marcos_proceso); i++)
	{
		t_marco *marco_proximamente_libre = list_get(marcos, i);
		liberar_marco_indice(marco_proximamente_libre->num_de_marco);
	}
}

void recibir_pedido_marco(int *pid_tr, int *index, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	printf("size del stream a deserializar \n%d", size);
	memcpy(pid_tr, buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(index, buffer + offset, sizeof(int));

	free(buffer);
}

int obtener_marco_pid(int pid_pedido, int entrada)
{
	t_proceso_memoria *proceso = obtener_proceso_pid((uint32_t)pid_pedido);
	t_entrada_tabla_pag *entrada_tabla = (t_entrada_tabla_pag *)list_get(proceso->tabla_paginas->entradas_tabla, entrada);

	log_info(logger_memoria_info, "Se obtiene la pagina [%d] del PID [%d]", entrada_tabla->indice, proceso->pid);

	log_info(logger_memoria_info, "PID: [%d] - Pagina: [%d] - MARCO: [%d]", proceso->pid, entrada_tabla->indice, entrada_tabla->marco); // LOG OBLIGATORIO

	if (entrada_tabla->bit_presencia)
	{
		log_info(logger_memoria_info, "Pagina presente en memoria, no hay PF");
		return entrada_tabla->marco;
	}
	else // HAY PAGE FAULT
	{
		log_info(logger_memoria_info, "PAGE FAULT / LA PAGINA TIENE EL BIT DE PRESENCIA EN 0");
		return -1;
	}
}

void enviar_marco_cpu(int marco, int socket, op_code codigo)
{

	t_paquete *paquete_marco = crear_paquete_con_codigo_de_operacion(codigo);
	paquete_marco->buffer->size += sizeof(int);
	paquete_marco->buffer->stream = realloc(paquete_marco->buffer->stream, paquete_marco->buffer->size);
	memcpy(paquete_marco->buffer->stream, &(marco), sizeof(int));
	enviar_paquete(paquete_marco, socket);
	eliminar_paquete(paquete_marco);
}

// CREAR PROCESO

int paginas_necesarias_proceso(uint32_t tamanio_proc, uint32_t tamanio_pag)
{
	double entero = ((double)tamanio_proc) / (double)tamanio_pag;
	if ((tamanio_proc % tamanio_pag) > 0)
	{
		entero++;
	}
	return (int)ceil(entero);
}

t_proceso_memoria *recibir_proceso_nuevo(int socket)
{

	int size;
	void *buffer;

	buffer = recibir_buffer(&size, socket);
	printf("Size del stream a deserializar: %d \n", size);

	t_proceso_memoria *proceso_nuevo = malloc(sizeof(t_proceso_memoria));

	int offset = 0;

	memcpy(&(proceso_nuevo->pid), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	memcpy(&(proceso_nuevo->tamano), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	uint32_t long_path;

	memcpy(&(long_path), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	proceso_nuevo->path = malloc(long_path);
	memcpy(proceso_nuevo->path, buffer + offset, long_path);

	printf("mi path recibido es %s \n", proceso_nuevo->path); // TODO - BORRAR

	return proceso_nuevo;
}


void inicializar_nuevo_proceso(t_proceso_memoria *proceso_nuevo)
{
	int q_pags = inicializar_estructuras_memoria_nuevo_proceso(proceso_nuevo);
	pedido_inicio_swap(q_pags, socket_fs_int);

	resp_code_fs = recibir_operacion(socket_fs_int);
	if (resp_code_fs == LISTA_BLOQUES_SWAP)
	{
		t_list *lista_id_bloques = recibir_listado_id_bloques(socket_fs_int);
		asignar_id_bloque_swap(proceso_nuevo, lista_id_bloques);
		list_destroy_and_destroy_elements(lista_id_bloques, free);
	}
	else
	{
		log_error(logger_memoria_info, "Se recibio un codigo de operacion de FS distinto a LISTA_BLOQUES_SWAP al pedido de inicio de SWAP para el proceso PID [%d]", proceso_nuevo->pid);
	}
}

// Inicializo la tabla de paginas y devuelvo la cantidad de paginas para inicializar el swap en fs.
int inicializar_estructuras_memoria_nuevo_proceso(t_proceso_memoria *proceso_nuevo)
{

	// proceso_nuevo->bloques_swap = list_create(); BORRAR - los id de bloques los obtengo recorriendo las paginas para no redundar la info
	proceso_nuevo->tabla_paginas = malloc(sizeof(t_tabla_paginas));
	proceso_nuevo->tabla_paginas->entradas_tabla = list_create();

	cantidad_pags = paginas_necesarias_proceso(proceso_nuevo->tamano, config_valores_memoria.tamanio_pagina);

	proceso_nuevo->tabla_paginas->cantidad_paginas = cantidad_pags;

	for (int i = 0; i < cantidad_pags; i++)
	{
		t_entrada_tabla_pag *entrada = malloc(sizeof(t_entrada_tabla_pag));
		entrada->indice = i;
		entrada->marco = -1;
		entrada->bit_presencia = 0;
		entrada->bit_modificado = 0;
		entrada->id_bloque_swap = 0;
		entrada->tiempo_lru = MAX_LRU;
	}

	log_info(logger_memoria_info, "CREACION DE TABLA DE PAGINAS - PID [%d] - Tamano: [%d]", proceso_nuevo->pid, proceso_nuevo->tabla_paginas->cantidad_paginas);

	return cantidad_pags;
}

void asignar_id_bloque_swap(t_proceso_memoria *proceso_nuevo, t_list *lista_id_bloques)
{
	int cant_bloques = list_size(proceso_nuevo->tabla_paginas->entradas_tabla);

	for (int i = 0; i < cant_bloques; i++)
	{
		t_entrada_tabla_pag *entrada = list_get(proceso_nuevo->tabla_paginas->entradas_tabla, i);
		int *id_bloque = list_get(lista_id_bloques, i);
		entrada->id_bloque_swap = *id_bloque;
	}
	log_warning(logger_memoria_info, "Se asignaron OK los id de los bloques SWAP para el PID [%d]", proceso_nuevo->pid);
}

void pedido_inicio_swap(int cant_pags, int socket)
{

	t_paquete *paquete_pedido_swap = crear_paquete_con_codigo_de_operacion(INICIO_SWAP);
	paquete_pedido_swap->buffer->size += sizeof(uint32_t);
	paquete_pedido_swap->buffer->stream = realloc(paquete_pedido_swap->buffer->stream, paquete_pedido_swap->buffer->size);
	memcpy(paquete_pedido_swap->buffer->stream, &(cant_pags), sizeof(uint32_t));
	enviar_paquete(paquete_pedido_swap, socket);
	eliminar_paquete(paquete_pedido_swap);
}

t_algoritmo obtener_algoritmo()
{
	if (string_equals_ignore_case(config_valores_memoria.algoritmo_reemplazo, "FIFO"))
	{
		log_info(logger_memoria_info, "Se elegio algotitmo FIFO para reemplazo de paginas");
		return FIFO;
	}
	else
	{
		log_info(logger_memoria_info, "Se elegio algotitmo para reemplazo de paginas");
		return LRU;
	}
}

double marcosTotales()
{
	return (double)(config_valores_memoria.tamanio_memoria) / (double)(config_valores_memoria.tamanio_memoria);
}

void inicializar_marcos()
{
	marcos = list_create();
	for (int i = 0; i < marcosTotales(); i++)
	{
		t_marco *entradaMarco = malloc(sizeof(t_marco));
		entradaMarco->pid = -1;
		entradaMarco->num_de_marco = i;
		list_add(marcos, entradaMarco);
	}
}

// FINALIZAR PROCESO

void limpiar_swap(t_proceso_memoria *proceso_a_eliminar) // listo los bloques swap y se los envio a FS para que los marque como libre
{
	t_list *ids_bloques_swap = obtener_lista_id_bloque_swap(proceso_a_eliminar);
	enviar_bloques_swap_a_liberar(ids_bloques_swap, socket_fs_int);
	list_destroy_and_destroy_elements(ids_bloques_swap, free);
}

t_list *obtener_lista_id_bloque_swap(t_proceso_memoria *proceso)
{
	t_list *lista_id_bloques_swap = list_create();

	int cant_bloques = list_size(proceso->tabla_paginas->entradas_tabla);

	for (int i = 0; i < cant_bloques; i++)
	{
		t_entrada_tabla_pag *entrada = list_get(proceso->tabla_paginas->entradas_tabla, i);
		int *id_bloque = malloc(sizeof(int));
		*id_bloque = entrada->id_bloque_swap;
		list_add(lista_id_bloques_swap, id_bloque);
	}

	return lista_id_bloques_swap;
}

void enviar_bloques_swap_a_liberar(t_list *lista_bloques, int socket)
{
	t_paquete *paquete_swap_a_borrar = crear_paquete_con_codigo_de_operacion(SWAP_A_LIBERAR);
	serializar_lista_swap(lista_bloques,paquete_swap_a_borrar);
	enviar_paquete(paquete_swap_a_borrar, socket);
	eliminar_paquete(paquete_swap_a_borrar);

}

void eliminar_proceso_memoria(t_proceso_memoria *proceso_a_eliminar) // Libero las entradas de la tabla de pagina y lo elimino de la lista de procesos
{

	// TODO -- Eliminar las paginas del proceso.

	log_info(logger_memoria_info,"DESTRUCCION TABLA DE PAGINAS - PID [%d] - Tamano [%d]", proceso_a_eliminar->pid, proceso_a_eliminar->tabla_paginas->cantidad_pagina); // LOG OBLIGATORIO
}

void finalizar_memoria()
{
	log_info(logger_memoria_info, "Finalizando Memoria");
	log_destroy(logger_memoria_info);
	close(server_memoria);
	close(socket_cpu_int);
	close(socket_kernel_int);
	close(socket_fs_int);
	config_destroy(config);
}