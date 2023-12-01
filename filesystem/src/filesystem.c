#include "filesystem.h"
// para ejecutar el fs es: ./filesyste ./cfg/fs.cfg

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
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }
    filesystem_logger_info = log_create("./cfg/filesystem.log", "FILESYSTEM", true, LOG_LEVEL_INFO);
    cargar_configuracion(argv[1]);
    retardo_acceso_bloque = config_valores_filesystem.retardo_acceso_bloque;
    cant_bloques = config_valores_filesystem.cant_bloques_total;
    tamanio_bloque = config_valores_filesystem.tam_bloque;
    tam_memoria_file_system = config_valores_filesystem.cant_bloques_total * config_valores_filesystem.tam_bloque;
    tamanio_fat = (config_valores_filesystem.cant_bloques_total - config_valores_filesystem.cant_bloques_swap);
    fat = malloc(tamanio_fat * sizeof(bloque));
    path_fcb = config_valores_filesystem.path_fcb;
    bloques_swap = config_valores_filesystem.cant_bloques_swap;
    swap = malloc(bloques_swap * sizeof(bloque));
    socket_memoria = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    realizar_handshake(socket_memoria, HANDSHAKE_FILESYSTEM, filesystem_logger_info);
    op_code handshake_memoria = recibir_operacion(socket_memoria);
    if (handshake_memoria == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(socket_memoria, filesystem_logger_info);
        log_info(filesystem_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(filesystem_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(filesystem_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }
    // t_paquete *respuesta = recibir_paquete(socket_memoria);

    // CONEXION CON KERNEL
    server_filesystem = iniciar_servidor(filesystem_logger_info, config_valores_filesystem.ip_escucha, config_valores_filesystem.puerto_escucha);
    socket_kernel = esperar_cliente(server_filesystem, filesystem_logger_info);
    log_info(filesystem_logger_info, "Filesystem listo para recibir al Kernel");
    op_code codigo_operacion = recibir_operacion(socket_kernel);

    log_info(filesystem_logger_info, "Se recibio una operacion de KERNEL: %d", codigo_operacion);
    switch (codigo_operacion)
    {
    case HANDSHAKE_KERNEL:
        log_info(filesystem_logger_info, "Handshake exitosa con KERNEL, se conecto un KERNEL");
        recibir_handshake(socket_kernel, filesystem_logger_info);
        realizar_handshake(socket_kernel, HANDSHAKE_FILESYSTEM, filesystem_logger_info);
        break;
    default:
        log_error(filesystem_logger_info, "Fallo la comunicacion con KERNEL. Abortando \n");
        abort();
        // finalizar_memoria();
        break;
    }
    // Mapeo de memoria
    int fd;
    fd = open(config_valores_filesystem.path_bloques, O_RDWR);
    ftruncate(fd, tam_memoria_file_system);
    memoria_file_system = mmap(NULL, tam_memoria_file_system, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (formatear == 1)
        inicializar_datos_memoria(tam_memoria_file_system, memoria_file_system);
    inicializarFATDesdeDirectorio(config_valores_filesystem.path_fcb, filesystem_logger_info);
    int exit_status = crearFAT(config_valores_filesystem.path_fat, filesystem_logger_info);
    if (exit_status == -1)
    {
        return -1;
    }
    pthread_t thread_kernel;
    pthread_create(&thread_kernel, NULL, (void *)comunicacion_kernel, (void *)socket_kernel);
    pthread_join(thread_kernel, NULL);
    //eliminar_paquete(respuesta);

    while(1);
    abort();
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
    close(socket_kernel);
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

int obtener_cantidad_de_bloques(int id_fcb)
{
    int tamanio_fcb = valor_fcb(id_fcb, TAMANIO_ARCHIVO);
    int cant_bloques_fcb = ceil((double)tamanio_fcb / config_valores_filesystem.tam_bloque);
    return cant_bloques_fcb;
}

off_t obtener_primer_bloque_libre(bloque *fat)
{
    for (off_t i = 0; i < tamanio_fat; i++)
    {
        if (!fat[i].utilizado)
        {
            return i;
        }
    }
    return -1;
}

bool bloque_esta_ocupado(bloque *fat, int tamanio_fat, int id_bloque)
{
    if (id_bloque < 0 || id_bloque >= tamanio_fat)
    {
        return false;
    }
    bloque *bloque = &fat[id_bloque];
    return bloque->utilizado;
}

int crear_fat(int cantidad_de_bloques, char *path_fat, t_log *logger)
{
    int tamanio_fat = cantidad_de_bloques * sizeof(bloque); 
    int file_descriptor = open(path_fat, O_CREAT | O_RDWR, 0644);
    if (file_descriptor < 0)
    {
        log_error(logger, "Error al crear el archivo FAT");
        return -1;
    }
    if (ftruncate(file_descriptor, tamanio_fat) != 0)
    {
        log_error(logger, "Error al asignar espacio en disco para el archivo FAT");
        close(file_descriptor);
        return -1;
    }
    bloque *fat = mmap(NULL, tamanio_fat, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (fat == MAP_FAILED)
    {
        log_error(logger, "Error al mapear el archivo FAT en memoria");
        close(file_descriptor);
        return -1;
    }
    if (formatear == 1)
    {
        for (int i = 0; i < cantidad_de_bloques; i++)
        {
            fat[i].utilizado = false;
        }
        log_info(logger, "Tabla FAT formateada exitosamente");
    }
    fat[0].utilizado = true;
    fat[0].next_block = 0; 
    
    log_info(logger, "Tabla FAT creada");
    return 0;
}

void leerBloque(bloque *block, int block_number)
{
    if (block_number < 0 || block_number >= tamanio_fat)
    {
        log_error(filesystem_logger_info, "Error al leer el bloque %d desde la tabla FAT. Bloque fuera de los límites.", block_number);
        return;
    }
    *block = fat[block_number];
}

void escribirBloque(bloque *block, int block_number)
{
    if (block_number < 0 || block_number >= tamanio_fat)
    {
        log_error(filesystem_logger_info, "Error al escribir en el bloque %d en la tabla FAT. Bloque fuera de los límites.", block_number);
        return;
    }
    fat[block_number] = *block;
}


void guardarFAT(const char *nombreArchivo, bloque *fat, int tamanio_fat)
{
    int file_descriptor = open(nombreArchivo, O_CREAT | O_RDWR, 0644);
    if (file_descriptor < 0)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo FAT para escritura");
        return;
    }

    if (ftruncate(file_descriptor, tamanio_fat * sizeof(bloque)) != 0)
    {
        log_error(filesystem_logger_info, "Error al asignar espacio en disco para el archivo FAT");
        close(file_descriptor);
        return;
    }
    bloque *archivo_mapeado = mmap(NULL, tamanio_fat * sizeof(bloque), PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (archivo_mapeado == MAP_FAILED)
    {
        log_error(filesystem_logger_info, "Error al mapear el archivo FAT en memoria");
        close(file_descriptor);
        return;
    }
    memcpy(archivo_mapeado, fat, tamanio_fat * sizeof(bloque));
    munmap(archivo_mapeado, tamanio_fat * sizeof(bloque));
    close(file_descriptor);
}

bloque *cargarFAT(const char *nombreArchivo)
{
    FILE *archivo = fopen(nombreArchivo, "rb"); 

    if (archivo)
    {
        if (fat == NULL)
        {
            fclose(archivo);
            log_error(filesystem_logger_info, "Error al asignar memoria para cargar la FAT");
            return NULL;
        }

        if (fread(fat, sizeof(bloque), tamanio_fat, archivo) != tamanio_fat)
        {
            fclose(archivo);
            free(fat);
            log_error(filesystem_logger_info, "Error al leer la FAT desde el archivo");
            return NULL;
        }

        fclose(archivo);
        return fat;
    }
    else
    {
        log_error(filesystem_logger_info, "No se pudo abrir el archivo '%s' para lectura", nombreArchivo);
        return NULL;
    }
}

void liberarMemoriaFAT(bloque *fat)
{
    free(fat);
}

void inicializarFATDesdeDirectorio(char *directorio, t_log *logger)
{
    DIR *dir_fat = opendir(directorio);
    if (dir_fat == NULL)
    {
        log_error(logger, "No se pudo abrir directorio: %s", directorio);
        return;
    }
    struct dirent *dir_entry;
    while ((dir_entry = readdir(dir_fat)) != NULL)
    {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0)
            continue;
        char *nombre_completo = string_new();
        string_append(&nombre_completo, directorio);
        string_append(&nombre_completo, dir_entry->d_name);
        bloque *bloque_existente = (bloque *)malloc(sizeof(bloque));
        if (bloque_existente != NULL)
        {
            list_add(fat, bloque_existente);
        }
        free(nombre_completo);
    }
    log_info(logger, "FAT inicializada correctamente desde el directorio");
    closedir(dir_fat);
}

int crearFAT(char *path_fat, t_log *logger)
{
    FILE *archivo = fopen(path_fat, "wb");
    if (archivo)
    {
        bloque *fat = malloc(tamanio_fat * sizeof(bloque));
        if (fat == NULL)
        {
            log_error(logger, "Error al asignar memoria para la tabla FAT");
            fclose(archivo);
            return -1;
        }

        for (int i = 0; i < tamanio_fat; i++)
        {
            fat[i].next_block = 0;
            fat[i].tamanio = tamanio_bloque;
            fat[i].id = i;
        }

        fwrite(fat, sizeof(bloque), tamanio_fat, archivo);
        fclose(archivo);

        free(fat);

        log_info(logger, "Tabla FAT creada correctamente");
        return 0;
    }
    else
    {
        log_error(logger, "No se pudo abrir el archivo para escritura");
        return -1;
    }
}

int crear_fcb(char *nombre_fcb)
{
    int resultado = -1;

    if (buscar_fcb(nombre_fcb) != -1)
    {
        log_error(filesystem_logger_info, "Ya existe un FCB con ese nombre: %s", nombre_fcb);
        return resultado;
    }

    char *nombre_completo = string_from_format("%s%s.dat", path_fcb, nombre_fcb);
    if (nombre_completo == NULL)
    {
        log_error(filesystem_logger_info, "Error al construir el nombre completo del FCB");
        return resultado;
    }

    fcb_t *nuevo_fcb = inicializar_fcb();
    if (nuevo_fcb == NULL)
    {
        log_error(filesystem_logger_info, "Error al inicializar el FCB");
        free(nombre_completo);
        return resultado;
    }

    nuevo_fcb->id = fcb_id++;
    nuevo_fcb->nombre_archivo = string_duplicate(nombre_fcb);
    nuevo_fcb->ruta_archivo = string_duplicate(nombre_completo);

    t_config *fcb_fisico = malloc(sizeof(t_config));
    if (fcb_fisico == NULL)
    {
        log_error(filesystem_logger_info, "Error al asignar memoria para el FCB físico");
        borrar_fcb(nuevo_fcb);
        free(nombre_completo);
        return resultado;
    }

    fcb_fisico->path = string_duplicate(nombre_completo);
    fcb_fisico->properties = dictionary_create();
    if (fcb_fisico->path == NULL || fcb_fisico->properties == NULL)
    {
        log_error(filesystem_logger_info, "Error al asignar memoria para el FCB físico");
        borrar_fcb(nuevo_fcb);
        free(nombre_completo);
        free(fcb_fisico->path);
        free(fcb_fisico->properties);
        free(fcb_fisico);
        return resultado;
    }

    char *nombre_duplicado = string_duplicate(nombre_fcb);
    dictionary_put(fcb_fisico->properties, "NOMBRE_ARCHIVO", nombre_duplicado);
    dictionary_put(fcb_fisico->properties, "TAMANIO_ARCHIVO", "0");
    dictionary_put(fcb_fisico->properties, "BLOQUE_INICIAL", "0");

    if (!config_save_in_file(fcb_fisico, nombre_completo))
    {
        log_error(filesystem_logger_info, "Error al guardar el FCB físico en el archivo");
        borrar_fcb(nuevo_fcb);
        free(nombre_completo);
        free(fcb_fisico->path);
        free(fcb_fisico->properties);
        free(fcb_fisico);
        return resultado;
    }

    list_add(lista_fcb, nuevo_fcb);
    free(nombre_duplicado);
    free(nombre_completo);
    dictionary_destroy(fcb_fisico->properties);
    free(fcb_fisico->path);
    free(fcb_fisico);

    log_info(filesystem_logger_info, "FCB creado correctamente: %s", nombre_fcb);
    return nuevo_fcb->id;
}

uint32_t buscar_fcb(char *nombre_fcb)
{
    uint32_t resultado = -1;
    for (int i = 0; i < tamanio_fat; i++)
    {
        fcb_t *fcb = &(fat[i]);

        if (strcmp(fcb->nombre_archivo, nombre_fcb) == 0)
        {
            resultado = fcb->id;
            break;
        }
    }
    log_info(filesystem_logger_info, "No se encontró un FCB con el nombre: %s", nombre_fcb);
    return resultado;
}

uint32_t buscar_fcb_id(int id)
{
    int resultado = -1;
    int size = list_size(lista_fcb);
    for (int i = 0; i < size; i++)
    {
        fcb_t *fcb = list_get(lista_fcb, i);
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
    memset(new_fcb->nombre_archivo, 0, sizeof(char) * 15);
    new_fcb->ruta_archivo = malloc(sizeof(char) * 20);
    memset(new_fcb->ruta_archivo, 0, sizeof(char) * 20);
    new_fcb->id = 0;
    new_fcb->bloque_inicial = 0;
    new_fcb->tamanio_archivo = 0;

    return new_fcb;
}

fcb_t *_get_fcb_id(int id)
{
    fcb_t *resultado = NULL;
    int size = list_size(lista_fcb);
    for (int i = 0; i < size; i++)
    {
        fcb_t *fcb = list_get(lista_fcb, i);

        if (fcb->id == id)
        {
            resultado = fcb;
            break;
        }
    }
    return resultado;
}

fcb_t *_get_fcb(char *nombre)
{
    int id = buscar_fcb(nombre);
    if (id != -1)
    {
        return _get_fcb_id(id);
    }
    return NULL;
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
    case NOMBRE_ARCHIVO:
        valor = fcb->nombre_archivo;
        break;
    default:
        break;
    }
    return valor;
}

uint32_t valor_fcb(int id, fcb_prop_t llave)
{
    fcb_t *fcb = _get_fcb_id(id);
    if (fcb != NULL)
    {
        return valor_para_fcb(fcb, llave);
    }
    return 0; 
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
    case NOMBRE_ARCHIVO:
        fcb->nombre_archivo = valor;
        config_set_value(fcb_fisico, "NOMBRE_ARCHIVO", valor_string);
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
    fcb_t *fcb = _get_fcb_id(id);
    if (fcb != NULL)
    {
        return _modificar_fcb(fcb, llave, valor);
    }
    return -1; 
}

void escribir_int(uint32_t dato, bloque *fat, int indice)
{
    int offset = indice * sizeof(bloque);
    memcpy(fat + indice, &dato, sizeof(uint32_t));
    msync(fat, tamanio_fat * sizeof(bloque), MS_SYNC);
    sleep(config_valores_filesystem.retardo_acceso_bloque);
}

void escribir_bloques(bloque *fat, int indice_inicial, int size)
{
    for (int i = indice_inicial; i < size; i++)
    {
        bloque *bloque = &fat[i];
        escribir_int(bloque->id, fat, i);
    }
    sleep(config_valores_filesystem.retardo_acceso_bloque);
}

uint32_t leer_int(bloque *fat, int indice)
{
    uint32_t dato = 0;
    int offset = indice * sizeof(bloque);
    memcpy(&dato, fat + indice, sizeof(uint32_t));
    msync(fat, tamanio_fat * sizeof(bloque), MS_SYNC);
    sleep(config_valores_filesystem.retardo_acceso_bloque);
    return dato;
}

t_list *obtener_lista_total_de_bloques(int id_fcb, FILE *file)
{
    t_list *lista_de_bloques = list_create();
    int tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO);
    if (tamanio_archivo == 0)
        return lista_de_bloques;
    int cant_bloques_fcb = obtener_cantidad_de_bloques(id_fcb);
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);
    for (int i = 0; i < cant_bloques_fcb; i++)
    {
        offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
        bloque->id_bloque = bloque_inicial + i;
        bloque->offset = (long)bloque->id_bloque * sizeof(bloque);
        list_add(lista_de_bloques, bloque);
    }
    return lista_de_bloques;
}

void asignar_bloques(int id_fcb, int nuevo_tamanio, bloque *fat)
{
    int tam_bloque = config_valores_filesystem.tam_bloque;
    int cant_bloques_a_asignar = ceil((double)nuevo_tamanio / tam_bloque);
    int cant_bloques_actual = obtener_cantidad_de_bloques(id_fcb);
    for (int i = cant_bloques_actual; i < cant_bloques_a_asignar; i++)
    {
        offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
        bloque->id_bloque = obtener_primer_bloque_libre(fat);
        if (bloque->id_bloque == -1)
        {
            free(bloque);
            break;
        }
        bloque->offset = (uint32_t)bloque->id_bloque * tam_bloque;
        bloque->tamanio = tam_bloque;
        fat[bloque->id_bloque].utilizado = true;
        modificar_fcb(id_fcb, BLOQUE_INICIAL, fat[bloque->id_bloque].id);
        free(bloque);
    }
}

void* list_pop(t_list* list){
	int last_element_index = (list->elements_count - 1);
	return list_remove(list, last_element_index);
}

void desasignar_bloques(int id_fcb, int nuevo_tamanio, bloque *fat)
{
    t_list *lista_de_bloques = obtener_lista_total_de_bloques(id_fcb, fat);
    int tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO);
    int cant_bloques_a_desasignar = ceil(((double)(tamanio_archivo - nuevo_tamanio) / config_valores_filesystem.tam_bloque));
    int i = 0;
    while (i < cant_bloques_a_desasignar)
    {
        offset_fcb_t *bloque = list_pop(lista_de_bloques);
        fat[bloque->id_bloque].utilizado = false;
        i++;
    }
    list_destroy_and_destroy_elements(lista_de_bloques, free);
    modificar_fcb(id_fcb, TAMANIO_ARCHIVO, nuevo_tamanio);
}

int truncar_fcb(char *nombre_fcb, uint32_t nuevo_tamanio)
{
    int resultado = -1;
    int id_fcb = buscar_fcb(nombre_fcb);
    if (id_fcb != -1)
    {
        uint32_t tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO);
        if (nuevo_tamanio > tamanio_archivo)
        {
            asignar_bloques(id_fcb, nuevo_tamanio, fat);
        }
        else if (nuevo_tamanio < tamanio_archivo)
        {
            desasignar_bloques(id_fcb, nuevo_tamanio, fat);
        }
        resultado = 0;
    }
    return resultado;
}

