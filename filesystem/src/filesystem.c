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
    path_fcb = config_valores_filesystem.path_fcb;
    path_bloques = config_valores_filesystem.path_bloques;
    bloques_swap = config_valores_filesystem.cant_bloques_swap;
    fat = malloc(tamanio_fat);
    swap = malloc(bloques_swap);
    bloques = inicializar_bloque_de_datos(path_bloques, cant_bloques);
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
    // inicializarFATDesdeDirectorio(config_valores_filesystem.path_fcb, filesystem_logger_info);
    int exit_status = crear_fat(config_valores_filesystem.path_fat);
    if (exit_status == -1)
    {
        return -1;
    }
    pthread_t hilo_cliente;
    pthread_create(&hilo_cliente, NULL, comunicacion_kernel, NULL);
    pthread_detach(hilo_cliente);
    while (1);
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
    free(fat);
    free(swap);
    free(bloques);
}

bloque_t crear_bloque_vacio(int tamanio_bloque)
{
    bloque_t bloque_fs;
    bloque_fs.datos = malloc(tamanio_bloque);
    if (bloque_fs.datos != NULL)
    {
        memset(bloque_fs.datos, 0, tamanio_bloque);
    }

    return bloque_fs;
}

bloque_t *inicializar_bloque_de_datos(const char *path, int cant_bloques_total)
{
    FILE *archivo = fopen(path, "rb");
    bloque_t *bloques;

    if (archivo == NULL)
    {
        bloques = malloc(cant_bloques_total * sizeof(bloque_t));
        for (int i = 0; i < cant_bloques_total; ++i)
        {
            bloques[i] = crear_bloque_vacio(tamanio_bloque);
        }
    }
    else
    {
        bloques = malloc(cant_bloques_total * sizeof(bloque_t));
        fread(bloques, sizeof(bloque_t), cant_bloques_total, archivo);
        fclose(archivo);
    }

    return bloques;
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

uint32_t obtener_primer_bloque_libre()
{
    for (uint32_t i = 0; i < tamanio_fat; i++)
    {
        if (fat[i] == -1)
        {
            return i;
        }
    }
    return -1;
}

bool bloque_esta_ocupado(uint32_t *fat, int id_bloque)
{
    if (id_bloque < 0 || id_bloque >= tamanio_fat)
    {
        return false;
    }

    return fat[id_bloque] != -1;
}

int crear_fat(char *path_fat)
{
    int file_descriptor = open(path_fat, O_CREAT | O_RDWR, 0644);
    if (file_descriptor < 0)
    {
        log_error(filesystem_logger_info, "Error al crear el archivo FAT");
        return -1;
    }
    if (ftruncate(file_descriptor, tamanio_fat) != 0)
    {
        log_error(filesystem_logger_info, "Error al asignar espacio en disco para el archivo FAT");
        close(file_descriptor);
        return -1;
    }
    fat = mmap(NULL, tamanio_fat, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (fat == MAP_FAILED)
    {
        log_error(filesystem_logger_info, "Error al mapear el archivo FAT en memoria");
        close(file_descriptor);
        return -1;
    }
    if (formatear == 1)
    {
        for (int i = 0; i < tamanio_fat; i++)
        {
            fat[i] = -1;
        }
        log_info(filesystem_logger_info, "Tabla FAT formateada exitosamente");
    }
    fat[0] = 0;
    log_info(filesystem_logger_info, "Tabla FAT creada");
    return 0;
}

int inicializar_swap()
{
    swap = mmap(NULL, bloques_swap, PROT_READ | PROT_WRITE, MAP_SHARED, -1, 0);
    if (swap == MAP_FAILED)
    {
        log_error(filesystem_logger_info, "Error al mapear la partición SWAP en memoria");
        return -1;
    }

    log_info(filesystem_logger_info, "Partición SWAP inicializada");

    return 0;
}
/*
void guardarFAT(const char *nombreArchivo, int tamanio_fat)
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
*/
void liberarMemoriaFAT()
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
        bloque_t *bloque_existente = malloc(sizeof(bloque_t));
        if (bloque_existente != NULL)
        {
            list_add(fat, bloque_existente);
        }
        free(nombre_completo);
    }
    log_info(logger, "FAT inicializada correctamente desde el directorio");
    closedir(dir_fat);
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
        borrar_fcb(nuevo_fcb->id);
        free(nombre_completo);
        return resultado;
    }

    fcb_fisico->path = string_duplicate(nombre_completo);
    fcb_fisico->properties = dictionary_create();
    if (fcb_fisico->path == NULL || fcb_fisico->properties == NULL)
    {
        log_error(filesystem_logger_info, "Error al asignar memoria para el FCB físico");
        borrar_fcb(nuevo_fcb->id);
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
        borrar_fcb(nuevo_fcb->id);
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

uint32_t conseguir_id_fcb(char *nombre_fcb)
{
    uint32_t resultado = -1;
    int size = list_size(lista_fcb);
    for (int i = 0; i < size; i++)
    {
        fcb_t *fcb = list_get(lista_fcb, i);
        if (strcmp(fcb->nombre_archivo, nombre_fcb) == 0)
        {
            resultado = buscar_fcb_id(fcb->id);
            break;
        }
    }
    if (resultado == -1)
    {
        log_info(filesystem_logger_info, "No se encontró un FCB con el nombre: %s", nombre_fcb);
    }
    return resultado;
}

uint32_t buscar_fcb(char *nombre_fcb)
{
    uint32_t resultado = -1;
    uint32_t id_fcb = conseguir_id_fcb(nombre_fcb);
    uint32_t bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);

    if (bloque_inicial != -1)
    {
        while (bloque_inicial != -1)
        {
            fcb_t *fcb = &(fat[bloque_inicial]);

            if (strcmp(fcb->nombre_archivo, nombre_fcb) == 0)
            {
                resultado = fcb->id;
                break;
            }
            bloque_inicial = fat[bloque_inicial];
        }
    }
    if (resultado == -1)
    {
        log_info(filesystem_logger_info, "No se encontró un FCB con el nombre: %s", nombre_fcb);
    }
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

void asignar_bloques(int id_fcb, int nuevo_tamanio)
{
    int cant_bloques_a_asignar = ceil((double)nuevo_tamanio / tamanio_bloque);
    int cant_bloques_actual = obtener_cantidad_de_bloques(id_fcb);
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);

    if (bloque_inicial == -1)
    {
        bloque_inicial = obtener_primer_bloque_libre();
        if (bloque_inicial == -1)
        {
            log_warning(filesystem_logger_info, "No hay bloques libres disponibles");
            return;
        }
        modificar_fcb(id_fcb, BLOQUE_INICIAL, bloque_inicial);
    }
    int bloque_actual = bloque_inicial;
    for (int i = cant_bloques_actual; i < cant_bloques_a_asignar; i++)
    {
        if (bloque_actual == -1)
        {
            log_warning(filesystem_logger_info, "No hay bloques libres disponibles");
            break;
        }

        fat[bloque_actual] = obtener_primer_bloque_libre();
        bloque_actual = fat[bloque_actual];
    }
}

