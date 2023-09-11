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
    realizar_handshake(socket_memoria, HANDSHAKE_FILESYSTEM, filesystem_logger_info);

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