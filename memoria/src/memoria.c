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
		; // TODO -- BORRAR ESPERA ACTIVA
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
	config_valores_memoria.puerto_filesystem_swap = config_get_string_value(config, "PUERTO_FILESYSTEM_SWAP");
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
	paginas_utilizadas = list_create();
	algoritmo_pags = obtener_algoritmo();

	pthread_mutex_init(&mutex_procesos, NULL);
	pthread_mutex_init(&mutex_memoria_usuario, NULL);
	pthread_mutex_init(&mutex_marcos, NULL);
	pthread_mutex_init(&mutex_contador_LRU, NULL);
}

void atender_clientes_memoria()
{
	atender_cliente_cpu();
	atender_cliente_fs_swap();
	atender_cliente_fs_ops();
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

int atender_cliente_fs_swap()
{

	socket_fs = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_fs != -1)
	{
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_filesystem_swap, (void *)&socket_fs);
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

int atender_cliente_fs_ops()
{

	socket_fs_ops = esperar_cliente(server_memoria, logger_memoria_info); // se conecta primero cpu, segundo fs y 3ro kernel

	if (socket_fs_ops != -1)
	{
		pthread_t hilo_cliente;
		pthread_create(&hilo_cliente, NULL, manejo_conexion_filesystem_ops, (void *)&socket_fs_ops);
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

		// log_info(logger_memoria_info, "Se recibio una operacion de CPU: %d", codigo_operacion);
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
			log_info(logger_memoria_info, "Se envia la instruccion a CPU de PC %d para el PID %d y es: %s - %s - %s \n", pc, pid, obtener_nombre_instruccion(instruccion_pedida->codigo), instruccion_pedida->parametro1, instruccion_pedida->parametro2);
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
				enviar_marco_cpu(marco, socket_cpu_int, MARCO_PAGE_FAULT);
			}
			break;
		case MOV_OUT_CPU: // me pasa por parametro un uint32_t y tengo que guardarlo en el marco que me dice
			log_info(logger_memoria_info, "Me llego un MOV OUT DESDE CPU");
			uint32_t valor;
			int dir_fisica;
			recibir_mov_out_cpu(&valor, &dir_fisica, socket_cpu_int);

			log_info(logger_memoria_info, "VALOR %d , DIRECCION FISICA %d", valor, dir_fisica);
			escribir_memoria_cpu(dir_fisica, valor);

			// TODO -- VER SI DEVUELVO OK.

			break;
		case MOV_IN_CPU: // Lee el valor del marco y lo devuelve para guardarlo en el registro (se pide la direccion) - recibo direccion fisica

			int df;
			recibir_mov_in_cpu(&df, socket_cpu_int);
			uint32_t valor_leido = leer_memoria_cpu(df);

			enviar_valor_mov_in_cpu(valor_leido, socket_cpu_int); // MOV_IN_CPU

			break;
		default:
			log_error(logger_memoria_info, "Fallo la comunicacion CON CPU. Abortando \n");
			finalizar_memoria();
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

		// log_info(logger_memoria_info, "Se recibio una operacion de KERNEL: %d", codigo_operacion);
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
			proceso_memoria = iniciar_proceso_path(proceso_memoria); // inicio en memoria y pido listado de bloques a FS

			codigo_operacion = recibir_operacion(socket_fs_int);

			if (codigo_operacion == LISTA_BLOQUES_SWAP)
			{
				t_list *bloques_reservados = recibir_listado_id_bloques(socket_fs_int);
				// log_info(logger_memoria_info, "el size de la lista de bloques es de %d", list_size(bloques_reservados)); TODO BORRAR
				asignar_id_bloque_swap(proceso_memoria, bloques_reservados);
				log_info(logger_memoria_info, "Se creo correctamente el proceso PID [%d]", proceso_memoria->pid);
			}
			else
			{
				log_error(logger_memoria_info, "No se recibio OK la lista de bloques swap iniciales");
			}
			log_info(logger_memoria_info, "Proceso inicializado OK - Instrucciones/ Tabla de paginas / SWAP - PID [%d]", proceso_memoria->pid);
			// TODO -- RESPONDERLE AL KERNEL QUE SE CREO OK.
			break;
		case FINALIZAR_PROCESO:

			int pid_a_finalizar;
			recibir_pid(socket_kernel_int, &pid_a_finalizar);
			t_proceso_memoria *proceso_a_eliminar = obtener_proceso_pid(pid_a_finalizar);

			log_info(logger_memoria_info, "Me llego el pedido de finalizar el PID %d - obtengo el pid en mi lista de procesos %d", pid_a_finalizar, proceso_a_eliminar->pid);

			limpiar_swap(proceso_a_eliminar); // listo los bloques swap y se los envio a FS para que los marque como libre

			codigo_operacion = recibir_operacion(socket_fs_int);
			if (codigo_operacion == SWAP_LIBERADA)
			{
				int prueba = recibir_int(socket_fs_int);
				if (prueba != -1)
				{
					log_info(logger_memoria_info, "Se liberaron los bloques SWAP correspondientes al proceso PID [%d]", proceso_a_eliminar->pid);
				}
			}
			else
			{
				log_error(logger_memoria_info, "No se recibio OK la liberacion de bloques para el PID [%d]", proceso_a_eliminar->pid);
			}
			eliminar_proceso_memoria(proceso_a_eliminar); // Libero las entradas de la tabla de pagina y lo elimino de la lista de procesos --

			// TODO -- RESPONDERLE al kernel que finalizo OK el proceso??
			break;
		case PAGE_FAULT_KERNEL:

			int pid_pf, nro_pag_pf;
			recibir_pf_kernel(socket_kernel_int, &pid_pf, &nro_pag_pf);

			log_info(logger_memoria_info, "Me llego PF para PID [%d] , NUM DE PAG [%d]", pid_pf, nro_pag_pf);
			t_proceso_memoria *proceso_pf = obtener_proceso_pid(pid_pf);
			t_entrada_tabla_pag *entrada_a_traer = list_get(proceso_pf->tabla_paginas->entradas_tabla, nro_pag_pf);
			// log_info(logger_memoria_info, "El nro de pagina de de PF es %d y el indice de la entrada es %d", nro_pag_pf, entrada_a_traer->indice); TODO - BORRAR
			entrada_a_traer->bit_presencia = 1;
			entrada_a_traer->bit_modificado = 0;
			int marco_a_asignar;

			if (hay_marcos_libres())
			{
				log_info(logger_memoria_info, "HAY MARCOS LIBRES - no se debe reemplazar ninguna pagina en memoria");
				marco_a_asignar = asignar_marco_libre(pid_pf);
				entrada_a_traer->marco = marco_a_asignar;
				log_info(logger_memoria_info, "El marco a asignar es  [%d] para PID [%d] - Pag [%d]", marco_a_asignar, pid_pf, nro_pag_pf);
			}
			else // DEBO REEMPLAZAR ALGUNA PAGINA EN MEMORIA
			{
				t_entrada_tabla_pag *entrada_a_swapear = paginaAReemplazar();
				marco_a_asignar = entrada_a_swapear->marco;

				t_marco *marco_real = list_get(marcos, marco_a_asignar);
				t_proceso_memoria *proceso_swapeado = obtener_proceso_pid(marco_real->pid);

				log_info(logger_memoria_info, "****** REEMPLAZO - Marco: [%d] - Page Out: PID [%d] - PAG [%d] - Page In: PID [%d] - PAG [%d]", marco_a_asignar, proceso_swapeado->pid, entrada_a_swapear->indice,
						 pid_pf, nro_pag_pf); // LOG OBLIGATORIO

				if (entrada_a_swapear->bit_modificado == 1) // SI ESTA MODIFICADO LO ESCRIBO EN SWAP
				{
					log_info(logger_memoria_info, "La pagina a reemplazar esta modificada, debo escribirla en swap antes de reemplazarla");
					pedido_escritura_swap(socket_fs_int, entrada_a_swapear);

					codigo_operacion = recibir_operacion(socket_fs_int);
					if (codigo_operacion == ESCRITURA_BLOQUE_OK)
					{
						int valor = recibir_int(socket_fs_int);
						valor += 1;																																													   // asi no tira unnused
						log_info(logger_memoria_info, "SWAP OUT -  PID: [%d] - Marco: [%d] - Page Out: PID [%d]- PAG [%d]", proceso_swapeado->pid, marco_a_asignar, proceso_swapeado->pid, entrada_a_swapear->indice); // LOG OBLIGATORIO
					}
					else
					{
						log_error(logger_memoria_info, "No se recibio OK la escritura en el bloque SWAP para el PID[%d]", proceso_swapeado->pid);
					}
				}

				liberar_presencia_pagina(entrada_a_swapear);
				entrada_a_traer->marco = marco_a_asignar;

				// CARGO LA PAGINA -- REPITO LOGICA SI HAY MARCO LIBRE, REFACTOR LLEVARLO A UNA FUNCION
			}

			pedido_lectura_swap(socket_fs_int, entrada_a_traer);

			log_info(logger_memoria_info, "Se pidio pagina a SWAP con id bloque %d", entrada_a_traer->id_bloque_swap);

			cargar_pagina_swap_en_memoria(socket_fs_int, marco_a_asignar, pid_pf);

			log_info(logger_memoria_info, "Se cargo en memoria la pagina indicada");

			enviar_op_con_int(socket_kernel_int, PAGINA_CARGADA, pid_pf);
			log_info(logger_memoria_info, "***** SWAP IN -  PID: [%d] - Marco: [%d] - Page In: PID [%d] -PAG [%d]", proceso_pf->pid, marco_a_asignar, proceso_pf->pid, entrada_a_traer->indice); // LOG OBLIGATORIO

			break;
		default:
			log_error(logger_memoria_info, "Fallo la comunicacion con KERNEL. Abortando");
			finalizar_memoria();
			break;
		}
	}
	return NULL;
}