void escribir_dato(void *dato, int offset, int size)
{
	memcpy(memoria_file_system + offset, dato, size);
	msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
	sleep(retardo_acceso_bloque);
}

void escribir_datos(void *datos, t_list *lista_offsets, int id_fcb, bloque *fat) {
    int cant_bloques = list_size(lista_offsets);

    for (int i = 0; i < cant_bloques; i++) {
        offset_fcb_t *bloque_info = list_get(lista_offsets, i);
        int bloque_id = bloque_info->id_bloque;
        log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);

        if (!fat[bloque_id].utilizado) {
            asignar_bloques(id_fcb, bloque_info->tamanio, fat);
        }
        escribir_dato(datos + bloque_info->offset, bloque_info->offset, bloque_info->tamanio);
    }
}

t_list *obtener_lista_de_bloques(int id_fcb, int p_seek, int tam_a_leer, t_list *lista_fcb, t_log *logger) {
    t_list *lista_bloques = list_create();
    int tamanio_bloque = config_valores_filesystem.tam_bloque;
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);
    int bloque_apuntado = ceil((double)(p_seek + 1) / tamanio_bloque);
    if (bloque_inicial == -1) {
        log_error(logger, "El FCB no tiene bloque inicial asignado");
        return NULL;
    }
    if (tam_a_leer <= 0) {
        log_error(logger, "El tamaño a leer es inválido");
        return NULL;
    }
    int bloque_actual = bloque_inicial;
    int tam_leido = 0;
    while (tam_leido < tam_a_leer) {
        int offset_bloque = bloque_actual * tamanio_bloque;
        int tam_restante = tam_a_leer - tam_leido;
        int bytes_leer = (tam_restante < tamanio_bloque) ? tam_restante : tamanio_bloque;
        offset_fcb_t *bloque_info = malloc(sizeof(offset_fcb_t));
        if (bloque_info == NULL) {
            log_error(logger, "Error de asignación de memoria para bloque_info");
            break;
        }
        bloque_info->id_bloque = bloque_actual;
        bloque_info->offset = offset_bloque;
        bloque_info->tamanio = bytes_leer;
        list_add(lista_bloques, bloque_info);
        tam_leido += bytes_leer;
        bloque_actual = leer_int(fat, bloque_actual);
        if (bloque_actual < 0 || bloque_actual >= tamanio_fat) {
            log_error(logger, "Error: El bloque apuntado está fuera de rango");
            break;
        }
        int bloque_siguiente = ceil((double)(p_seek + 1 + tam_leido) / tamanio_bloque);
        if (bloque_actual != bloque_siguiente) {
            log_error(logger, "Error en la secuencia de bloques");
            break;
        }
    }
    if (tam_leido != tam_a_leer) {
        list_destroy_and_destroy_elements(lista_bloques, free);
        return NULL;
    }
    return lista_bloques;
}


