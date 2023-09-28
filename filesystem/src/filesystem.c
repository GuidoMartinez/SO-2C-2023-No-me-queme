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

	formatear = 0;
	tam_memoria_file_system = config_valores_filesystem.cant_bloques_total * config_valores_filesystem.tam_bloque;
	tamanio_fat = (config_valores_filesystem.cant_bloques_total - config_valores_filesystem.cant_bloques_swap) * sizeof(uint32_t);

    socket_memoria = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    realizar_handshake(socket_memoria, HANDSHAKE_FILESYSTEM, filesystem_logger_info);


    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(socket_memoria,filesystem_logger_info);
        log_info(filesystem_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(filesystem_logger_info,"HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(filesystem_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }

    server_filesystem = iniciar_servidor(filesystem_logger_info, config_valores_filesystem.ip_escucha, config_valores_filesystem.puerto_escucha);
    socket_kernel = esperar_cliente(server_filesystem, filesystem_logger_info);
    log_info(filesystem_logger_info, "Filesystem listo para recibir al Kernel");
    op_code codigo_operacion = recibir_operacion(socket_kernel);

    // Mapeo de memoria
	int fd;
	fd = open(config_valores_filesystem.path_bloques, O_RDWR);
	ftruncate(fd, tam_memoria_file_system);
	void* memoria_file_system = mmap(NULL, tam_memoria_file_system, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (formatear == 1)
		inicializar_datos_memoria(tam_memoria_file_system, memoria_file_system);
	inicializar_fcb_list(config_valores_filesystem.path_fcb, fcb_id, filesystem_logger_info);

	int exit_status = crear_fat(tamanio_fat, config_valores_filesystem.path_fat, filesystem_logger_info);
	if (exit_status == -1)
	{
		return -1;
	}

	pthread_t thread_kernel;
	//pthread_create(&thread_kernel, NULL, (void *)comunicacion_kernel, (void *)cliente);
	//pthread_join(thread_kernel, NULL);

	//eliminar_paquete(respuesta);

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

void inicializar_datos_memoria(int tam_memoria_file_system, void *memoria_file_system)
{
	int offset = tam_memoria_file_system;
	char caracter = '0';

	for (int i = 0; i < offset; i++)
	{
		memcpy(memoria_file_system + i, &caracter, sizeof(char));
	}
}

off_t obtener_primer_bloque_libre(t_bitarray *bitarray)
{
	off_t bit_libre;
	size_t tamanio_bitarray = bitarray_get_max_bit(bitarray);
	for (bit_libre = 0; bitarray_test_bit(bitarray, bit_libre); bit_libre++)
	{
		log_info(filesystem_logger_info, "El valor del bit analizado es: %ld", bit_libre);
		if (tamanio_bitarray == bit_libre)
		{
			log_info(filesystem_logger_info, "No hay bit libre dentro del bitmap");
			return -1;
		}
	}
	log_info(filesystem_logger_info, "El valor del bit libre es: %ld", bit_libre);
	return bit_libre;
}

void setear_bit_en_bitmap(t_bitarray *bitarray, off_t id_bloque) // Setea el bit en 1 para el bloque indicado
{
	bitarray_set_bit(bitarray, id_bloque);
	msync(memoria_file_system, bitarray->size, MS_SYNC);
	log_info(filesystem_logger_info, "Acceso a Bitmap - Bloque: %d - Estado: 1", id_bloque);
}

void limpiar_bit_en_bitmap(t_bitarray *bitarray, off_t id_bloque) // Setea el bit en 0 para el bloque indicado
{
	bitarray_clean_bit(bitarray, id_bloque);
	msync(memoria_file_system, bitarray->size, MS_SYNC);
	log_info(filesystem_logger_info, "Acceso a Bitmap - Bloque: %d - Estado: 0", id_bloque);
}

off_t obtener_bit_en_bitmap(t_bitarray *bitarray, off_t id_bloque) // Devuelve el bit del bloque indicado
{
	return bitarray_test_bit(bitarray, id_bloque);
}

int crear_fat(int cantidad_de_bloques, char *path_fat, t_log *logger)
{
    int tamanio_fat = cantidad_de_bloques;
    int file_descriptor = open(path_fat, O_CREAT | O_RDWR, 0644); // 0644 -> permissions for read/write
    if (file_descriptor < 0)
    {
        log_error(logger, "Failed to create the FAT file");
        return -1;
    }

    if (ftruncate(file_descriptor, tamanio_fat) != 0)
    {
        log_error(logger, "Failed to allocate disk space for the FAT file");
        close(file_descriptor);
        return -1;
    }

    void *fat_data = mmap(NULL, tamanio_fat, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (fat_data == MAP_FAILED)
    {
        log_error(logger, "Failed to map the FAT file into memory");
        close(file_descriptor);
        return -1;
    }

    fat_table = bitarray_create_with_mode(fat_data, tamanio_fat * 8, LSB_FIRST); // * 8 para obtener el tamaño en bits
    if (formatear == 1)
    {
        memset(fat_data, 0, tamanio_fat);
        log_info(logger, "FAT table formateada exitosamente");
    }

    // Marcar el bloque lógico 0 como reservado
    bitarray_set_bit(fat_table, 0);

    log_info(logger, "FAT table creada");
    return 0;
}

uint32_t buscar_fcb(char *nombre_fcb)
{
	int resultado = -1;
	int size = list_size(lista_global_fcb->lista_fcb);

	for (int i = 0; i < size; i++)
	{
		fcb_t *fcb = list_get(lista_global_fcb->lista_fcb, i);

		if (strcmp(fcb->nombre_archivo, nombre_fcb) == 0)
		{
			resultado = fcb->id;
			break;
		}
	}

	return resultado;
}

uint32_t buscar_fcb_id(int id)
{
	int resultado = -1;
	int size = list_size(lista_global_fcb->lista_fcb);

	for (int i = 0; i < size; i++)
	{
		fcb_t *fcb = list_get(lista_global_fcb->lista_fcb, i);

		if (fcb->id == id)
		{
			resultado = fcb->id;
			break;
		}
	}

	return resultado;
}

fcb_t *inicializar_fcb()
{
	fcb_t *new_fcb = malloc(sizeof(fcb_t));

	new_fcb->nombre_archivo = malloc(sizeof(char) * 15);
	memcpy(new_fcb->nombre_archivo, "0", sizeof(char) * 15);
	new_fcb->ruta_archivo = malloc(sizeof(char) * 20);
	memcpy(new_fcb->nombre_archivo, "0", sizeof(char) * 20);
	new_fcb->id = 0;
	new_fcb->bloque_inicial = 0;
	new_fcb->tamanio_archivo = 0;

	return new_fcb;
}

void inicializar_fcb_list(char *path_fcb, int fcb_id, t_log *logger)
{
	lista_global_fcb = malloc(sizeof(fcb_list_t));
	if (lista_global_fcb == NULL)
	{
		log_error(logger, "No se puedo alocar memoria para fcb_list_t");
		return -1;
	}
	lista_global_fcb->lista_fcb = list_create();

	char *directorio = path_fcb;

	DIR *dir_fcb = opendir(directorio);
	if (dir_fcb == NULL)
	{
		log_error(logger, "No se pudo abrir directorio: %s", directorio);
		return;
	}

	struct dirent *dir_entry;

	while ((dir_entry = readdir(dir_fcb)) != NULL)
	{
		if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0)
			continue;

		int error = 0;
		if (formatear == 1)
		{
			char *nombre_completo = string_new();
			string_append(&nombre_completo, directorio);
			string_append(&nombre_completo, dir_entry->d_name);

			remove(nombre_completo);
		}
		else
		{
			fcb_t *fcb_existente = malloc(sizeof(fcb_t));

			fcb_existente->nombre_archivo = string_new();

			char *nombre_completo = string_new();
			string_append(&nombre_completo, directorio);
			string_append(&nombre_completo, dir_entry->d_name);

			t_config *fcb = config_create(nombre_completo);
			if (config_has_property(fcb, "NOMBRE_ARCHIVO"))
				string_append(&fcb_existente->nombre_archivo, config_get_string_value(fcb, "NOMBRE_ARCHIVO"));
			else
				error = 1;
			if (config_has_property(fcb, "TAMANIO_ARCHIVO"))
				fcb_existente->tamanio_archivo = (uint32_t)config_get_int_value(fcb, "TAMANIO_ARCHIVO");
			else
				error = 1;
			if (config_has_property(fcb, "BLOQUE_INICIAL"))
				fcb_existente->bloque_inicial = (uint32_t)config_get_int_value(fcb, "PUNTERO_DIRECTO");
			else
				error = 1;
			fcb_existente->ruta_archivo = string_new();
			string_append(&fcb_existente->ruta_archivo, nombre_completo);
			fcb_existente->id = fcb_id;

			if (error == 0)
			{
				list_add(lista_global_fcb->lista_fcb, fcb_existente);
				fcb_id++;
			}
			else
			{
				log_error(logger, "Formato incorrecto de FCB");
				free(fcb_existente->nombre_archivo);
				free(fcb_existente->ruta_archivo);
				free(fcb_existente);
			}

			free(nombre_completo);
			config_destroy(fcb);
		}
	}
	log_info(logger, "Lista de FCB creada correctamente");
	closedir(dir_fcb);
}

int crear_fcb(char *nombre_fcb, t_log *logger, fcb_list_t *lista_global_fcb)
{
	int resultado = -1;

	if (buscar_fcb(nombre_fcb) != -1)
	{
		log_error(logger, "Ya existe un FCB con ese nombre");
		return resultado;
	}

	char *nombre_completo = string_new(); // esto creo que hay que vincularlo con el archivo actual
	string_append(&nombre_completo, nombre_fcb);
	string_append(&nombre_completo, ".dat");

	fcb_t *nuevo_fcb = inicializar_fcb();
	nuevo_fcb->id = fcb_id++;
	nuevo_fcb->nombre_archivo = string_new();
	string_append(&nuevo_fcb->nombre_archivo, nombre_fcb);

	t_config *fcb_fisico = malloc(sizeof(t_config));
	fcb_fisico->path = string_new();
	fcb_fisico->properties = dictionary_create();
	string_append(&fcb_fisico->path, nombre_completo);
	char *nombre_duplicado = string_duplicate(nombre_fcb);

	dictionary_put(fcb_fisico->properties, "NOMBRE_ARCHIVO", nombre_duplicado);
	dictionary_put(fcb_fisico->properties, "TAMANIO_ARCHIVO", "0");
	dictionary_put(fcb_fisico->properties, "BLOQUE_INICIAL", "0");

	config_save_in_file(fcb_fisico, nombre_completo);
	list_add(lista_global_fcb->lista_fcb, nuevo_fcb);
	dictionary_destroy(fcb_fisico->properties);
	free(nombre_duplicado);
	free(fcb_fisico->path);
	free(fcb_fisico);
	free(nombre_completo);

	return nuevo_fcb->id;
}

fcb_t *_get_fcb_id(int id, fcb_list_t *lista_global_fcb)
{
	fcb_t *resultado;
	int size = list_size(lista_global_fcb->lista_fcb);
	for (int i = 0; i < size; i++)
	{
		fcb_t *fcb = list_get(lista_global_fcb->lista_fcb, i);

		if (fcb->id == id)
		{
			resultado = fcb;
			break;
		}
	}

	return resultado;
}

fcb_t *_get_fcb(char *nombre, fcb_list_t *lista_global_fcb)
{
	return _get_fcb_id(buscar_fcb(nombre), lista_global_fcb);
}

uint32_t valor_para_fcb(fcb_t *fcb, fcb_prop_t llave)
{
	uint32_t valor = 0;
	switch (llave)
	{
	case TAMANIO_ARCHIVO:
		valor = fcb->tamanio_archivo;
		break;
	case BLOQUE_INICIAL:
		valor = fcb->bloque_inicial;
		break;
	default:
		break;
	}
	return valor;
}

uint32_t valor_fcb(int id, fcb_prop_t llave, fcb_list_t *lista_global_fcb)
{
	fcb_t *fcb = _get_fcb_id(id, lista_global_fcb);
	uint32_t valor = valor_para_fcb(fcb, llave);
	return valor;
}

int _modificar_fcb(fcb_t *fcb, fcb_prop_t llave, uint32_t valor)
{
	int resultado = 1;

	t_config *fcb_fisico = config_create(fcb->ruta_archivo);
	char *valor_string = string_itoa(valor);

	switch (llave)
	{
	case TAMANIO_ARCHIVO:
		fcb->tamanio_archivo = valor;
		config_set_value(fcb_fisico, "TAMANIO_ARCHIVO", valor_string);
		break;
	case BLOQUE_INICIAL:
		fcb->bloque_inicial = valor;
		config_set_value(fcb_fisico, "BLOQUE_INICIAL", valor_string);
		break;
	default:
		resultado = -1;
		break;
	}

	config_save(fcb_fisico);

	config_destroy(fcb_fisico);
	free(valor_string);

	return resultado;
}

int modificar_fcb(int id, fcb_prop_t llave, uint32_t valor)
{
	fcb_t *fcb = _get_fcb_id(id, lista_global_fcb);

	int resultado = _modificar_fcb(fcb, llave, valor);

	return resultado;
}

void escribir_int(uint32_t dato, int offset)
{
	memcpy(memoria_file_system + offset, &dato, 4);
	msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
	sleep(config_valores_filesystem.retardo_acceso_bloque);
}
/*
t_instruccion_fs *inicializar_instruc_mov()
{
	t_instruccion_fs *instruccion = malloc(sizeof(t_instruccion_fs));
	instruccion->pid = 0;
	instruccion->param1 = malloc(sizeof(char) * 2);
	memcpy(instruccion->param1, "0", (sizeof(char) * 2));
	instruccion->param1_length = sizeof(char) * 2;
	instruccion->param2 = malloc(sizeof(char) * 2);
	memcpy(instruccion->param2, "0", (sizeof(char) * 2));
	instruccion->param2_length = sizeof(char) * 2;
	instruccion->param3 = malloc(sizeof(char) * 2);
	memcpy(instruccion->param3, "0", (sizeof(char)));
	instruccion->param3_length = sizeof(char);
	instruccion->estado = CREATE_SEGMENT;
	log_info(filesystem_logger_info, "Instruccion mov inicializada exitosamente");

	return instruccion;
}
*/
void escribir_bloques(t_list *lista_bloques, int indice_inicial)
{
	int size = list_size(lista_bloques);

	for (int i = indice_inicial; i < size; i++)
	{
		offset_fcb_t *bloque = list_get(lista_bloques, i);

		int offset = (i * sizeof(uint32_t));

		escribir_int(bloque->id_bloque, offset);
	}

	sleep(config_valores_filesystem.retardo_acceso_bloque);
}

uint32_t leer_int(int offset, int size)
{
	uint32_t dato = 0;

	memcpy(&dato, memoria_file_system + offset, size);
	msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
	sleep(config_valores_filesystem.retardo_acceso_bloque);

	return dato;
}

void leer_bloques(int id_fcb, t_list *lista_de_bloques, int offset_inicial, int offset_final)
{
	/*
	int offset_fcb = offset_inicial;
	uint32_t bloque = valor_fcb(id_fcb, PUNTERO_INDIRECTO, lista_global_fcb);
	int offset = floor(((double)offset_fcb / block_size) - 1) * sizeof(uint32_t);

	while (offset_fcb < offset_final)
	{
		offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
		uint32_t dato = leer_int((bloque_indirecto * block_size) + offset, sizeof(uint32_t));
		memcpy(&bloque->id_bloque, &dato, sizeof(uint32_t));
		bloque->offset = bloque->id_bloque * block_size;
		list_add(lista_de_bloques, bloque);
		offset += sizeof(uint32_t);
		offset_fcb += block_size;
	}

	log_info(logger, "Acceso Bloque - Archivo: %s - Bloque File System: %d", nombre_archivo, valor_fcb(id_fcb, PUNTERO_INDIRECTO, lista_global_fcb));
	sleep(retardo_acceso_bloque);
	*/
}


int obtener_cantidad_de_bloques(int id_fcb)
{
	int tamanio_fcb = valor_fcb(id_fcb, TAMANIO_ARCHIVO, lista_global_fcb);
	int cant_bloques_fcb = ceil((double)tamanio_fcb / config_valores_filesystem.tam_bloque);

	return cant_bloques_fcb;
}

t_list *obtener_lista_total_de_bloques(int id_fcb)
{
	t_list *lista_de_bloques = list_create();
	int tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO, lista_global_fcb);
	if (tamanio_archivo == 0)
		return lista_de_bloques;
	int offset_fcb = 0;

	int cant_bloques_fcb = obtener_cantidad_de_bloques(id_fcb);
	int size_final = cant_bloques_fcb * config_valores_filesystem.tam_bloque;

	offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
	bloque->id_bloque = valor_fcb(id_fcb, BLOQUE_INICIAL, lista_global_fcb);
	bloque->offset = bloque->id_bloque * config_valores_filesystem.tam_bloque;
	cant_bloques_fcb--;
	offset_fcb += config_valores_filesystem.tam_bloque;

	list_add(lista_de_bloques, bloque);

	leer_bloques(id_fcb, lista_de_bloques, offset_fcb, size_final);

	return lista_de_bloques;
}

void asignar_bloques(int id_fcb, int nuevo_tamanio)
{
	t_list *lista_total_de_bloques = obtener_lista_total_de_bloques(id_fcb);
	int size_inicial = list_size(lista_total_de_bloques);
	int cant_bloques_a_asignar = ceil((double)nuevo_tamanio / config_valores_filesystem.tam_bloque);
	int cant_bloques_actual = ceil((double)valor_fcb(id_fcb, TAMANIO_ARCHIVO, lista_global_fcb) / config_valores_filesystem.tam_bloque);
	for (int i = cant_bloques_actual; i < cant_bloques_a_asignar; i++)
	{
		int id_bloque = obtener_primer_bloque_libre(fat_table);
		offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));

		size_inicial++;
		modificar_fcb(id_fcb, BLOQUE_INICIAL, id_bloque);
		setear_bit_en_bitmap(fat_table, id_bloque);
		bloque->id_bloque = id_bloque;
		list_add(lista_total_de_bloques, bloque);

		bloque->id_bloque = id_bloque;
		setear_bit_en_bitmap(fat_table, id_bloque);
		list_add(lista_total_de_bloques, bloque);
	}

	int size_final = list_size(lista_total_de_bloques);


	list_destroy_and_destroy_elements(lista_total_de_bloques, free);

	modificar_fcb(id_fcb, TAMANIO_ARCHIVO, nuevo_tamanio);
}

/*

void desasignar_bloques(int id_fcb, int nuevo_tamanio)
{
	t_list *lista_de_bloques = obtener_lista_total_de_bloques(id_fcb);
	int tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO, lista_global_fcb);
	int cant_bloques_a_desasignar = ceil(((double)(tamanio_archivo - nuevo_tamanio) / config_valores_filesystem.tam_bloque)); // Usar ceil()

	int i = 0;
	if (nuevo_tamanio <= config_valores_filesystem.tam_bloque && tamanio_archivo > config_valores_filesystem.tam_bloque)
	{
		cant_bloques_a_desasignar++;
	}

	while (i < cant_bloques_a_desasignar)
	{
		offset_fcb_t *bloque = list_pop(lista_de_bloques);
		limpiar_bit_en_bitmap(fat_table, bloque->id_bloque);
		i++;
	}

	list_destroy_and_destroy_elements(lista_de_bloques, free);

	modificar_fcb(id_fcb, TAMANIO_ARCHIVO, nuevo_tamanio);
}*/