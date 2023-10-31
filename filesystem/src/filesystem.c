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
    cant_bloques = config_valores_filesystem.cant_bloques_total;
    tamanio_bloque = config_valores_filesystem.tam_bloque;
    tam_memoria_file_system = config_valores_filesystem.cant_bloques_total * config_valores_filesystem.tam_bloque;
    tamanio_fat = (config_valores_filesystem.cant_bloques_total - config_valores_filesystem.cant_bloques_swap) * sizeof(bloque);
    t_list *lista_fat = list_create();

    socket_memoria = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    realizar_handshake(socket_memoria, HANDSHAKE_FILESYSTEM, filesystem_logger_info);

    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(socket_memoria, filesystem_logger_info);
        log_info(filesystem_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(filesystem_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(filesystem_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }

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

    // FIN CONEXION KERNEL -> VER SI HAY QUE MANEJARLO CON HILOS

    
    // Mapeo de memoria
    int fd;
    fd = open(config_valores_filesystem.path_bloques, O_RDWR);
    ftruncate(fd, tam_memoria_file_system);
    void *memoria_file_system = mmap(NULL, tam_memoria_file_system, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (formatear == 1)
        inicializar_datos_memoria(tam_memoria_file_system, memoria_file_system);
    // inicializar_fcb_list(config_valores_filesystem.path_fcb, fcb_id, filesystem_logger_info);
    inicializarFATDesdeDirectorio(config_valores_filesystem.path_fcb, filesystem_logger_info);

    // int exit_status = crear_fat(tamanio_fat, config_valores_filesystem.path_fat, filesystem_logger_info);
    int exit_status = crearFAT(tamanio_fat, config_valores_filesystem.path_fat, filesystem_logger_info);
    if (exit_status == -1)
    {
        return -1;
    }

    pthread_t thread_kernel;
    // pthread_create(&thread_kernel, NULL, (void *)comunicacion_kernel, (void *)cliente);
    // pthread_join(thread_kernel, NULL);

    // eliminar_paquete(respuesta);

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

off_t obtener_primer_bloque_libre(t_list *lista_bloques)
{
    int size = list_size(lista_bloques);
    for (off_t i = 0; i < size; i++)
    {
        bloque *bloque = list_get(lista_bloques, i);
        if (!bloque->utilizado)
        {
            return i;
        }
    }
    return -1;
}

bool bloque_esta_ocupado(t_list *lista_bloques, off_t id_bloque)
{
    if (id_bloque < 0 || id_bloque >= list_size(lista_bloques))
    {
        return false;
    }
    bloque *bloque = list_get(lista_bloques, id_bloque);
    return bloque->utilizado;
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

bloque leerBloque(FILE *file, int block_number)
{
    bloque block;

    long position = (long)block_number * sizeof(bloque);

    fseek(file, position, SEEK_SET);

    fread(&block, sizeof(bloque), 1, file);

    return block;
}

void escribirBloque(FILE *file, bloque *block, int block_number)
{
    long position = (long)block_number * sizeof(bloque);

    fseek(file, position, SEEK_SET);

    fwrite(block, sizeof(bloque), 1, file);
}

void guardarFAT(const char *nombreArchivo)
{
    FILE *archivo = fopen(nombreArchivo, "wb"); // Abre el archivo en modo binario para escritura

    if (archivo)
    {
        int cantidad = list_size(lista_fat);
        fwrite(&cantidad, sizeof(int), 1, archivo);

        for (int i = 0; i < cantidad; i++)
        {
            bloque *elemento = list_get(lista_fat, i);
            fwrite(elemento, sizeof(bloque), 1, archivo);
        }

        fclose(archivo);
    }
    else
    {
        printf("No se pudo abrir el archivo para escritura\n");
    }
}

t_list *cargarFAT(const char *nombreArchivo)
{
    FILE *archivo = fopen(nombreArchivo, "rb"); // Abre el archivo en modo binario para lectura

    if (archivo)
    {
        int cantidad;
        fread(&cantidad, sizeof(int), 1, archivo);

        for (int i = 0; i < cantidad; i++)
        {
            bloque *elemento = malloc(sizeof(bloque));
            fread(elemento, sizeof(bloque), 1, archivo);
            list_add(lista_fat, elemento);
        }

        fclose(archivo);
        return lista_fat;
    }
    else
    {
        printf("No se pudo abrir el archivo para lectura\n");
        return NULL;
    }
}

void liberarMemoriaFAT()
{
    int cantidad = list_size(lista_fat);
    for (int i = 0; i < cantidad; i++)
    {
        bloque *elemento = list_get(lista_fat, i);
        free(elemento);
    }
    list_destroy(lista_fat);
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

        fcb_t *fcb_existente = inicializar_fcb();
        if (fcb_existente != NULL)
        {
            list_add(lista_fat, fcb_existente);
            fcb_id++;
        }

        free(nombre_completo);
    }
    log_info(logger, "Lista de FCB creada correctamente");
    closedir(dir_fat);
}

int crearFCB(char *nombre_fcb, t_log *logger, t_list *lista_fat)
{
    if (buscar_fcb(nombre_fcb, lista_fat) != -1)
    {
        log_error(logger, "Ya existe un FCB con ese nombre");
        return -1;
    }

    char *nombre_completo = string_new();
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
    list_add(lista_fat, nuevo_fcb);

    dictionary_destroy(fcb_fisico->properties);
    free(nombre_duplicado);
    free(fcb_fisico->path);
    free(fcb_fisico);
    free(nombre_completo);

    return nuevo_fcb->id;
}

int crearFAT(int cantidad_de_bloques, char *path_fat, t_log *logger)
{
    tamanio_fat = cantidad_de_bloques;
    FILE *archivo = fopen(path_fat, "wb"); // Abre el archivo en modo binario para escritura

    if (archivo)
    {
        for (int i = 0; i < cantidad_de_bloques; i++)
        {
            bloque fat_entry;
            fat_entry.next_block = 0;           // Inicializa como 0 (puede variar según tu lógica)
            fat_entry.tamanio = tamanio_bloque; // Inicializa como 0 (puede variar según tu lógica)
            fat_entry.id = i;
            fwrite(&fat_entry, sizeof(bloque), 1, archivo);
        }

        fclose(archivo);
        log_info(logger, "FAT table creada correctamente");
        return 0;
    }
    else
    {
        log_error(logger, "No se pudo abrir el archivo para escritura");
        return -1;
    }
}

uint32_t buscar_fcb(char *nombre_fcb, t_list *lista_fat)
{
    int resultado = -1;
    int size = list_size(lista_fat);

    for (int i = 0; i < size; i++)
    {
        fcb_t *fcb = list_get(lista_fat, i);

        if (strcmp(fcb->nombre_archivo, nombre_fcb) == 0)
        {
            resultado = fcb->id;
            break;
        }
    }

    return resultado;
}

uint32_t buscar_fcb_id(int id, t_list *lista_fcb)
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

fcb_t *_get_fcb_id(int id, t_list *lista_fcb)
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

fcb_t *_get_fcb(char *nombre, t_list *lista_fcb)
{
    int id = buscar_fcb(nombre, lista_fcb);
    if (id != -1)
    {
        return _get_fcb_id(id, lista_fcb);
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
    default:
        break;
    }
    return valor;
}

uint32_t valor_fcb(int id, fcb_prop_t llave, t_list *lista_fcb)
{
    fcb_t *fcb = _get_fcb_id(id, lista_fcb);
    if (fcb != NULL)
    {
        return valor_para_fcb(fcb, llave);
    }
    return 0; // Valor predeterminado si el FCB no se encuentra
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

int modificar_fcb(int id, fcb_prop_t llave, uint32_t valor, t_list *lista_fcb)
{
    fcb_t *fcb = _get_fcb_id(id, lista_fcb);
    if (fcb != NULL)
    {
        return _modificar_fcb(fcb, llave, valor);
    }
    return -1; // Devuelve -1 si el FCB no se encuentra
}

void escribir_int(uint32_t dato, int offset)
{
    memcpy(memoria_file_system + offset, &dato, 4);
    msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
    sleep(config_valores_filesystem.retardo_acceso_bloque);
}

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

int obtener_cantidad_de_bloques(int id_fcb, FILE *file)
{
    int tamanio_fcb = valor_fcb(id_fcb, TAMANIO_ARCHIVO, file);
    int cant_bloques_fcb = ceil((double)tamanio_fcb / config_valores_filesystem.tam_bloque);
    return cant_bloques_fcb;
}

t_list *obtener_lista_total_de_bloques(int id_fcb, FILE *file)
{
    t_list *lista_de_bloques = list_create();
    int tamanio_archivo = valor_fcb(id_fcb, TAMANIO_ARCHIVO, file);

    if (tamanio_archivo == 0)
        return lista_de_bloques;

    int cant_bloques_fcb = obtener_cantidad_de_bloques(id_fcb, tamanio_archivo);
    int offset_fcb = 0;

    for (int i = 0; i < cant_bloques_fcb; i++)
    {
        offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
        bloque->id_bloque = valor_fcb(id_fcb, BLOQUE_INICIAL, file) + i;
        bloque->offset = (long)bloque->id_bloque * sizeof(bloque);
        list_add(lista_de_bloques, bloque);
    }

    return lista_de_bloques;
}

void asignar_bloques(int id_fcb, int nuevo_tamanio, FILE *file)
{
    t_list *lista_total_de_bloques = obtener_lista_total_de_bloques(id_fcb, file);
    int size_inicial = list_size(lista_total_de_bloques);
    int tam_bloque = config_valores_filesystem.tam_bloque;
    int cant_bloques_a_asignar = ceil((double)nuevo_tamanio / tam_bloque);
    int cant_bloques_actual = obtener_cantidad_de_bloques(id_fcb, file);

    for (int i = cant_bloques_actual; i < cant_bloques_a_asignar; i++)
    {
        offset_fcb_t *bloque = malloc(sizeof(offset_fcb_t));
        bloque->id_bloque = obtener_primer_bloque_libre(lista_total_de_bloques);
        bloque->offset = (uint32_t)bloque->id_bloque * tam_bloque;
        bloque->tamanio = tam_bloque;

        size_inicial++;
        modificar_fcb(id_fcb, BLOQUE_INICIAL, bloque->id_bloque, file);
        list_add(lista_total_de_bloques, bloque);
    }

    int size_final = list_size(lista_total_de_bloques);

    // No necesitas destruir elementos ni modificar la tabla FAT

    modificar_fcb(id_fcb, TAMANIO_ARCHIVO, nuevo_tamanio, file);
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