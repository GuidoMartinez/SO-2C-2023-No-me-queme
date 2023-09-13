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
	int tam_memoria_file_system = config_valores_filesystem.cant_bloques_total * config_valores_filesystem.tam_bloque;

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

    // Mapeo de memoria
	int fd;
	fd = open(config_valores_filesystem.path_bloques, O_RDWR);
	ftruncate(fd, tam_memoria_file_system);
	void* memoria_file_system = mmap(NULL, tam_memoria_file_system, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (formatear == 1)
		inicializar_datos_memoria(tam_memoria_file_system, memoria_file_system);
	inicializar_fcb_list(config_valores_filesystem.path_fcb, fcb_id, filesystem_logger_info);

	int exit_status = crear_fat(config_valores_filesystem.cant_bloques_total, config_valores_filesystem.path_fat, filesystem_logger_info);
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

int crear_fat(int cantidad_de_bloques, char *path_bitmap, t_log *logger)
{
	int tamanio_bitmap = cantidad_de_bloques / 8;
	int file_descriptor = open(path_bitmap, O_CREAT | O_RDWR, 0644); // 0644 -> permissions for read/write
	if (file_descriptor < 0)
	{
		log_error(logger, "Failed to create the bitmap file");
		return -1;
	}

	if (ftruncate(file_descriptor, tamanio_bitmap) != 0)
	{
		log_error(logger, "Failed to allocate disk space for the bitmap file");
		close(file_descriptor);
		return -1;
	}
	else
		ftruncate(file_descriptor, tamanio_bitmap);

	void *bitmap_data = mmap(NULL, tamanio_bitmap, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
	if (bitmap_data == MAP_FAILED)
	{
		log_error(logger, "Failed to map the bitmap file into memory");
		close(file_descriptor);
		return -1;
	}

	if (formatear == 1)
	{
		memset(bitmap_data, 0, tamanio_bitmap);
		log_info(logger, "Bitmap formateado exitosamente");
	}
	bitarray = bitarray_create_with_mode(bitmap_data, tamanio_bitmap, LSB_FIRST);
	log_info(logger, "Bitmap creado");
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