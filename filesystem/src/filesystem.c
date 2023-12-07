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
    path_fat = config_valores_filesystem.path_fat;
    lista_fcb = list_create();
    pthread_mutex_init(&mutex_fat, NULL);
    inicializar_swap();
    formatear = 1;
    int exit_status_bloques = crear_archivo_bloques(path_bloques);
    if (exit_status_bloques == -1)
    {
        return -1;
    }

    // CONEXION MEMORIA SWAP
    socket_memoria_swap = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    realizar_handshake(socket_memoria_swap, HANDSHAKE_FILESYSTEM, filesystem_logger_info);

    pthread_t conexion_memoria_swap;
    pthread_create(&conexion_memoria_swap, NULL, manejo_conexion_memoria_swap, NULL);
    pthread_detach(conexion_memoria_swap);

    // CONEXION CON MEMORIA PARA F_READ Y F_WRITE

    socket_memoria_op = crear_conexion(config_valores_filesystem.ip_memoria, config_valores_filesystem.puerto_memoria);
    realizar_handshake(socket_memoria_op, HANDSHAKE_FILESYSTEM_OPS, filesystem_logger_info);

    op_code codigo_op = recibir_operacion(socket_memoria_op);
    log_info(filesystem_logger_info, "Se recibio una operacion de MEMORIA - OPS: %d", codigo_op);

    if (codigo_op == HANDSHAKE_MEMORIA)
    {
        codigo_op = recibir_handshake(socket_memoria_op, filesystem_logger_info);
        log_info(filesystem_logger_info, "Deserialice el codigo de operacion %d", socket_memoria_op);
        log_info(filesystem_logger_info, "HANDSHAKE EXITOSO CON MEMORIA PARA OPs");
    }
    else
    {
        log_warning(filesystem_logger_info, "No me pude conectar a MEMORIA para F_READ Y F_WRITE");
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
    inicializar_fcb_list(config_valores_filesystem.path_fcb);
    int exit_status = crear_fat(config_valores_filesystem.path_fat);
    if (exit_status == -1)
    {
        return -1;
    }
    pthread_t hilo_cliente;
    pthread_create(&hilo_cliente, NULL, comunicacion_kernel, NULL);
    pthread_detach(hilo_cliente);
    while (1)
        ;
    abort();
}

void *leer_bloque_swap(uint32_t numero_bloque)
{
    void *datos = NULL;
    FILE *archivo_bloques = fopen(path_bloques, "rb");
    numero_bloque += tamanio_fat - 1;
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo de Bloques para lectura");
        return -1;
    }

    fseek(archivo_bloques, numero_bloque * tamanio_bloque, SEEK_SET);
    fread(datos, tamanio_bloque, 1, archivo_bloques);
    fclose(archivo_bloques);
    sleep(retardo_acceso_bloque / 1000);

    return datos;
}

int escribir_bloque_swap(uint32_t numero_bloque, void *datos)
{
    FILE *archivo_bloques = fopen(path_bloques, "rb+");
    numero_bloque += tamanio_fat - 1;
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo de Bloques para escritura");
        return -1;
    }
    fseek(archivo_bloques, numero_bloque * tamanio_bloque, SEEK_SET);
    fwrite(datos, tamanio_bloque, 1, archivo_bloques);
    fflush(archivo_bloques);
    fclose(archivo_bloques);

    sleep(retardo_acceso_bloque / 1000);

    return 0;
}

uint32_t posicion_en_swap(uint32_t indice)
{
    FILE *archivo_bloques = fopen(path_bloques, "rb");
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo Bloques para lectura");
        return -1;
    }

    fseek(archivo_bloques, (tamanio_fat + indice - 1) * tamanio_bloque, SEEK_SET);

    uint32_t valor;
    fread(&valor, tamanio_bloque, 1, archivo_bloques);

    fclose(archivo_bloques);

    return (memcmp(&valor, "\0", tamanio_bloque) == 0) ? 0 : indice;
}