t_list *armar_lista_offsets(int id_fcb, int tam_a_leer, int p_seek) {
    t_list *lista_offsets = list_create();
    int bloque_apuntado = ceil((double)(p_seek + 1) / tamanio_bloque);
    int nro_bloque = 0;
    t_list *lista_bloques = obtener_lista_de_bloques(id_fcb, p_seek, tam_a_leer, lista_fcb, filesystem_logger_info);
    if (lista_bloques != NULL) {
        int cant_bloques = list_size(lista_bloques);
        while (nro_bloque < cant_bloques) {
            offset_fcb_t *nuevo_offset = malloc(sizeof(offset_fcb_t));

            if (nuevo_offset != NULL) {
                offset_fcb_t *bloque = list_get(lista_bloques, nro_bloque);

                nuevo_offset->offset = bloque->id_bloque * tamanio_bloque;
                nuevo_offset->id_bloque = bloque->id_bloque;
                nuevo_offset->tamanio = tamanio_bloque;

                if (tam_a_leer < tamanio_bloque) {
                    nuevo_offset->offset = nuevo_offset->offset + (p_seek - ((bloque_apuntado - 1) * tamanio_bloque));
                    nuevo_offset->tamanio = tam_a_leer;
                } else if (nro_bloque + 1 == cant_bloques) {
                    nuevo_offset->tamanio = (p_seek + tam_a_leer) - ((bloque_apuntado - 1) * tamanio_bloque);
                } else if (nro_bloque == 0) {
                    nuevo_offset->offset = nuevo_offset->offset + (p_seek - ((bloque_apuntado - 1) * tamanio_bloque));
                    nuevo_offset->tamanio = (bloque_apuntado * tamanio_bloque) - p_seek;
                }

                nro_bloque++;
                bloque_apuntado = ceil(((double)(p_seek + 1) + (tamanio_bloque * nro_bloque)) / tamanio_bloque);

                list_add(lista_offsets, nuevo_offset);
            } else {
                log_error(filesystem_logger_info, "Error: No se pudo asignar memoria para nuevo_offset.");
                break;
            }
        }

        list_destroy_and_destroy_elements(lista_bloques, free);
    } else {
        log_error(filesystem_logger_info, "Error: No se pudieron obtener los bloques necesarios.");
    }

    return lista_offsets;
}