void desasignar_bloques(int id_fcb, int nuevo_tamanio)
{
    uint32_t tamanio_bloque = config_valores_filesystem.tam_bloque;
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);
    if (bloque_inicial == -1)
    {
        return;
    }
    uint32_t tamanio_actual = 0;
    while (bloque_inicial != -1 && tamanio_actual < nuevo_tamanio)
    {
        int siguiente_bloque = fat[bloque_inicial];
        fat[bloque_inicial] = -1;
        modificar_fcb(id_fcb, BLOQUE_INICIAL, siguiente_bloque);
        bloque_inicial = siguiente_bloque;
        tamanio_actual += tamanio_bloque;
    }
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
            asignar_bloques(id_fcb, nuevo_tamanio);
        }
        else if (nuevo_tamanio < tamanio_archivo)
        {
            desasignar_bloques(id_fcb, nuevo_tamanio);
        }
        modificar_fcb(id_fcb, TAMANIO_ARCHIVO, nuevo_tamanio);
        resultado = 0;
    }
    return resultado;
}

void escribir_dato(void *dato, uint32_t offset, uint32_t size)
{
    uint32_t bloque_actual = offset / tamanio_bloque;
    uint32_t offset_bloque = offset % tamanio_bloque;
    uint32_t bytes_restantes = size;
    uint32_t bytes_escritos = 0;

    while (bytes_restantes > 0)
    {
        uint32_t bytes_a_escribir = (bytes_restantes < (tamanio_bloque - offset_bloque)) ? bytes_restantes : (tamanio_bloque - offset_bloque);

        memcpy(memoria_file_system + (bloque_actual * tamanio_bloque) + offset_bloque, dato + bytes_escritos, bytes_a_escribir);

        msync(memoria_file_system, tam_memoria_file_system, MS_SYNC);
        sleep(retardo_acceso_bloque);

        bytes_restantes -= bytes_a_escribir;
        bytes_escritos += bytes_a_escribir;

        offset_bloque = 0;
        bloque_actual = fat[bloque_actual];
    }
}