uint32_t obtener_primer_bloque_libre_swap()
{
    for (uint32_t i = tamanio_fat - 1; i < cant_bloques; i++)
    {
        if (posicion_en_swap(i) == 0)
        {
            return i; // Ajustar para devolver el número de bloque relativo a la partición de swap
        }
    }
    return -1;
}

void liberar_bloques_swap(t_list *lista_bloques_a_liberar)
{
    for (int i = 0; i < list_size(lista_bloques_a_liberar); i++)
    {
        uint32_t nro_bloque = (uint32_t)list_get(lista_bloques_a_liberar, i);

        if (nro_bloque < bloques_swap)
        {
            void *valor_libre = malloc(tamanio_bloque);
            memset(valor_libre, 0, tamanio_bloque);
            escribir_bloque_swap(nro_bloque, valor_libre);
            free(valor_libre);
        }
        else
        {
            log_info(filesystem_logger_info, "Número de bloque inválido en la lista: %d", nro_bloque);
        }
    }
}

t_list *lista_bloques_swap_reservados(int cantidad_bloques_deseada)
{
    t_list *lista = list_create();
    int bloques_asignados = 0;

    while (bloques_asignados < cantidad_bloques_deseada)
    {
        uint32_t bloque_libre = obtener_primer_bloque_libre_swap();

        if (bloque_libre != -1)
        {
            // Marcar el bloque como ocupado en el archivo de bloques
            void *valor_ocupado = malloc(tamanio_bloque);
            memset(valor_ocupado, 0, tamanio_bloque);
            escribir_bloque_swap(bloque_libre, &valor_ocupado);
            free(valor_ocupado);

            list_add(lista, bloque_libre);
            bloques_asignados++;
        }
        else
        {
            log_info(filesystem_logger_info, "No hay suficientes bloques libres en la partición de swap.");
            break;
        }
    }
    return lista;
}

bloque_con_id_t recibir_escritura_swap(int socket)
{

    bloque_con_id_t bloque_con_id;

    int id_bloque;
    void *datos;

    int size;
    void *buffer = recibir_buffer(&size, socket);
    int offset = 0;

    printf("size del stream a deserializar %d \n ", size);
    memcpy(&id_bloque, buffer + offset, sizeof(int));
    offset += sizeof(int);
    datos = malloc(tamanio_bloque);
    memcpy(datos, buffer + offset, tamanio_bloque);

    bloque_con_id.id_bloque = id_bloque;
    bloque_con_id.bloque.datos = datos;

    free(buffer);
    return bloque_con_id;
}