void realizar_f_write(t_instruccion_fs *instruccion_file) {
    uint32_t pid = instruccion_file->pid;
    uint32_t direccion_fisica = instruccion_file->param1_length; 
    int tamanio = instruccion_file->param2_length; 
    int puntero_archivo = instruccion_file->puntero; 
    t_paquete *paquete = crear_paquete_con_codigo_de_operacion(F_WRITE);
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    enviar_paquete(paquete, socket_memoria);
    eliminar_paquete(paquete);
    op_code codigo = recibir_operacion(socket_memoria);
    if (codigo == F_WRITE) {
        t_list *paquete_recibido = recibir_paquete(socket_memoria);
        if (paquete_recibido != NULL && list_size(paquete_recibido) > 0) {
            char *valor_recibido = instruccion_file->param1; 
            uint32_t id_fcb = buscar_fcb(instruccion_file->param1); 
            t_list *lista_offsets = armar_lista_offsets(id_fcb, tamanio, direccion_fisica);
            if (lista_offsets != NULL) {
                int cant_bloques = list_size(lista_offsets);
                for (int i = 0; i < cant_bloques; i++) {
                    offset_fcb_t *bloque_info = list_get(lista_offsets, i);
                    int bloque_id = bloque_info->id_bloque;
                    log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);

                    if (!fat[bloque_id].utilizado) {
                        asignar_bloques(id_fcb, bloque_info->tamanio, fat);
                    }

                    escribir_dato(valor_recibido + bloque_info->offset, bloque_info->offset, bloque_info->tamanio);
                }
                list_destroy_and_destroy_elements(lista_offsets, free);
            }
            list_destroy_and_destroy_elements(paquete_recibido, free);
        }
    }
}