void escribir_datos(void *datos, int cant_bloques, fcb_t id_fcb)
{
    uint32_t bloque_actual = id_fcb.bloque_inicial;
    uint32_t bloque_siguiente;

    for (int i = 0; i < cant_bloques; i++)
    {
        offset_fcb_t *bloque_info = &(bloques[i].datos);
        uint32_t bloque_id = bloque_info->id_bloque;
        log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);

        if (fat[bloque_id] == -1)
        {
            asignar_bloques(id_fcb.id, bloque_info->tamanio);
            bloque_siguiente = UINT32_MAX;
        }
        else
        {
            bloque_siguiente = fat[bloque_id];
        }

        escribir_dato(datos + bloque_info->offset, bloque_info->offset, bloque_info->tamanio);
        fat[bloque_id] = bloque_siguiente;

        if (bloque_siguiente == UINT32_MAX)
        {
            break;
        }

        bloque_actual = bloque_siguiente;
    }
}

void *leer_bloque(uint32_t id_bloque)
{
    if (id_bloque < 0 || id_bloque >= config_valores_filesystem.cant_bloques_total)
    {
        return NULL;
    }

    void *bloque = malloc(config_valores_filesystem.tam_bloque);
    if (bloque == NULL)
    {
        return NULL;
    }

    memcpy(bloque, bloques[id_bloque].datos, config_valores_filesystem.tam_bloque);

    sleep(retardo_acceso_bloque);
    return bloque;
}

bloque_t* obtener_lista_de_bloques(int id_fcb, int p_seek, int tam_a_leer, t_list* lista_fcb, t_log* logger)
{
    int max_bloques = ceil((double)tam_a_leer / tamanio_bloque);
    bloque_t* lista_bloques = malloc(max_bloques * sizeof(bloque_t));
    if (lista_bloques == NULL)
    {
        log_error(logger, "Error de asignación de memoria para lista_bloques");
        return NULL;
    }
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);
    int bloque_actual = bloque_inicial;
    int tam_leido = 0;
    int idx_bloque = 0;
    while (tam_leido < tam_a_leer)
    {
        int offset_bloque = bloque_actual * tamanio_bloque;
        int tam_restante = tam_a_leer - tam_leido;
        int bytes_leer = (tam_restante < tamanio_bloque) ? tam_restante : tamanio_bloque;
        lista_bloques[idx_bloque].datos = leer_bloque(bloque_actual);
        tam_leido += bytes_leer;
        idx_bloque++;
        bloque_actual = fat[bloque_actual];
        if (bloque_actual < 0 || bloque_actual >= tamanio_fat)
        {
            log_error(logger, "Error: El bloque apuntado está fuera de rango");
            break;
        }
        int bloque_siguiente = ceil((double)(p_seek + 1 + tam_leido) / tamanio_bloque);
        if (bloque_actual != bloque_siguiente)
        {
            log_error(logger, "Error en la secuencia de bloques");
            break;
        }
    }
    if (tam_leido != tam_a_leer)
    {
        for (int i = 0; i < idx_bloque; i++)
        {
            free(lista_bloques[i].datos);
        }
        free(lista_bloques);
        log_error(logger, "Error tamanio leido es distinto al tamanio a leer");
        return NULL;
    }

    return lista_bloques;
}