void *manejo_conexion_memoria_swap(void *arg)
{

    while (1)
    {
        op_code codigo_operacion = recibir_operacion(socket_memoria_swap);
        log_info(filesystem_logger_info, "Se recibio una operacion de MEMORIA - SWAP: %d", codigo_operacion);

        switch (codigo_operacion)
        {
        case HANDSHAKE_MEMORIA:
            codigo_operacion = recibir_handshake(socket_memoria_swap, filesystem_logger_info);
            log_info(filesystem_logger_info, "Deserialice el codigo de operacion %d", codigo_operacion);
            log_info(filesystem_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
            break;

        case INICIO_SWAP:
            int cantidad_bloques = recibir_int(socket_memoria_swap);
            t_list *lista_bloques_swap = lista_bloques_swap_reservados(cantidad_bloques);

            t_paquete *paquete_bloques_swap = crear_paquete_con_codigo_de_operacion(LISTA_BLOQUES_SWAP);
            serializar_lista_swap(lista_bloques_swap, paquete_bloques_swap);

            log_info(filesystem_logger_info, "Se reservaron %d bloques para el proceso pedido", cantidad_bloques);

            enviar_paquete(paquete_bloques_swap, socket_memoria_swap);
            eliminar_paquete(paquete_bloques_swap);

            // list_destroy_and_destroy_elements(lista_bloques_swap, free);

            break;

        case SWAP_A_LIBERAR:
            t_list *bloques_a_liberar = recibir_listado_id_bloques(socket_memoria_swap);
            log_info(filesystem_logger_info, "PEDIDO para liberar %d bloques", list_size(bloques_a_liberar));
            liberar_bloques_swap(bloques_a_liberar);
            log_info(filesystem_logger_info, "BLOQUES LIBERADOS");

            enviar_op_con_int(socket_memoria_swap, SWAP_LIBERADA, 1);
            break;

        case LEER_BLOQUE:
            int bloque_a_leer = recibir_int(socket_memoria_swap);

            log_info(filesystem_logger_info, "Pedido de lectura de bloque %d", bloque_a_leer);
            if (bloque_a_leer < bloques_swap)
            {
                bloque_t bloque;
                bloque.datos = leer_bloque_swap(bloque_a_leer);
                enviar_bloque(socket_memoria_swap, bloque, tamanio_bloque);
            }
            else
            {
                log_warning(filesystem_logger_info, "Número de bloque inválido: %d", bloque_a_leer);
            }

            break;

        case ESCRIBIR_BLOQUE:
            bloque_con_id_t bloque_id = recibir_escritura_swap(socket_memoria_swap);
            uint32_t id_bloque_a_escribir = (uint32_t)bloque_id.id_bloque;
            void *datos_a_escribir = bloque_id.bloque.datos;
            escribir_bloque_swap(id_bloque_a_escribir, datos_a_escribir);

            t_paquete *paquete_respuesta = crear_paquete_con_codigo_de_operacion(ESCRITURA_BLOQUE_OK);
            int respuesta = 1;
            agregar_a_paquete(paquete_respuesta, &respuesta, sizeof(int));
            enviar_paquete(paquete_respuesta, socket_memoria_swap);
            eliminar_paquete(paquete_respuesta);
            break;

        default:
            log_error(filesystem_logger_info, "Fallo la comunicacion MEMORIA. Abortando \n");
            finalizar_filesystem();
            abort();

            break;
        }
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
    close(socket_memoria_swap);
    close(socket_kernel);
}

int obtener_cantidad_de_bloques(int id_fcb)
{
    int tamanio_fcb = valor_fcb(id_fcb, TAMANIO_ARCHIVO);
    int cant_bloques_fcb = ceil((double)tamanio_fcb / tamanio_bloque);
    return cant_bloques_fcb;
}

uint32_t obtener_primer_bloque_libre()
{
    for (uint32_t i = 1; i < tamanio_fat; i++)
    {
        if (posicion_en_fat(i) == 0)
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

    return posicion_en_fat(id_bloque) != 0;
}

int crear_archivo_bloques(char *path_bloques)
{
    FILE *archivo_bloques = fopen(path_bloques, "wb+");
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al crear el archivo Bloques");
        return -1;
    }

    uint32_t valor_inicial = 0;
    for (uint32_t i = 0; i < cant_bloques; i++)
    {
        fwrite(&valor_inicial, tamanio_bloque, 1, archivo_bloques);
    }
    fflush(archivo_bloques);
    fclose(archivo_bloques);

    log_info(filesystem_logger_info, "Archivo de bloques creado");
    return 0;
}

void *leer_bloque(uint32_t numero_bloque)
{
    void *datos = NULL;
    FILE *archivo_bloques = fopen(path_bloques, "rb");
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo de Bloques para lectura");
        return -1;
    }

    fseek(archivo_bloques, numero_bloque * tamanio_bloque, SEEK_SET);
    fread(datos, tamanio_bloque, 1, archivo_bloques);
    fclose(archivo_bloques);
    sleep(retardo_acceso_bloque / 1000);

    return datos;
}

int crear_fat(char *path_fat)
{
    FILE *archivo_fat = fopen(path_fat, "wb+");
    if (archivo_fat == NULL)
    {
        log_error(filesystem_logger_info, "Error al crear el archivo FAT");
        return -1;
    }

    uint32_t valor_inicial = 0;
    for (uint32_t i = 1; i < tamanio_fat; i++)
    {
        fwrite(&valor_inicial, sizeof(uint32_t), 1, archivo_fat);
    }
    fflush(archivo_fat);
    fclose(archivo_fat);

    log_info(filesystem_logger_info, "Tabla FAT creada");
    return 0;
}

uint32_t posicion_en_fat(uint32_t posicion)
{
    FILE *archivo_fat = fopen(path_fat, "rb");
    if (archivo_fat == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo FAT");
        return -1;
    }

    fseek(archivo_fat, posicion * sizeof(uint32_t), SEEK_SET);

    uint32_t valor;
    fread(&valor, sizeof(uint32_t), 1, archivo_fat);

    fclose(archivo_fat);

    return valor;
}

void modificar_en_fat(uint32_t posicion, uint32_t nuevo_valor)
{
    FILE *archivo_fat = fopen(path_fat, "r+b"); // Abrir en modo lectura y escritura binaria
    if (archivo_fat == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo FAT");
        return -1;
    }

    fseek(archivo_fat, posicion * sizeof(uint32_t), SEEK_SET);

    fwrite(&nuevo_valor, sizeof(uint32_t), 1, archivo_fat);

    fclose(archivo_fat);

    return;
}

void inicializar_swap()
{
    FILE *archivo_bloques = fopen(path_bloques, "wb+");
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al crear el archivo Bloques");
        return;
    }

    uint32_t valor_swap = 0;
    for (uint32_t i = tamanio_fat - 1; i < bloques_swap; i++)
    {
        fwrite(&valor_swap, tamanio_bloque, 1, archivo_bloques);
    }

    fflush(archivo_bloques);
    fclose(archivo_bloques);

    log_info(filesystem_logger_info, "Swap inicializado");
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
fcb_t *obtener_fcb_desde_archivo(char *nombre_archivo)
{
    fcb_t *fcb_existente = malloc(sizeof(fcb_t));
    if (fcb_existente == NULL)
    {
        return NULL;
    }
    FILE *archivo = fopen(nombre_archivo, "rb");
    if (archivo == NULL)
    {
        free(fcb_existente);
        return NULL;
    }
    fread(fcb_existente, sizeof(fcb_t), 1, archivo);
    fclose(archivo);
    return fcb_existente;
}

void inicializar_fcb_list(char *path_fcb)
{
    lista_fcb = list_create();
    uint32_t fcb_id = 0;
    char *directorio = path_fcb;
    DIR *dir_fcb = opendir(directorio);

    if (dir_fcb == NULL)
    {
        log_error(filesystem_logger_info, "No se pudo abrir directorio: %s", directorio);
        return;
    }

    struct dirent *dir_entry;

    while ((dir_entry = readdir(dir_fcb)) != NULL)
    {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0)
            continue;

        char *nombre_completo = string_new();
        string_append(&nombre_completo, directorio);
        string_append(&nombre_completo, "/");
        string_append(&nombre_completo, dir_entry->d_name);

        fcb_t *fcb_existente = obtener_fcb_desde_archivo(nombre_completo);

        if (fcb_existente == NULL)
        {
            log_error(filesystem_logger_info, "Error al obtener el FCB desde el archivo: %s", nombre_completo);
            free(nombre_completo);
            continue;
        }
        fcb_existente->id = fcb_id;
        list_add(lista_fcb, fcb_existente);
        fcb_id++;
        free(nombre_completo);
    }

    log_info(filesystem_logger_info, "Lista de FCB creada correctamente");
    closedir(dir_fcb);
}