void *leer_dato(int offset, int size) {
    void *dato = malloc(size);
    memcpy(dato, memoria_file_system + offset, size);
    msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
    sleep(retardo_acceso_bloque);
    return dato;
}

void *leer_datos(t_list *lista_offsets) {
    int cant_bloques = list_size(lista_offsets);
    int offset = 0;
    void *datos = malloc(0); 
    for (int i = 0; i < cant_bloques; i++) {
        offset_fcb_t *bloque = list_get(lista_offsets, i);
        log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque->id_bloque);
        void *dato = leer_dato(bloque->offset, bloque->tamanio);
        datos = realloc(datos, offset + bloque->tamanio);
        memcpy(datos + offset, dato, bloque->tamanio);
        offset += bloque->tamanio;
        free(dato);
    }
    return datos;
}

void realizar_f_read(t_instruccion_fs *instruccion_file)
{
		int direccion_fisica = atoi(instruccion_file->param2);
		int puntero_archivo = instruccion_file->puntero;
		int id_fcb = buscar_fcb(instruccion_file->param1);
		t_list *lista_de_bloques = armar_lista_offsets(id_fcb, tamanio_bloque, direccion_fisica);
		log_info(filesystem_logger_info, "Entro a realizar f read");
		void *datos = leer_datos(lista_de_bloques);
		t_paquete *paquete = crear_paquete_con_codigo_de_operacion(F_READ_FS);
		agregar_a_paquete(paquete, &(instruccion_file->pid), sizeof(uint32_t));
		agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
		agregar_a_paquete(paquete, &tamanio_bloque, sizeof(uint32_t));
		agregar_a_paquete(paquete, datos, tamanio_bloque);
		enviar_paquete(paquete, socket_memoria);
		eliminar_paquete(paquete);
		op_code respuesta = recibir_operacion(socket_memoria);
		if (respuesta == MENSAJE)
		{
			char *mensaje = recibir_mensaje_sin_log(socket_memoria);
			free(mensaje);
		}
}