void realizar_f_write(t_instruccion_fs *instruccion_file)
{
    uint32_t pid = instruccion_file->pid;
    uint32_t direccion_fisica = instruccion_file->param1_length;
    int tamanio = instruccion_file->param2_length;
    int cantidad_bloques = (int)ceil((double)tamanio / tamanio_bloque);
    int puntero_archivo = instruccion_file->puntero;
    t_paquete *paquete = crear_paquete_con_codigo_de_operacion(F_WRITE);
    agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
    agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
    agregar_a_paquete(paquete, &tamanio, sizeof(uint32_t));
    enviar_paquete(paquete, socket_memoria);
    eliminar_paquete(paquete);
    op_code codigo = recibir_operacion(socket_memoria);
    if (codigo == F_WRITE)
    {
        t_list *paquete_recibido = recibir_paquete(socket_memoria);
        if (paquete_recibido != NULL && list_size(paquete_recibido) > 0)
        {
            char *valor_recibido = instruccion_file->param1;
            uint32_t id_fcb = buscar_fcb(instruccion_file->param1);
            bloque_t *lista_bloques = obtener_lista_de_bloques(id_fcb, direccion_fisica, tamanio, lista_fcb, filesystem_logger_info);
            if (lista_bloques != NULL)
            {
                for (int i = 0; i < cantidad_bloques; i++)
                {
                    offset_fcb_t *bloque_info = &(lista_bloques[i].datos);
                    int bloque_id = bloque_info->id_bloque;
                    log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);
                    if (fat[bloque_id] == -1)
                    {
                        asignar_bloques(id_fcb, bloque_info->tamanio);
                        fat[bloque_id] = id_fcb;
                    }
                    escribir_dato(valor_recibido + bloque_info->offset, bloque_info->offset, bloque_info->tamanio);
                }
                free(lista_bloques);
            }
            list_destroy_and_destroy_elements(paquete_recibido, free);
        }
    }
}