int crear_fcb(char *nombre_fcb)
{
    int resultado = -1;

    if (buscar_fcb(nombre_fcb) != -1)
    {
        log_error(filesystem_logger_info, "Ya existe un FCB con ese nombre");
        return resultado;
    }

    char *nombre_completo = string_new();
    string_append(&nombre_completo, path_fcb);
    string_append(&nombre_completo, nombre_fcb);
    string_append(&nombre_completo, ".dat");

    fcb_t *nuevo_fcb = inicializar_fcb();

    if (list_size(lista_fcb) == 0)
    {
        nuevo_fcb->id = 0;
    }
    else
    {
        uint32_t fcb_id = list_size(lista_fcb);
        nuevo_fcb->id = fcb_id;
        fcb_id++; // Incrementa después de asignar
    }

    nuevo_fcb->nombre_archivo = string_new();
    string_append(&nuevo_fcb->nombre_archivo, nombre_fcb);
    nuevo_fcb->ruta_archivo = string_new();
    string_append(&nuevo_fcb->ruta_archivo, nombre_completo);

    t_config *fcb_fisico = malloc(sizeof(t_config));
    fcb_fisico->path = string_new();
    fcb_fisico->properties = dictionary_create();
    string_append(&fcb_fisico->path, nombre_completo);
    char *nombre_duplicado = string_duplicate(nombre_fcb);

    dictionary_put(fcb_fisico->properties, "NOMBRE_ARCHIVO", nombre_duplicado);
    dictionary_put(fcb_fisico->properties, "TAMANIO_ARCHIVO", "0");
    dictionary_put(fcb_fisico->properties, "BLOQUE_INICIAL", "0");

    config_save_in_file(fcb_fisico, nombre_completo);
    list_add(lista_fcb, nuevo_fcb);
    dictionary_destroy(fcb_fisico->properties);
    free(nombre_duplicado);
    free(fcb_fisico->path);
    free(fcb_fisico);
    free(nombre_completo);
    log_warning(filesystem_logger_info, "El id del fcb en crear_fcb es %d", nuevo_fcb->id);

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
            resultado = fcb->id;
            log_warning(filesystem_logger_info, "El id del fcb en conseguir_fcb_id es %d", fcb->id);
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
    log_warning(filesystem_logger_info, "Posicion en el bloque fat %d con el valor %d, y el id del fcb es %d", bloque_inicial, id_fcb);
    if (posicion_en_fat(bloque_inicial) != 0)
    {
        return id_fcb;
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
    int id = conseguir_id_fcb(nombre);
    if (id != -1)
    {
        return _get_fcb_id(id);
    }
    return NULL;
}

uint32_t valor_para_fcb(fcb_t *fcb, fcb_prop_t llave)
{
    uint32_t valor = -1;
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
        if (bloque_actual >= tamanio_fat || bloque_actual < 0)
        {
            log_warning(filesystem_logger_info, "Indice de bloque fuera del espacio mapeado para FAT");
        }
        modificar_en_fat(bloque_actual, (obtener_primer_bloque_libre()));
        log_warning(filesystem_logger_info, "El valor del bloque actual en fat es %d", posicion_en_fat(bloque_actual));
        bloque_actual = posicion_en_fat(bloque_actual);
    }
}