int borrar_fcb(int id)
{
    int resultado = -1;

    if (buscar_fcb_id(id) == -1)
    {
        log_error(filesystem_logger_info, "No existe un FCB con ese ID: %d", id);
        return resultado;
    }

    fcb_t *fcb = _get_fcb_id(id);

    if (remove(fcb->ruta_archivo) != 0)
    {
        log_error(filesystem_logger_info, "Error al eliminar el archivo asociado al FCB (ID: %d)", id);
        return resultado;
    }

    list_remove_element(lista_fcb, fcb);
    free(fcb->nombre_archivo);
    free(fcb->ruta_archivo);
    free(fcb);

    resultado = 0;
    return resultado;
}

void destroy_instruccion_file(t_instruccion_fs *instruccion)
{
	free(instruccion->param1);
	free(instruccion->param2);
	free(instruccion->puntero);
	free(instruccion);
	log_info(filesystem_logger_info, "Instruccion fs destruida exitosamente");
}

t_paquete *crear_paquete_con_respuesta(t_resp_file *estado_file)
{
		t_paquete *paq_respuesta = crear_paquete_con_codigo_de_estado(estado_file);
		enviar_paquete(paq_respuesta, socket_kernel);
		eliminar_paquete(paq_respuesta);
}

void comunicacion_kernel() {
    int exit_status = 0;
    while (exit_status == 0) {
        t_paquete *paquete = crear_paquete_con_codigo_de_operacion(HANDSHAKE_FILESYSTEM);
        paquete->buffer = malloc(sizeof(t_buffer));
        nombre_instruccion codigo_op = recibir_operacion(socket_kernel);

        switch (codigo_op) {
            case 1: {
                t_instruccion_fs *nueva_instruccion = deserializar_instruccion_fs(socket_kernel);
                uint32_t pid = nueva_instruccion->pid;
                t_resp_file estado_file = F_ERROR;

                switch (nueva_instruccion->estado) {
                    case F_OPEN:
                        if (buscar_fcb(nueva_instruccion->param1) != -1) {
                            log_info(filesystem_logger_info, "PID: %d - F_OPEN: %s", nueva_instruccion->pid, nueva_instruccion->param1);
                            log_info(filesystem_logger_info, "Abrir Archivo: %s", nueva_instruccion->param1);
                            estado_file = F_OPEN_SUCCESS;
                        } else {
                            log_info(filesystem_logger_info, "PID: %d - F_OPEN: %s - NO EXISTE", nueva_instruccion->pid, nueva_instruccion->param1);
                            estado_file = FILE_DOESNT_EXISTS;
                        }
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case F_CREATE:
                        if (crear_fcb(nueva_instruccion->param1) != -1) {
                            log_info(filesystem_logger_info, "PID: %d - F_CREATE: %s", nueva_instruccion->pid, nueva_instruccion->param1);
                            log_info(filesystem_logger_info, "Crear Archivo: %s", nueva_instruccion->param1);
                            estado_file = F_CREATE_SUCCESS;
                        }
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case F_CLOSE:
                        if (buscar_fcb(nueva_instruccion->param1) != -1) {
                            estado_file = F_CLOSE_SUCCESS;
                            log_info(filesystem_logger_info, "PID: %d - F_CLOSE: %s", nueva_instruccion->pid, nueva_instruccion->param1);
                        }
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case F_TRUNCATE:
                        log_info(filesystem_logger_info, "PID: %d - F_TRUNCATE: %s - Tamanio: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->param2);
                        log_info(filesystem_logger_info, "Truncar Archivo: %s - Tamaño: %s", nueva_instruccion->param1, nueva_instruccion->param2);
                        if (truncar_fcb(nueva_instruccion->param1, atoi(nueva_instruccion->param2)) != -1) {
                            estado_file = F_TRUNCATE_SUCCESS;
                        }
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case F_WRITE:
                        log_info(filesystem_logger_info, "PID: %d - F_WRITE: %s - Puntero: %d - Memoria: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
                        log_info(filesystem_logger_info, "Escribir Archivo: %s - Puntero: %d - Memoria: %s", nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
                        realizar_f_write(nueva_instruccion);
                        estado_file = F_WRITE_SUCCESS;
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case F_READ:
                        log_info(filesystem_logger_info, "PID: %d - F_READ: %s - Puntero: %d - Memoria: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
                        log_info(filesystem_logger_info, "Escribir Archivo: %s - Puntero: %d - Memoria: %s", nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
                        realizar_f_read(nueva_instruccion);
                        estado_file = F_READ_SUCCESS;
                        serializar_respuesta_file_kernel(socket_kernel, estado_file);
                        break;
                    case PRINT_FILE_DATA:
                        log_info(filesystem_logger_info, "PID: %d Solicito impresion de datos", pid);
                        break;
                    default:
                        break;
                }
                destroy_instruccion_file(nueva_instruccion);
                break;
            }
            default:
                exit_status = 1;
                break;
        }
        eliminar_paquete(paquete);
    }
}

// Swap y Memoria
void inicializar_swap(int cantidad_bloques) {
    swap = (bloque *)malloc(cantidad_bloques * sizeof(bloque));

    for (int i = 0; i < cantidad_bloques; i++) {
        swap[i].utilizado = false;
    }
}

void iniciar_swap(uint32_t cantidad_bloques) {
    int bloques_reservados = reservar_bloques_swap(cantidad_bloques);
    enviar_op_con_int(socket_memoria, LISTA_BLOQUES_SWAP, bloques_reservados);
}

int reservar_bloques_swap(int cantidad_bloques, int *bloques_reservados) {
    int bloques_reservados_count = 0;
    for (int i = 0; i < cantidad_bloques; i++) {
        if (!swap[i].utilizado) {
            swap[i].utilizado = true;
            bloques_reservados[bloques_reservados_count++] = i;
        } else {
            log_error(filesystem_logger_info, "Bloque %d ya está en uso en la swap", i);
        }
    }
    return bloques_reservados_count;
}

void liberar_bloques_swap(int *bloques_a_liberar, int cantidad_bloques) {
    for (int i = 0; i < cantidad_bloques; i++) {
        int bloque = bloques_a_liberar[i];
        if (bloque >= 0 && bloque < cantidad_bloques) {
            if (swap[bloque].utilizado) {
                swap[bloque].utilizado = false;
            } else {
                log_error(filesystem_logger_info, "Intento de liberar bloque %d que no está en uso en la swap", bloque);
            }
        } else {
            log_error(filesystem_logger_info, "ID de bloque %d fuera de rango en la swap", bloque);
        }
    }
}

uint32_t leer_bloque_swap(int id_bloque) {
    if (id_bloque >= 0 && id_bloque < bloques_swap) {
        return swap[id_bloque].tamanio;
    } else {
        log_error(filesystem_logger_info, "ID de bloque %d fuera de rango en la swap", id_bloque);
        return 0;
    }
}

// Esta funcion habria que ver si pide escribir un bloque o escribir un valor !!
void escribir_bloque_swap(int id_bloque, uint32_t valor) {
    if (id_bloque >= 0 && id_bloque < bloques_swap) {
        swap[id_bloque].tamanio = valor;
    } else {
        log_error(filesystem_logger_info, "ID de bloque %d fuera de rango en la swap", id_bloque);
    }
}

void enviar_escritura_bloque_ok(int socket, int resultado) {
    enviar_op_con_int(socket, ESCRITURA_BLOQUE_OK, resultado);
}

void obtener_estado_swap(int cantidad_bloques) {
    for (int i = 0; i < cantidad_bloques; i++) {
        log_error(filesystem_logger_info, "Bloque %d: %s\n", i, swap[i].utilizado ? "Ocupado" : "Libre");
    }
}

void liberar_swap() {
    free(swap);
}