void realizar_f_read(t_instruccion_fs *instruccion_file)
{
    int direccion_fisica = atoi(instruccion_file->param2);
    int puntero_archivo = instruccion_file->puntero;
    int id_fcb = buscar_fcb(instruccion_file->param1);
    log_info(filesystem_logger_info, "Entro a realizar f read");
    uint32_t id_bloque_inicial = (uint32_t)ceil((double)direccion_fisica / tamanio_bloque);
    int offset_inicial = direccion_fisica % tamanio_bloque;
    int tamanio_restante = atoi(instruccion_file->param1);
    for (uint32_t id_bloque = id_bloque_inicial; tamanio_restante > 0; fat[id_bloque])
    {
        void *datos = leer_bloque(id_bloque);
        int bytes_leer = (tamanio_restante < tamanio_bloque) ? tamanio_restante : tamanio_bloque;

        t_paquete *paquete = crear_paquete_con_codigo_de_operacion(F_READ_FS);
        agregar_a_paquete(paquete, &(instruccion_file->pid), sizeof(uint32_t));
        agregar_a_paquete(paquete, &direccion_fisica, sizeof(uint32_t));
        agregar_a_paquete(paquete, &bytes_leer, sizeof(uint32_t));
        agregar_a_paquete(paquete, datos + offset_inicial, bytes_leer); 
        enviar_paquete(paquete, socket_memoria);
        eliminar_paquete(paquete);
        op_code respuesta = recibir_operacion(socket_memoria);
        if (respuesta == MENSAJE)
        {
            char *mensaje = recibir_mensaje_sin_log(socket_memoria);
            free(mensaje);
        }
        tamanio_restante -= bytes_leer;
        offset_inicial = 0;
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
    free(instruccion);
    log_info(filesystem_logger_info, "Instruccion fs destruida exitosamente");
}

void crear_paquete_con_respuesta(t_resp_file estado_file)
{
    t_paquete *paq_respuesta = crear_paquete_con_codigo_de_operacion(estado_file);
    enviar_paquete(paq_respuesta, socket_kernel);
    eliminar_paquete(paq_respuesta);
}

t_instruccion_fs *deserializar_instruccion_file(int socket_cliente)
{
    int size;
    void *buffer;

    buffer = recibir_buffer(&size, socket_cliente);
    printf("Size del stream a deserializar: %d \n", size);

    t_instruccion_fs *instruccion = malloc(sizeof(t_instruccion_fs));

    int offset = 0;

    memcpy(&(instruccion->estado), buffer + offset, sizeof(nombre_instruccion));
    offset += sizeof(nombre_instruccion);

    memcpy(&(instruccion->pid), buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    memcpy(&(instruccion->param1_length), buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    instruccion->param1 = malloc(instruccion->param1_length);
    memcpy(instruccion->param1, buffer + offset, instruccion->param1_length);
    offset += instruccion->param1_length;

    memcpy(&(instruccion->param2_length), buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    instruccion->param2 = malloc(instruccion->param2_length);
    memcpy(instruccion->param2, buffer + offset, instruccion->param2_length);
    offset += instruccion->param2_length;

    memcpy(&(instruccion->puntero), buffer + offset, sizeof(uint32_t));

    return instruccion;
}

void* comunicacion_kernel()
{
    int exit_status = 0;
    while (exit_status == 0)
    {
        nombre_instruccion codigo_op = recibir_operacion(socket_kernel);

        switch (codigo_op)
        {
        case OP_FILESYSTEM:
        {
            t_instruccion_fs *nueva_instruccion = deserializar_instruccion_fs(socket_kernel);
            uint32_t pid = nueva_instruccion->pid;
            switch (nueva_instruccion->estado)
            {
            case F_OPEN:
                pthread_t f_open_t;
                pthread_create(&f_open_t, NULL, (void *)hilo_f_open, nueva_instruccion);
                pthread_detach(f_open_t);
                break;
            case F_CREATE:
                pthread_t f_create_t;
                pthread_create(&f_create_t, NULL, (void *)hilo_f_create, nueva_instruccion);
                pthread_detach(f_create_t);
                break;
            case F_CLOSE:
                pthread_t f_close_t;
                pthread_create(&f_close_t, NULL, (void *)hilo_f_close, nueva_instruccion);
                pthread_detach(f_close_t);
                break;
            case F_TRUNCATE:
                pthread_t f_truncate_t;
                pthread_create(&f_truncate_t, NULL, (void *)hilo_f_truncate, nueva_instruccion);
                pthread_detach(f_truncate_t);
                break;
            case F_WRITE:
                pthread_t f_write_t;
                pthread_create(&f_write_t, NULL, (void *)hilo_f_write, nueva_instruccion);
                pthread_detach(f_write_t);
                break;
            case F_READ:
                pthread_t f_read_t;
                pthread_create(&f_read_t, NULL, (void *)hilo_f_read, nueva_instruccion);
                pthread_detach(f_read_t);
                break;
            case PRINT_FILE_DATA:
                log_info(filesystem_logger_info, "PID: %d Solicito impresion de datos", pid);
                break;
            default:
                break;
            }
            break;
        }
        default:
            exit_status = 1;
            break;
        }
    }

    return NULL;
}

void *hilo_f_open(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    if (buscar_fcb(nueva_instruccion->param1) != -1)
    {
        log_info(filesystem_logger_info, "PID: %d - F_OPEN: %s", nueva_instruccion->pid, nueva_instruccion->param1);
        log_info(filesystem_logger_info, "Abrir Archivo: %s", nueva_instruccion->param1);
        estado_file = F_OPEN_SUCCESS;
    }
    else
    {
        log_info(filesystem_logger_info, "PID: %d - F_OPEN: %s - NO EXISTE", nueva_instruccion->pid, nueva_instruccion->param1);
        estado_file = FILE_DOESNT_EXISTS;
    }
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

void *hilo_f_create(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    if (crear_fcb(nueva_instruccion->param1) != -1)
    {
        log_info(filesystem_logger_info, "PID: %d - F_CREATE: %s", nueva_instruccion->pid, nueva_instruccion->param1);
        log_info(filesystem_logger_info, "Crear Archivo: %s", nueva_instruccion->param1);
        estado_file = F_CREATE_SUCCESS;
    }
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

void *hilo_f_close(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    if (buscar_fcb(nueva_instruccion->param1) != -1)
    {
        estado_file = F_CLOSE_SUCCESS;
        log_info(filesystem_logger_info, "PID: %d - F_CLOSE: %s", nueva_instruccion->pid, nueva_instruccion->param1);
    }
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

void *hilo_f_truncate(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    log_info(filesystem_logger_info, "PID: %d - F_TRUNCATE: %s - Tamanio: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->param2);
    log_info(filesystem_logger_info, "Truncar Archivo: %s - Tamaño: %s", nueva_instruccion->param1, nueva_instruccion->param2);
    if (truncar_fcb(nueva_instruccion->param1, atoi(nueva_instruccion->param2)) != -1)
    {
        estado_file = F_TRUNCATE_SUCCESS;
    }
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

void *hilo_f_write(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    log_info(filesystem_logger_info, "PID: %d - F_WRITE: %s - Puntero: %d - Memoria: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
    log_info(filesystem_logger_info, "Escribir Archivo: %s - Puntero: %d - Memoria: %s", nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
    realizar_f_write(nueva_instruccion);
    estado_file = F_WRITE_SUCCESS;
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

void *hilo_f_read(void *instruccion)
{
    t_resp_file estado_file = F_ERROR;
    t_instruccion_fs *nueva_instruccion = (t_instruccion_fs *)instruccion;
    log_info(filesystem_logger_info, "PID: %d - F_READ: %s - Puntero: %d - Memoria: %s", nueva_instruccion->pid, nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
    log_info(filesystem_logger_info, "Escribir Archivo: %s - Puntero: %d - Memoria: %s", nueva_instruccion->param1, nueva_instruccion->puntero, nueva_instruccion->param2);
    realizar_f_read(nueva_instruccion);
    estado_file = F_READ_SUCCESS;
    enviar_op_con_int(socket_kernel, estado_file, nueva_instruccion->pid);
    destroy_instruccion_file(nueva_instruccion);
    return NULL;
}

/*
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

int reservar_bloques_swap(int cantidad_bloques) {
    int bloques_reservados_count = 0;
    for (int i = 0; i < cantidad_bloques; i++) {
        if(cantidad_bloques > bloques_swap){
            log_error(filesystem_logger_info, "La cantidad de bloques seleccionados exede al numero de bloques en swap");
            bloques_reservados_count = -1;
            return bloques_reservados_count;
        }
        if (!swap[i].utilizado) {
            swap[i].utilizado = true;
            bloques_reservados_count+= i;
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

void enviar_escritura_bloque_ok(int resultado) {
    enviar_op_con_int(socket_memoria, ESCRITURA_BLOQUE_OK, resultado);
}

void obtener_estado_swap(int cantidad_bloques) {
    for (int i = 0; i < cantidad_bloques; i++) {
        log_error(filesystem_logger_info, "Bloque %d: %s", i, swap[i].utilizado ? "Ocupado" : "Libre");
    }
}

void liberar_swap() {
    free(swap);
}

void enviar_lista_bloques_swap(uint32_t cantidad_bloques) {
    int bloques_reservados[cantidad_bloques];
    int bloques_reservados_count = reservar_bloques_swap(cantidad_bloques, bloques_reservados);

    if (bloques_reservados_count > 0 || bloques_reservados_count > bloques_swap) {
        enviar_op_con_int_array(socket_memoria, LISTA_BLOQUES_SWAP, bloques_reservados, bloques_reservados_count);
    } else {
        log_error(filesystem_logger_info, "No hay bloques disponibles en la swap");
        enviar_op_con_int(socket_memoria, LISTA_BLOQUES_SWAP, 0); // Envía un valor de 0 para indicar que no hay bloques disponibles
    }
}

void enviar_lista_bloques_swap(t_list *lista_bloques) {
    int cantidad_bloques = list_size(lista_bloques);
    if (send(socket_memoria, &cantidad_bloques, sizeof(int), 0) == -1) {
        log_error(filesystem_logger_info, "Error al enviar la cantidad de bloques al socket");
        return;
    }
    for (int i = 0; i < cantidad_bloques; i++) {
        int *bloque_swap = list_get(lista_bloques, i);

        if (send(socket_memoria, bloque_swap, sizeof(int), 0) == -1) {
            log_error(filesystem_logger_info, "Error al enviar el bloque %d al socket", i);
            return;
        }
    }
}

void comunicacion_memoria() {
    int exit_status = 0;
    inicializar_swap(bloques_swap);
    while (exit_status == 0) {
        t_paquete *paquete = crear_paquete_con_codigo_de_operacion(HANDSHAKE_MEMORIA);
        nombre_instruccion codigo_op = recibir_operacion(socket_kernel);

        switch (codigo_op) {
            case INICIO_SWAP:
                iniciar_swap(cantidad_bloques);
                break;
            case LISTA_BLOQUES_SWAP:
                enviar_lista_bloques_swap(cantidad_bloques);
                break;
            case SWAP_A_LIBERAR:

                break;
            case LEER_BLOQUE:
                break;
            case VALOR_BLOQUE:
                break;
            case ESCRIBIR_BLOQUE:
                break;
            case ESCRITURA_BLOQUE_OK:
                break;
        }

        */