void desasignar_bloques(int id_fcb, int nuevo_tamanio)
{
    uint32_t tamanio_bloque_32 = tamanio_bloque;
    int bloque_inicial = valor_fcb(id_fcb, BLOQUE_INICIAL);
    if (bloque_inicial == -1)
    {
        return;
    }
    uint32_t tamanio_actual = 0;
    while (bloque_inicial != -1 && tamanio_actual < nuevo_tamanio)
    {
        int siguiente_bloque = posicion_en_fat(bloque_inicial);
        modificar_en_fat(bloque_inicial, -1);
        modificar_fcb(id_fcb, BLOQUE_INICIAL, siguiente_bloque);
        bloque_inicial = siguiente_bloque;
        tamanio_actual += tamanio_bloque_32;
    }
}

int truncar_fcb(char *nombre_fcb, uint32_t nuevo_tamanio)
{
    int resultado = -1;
    uint32_t id_fcb = conseguir_id_fcb(nombre_fcb);
    log_warning(filesystem_logger_info, "El id del fcb en truncar_fcb es %d de tamanio %d", id_fcb, nuevo_tamanio);
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

void escribir_bloque(uint32_t numero_bloque, void *datos)
{
    FILE *archivo_bloques = fopen(path_bloques, "rb+");
    if (archivo_bloques == NULL)
    {
        log_error(filesystem_logger_info, "Error al abrir el archivo de Bloques para escritura");
        return;
    }

    fseek(archivo_bloques, numero_bloque * tamanio_bloque, SEEK_SET);
    fwrite(datos, tamanio_bloque, 1, archivo_bloques);
    fflush(archivo_bloques);
    fclose(archivo_bloques);

    sleep(retardo_acceso_bloque / 1000);

    return;
}

void escribir_datos(void *datos, int cant_bloques, fcb_t *id_fcb)
{
    uint32_t bloque_actual = id_fcb->bloque_inicial;
    uint32_t bloque_siguiente;

    for (int i = 0; i < cant_bloques; i++)
    {
        bloque_t bloque_info;
        bloque_info.datos = bloques[bloque_actual].datos;
        uint32_t bloque_id = id_fcb->bloque_inicial;
        log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);
        if (posicion_en_fat(bloque_actual) == 0)
        {
            asignar_bloques(id_fcb->id, tamanio_bloque);
            bloque_siguiente = UINT32_MAX;
        }
        else
        {

            bloque_siguiente = posicion_en_fat(bloque_id);
        }
        escribir_bloque(bloque_id, datos);
        if (bloque_siguiente == UINT32_MAX)
        {
            break;
        }
        bloque_actual = bloque_siguiente;
    }
}