void *manejo_conexion_filesystem_swap(void *arg)
{

	socket_fs_int = *(int *)arg;

	op_code codigo_operacion = recibir_operacion(socket_fs_int);

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
		break;
	}
	return NULL;
}

void *manejo_conexion_filesystem_ops(void *arg)
{
	int direc_fisica;
	void *bloque;
	socket_fs_arch_ops = *(int *)arg;
	while (1)
	{

		op_code codigo_operacion = recibir_operacion(socket_fs_arch_ops);

		log_info(logger_memoria_info, "Se recibio una operacion de FS para READ/WRITE: %d", codigo_operacion);

		switch (codigo_operacion)
		{
		case HANDSHAKE_FILESYSTEM_OPS:
			log_info(logger_memoria_info, "Handshake exitosa con FILESYSTEM, se conecto un FILESYSTEM para read write");
			recibir_handshake(socket_fs_arch_ops, logger_memoria_info);
			realizar_handshake(socket_fs_arch_ops, HANDSHAKE_MEMORIA, logger_memoria_info);
			break;
		case F_WRITE_FS: // leo de memoria para escribir en fs
			recibir_f_write_fs(&direc_fisica, socket_fs_arch_ops);
			bloque = leer_memoria_fs(direc_fisica);

			enviar_pagina_leida_fs(bloque, socket_fs_arch_ops);
			free(bloque);
			break;
		case F_READ_FS: // escribo en memoria lo que me manda fs
			bloque = recibir_f_read_fs(&direc_fisica, socket_fs_arch_ops);

			escribir_memoria_fs(direc_fisica, bloque);

			enviar_op_con_int(socket_fs_arch_ops, F_READ_FS, 1);
			free(bloque);
			break;
		default:
			log_error(logger_memoria_info, "Fallo la comunicacion con memoria para f read / f write. Abortando \n");
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
	char*path_completo = string_new();
	string_append(&path_completo, "./cfg/");
	string_append(&path_completo, path);
	char *pseudo_codigo_leido = leer_archivo(path_completo);
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
			list_add(instrucciones, armar_estructura_instruccion(F_OPEN, palabras[1], palabras[2]));
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
	free(path_completo);
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
	// log_info(logger_memoria_info, "Archivo de instrucciones leido.");
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
	printf("%s - %s - %s \n", obtener_nombre_instruccion(estructura->codigo), estructura->parametro1, estructura->parametro2); // PRINT INSTRUCCIONES

	return estructura;
}

t_proceso_memoria *iniciar_proceso_path(t_proceso_memoria *proceso_nuevo)
{
	proceso_nuevo->instrucciones = parsear_instrucciones(proceso_nuevo->path);
	log_info(logger_memoria_info, "Instrucciones parseadas OK para el proceso PID [%d]", proceso_nuevo->pid);
	pthread_mutex_lock(&mutex_procesos);
	list_add(procesos_totales, proceso_nuevo);
	pthread_mutex_unlock(&mutex_procesos);
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
	pthread_mutex_lock(&mutex_procesos);
	proceso_elegido = list_find(procesos_totales, _proceso_id);
	pthread_mutex_unlock(&mutex_procesos);
	return proceso_elegido;
}

// INSTRUCCIONES PROCESO

t_instruccion *obtener_instrccion_pc(t_proceso_memoria *proceso, uint32_t pc_pedido)
{
	return list_get(proceso->instrucciones, pc_pedido);
}

t_instruccion *obtener_instruccion_pid_pc(uint32_t pid_pedido, uint32_t pc_pedido)
{
	// log_error(logger_memoria_info, "Voy a buscar la instruccion de PID %d con PC %d", pid_pedido, pc_pedido);
	t_proceso_memoria *proceso = obtener_proceso_pid(pid_pedido);
	//if (config_valores_memoria.retardo_respuesta / 1000 > 0)
	nanosleep(config_valores_memoria.retardo_respuesta * 1000, NULL);
	//else
	//sleep(1);
	return obtener_instrccion_pc(proceso, pc_pedido);
}

// PAGINAS

void actualizo_entrada_para_futuro_reemplazo(t_entrada_tabla_pag *entrada)
{

	pthread_mutex_lock(&mutex_contador_LRU);
	contador_lru++;
	pthread_mutex_unlock(&mutex_contador_LRU);
	entrada->tiempo_lru = contador_lru;
	agregar_pagina_fifo(entrada);
}

void agregar_pagina_fifo(t_entrada_tabla_pag *entrada)
{
	bool _mismo_id_bloque(void *elemento)
	{
		return ((t_entrada_tabla_pag *)elemento)->id_bloque_swap == entrada->id_bloque_swap;
	}

	t_entrada_tabla_pag *entrada_existente;
	pthread_mutex_lock(&mutex_fifo);
	entrada_existente = list_find(paginas_utilizadas, _mismo_id_bloque);

	if (entrada_existente == NULL)
	{
		// Si la entrada no existe, agrégala a la lista
		log_error(logger_memoria_info, "marco de la entrada: %d", entrada->marco);
		list_add(paginas_utilizadas, entrada);
	}
	pthread_mutex_unlock(&mutex_fifo);
}

bool son_iguales(t_entrada_tabla_pag *entrada1, t_entrada_tabla_pag *entrada2)
{
	return entrada1->indice == entrada2->indice && entrada1->marco == entrada2->marco && entrada1->id_bloque_swap == entrada2->id_bloque_swap;
}

void liberar_presencia_pagina(t_entrada_tabla_pag *pagina)
{

	pagina->marco = -1;
	pagina->bit_presencia = 0;
	pagina->bit_modificado = 0;
}

// MANEJO PF

void recibir_pf_kernel(int socket, int *pid, int *nro_pag)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	// printf("size del stream a deserializar %d \n", size);
	memcpy(pid, buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(nro_pag, buffer + offset, sizeof(int));

	free(buffer);
}

t_entrada_tabla_pag *paginaAReemplazar()
{
	t_entrada_tabla_pag *pagina_a_reemplazar;
	t_marco *marco_reemplazo;

	switch (algoritmo_pags)
	{
	case FIFO:
		pagina_a_reemplazar = obtenerPaginaFIFO();
		if (pagina_a_reemplazar->marco != -1)
			log_info(logger_memoria_info, "el marco de la pagina es: %d", pagina_a_reemplazar->marco);
		marco_reemplazo = list_get(marcos, pagina_a_reemplazar->marco);
		if (marco_reemplazo == NULL)
			log_info(logger_memoria_info, "marco reemplazo es NULL");

		log_info(logger_memoria_info, "PID [%d] - Pagina a reemplazar Nro Pag: [%d]", marco_reemplazo->pid, pagina_a_reemplazar->indice);
		return pagina_a_reemplazar;
		break;
	case LRU:
		pagina_a_reemplazar = obtenerPaginaLRU();
		marco_reemplazo = list_get(marcos, pagina_a_reemplazar->marco);

		log_info(logger_memoria_info, "PID [%d] - Pagina a reemplazar Nro Pag: [%d]", marco_reemplazo->pid, pagina_a_reemplazar->indice);
		return pagina_a_reemplazar;
		break;
	default:
		log_error(logger_memoria_info, "no se encontro el algoritmo de reemplazo");
		return NULL;
	}
}

t_entrada_tabla_pag *obtenerPaginaLRU()
{
	t_list *paginas_en_memoria = obtener_total_pags_en_memoria(procesos_totales);

	t_entrada_tabla_pag *pagina = obtener_entrada_menor_tiempo_lru(paginas_en_memoria);

	return pagina;
}

t_entrada_tabla_pag *obtener_entrada_menor_tiempo_lru(t_list *lista_entradas)
{
	t_entrada_tabla_pag *entrada_menor_tiempo_lru = NULL;
	int menor_tiempo_lru = MAX_LRU; // Inicializar con un valor grande

	// Iterar sobre la lista
	for (int i = 0; i < list_size(lista_entradas); i++)
	{
		t_entrada_tabla_pag *entrada_actual = list_get(lista_entradas, i);

		// Verificar si el tiempo_lru es menor al menor registrado
		if (entrada_actual->tiempo_lru < menor_tiempo_lru)
		{
			menor_tiempo_lru = entrada_actual->tiempo_lru;
			entrada_menor_tiempo_lru = entrada_actual;
		}
	}

	return entrada_menor_tiempo_lru;
}

t_entrada_tabla_pag *obtenerPaginaFIFO()
{
	pthread_mutex_lock(&mutex_fifo);
	log_error(logger_memoria_info, "List size de paginas utilizadas antes de sacar pag a reemplazar %d", list_size(paginas_utilizadas));
	t_entrada_tabla_pag *pagina = list_remove(paginas_utilizadas, 0);
	pthread_mutex_unlock(&mutex_fifo);
	return pagina;
}

void escribirPagEnMemoria(void *valor, int numMarco)
{
	pthread_mutex_lock(&mutex_memoria_usuario);
	memcpy(memoria_usuario + numMarco * config_valores_memoria.tamanio_pagina, valor, config_valores_memoria.tamanio_pagina);
	pthread_mutex_unlock(&mutex_memoria_usuario);
}

void pedido_lectura_swap(int socket, t_entrada_tabla_pag *entrada)
{
	int index_bloque = entrada->id_bloque_swap;
	t_paquete *paquete_pagina = crear_paquete_con_codigo_de_operacion(LEER_BLOQUE);
	paquete_pagina->buffer->size += sizeof(int);
	paquete_pagina->buffer->stream = malloc(paquete_pagina->buffer->size);

	int offset = 0;

	memcpy(paquete_pagina->buffer->stream + offset, &(index_bloque), sizeof(int));

	enviar_paquete(paquete_pagina, socket);
	eliminar_paquete(paquete_pagina);
}

void cargar_pagina_swap_en_memoria(int socket, int marco_asignar, int pid_pfs)
{

	op_code codigo_operacion = recibir_operacion(socket);
	// log_info(logger_memoria_info, "Me llego un codigo de operacion de FS - SWAP");

	if (codigo_operacion == VALOR_BLOQUE)
	{
		void *pagina_SWAP = recibir_bloque_swap(socket);
		log_info(logger_memoria_info, "Recibi el SWAP del PID [%d] para cargar en memoria", pid_pfs);
		escribirPagEnMemoria(pagina_SWAP, marco_asignar);
	}
	else
	{
		log_error(logger_memoria_info, "No se recibio correctamente el bloque swap para traerlo a memoria del PID [%d]", pid_pfs);
	}
}

void pedido_escritura_swap(int socket, t_entrada_tabla_pag *entrada)
{
	void *datos_a_swapear = leer_pagina_para_swapear(entrada->marco);

	t_paquete *pagina_a_swapear = crear_paquete_con_codigo_de_operacion(ESCRIBIR_BLOQUE);
	pagina_a_swapear->buffer->size += sizeof(int) + config_valores_memoria.tamanio_pagina;
	pagina_a_swapear->buffer->stream = realloc(pagina_a_swapear->buffer->stream, pagina_a_swapear->buffer->size);

	int offset = 0;

	printf("Size del stream a deserializar -- swap a escribir: %d \n", pagina_a_swapear->buffer->size);

	memcpy(pagina_a_swapear->buffer->stream + offset, &(entrada->id_bloque_swap), sizeof(int));
	offset += sizeof(int);

	memcpy(pagina_a_swapear->buffer->stream + offset, datos_a_swapear, config_valores_memoria.tamanio_pagina);

	enviar_paquete(pagina_a_swapear, socket);
	eliminar_paquete(pagina_a_swapear);
}

void *recibir_bloque_swap(int socket)
{

	int size;
	void *buffer = recibir_buffer(&size, socket);
	return buffer;
}

t_list *obtener_total_pags_en_memoria(t_list *lista_procesos)
{
	t_list *lista_filtrada = list_create();

	// Iterar sobre todos los procesos en la lista
	for (int i = 0; i < list_size(lista_procesos); i++)
	{
		t_proceso_memoria *proceso = list_get(lista_procesos, i);

		// Iterar sobre todas las entradas de la tabla de páginas del proceso
		for (int j = 0; j < list_size(proceso->tabla_paginas->entradas_tabla); j++)
		{
			t_entrada_tabla_pag *entrada = list_get(proceso->tabla_paginas->entradas_tabla, j);

			// Filtrar las entradas con bit_presencia igual a 1
			if (tiene_bit_presencia_igual_a_1(entrada))
			{
				list_add(lista_filtrada, entrada);
			}
		}
	}

	return lista_filtrada;
}

// MARCOS

t_list *obtener_marcos_pid(uint32_t pid_pedido)
{
	global_pid_pedido = pid_pedido;
	pthread_mutex_lock(&mutex_marcos);
	t_list *marcos_proceso = (t_list *)list_filter(marcos, mismo_pid_marco);
	pthread_mutex_unlock(&mutex_marcos);
	return marcos_proceso;
}

bool mismo_pid_marco(t_marco *marco)
{
	return marco->pid == global_pid_pedido;
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

	log_error(logger_memoria_info, "cantidad de marcos: %d", list_size(marcos_proceso));

	for (int i = 0; i < list_size(marcos_proceso); i++)
	{
		t_marco *marco_proximamente_libre = list_get(marcos_proceso, i);
		liberar_marco_indice(marco_proximamente_libre->num_de_marco);

		log_error(logger_memoria_info, "se libera marco: %d", marco_proximamente_libre->num_de_marco);
	}

	for (int j = 0; j < list_size(marcos); j++)
	{
		t_marco *marquito = list_get(marcos, j);
		log_warning(logger_memoria_info, "El marco en la posicion %d con indice %d tiene como pid %d ", j, marquito->num_de_marco, marquito->pid);
	}
}

void recibir_pedido_marco(int *pid_tr, int *index, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	// printf("size del stream a deserializar %d \n ", size);
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

	log_info(logger_memoria_info, "***** ACCESO A TABLA DE PAGINAS - PID: [%d] - Pagina: [%d] - MARCO: [%d]", proceso->pid, entrada_tabla->indice, entrada_tabla->marco); // LOG OBLIGATORIO

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

t_marco *marco_desde_df(int df)
{
	int num_marco = floor(df / config_valores_memoria.tamanio_pagina);
	log_info(logger_memoria_info, "obtengo el marco modificado");
	pthread_mutex_lock(&mutex_marcos);
	t_marco *marco_elegido = list_get(marcos, num_marco);
	pthread_mutex_unlock(&mutex_marcos);
	log_error(logger_memoria_info, "marco elegido: %d - direccion fisica: %d", marco_elegido->num_de_marco, df);
	return marco_elegido;
}

void marcar_pag_modificada(int pid_mod, int marco_mod)
{
	t_proceso_memoria *proceso = obtener_proceso_pid((uint32_t)pid_mod);
	log_error(logger_memoria_info, "Valor del pid: %d", proceso->pid);
	t_list *paginas_en_memoria = obtener_entradas_con_bit_presencia_1(proceso);
	log_error(logger_memoria_info, "cantidad de paginas: %d - tamaño de la lista total: %d - lista en memoria: %d", proceso->tabla_paginas->cantidad_paginas, list_size(proceso->tabla_paginas->entradas_tabla), list_size(paginas_en_memoria));
	t_entrada_tabla_pag *pagina_modificada = obtener_entrada_con_marco(paginas_en_memoria, marco_mod);
	log_error(logger_memoria_info, "marco: %d - indice: %d - marco en pagina: %d", marco_mod, pagina_modificada->indice, pagina_modificada->marco);

	cambiar_bit_modificado(proceso, pagina_modificada->indice, 1);
	actualizo_entrada_para_futuro_reemplazo(pagina_modificada);
}

void cambiar_bit_modificado(t_proceso_memoria *proceso, int index_entrada, int valor)
{
	t_entrada_tabla_pag *entrada = list_get(proceso->tabla_paginas->entradas_tabla, index_entrada);
	entrada->bit_modificado = valor;
}

bool tiene_bit_presencia_igual_a_1(void *elemento)
{
	t_entrada_tabla_pag *entrada = (t_entrada_tabla_pag *)elemento;
	return entrada->bit_presencia == 1;
}

t_list *obtener_entradas_con_bit_presencia_1(t_proceso_memoria *proceso)
{
	t_list *lista_filtrada = list_filter(proceso->tabla_paginas->entradas_tabla, tiene_bit_presencia_igual_a_1);
	return lista_filtrada;
}

t_entrada_tabla_pag *obtener_entrada_con_marco(t_list *entradas, int marco_pedido)
{

	/*bool pagConMismoMarco(t_entrada_tabla_pag * pagina)
	{

		return pagina->marco == marco;
	}

	t_entrada_tabla_pag *paginaM = (t_entrada_tabla_pag *)list_find(entradas, pagConMismoMarco);
	return paginaM;*/

	bool _pag_mismo_marco(void *elemento)
	{
		return ((t_entrada_tabla_pag *)elemento)->marco == marco_pedido;
	}

	t_entrada_tabla_pag *entrada_elegida;
	pthread_mutex_lock(&mutex_procesos);
	entrada_elegida = list_find(entradas, _pag_mismo_marco);
	pthread_mutex_unlock(&mutex_procesos);
	return entrada_elegida;
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
	// printf("Size del stream a deserializar: %d \n", size);

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

	// printf("mi path recibido es %s \n", proceso_nuevo->path); // TODO - BORRAR

	return proceso_nuevo;
}

void inicializar_nuevo_proceso(t_proceso_memoria *proceso_nuevo)
{
	int q_pags = inicializar_estructuras_memoria_nuevo_proceso(proceso_nuevo);
	log_info(logger_memoria_info, "Estructuras de memoria para PID [%d] inicializadas - se pide SWAP", proceso_nuevo->pid);
	pedido_inicio_swap(q_pags, socket_fs_int);
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
		list_add(proceso_nuevo->tabla_paginas->entradas_tabla, entrada);
	}

	log_info(logger_memoria_info, "****** CREACION DE TABLA DE PAGINAS - PID [%d] - Tamano: [%d] - PAGINAS [%d]", proceso_nuevo->pid, proceso_nuevo->tabla_paginas->cantidad_paginas, list_size(proceso_nuevo->tabla_paginas->entradas_tabla));

	return cantidad_pags;
}

void asignar_id_bloque_swap(t_proceso_memoria *proceso_nuevo, t_list *lista_id_bloques)
{
	int cant_bloques = list_size(proceso_nuevo->tabla_paginas->entradas_tabla);

	for (int i = 0; i < cant_bloques; i++)
	{
		t_entrada_tabla_pag *entrada = list_get(proceso_nuevo->tabla_paginas->entradas_tabla, i);

		int id_bloque = (int)list_get(lista_id_bloques, i);
		// printf("el valor del bloque a cargar en la entrada es de %d, y el nro de entrada es de %d", id_bloque, entrada->indice); TODO - BORRAR
		entrada->id_bloque_swap = id_bloque;

		// int *id_bloque = list_get(lista_id_bloques, i);
		// entrada->id_bloque_swap = *id_bloque;
	}
	// log_info(logger_memoria_info, "Se asignaron OK los id de los bloques SWAP para el PID [%d]", proceso_nuevo->pid); // TODO - BORRAR
}

void pedido_inicio_swap(int cant_pags, int socket)
{

	t_paquete *paquete_pedido_swap = crear_paquete_con_codigo_de_operacion(INICIO_SWAP);
	paquete_pedido_swap->buffer->size += sizeof(int);
	paquete_pedido_swap->buffer->stream = malloc(paquete_pedido_swap->buffer->size);

	int offset = 0;

	memcpy(paquete_pedido_swap->buffer->stream + offset, &(cant_pags), sizeof(int));

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
	return (double)(config_valores_memoria.tamanio_memoria) / (double)(config_valores_memoria.tamanio_pagina);
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

// INSTRUCCIONES CPU

void recibir_mov_out_cpu(uint32_t *valor, int *marco, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	printf("size del stream a deserializar %d \n", size);
	memcpy(valor, buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(marco, buffer + offset, sizeof(int));

	free(buffer);
}

void recibir_mov_in_cpu(int *dir_fisica, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	// printf("size del stream a deserializar \n%d", size);
	memcpy(dir_fisica, buffer + offset, sizeof(int));

	free(buffer);
}

void enviar_valor_mov_in_cpu(uint32_t valor, int socket)
{
	t_paquete *paquete_mov_in = crear_paquete_con_codigo_de_operacion(MOV_IN_CPU);
	paquete_mov_in->buffer->size += sizeof(uint32_t);
	paquete_mov_in->buffer->stream = realloc(paquete_mov_in->buffer->stream, paquete_mov_in->buffer->size);
	memcpy(paquete_mov_in->buffer->stream, &(valor), sizeof(uint32_t));
	enviar_paquete(paquete_mov_in, socket);
	eliminar_paquete(paquete_mov_in);
}

void enviar_pagina_leida_fs(void *bloque, int socket)
{
	t_paquete *paquete_f_write = crear_paquete_con_codigo_de_operacion(F_WRITE_FS);
	paquete_f_write->buffer->size += config_valores_memoria.tamanio_pagina;
	paquete_f_write->buffer->stream = realloc(paquete_f_write->buffer->stream, paquete_f_write->buffer->size);
	memcpy(paquete_f_write->buffer->stream, bloque, config_valores_memoria.tamanio_pagina);
	enviar_paquete(paquete_f_write, socket);
	eliminar_paquete(paquete_f_write);
}

// MEMORIA USUARIO

void escribir_memoria_cpu(int dir_fisica, uint32_t valor)
{
	// TODO - AGREGAR MUTEX
	log_info(logger_memoria_info, "voy a escribir un valor en memoria usuario");
	pthread_mutex_lock(&mutex_memoria_usuario);
	memcpy(memoria_usuario + dir_fisica, &valor, sizeof(uint32_t));
	pthread_mutex_unlock(&mutex_memoria_usuario);

	t_marco *marco = marco_desde_df(dir_fisica);

	marcar_pag_modificada(marco->pid, marco->num_de_marco);
	log_info(logger_memoria_info, "Se marco pagina como modificada para PID %d", marco->pid);
	if (config_valores_memoria.retardo_respuesta / 1000 > 0)
		sleep(config_valores_memoria.retardo_respuesta / 1000);
	else
		sleep(1);
	log_info(logger_memoria_info, "***** ACCESO A ESPACIO USUARIO POR CPU- PID [%d] - ACCION: [ESCRIBIR] - DIRECCION FISICA: [%d]", marco->pid, dir_fisica); // LOG OBLIGATORIO
}

void escribir_memoria_fs(int dir_fisica, void *bloque)
{

	t_marco *marco_escibir = marco_desde_df(dir_fisica);
	escribirPagEnMemoria(bloque, marco_escibir->num_de_marco); // mutex dentro de la funcion // entiendo que el num de marco es la direcc fisica. // TODO CONVERTIR ?
	log_info(logger_memoria_info, "FS WRITE - Escribo bloque en dir fisica %d - marco %d", dir_fisica, marco_escibir->num_de_marco);

	// TODO -- VALIDAR -- cuando hago un F_READ debo marcar la pagina que tiene el marco como modificada
	marcar_pag_modificada(marco_escibir->pid, marco_escibir->num_de_marco);
	log_info(logger_memoria_info, "Se marco pagina como modificada para PID %d", marco_escibir->pid);
	if (config_valores_memoria.retardo_respuesta / 1000 > 0)
		sleep(config_valores_memoria.retardo_respuesta / 1000);
	else
		sleep(1);
	log_info(logger_memoria_info, "***** ACCESO A ESPACIO USUARIO POR FS - PID [%d] - ACCION: [ESCRIBIR] - DIRECCION FISICA: [%d]", marco_escibir->pid, dir_fisica); // LOG OBLIGATORIO
}

uint32_t leer_memoria_cpu(uint32_t dir_fisica)
{
	uint32_t valor_leido = 0;
	pthread_mutex_lock(&mutex_memoria_usuario);
	memcpy(&valor_leido, memoria_usuario + dir_fisica, sizeof(uint32_t));
	pthread_mutex_unlock(&mutex_memoria_usuario);

	t_marco *marco = marco_desde_df(dir_fisica);
	// TODO -- VALIDAR -- cuando hago un F_READ debo marcar la pagina que tiene el marco como modificada
	t_proceso_memoria *proceso = obtener_proceso_pid((uint32_t)marco->pid);
	t_list *paginas_en_memoria = obtener_entradas_con_bit_presencia_1(proceso);
	t_entrada_tabla_pag *pagina_modificada = obtener_entrada_con_marco(paginas_en_memoria, marco->num_de_marco);
	actualizo_entrada_para_futuro_reemplazo(pagina_modificada);

	if (config_valores_memoria.retardo_respuesta / 1000 > 0)
		sleep(config_valores_memoria.retardo_respuesta / 1000);
	else
		sleep(1);
	log_info(logger_memoria_info, "***** ACCESO A ESPACIO USUARIO - CPU - PID [%d] - ACCION: [LEER] - DIRECCION FISICA: [%d]", marco->pid, dir_fisica); // LOG OBLIGATORIO

	return valor_leido;
}

void *leer_memoria_fs(int dir_fisica)
{
	void *bloque_leido = malloc(config_valores_memoria.tamanio_pagina);
	pthread_mutex_lock(&mutex_memoria_usuario);
	memcpy(bloque_leido, memoria_usuario + dir_fisica, config_valores_memoria.tamanio_pagina);
	pthread_mutex_unlock(&mutex_memoria_usuario);

	t_marco *marco = marco_desde_df(dir_fisica);

	t_proceso_memoria *proceso = obtener_proceso_pid((uint32_t)marco->pid);
	t_list *paginas_en_memoria = obtener_entradas_con_bit_presencia_1(proceso);
	t_entrada_tabla_pag *pagina_modificada = obtener_entrada_con_marco(paginas_en_memoria, marco->num_de_marco);
	actualizo_entrada_para_futuro_reemplazo(pagina_modificada);

	if (config_valores_memoria.retardo_respuesta / 1000 > 0)
		sleep(config_valores_memoria.retardo_respuesta / 1000);
	else
		sleep(1);
	log_info(logger_memoria_info, "***** ACCESO A ESPACIO USUARIO - FS - PID [%d] - ACCION: [LEER] - DIRECCION FISICA: [%d]", marco->pid, dir_fisica); // LOG OBLIGATORIO

	return bloque_leido;
}

void *leer_pagina_para_swapear(int marco)
{
	void *pagina_leida = malloc(config_valores_memoria.tamanio_pagina);
	pthread_mutex_lock(&mutex_memoria_usuario);
	memcpy(pagina_leida, memoria_usuario + marco * config_valores_memoria.tamanio_pagina, config_valores_memoria.tamanio_pagina);
	pthread_mutex_unlock(&mutex_memoria_usuario);

	log_info(logger_memoria_info, "se lee el contenido del marco %d para escribir las modificaciones en swap", marco);
	return pagina_leida;
}

// FINALIZAR PROCESO

void limpiar_swap(t_proceso_memoria *proceso_a_eliminar) // listo los bloques swap y se los envio a FS para que los marque como libre
{
	t_list *ids_bloques_swap = obtener_lista_id_bloque_swap(proceso_a_eliminar);
	log_info(logger_memoria_info, " la cantidad de bloques a liberar es de %d", list_size(ids_bloques_swap));
	enviar_bloques_swap_a_liberar(ids_bloques_swap, socket_fs_int);
	// list_destroy_and_destroy_elements(ids_bloques_swap, free); // TODO CHEQUEAR SI HAY QUE LIBERAR
}

t_list *obtener_lista_id_bloque_swap(t_proceso_memoria *proceso)
{
	t_list *lista_id_bloques_swap = list_create();

	int cant_bloques = list_size(proceso->tabla_paginas->entradas_tabla);

	log_info(logger_memoria_info, "La cantidad de bloques que tiene el proceso es %d", cant_bloques);

	for (int i = 0; i < cant_bloques; i++)
	{
		t_entrada_tabla_pag *entrada = (t_entrada_tabla_pag *)list_get(proceso->tabla_paginas->entradas_tabla, i);

		int id_bloque = entrada->id_bloque_swap;
		// log_error(logger_memoria_info, "el id de bloque para enviar a liberar es %d y en la entrada tiene el id %d", id_bloque, entrada->id_bloque_swap); TODO - BORRAR
		//  int *id_bloque = malloc(sizeof(int));
		//*id_bloque = entrada->id_bloque_swap;
		list_add(lista_id_bloques_swap, id_bloque);
	}

	return lista_id_bloques_swap;
}

void enviar_bloques_swap_a_liberar(t_list *lista_bloques, int socket)
{
	t_paquete *paquete_swap_a_borrar = crear_paquete_con_codigo_de_operacion(SWAP_A_LIBERAR);
	serializar_lista_swap(lista_bloques, paquete_swap_a_borrar);
	enviar_paquete(paquete_swap_a_borrar, socket);
	eliminar_paquete(paquete_swap_a_borrar);
}

void eliminar_proceso_memoria(t_proceso_memoria *proceso_a_eliminar) // Libero las entradas de la tabla de pagina y lo elimino de la lista de procesos
{
	// t_list *paginas_en_memoria = obtener_entradas_con_bit_presencia_1(proceso_a_eliminar);
	liberar_marcos_proceso(proceso_a_eliminar->pid);

	log_info(logger_memoria_info, "***** DESTRUCCION TABLA DE PAGINAS - PID [%d] - Tamano [%d]", proceso_a_eliminar->pid, proceso_a_eliminar->tabla_paginas->cantidad_paginas); // LOG OBLIGATORIO

	pthread_mutex_lock(&mutex_procesos);
	list_remove_element(procesos_totales, proceso_a_eliminar);
	pthread_mutex_unlock(&mutex_procesos);

	t_list *paginas_a_elminar = proceso_a_eliminar->tabla_paginas->entradas_tabla;
	int cantidad_entradas = proceso_a_eliminar->tabla_paginas->cantidad_paginas;

	log_error(logger_memoria_info, "List size de paginas utilizadas %d", list_size(paginas_utilizadas));

	// libero paginas del proces oen FIFO
	for (int i = 0; i < cantidad_entradas; i++)
	{
		t_entrada_tabla_pag *pag_a_borrar = list_get(paginas_a_elminar, i);
		pthread_mutex_lock(&mutex_fifo);
		list_remove_element(paginas_utilizadas, pag_a_borrar);
		pthread_mutex_unlock(&mutex_fifo);
	}

	log_error(logger_memoria_info, "List size de paginas utilizadas %d", list_size(paginas_utilizadas));

	list_destroy_and_destroy_elements(proceso_a_eliminar->tabla_paginas->entradas_tabla, free);
	free(proceso_a_eliminar->path);
	liberar_lista_instrucciones(proceso_a_eliminar->instrucciones);
	free(proceso_a_eliminar);
}

// INSTRUCCIONES FILESYSTEM FS F READ F WRITE OPS

void *recibir_f_read_fs(int *ds, int socket)
{

	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	printf("size del stream a deserializar %d \n", size);
	memcpy(ds, buffer + offset, sizeof(int));
	offset += sizeof(int);
	void *bloque = malloc(config_valores_memoria.tamanio_pagina);
	memcpy(bloque, buffer + offset, config_valores_memoria.tamanio_pagina);

	free(buffer);
	return bloque;
}

void recibir_f_write_fs(int *df, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	// printf("size del stream a deserializar \n%d", size);
	memcpy(df, buffer + offset, sizeof(int));

	free(buffer);
}

void finalizar_memoria()
{
	log_info(logger_memoria_info, "Finalizando Memoria");
	log_destroy(logger_memoria_info);
	close(socket_cpu_int);
	close(socket_fs_int);
	close(socket_kernel_int);
	close(server_memoria);
	config_destroy(config);
	abort();
}