bloque_t *obtener_lista_de_bloques(int id_fcb, int p_seek, int tam_a_leer, t_log *logger)
{
    int max_bloques = ceil((double)tam_a_leer / tamanio_bloque);
    bloque_t *lista_bloques = malloc(max_bloques * sizeof(bloque_t));
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
        int tam_restante = tam_a_leer - tam_leido;
        int bytes_leer = (tam_restante < tamanio_bloque) ? tam_restante : tamanio_bloque;
        // lista_bloques[idx_bloque].datos = leer_bloque(bloque_actual);
        tam_leido += bytes_leer;
        idx_bloque++;
        bloque_actual = posicion_en_fat(bloque_actual);
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
    int direccion_fisica = atoi(instruccion_file->param2);
    int tamanio = tamanio_bloque;
    int cantidad_bloques = (int)ceil((double)tamanio / tamanio_bloque);
    enviar_op_con_int(socket_memoria_op, F_WRITE_FS, direccion_fisica);

    op_code codigo = recibir_operacion(socket_memoria_op);
    if (codigo == F_WRITE_FS)
    {

        int size; // PODES CHEQUEAR QUE SIZE SEA > 0.
        void *bloque_a_escribir = recibir_buffer(&size, socket);

        // Enunciado: F_WRITE (Nombre Archivo, Dirección Lógica): Esta instrucción solicita al Kernel que se escriba en el archivo indicado la información que es
        // obtenida a partir de la dirección física de Memoria.

        // F_WIRTE - FS: solicitará al módulo File System que escriba en el archivo desde la dirección física de memoria recibida por parámetro (y el puntero que deberia apuntar al primer byte del bloque)

        // DE ACA PARA ABAJO, tenes que guardar bloque_a_escribir en donde te dice la instruccion (es 1 solo bloque el que se escribe)
        //

        t_list *paquete_recibido = recibir_paquete(socket_memoria);
        if (paquete_recibido != NULL && list_size(paquete_recibido) > 0)
        {
            char *valor_recibido = instruccion_file->param1;
            uint32_t id_fcb = conseguir_id_fcb(instruccion_file->param1);
            modificar_fcb(id_fcb, TAMANIO_ARCHIVO, (uint32_t)tamanio);
            bloque_t *lista_bloques = obtener_lista_de_bloques(id_fcb, direccion_fisica, tamanio, filesystem_logger_info);
            if (lista_bloques != NULL)
            {
                for (int i = 0; i < cantidad_bloques; i++)
                {
                    bloque_t bloque_info = lista_bloques[i];
                    void *datos_a_escribir = bloque_a_escribir + i * tamanio_bloque;
                    uint32_t bloque_id = valor_fcb(id_fcb, BLOQUE_INICIAL);
                    uint32_t tamanio_a_escribir = tamanio;
                    log_info(filesystem_logger_info, "Acceso Bloque - Bloque File System: %d", bloque_id);

                    pthread_mutex_lock(&mutex_fat);
                    if (posicion_en_fat(bloque_id) == 0)
                    {
                        asignar_bloques(id_fcb, tamanio_a_escribir);
                        modificar_en_fat(bloque_id, id_fcb);
                    }
                    pthread_mutex_unlock(&mutex_fat);

                    escribir_datos(datos_a_escribir, tamanio_a_escribir, lista_bloques);
                }
                free(lista_bloques);
            }
            list_destroy_and_destroy_elements(paquete_recibido, free);
        }
    }
}

void realizar_f_read(t_instruccion_fs *instruccion_file) // ver comentarios al final
{
    int direccion_fisica = (instruccion_file->puntero);
    uint32_t id_fcb = conseguir_id_fcb(instruccion_file->param1);
    log_info(filesystem_logger_info, "Entro a realizar f read");
    uint32_t id_bloque_inicial = (uint32_t)valor_fcb(id_fcb, BLOQUE_INICIAL);
    int tamanio_restante = (instruccion_file->param2); // TODO -- TOMAS -- el parametro 1 es el nombre del archivo, aca que pasa

    // Crear un buffer para almacenar los datos leídos
    void *bloque_leido = malloc(tamanio_bloque);
    void *bloque_leer_temporal = bloque_leido;  //TODO
    for (uint32_t id_bloque = id_bloque_inicial; tamanio_restante > 0; id_bloque = posicion_en_fat(id_bloque))
    {
        void *datos = leer_bloque(id_bloque);
        int offset_inicial = direccion_fisica % tamanio_bloque;
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
        direccion_fisica += bytes_leer;
    }

    // DEBERIAS LEER EL BLOQUE COMPLETO Y METERLO EN UN VOID* que tenga de tamanio tamanio_bloque (void* bloque_leido por ej y enviarlo como queda aca abajo

    void *bloque_leido = malloc(tamanio_bloque);

    t_paquete *paq_f_read = crear_paquete_con_codigo_de_operacion(F_WRITE_FS);
    paq_f_read->buffer->size += sizeof(int) + config_valores_filesystem.tam_bloque;
    paq_f_read->buffer->stream = malloc(paq_f_read->buffer->size);

    int offset = 0;

    printf("Size del stream a enviar %d \n", paq_f_read->buffer->size);

    memcpy(paq_f_read->buffer->stream + offset, &(direccion_fisica), sizeof(int));
    offset += sizeof(int);

    memcpy(paq_f_read->buffer->stream + offset, bloque_leido, config_valores_filesystem.tam_bloque);

    enviar_paquete(paq_f_read, socket_memoria_op);
    eliminar_paquete(paq_f_read);

    op_code cod_f_read = recibir_operacion(socket_memoria_op);
    if (cod_f_read == F_READ_FS)
    {
        int valor = recibir_int(socket_memoria_op);
        if (valor == -1)
        {
            log_info(filesystem_logger_info, "no se pudo escribir el bloque en memoria");
        }
        log_info(filesystem_logger_info, "Se escribio correctamente el bloque en memoria");
    }
    else
    {
        log_info(filesystem_logger_info, "Recibi el codigo de operacion %d como respuesta a F_READ de memoria", cod_f_read);
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

    memcpy(&(instruccion->param2_length), buffer + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    instruccion->param1 = malloc(instruccion->param1_length);
    memcpy(instruccion->param1, buffer + offset, instruccion->param1_length);
    offset += instruccion->param1_length;

    instruccion->param2 = malloc(instruccion->param2_length);
    memcpy(instruccion->param2, buffer + offset, instruccion->param2_length);
    offset += instruccion->param2_length;

    memcpy(&(instruccion->puntero), buffer + offset, sizeof(uint32_t));

    return instruccion;
}

void *comunicacion_kernel()
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
            log_info(filesystem_logger_info, "Instruccion deserializada correctamente");
            uint32_t pid = nueva_instruccion->pid;
            log_warning(filesystem_logger_info, "Recibi la operacion %d del PID %d", nueva_instruccion->estado, pid);
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
    if (conseguir_id_fcb(nueva_instruccion->param1) != -1)
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
    if (conseguir_id_fcb(nueva_instruccion->param1) != -1)
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