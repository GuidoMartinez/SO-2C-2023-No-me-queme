#include "kernel.h"
#include "plani.h"
#include "shared_utils.h"
#include <readline/readline.h>
#include <readline/history.h>

t_log *kernel_logger_info;
t_config *config;

pthread_mutex_t mutex_cola_ready;
pthread_mutex_t mutex_cola_listos_para_ready;
pthread_mutex_t mutex_cola_block;
pthread_mutex_t mutex_cola_exec;
pthread_mutex_t mutex_cola_exit;
pthread_mutex_t leer_grado;
pthread_mutex_t mutex_generador_pid;
pthread_mutex_t mutex_tabla_archivos;
pthread_mutex_t mutex_PF;

sem_t sem_multiprog;
sem_t sem_exit;
sem_t sem_listos_ready;
sem_t sem_ready;
sem_t sem_exec;
sem_t sem_detener;
sem_t sem_blocked_w;
sem_t sem_detener_sleep;
sem_t sem_hilo_FS;
sem_t reanudaar_exec;
// sem_t sem_PF;

t_list *recursos_kernel;
t_list *lista_ready;
t_list *cola_block;
t_list *cola_listos_para_ready;
t_list *cola_exec;
t_list *cola_exit;
t_list *lista_global;
t_list *cola_blocked_recurso;

t_list *lista_ready_detenidos;
t_list *lista_blocked_detenidos;
t_list *lista_exec_detenidos;
t_list *lista_archivos_abiertos;
t_list *lista_global_archivos;

t_pcb *proceso_aux;
t_pcb *proceso_en_ejecucion;

pthread_t hiloFS;

int generador_de_id = 0;
int grado_multiprogramacion_ini;
int conexion_cpu_dispatch, conexion_cpu_interrupt, conexion_memoria, conexion_filesystem;
int pid_nuevo;
bool frenado;
op_code codigo_operacion;
recurso_instancia *recurso_proceso;
recurso_instancia *recurso_signal;
t_contexto_ejecucion *ultimo_contexto_ejecucion;

sem_t1 sem;
t_pcb *pcbelegido;
t_algoritmo ALGORITMO_PLANIFICACION;
arch_config config_valores_kernel;

void sighandler(int s)
{
    finalizar_kernel();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    if (argc < 1)
    {
        // para ejecutar el kernel es: ./kernel ./cfg/kernel.config
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }

    kernel_logger_info = log_create("./cfg/kernel.log", "KERNEL", true, LOG_LEVEL_INFO);

    cargar_configuracion(argv[1]);

    // CONEXION CON MEMORIA
    conexion_memoria = crear_conexion(config_valores_kernel.ip_memoria, config_valores_kernel.puerto_memoria);
    realizar_handshake(conexion_memoria, HANDSHAKE_KERNEL, kernel_logger_info);

    op_code respuesta = recibir_operacion(conexion_memoria);
    log_info(kernel_logger_info, "el codigo que me llego es %d", respuesta);
    if (respuesta == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(conexion_memoria, kernel_logger_info);
        log_info(kernel_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(kernel_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }

    // CONEXION CON CPU - DISPATCH
    conexion_cpu_dispatch = crear_conexion(config_valores_kernel.ip_cpu, config_valores_kernel.puerto_cpu_dispatch);
    realizar_handshake(conexion_cpu_dispatch, HANDSHAKE_CPU_DISPATCH, kernel_logger_info);

    respuesta = recibir_operacion(conexion_cpu_dispatch);
    if (respuesta == HANDSHAKE_CPU_DISPATCH)
    {
        op_code prueba = recibir_handshake(conexion_cpu_dispatch, kernel_logger_info);
        log_info(kernel_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(kernel_logger_info, "HANDSHAKE EXITOSO CON CPU DISPATCH");
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta de CPU DISPATCH.");
    }
    // CONEXION CON CPU - INTERRUPT
    conexion_cpu_interrupt = crear_conexion(config_valores_kernel.ip_cpu, config_valores_kernel.puerto_cpu_interrupt);
    realizar_handshake(conexion_cpu_interrupt, HANDSHAKE_CPU_INTERRUPT, kernel_logger_info);

    respuesta = recibir_operacion(conexion_cpu_interrupt);
    if (respuesta == HANDSHAKE_CPU_INTERRUPT)
    {
        op_code prueba = recibir_handshake(conexion_cpu_interrupt, kernel_logger_info);
        log_info(kernel_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(kernel_logger_info, "HANDSHAKE EXITOSO CON CPU INTERRUPT");
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta de CPU INTERRUPT.");
    }

    // conexion FILESYSTEM
    conexion_filesystem = crear_conexion(config_valores_kernel.ip_filesystem, config_valores_kernel.puerto_filesystem);
    realizar_handshake(conexion_filesystem, HANDSHAKE_KERNEL, kernel_logger_info);
    respuesta = recibir_operacion(conexion_filesystem);
    if (respuesta == HANDSHAKE_FILESYSTEM)
    {
        op_code prueba = recibir_handshake(conexion_filesystem, kernel_logger_info);
        log_info(kernel_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(kernel_logger_info, "HANDSHAKE EXITOSO CON FILESYSTEM");
    }
    else
    {
        log_warning(kernel_logger_info, "Operación desconocida. No se pudo recibir la respuesta del FILESYSTEM.");
    }

    pthread_create(&hiloFS, NULL, recibir_op_FS, NULL);
    pthread_detach(hiloFS);

    cola_block = list_create();
    cola_exit = list_create();
    cola_exec = list_create();
    cola_listos_para_ready = list_create();
    lista_ready = list_create();
    lista_global = list_create();
    // lista_block_detenidos = list_create();
    lista_ready_detenidos = list_create();
    // lista_exec_detenidos = list_create();
    lista_archivos_abiertos = list_create();
    lista_global_archivos = list_create();

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_listos_para_ready, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_cola_block, NULL);
    pthread_mutex_init(&mutex_generador_pid, NULL);
    pthread_mutex_init(&mutex_PF, NULL);

    sem_init(&sem_listos_ready, 0, 0);
    sem_init(&sem_ready, 0, 0);
    sem_init(&sem_exec, 0, 1);
    sem_init(&sem_exit, 0, 0);
    sem_init(&sem_detener, 0, 0);
    sem_init(&sem_blocked_w, 0, 0);
    sem_init(&sem_detener_sleep, 0, 0);
    sem_init(&sem_hilo_FS, 0, 0);
    sem_init(&reanudaar_exec, 0, 0);
    // sem_init(&sem_PF, 0, 0);

    while (1)
    {
        char *linea;

        linea = readline(">");

        if (!linea)
        {
            break;
        }

        if (linea)
        {
            add_history(linea);
        }

        if (!strncmp(linea, "exit", 4))
        {
            free(linea);
            break;
        }

        if (!strncmp(linea, "iniciar_proceso", 15))

        {
            char **palabras = string_split(linea, " ");
            char *path = palabras[1];
            int size = atoi(palabras[2]);
            int prioridad = atoi(palabras[3]);

            // log_info(kernel_logger_info, "Inicie proceso %s ",palabras[3]);
            iniciar_proceso(path, size, prioridad);

            // free(linea);
            // break;
        }

        if (!strncmp(linea, "finalizar_proceso", 17))
        {
            char **palabras = string_split(linea, " ");
            int pid = atoi(palabras[1]);

            finalizar_proceso(pid);

            // free(linea);
            // break;
        }

        if (!strncmp(linea, "iniciar_planificacion", 21))
        {

            log_info(kernel_logger_info, "Inicio de planificacion");
            iniciar_planificacion();

            //  free(linea);
            // break;
        }

        if (!strncmp(linea, "detener_planificacion", 21))
        {

            // log_info(kernel_logger_info, "FInalice proceso %s ",palabras[1]);
            log_info(kernel_logger_info, " PAUSA DE PLANIFICACION ");
            detener_planificacion();

            // break;
        }
        if (!strncmp(linea, "modificar_grado", 15))
        {
            char **palabras = string_split(linea, " ");
            int grado = atoi(palabras[1]);

            log_info(kernel_logger_info, "Grado anterior: %d - Grado actual: %d", sem.g_multiprog_ini, grado);
            sem.g_multiprog_ini = grado;

            //    int valor =sem.g_multiprog_ini;
            //  free(linea);
            // break;
        }

        // fr
    }

    return 0;
}

void cargar_configuracion(char *path)
{

    t_config *config = config_create(path); // Leo el archivo de configuracion

    if (config == NULL)
    {
        perror("Archivo de configuracion de kernel no encontrado \n");
        abort();
    }

    config_valores_kernel.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    config_valores_kernel.ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
    config_valores_kernel.ip_cpu = config_get_string_value(config, "IP_CPU");
    config_valores_kernel.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    config_valores_kernel.puerto_cpu_dispatch = config_get_string_value(config, "PUERTO_CPU_DISPATCH");
    config_valores_kernel.puerto_cpu_interrupt = config_get_string_value(config, "PUERTO_CPU_INTERRUPT");
    config_valores_kernel.puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");
    config_valores_kernel.quantum = config_get_int_value(config, "QUANTUM");
    config_valores_kernel.algoritmo_planificacion = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
    asignar_algoritmo(config_valores_kernel.algoritmo_planificacion);
    config_valores_kernel.grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION_INI");
    sem.g_multiprog_ini = config_valores_kernel.grado_multiprogramacion; // esto es una struct
    config_valores_kernel.recursos = config_get_array_value(config, "RECURSOS");
    config_valores_kernel.instancias_recursos = config_get_array_value(config, "INSTANCIAS_RECURSOS");

    // Cargo lista de recursosKernel con el archivo config
    recursos_kernel = list_create();

    int i;

    for (i = 0; config_valores_kernel.recursos[i] != NULL; i++)
    {
        recurso_instancia *recurso = (recurso_instancia *)malloc(sizeof(recurso_instancia));
        recurso->nombre = strdup(config_valores_kernel.recursos[i]);
        recurso->cantidad = (uint32_t)atoi(config_valores_kernel.instancias_recursos[i]);
        recurso->colabloqueado = queue_create();
        list_add(recursos_kernel, recurso);
    }
}

void finalizar_kernel()
{ // Chequear todo lo que deberia finalizarse al tirar Control + C
    log_info(kernel_logger_info, "Finalizando el modulo Kernel");
    log_destroy(kernel_logger_info);
    close(conexion_cpu_dispatch);
    close(conexion_cpu_interrupt);
    close(conexion_memoria);
    close(conexion_filesystem);

    free(config_valores_kernel.ip_memoria);
    free(config_valores_kernel.puerto_memoria);
    free(config_valores_kernel.ip_filesystem);
    free(config_valores_kernel.puerto_filesystem);
    free(config_valores_kernel.ip_cpu);
    free(config_valores_kernel.puerto_cpu_dispatch);
    free(config_valores_kernel.puerto_cpu_interrupt);
    free(config_valores_kernel.algoritmo_planificacion);
    string_array_destroy(config_valores_kernel.recursos);
    string_array_destroy(config_valores_kernel.instancias_recursos);

    abort();
}

void iniciar_proceso(char *path, int size, int prioridad)
{
    pthread_mutex_lock(&mutex_generador_pid);
    pid_nuevo = generador_de_id;
    generador_de_id++;
    pthread_mutex_unlock(&mutex_generador_pid);

    // TODO -- MUTEX PARA SOCKET MEMORIA??
    crear_proceso_memoria(pid_nuevo, size, path, conexion_memoria);

    pcb_create(prioridad, size, pid_nuevo);
}

void finalizar_proceso(int pid)
{
    if (proceso_en_ejecucion != NULL && proceso_en_ejecucion->pid == pid)
    {
        t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
        interrupcion->motivo_interrupcion = INTERRUPT_FIN_PROCESO;
        interrupcion->pid = pid;
        enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
    }
    else
    {
        log_info(kernel_logger_info, "El PID enviado no se encuentra en ejecucion");
        // return;
    }
    t_pcb *proceso_encontrado;
    proceso_encontrado = buscarProceso(pid);
    if (proceso_encontrado->estado == BLOCK)
    {
        log_info(kernel_logger_info, "lo saco de blocked");
        remove_blocked(proceso_encontrado->pid);
    }
    if (proceso_encontrado->estado == READY)
    {
        log_info(kernel_logger_info, "lo saco de ready");
        remove_ready(proceso_encontrado->pid);
    }
    pthread_mutex_lock(&mutex_cola_exit);
    list_add(cola_exit, proceso_encontrado);
    pthread_mutex_unlock(&mutex_cola_exit);
    cambiar_estado(proceso_encontrado, FINISH_EXIT);
    log_info(kernel_logger_info, "lo puse en exit");
    // list_add(lista_global, proceso encontrado);
    // char *motivo = motivo_exit_to_string(proceso_encontrado->motivo_exit);
    sem_post(&sem_exit);
    log_info(kernel_logger_info, "Llegue hasta finalizar ");
}

t_pcb *buscarProceso(int pid_pedido)
{
    bool _proceso_id(void *elemento)
    {
        return ((t_pcb *)elemento)->pid == pid_pedido;
    }
    t_pcb *proceso_elegido;
    proceso_elegido = list_find(lista_global, _proceso_id);
    return proceso_elegido;
}

t_pcb *buscarProcesoBloqueado(int pid_pedido)
{
    bool _proceso_id(void *elemento)
    {
        return ((t_pcb *)elemento)->pid == pid_pedido;
    }
    t_pcb *proceso_elegido;
    proceso_elegido = list_find(cola_block, _proceso_id);
    return proceso_elegido;
}

t_pcb *buscar_proceso_por_nro_pf(int nro_pf)
{
    bool _proceso_id(void *elemento)
    {
        return ((t_pcb *)elemento)->contexto_ejecucion->nro_pf == nro_pf;
    }
    t_pcb *proceso_elegido;
    proceso_elegido = list_find(lista_global, _proceso_id);
    return proceso_elegido;
}

void iniciar_planificacion()
{
    if (frenado)
    {
        log_info(kernel_logger_info, "entre a iniciar plani dsp de haber frenado");
        // list_add_all(lista_ready, lista_ready_detenidos);
        sem_post(&sem_detener);
        sem_post(&sem_detener_sleep);
        sem_post(&sem_ready);
        //if (ALGORITMO_PLANIFICACION != PRIORIDADES)
        sem_post(&sem_exec);
    }
    else
    {
        planificar_largo_plazo();
        planificar_corto_plazo();
    }

    frenado = false;
}

void detener_planificacion()
{
    frenado = true;
    log_warning(kernel_logger_info, "cambie el frenado %d", frenado);

    pthread_mutex_lock(&mutex_cola_ready);
    // pthread_mutex_lock(&mutex_cola_exec);

    /// list_add_all(lista_ready_detenidos, lista_ready);
    // list_add_all(cola_block, lista_block_detenidos);
    // list_add_all(cola_exec, lista_exec_detenidos);

    // list_clean(lista_ready);
    //  list_clean(cola_exec);
    pthread_mutex_unlock(&mutex_cola_ready);
    // pthread_mutex_unlock(&mutex_cola_exec);
}

void quantum_interrupter(void)
{
    t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
    interrupcion->motivo_interrupcion = INTERRUPT_FIN_QUANTUM;
    interrupcion->pid = -1;
    while (1)
    {
        usleep(config_valores_kernel.quantum * 1000);
        if (ALGORITMO_PLANIFICACION == RR)
        {
            enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
        }
    }
    free(interrupcion);
}

char *motivo_exit_to_string(motivo_exit motivo)
{
    switch (motivo)
    {
    case SUCCESS:
        return "SUCCESS";
    case SEG_FAULT:
        return "SEG_FAULT";
    case OUT_OF_MEMORY:
        return "OUT_OF_MEMORY";
    case RECURSO_INEXISTENTE:
        return "RECURSO_INEXISTENTE";
    default:
        return "INDETERMINADO";
    }
}

t_list *cargar_recursos_totales()
{

    t_list *lista = list_create();
    int i;
    for (i = 0; config_valores_kernel.recursos[i] != NULL; i++)
    {
        recurso_instancia *recurso = (recurso_instancia *)malloc(sizeof(recurso_instancia));
        recurso->nombre = strdup(config_valores_kernel.recursos[i]);
        recurso->cantidad = 0;
        list_add(lista, recurso);
    }
    return lista;
}

bool ocupa_recursos(t_list *recursos)
{
    bool _recursos_asignados(void *elemento)
    {
        return ((recurso_instancia *)elemento)->cantidad > 0;
    }
    return list_any_satisfy(recursos, _recursos_asignados);
}

bool tiene_recurso(t_list *recursos, char *recurso)
{
    bool _recurso(void *elemento)
    {
        return strcmp(((recurso_instancia *)elemento)->nombre, recurso) == 0;
    }
    recurso_instancia *recurso_inst = list_find(recursos, _recurso);
    if (recurso_inst->cantidad > 0)
        return true;
    else
        return false;
}

void crear_proceso_memoria(int pid_nuevo, int size, char *path, int conexion_memoria)
{
    t_paquete *paquete_proceso_nuevo = crear_paquete_con_codigo_de_operacion(INICIALIZAR_PROCESO);
    serializar_pedido_proceso_nuevo(paquete_proceso_nuevo, pid_nuevo, size, path);
    enviar_paquete(paquete_proceso_nuevo, conexion_memoria);
    eliminar_paquete(paquete_proceso_nuevo);
}

void serializar_pedido_proceso_nuevo(t_paquete *paquete, int pid, int size, char *path)
{
    paquete->buffer->size += sizeof(uint32_t) * 3 +
                             strlen(path) + 1;

    printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int desplazamiento = 0;

    memcpy(paquete->buffer->stream + desplazamiento, &(pid), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + desplazamiento, &(size), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    uint32_t long_path = strlen(path) + 1;
    memcpy(paquete->buffer->stream + desplazamiento, &(long_path), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + desplazamiento, path, long_path);
}

void serializar_pedido_archivo_fs(t_paquete *paquete, int pid, char *nombreArchivo)
{
    paquete->buffer->size += sizeof(uint32_t) * 3 +
                             strlen(nombreArchivo) + 1;

    printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int desplazamiento = 0;

    memcpy(paquete->buffer->stream + desplazamiento, &(pid), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + desplazamiento, &(nombreArchivo), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);
}

void serializar_truncate_archivo_fs(t_paquete *paquete, int pid, int tamanio)
{

    printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int desplazamiento = 0;

    memcpy(paquete->buffer->stream + desplazamiento, &(pid), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);

    memcpy(paquete->buffer->stream + desplazamiento, &(tamanio), sizeof(uint32_t));
    desplazamiento += sizeof(uint32_t);
}

void chequear_archivo_fs(int pid_nuevo, char *nombreArchivo, int conexion_filesystem)
{
    t_paquete *paquete_proceso_nuevo = crear_paquete_con_codigo_de_operacion(OP_FILESYSTEM);
    serializar_pedido_archivo_fs(paquete_proceso_nuevo, pid_nuevo, nombreArchivo);
    enviar_paquete(paquete_proceso_nuevo, conexion_filesystem);
    eliminar_paquete(paquete_proceso_nuevo);
}

void truncate_archivo_fs(int pid_nuevo, int tamanio, int conexion_filesystem)
{
    t_paquete *paquete_proceso_nuevo = crear_paquete_con_codigo_de_operacion(OP_FILESYSTEM);
    serializar_truncate_archivo_fs(paquete_proceso_nuevo, pid_nuevo, tamanio);
    enviar_paquete(paquete_proceso_nuevo, conexion_filesystem);
    eliminar_paquete(paquete_proceso_nuevo);
}

void enviarInstruccionFS(int conexion, t_instruccion_fs *inst_fs)
{
    t_paquete *paquete = crear_paquete_con_codigo_de_operacion(OP_FILESYSTEM);
    serializar_instruccion_fs(paquete, inst_fs);
    enviar_paquete(paquete, conexion);
    eliminar_paquete(paquete);
}

recurso_instancia *buscar_recurso(t_list *lista_recursos, char *nombre_recurso)
{
    bool _recursoPorNombre(void *elemento)
    {
        return strcmp(((recurso_instancia *)elemento)->nombre, nombre_recurso) == 0;
    }
    recurso_instancia *recurso_existente;
    recurso_existente = list_find(lista_recursos, _recursoPorNombre);
    return recurso_existente;
}

// Libera recursos del proceso y también los agrega en el kernel
void liberar_recursos(t_pcb *pcb)
{
    t_list *lista_recursos_liberar = pcb->recursos_asignados;
    recurso_instancia *recurso_en_kernel = (recurso_instancia *)malloc(sizeof(recurso_instancia));

    t_pcb *pcb_bloqueado;

    enviar_op_con_int(conexion_memoria, FINALIZAR_PROCESO, pcb->pid);

    for (int i = 0; i < list_size(lista_recursos_liberar); i++)
    {

        recurso_proceso = list_get(lista_recursos_liberar, i);
        recurso_en_kernel = buscar_recurso(recursos_kernel, recurso_proceso->nombre);

        if (recurso_proceso->cantidad < 1)
            continue;

        t_queue *cola_bloqueados = recurso_en_kernel->colabloqueado;
        // log_info(kernel_logger_info, "tamaño cola bloqueados %d", queue_size(cola_bloqueados));

        // log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n", recurso_en_kernel->cantidad);

        recurso_en_kernel->cantidad += recurso_proceso->cantidad;
        // log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n", recurso_en_kernel->cantidad);

        recurso_proceso->cantidad -= recurso_proceso->cantidad;
        t_queue *colaAux = queue_create();

        while (queue_size(cola_bloqueados) > 0)
        {
            pcb_bloqueado = queue_pop(cola_bloqueados);

            if (pcb_bloqueado->pid != pcb->pid)
            {
                queue_push(colaAux, pcb_bloqueado);
            }
        }
        t_pcb *aux;
        while (queue_size(colaAux) > 0)
        {
            aux = queue_pop(colaAux);
            queue_push(cola_bloqueados, aux);
        }
        if (queue_size(cola_bloqueados) > 0)
        {
            pcb_bloqueado = queue_pop(cola_bloqueados);

            remove_blocked(pcb_bloqueado->pid);
            recurso_proceso = buscar_recurso(pcb_bloqueado->recursos_asignados, recurso_proceso->nombre);
            recurso_proceso->cantidad += 1;
            recurso_en_kernel->cantidad -= 1;

            recurso_proceso = buscar_recurso(pcb_bloqueado->recursos_asignados, recurso_proceso->nombre);

            if (!frenado)
            {
                set_pcb_ready(pcb_bloqueado);
                sem_post(&sem_ready);
                if (list_size(lista_ready) > 0)
                    sem_post(&sem_exec);
            }
            else
            {
                list_add(lista_ready_detenidos, pcb_bloqueado);
            }
        }
    }
}

void finalizar_proceso_en_ejecucion()
{
    pthread_mutex_lock(&mutex_cola_exec);

    proceso_aux = list_remove(cola_exec, 0);

    pthread_mutex_unlock(&mutex_cola_exec);

    proceso_en_ejecucion = NULL;

    pthread_mutex_lock(&mutex_cola_exit);

    pcbelegido->estado = FINISH_EXIT;
    list_add(cola_exit, pcbelegido);

    pthread_mutex_unlock(&mutex_cola_exit);

    sem_post(&sem_exit);
}

t_archivo_abierto_proceso *crear_archivo_proceso(char *nombre, t_pcb *proceso)
{
    t_archivo_abierto_proceso *archivo = malloc(sizeof(t_archivo_abierto_proceso));
    archivo->puntero = 0;
    archivo->nombreArchivo = string_new();
    string_append(&(archivo->nombreArchivo), nombre);
    list_add(proceso->archivos_abiertos, archivo);
    return archivo;
}

void agregar_nuevo_archivo(char *nombre)
{
    list_add(lista_global_archivos, nombre);
}

int verif_crear_recurso_file(t_archivo_global *archivo)
{
    int result = 0;
    if (!archivo_existe(lista_global_archivos, archivo->nombreArchivo))
    {
        // crear_archivo_global(archivo->nombreArchivo);
        result = 1;
    }

    return result;
}

bool archivo_existe(t_list *lista_archivos, char *nombre_archivo)
{
    t_archivo_global *archivoGeneral = buscarArchivoGlobal(lista_archivos, nombre_archivo);

    // Verificar si el archivo existe antes de acceder a su campo nombreArchivo
    if (archivoGeneral != NULL && archivoGeneral->nombreArchivo != NULL)
    {
        return true;
    }

    return false;
}

t_archivo_abierto_proceso *buscar_archivo_proceso(t_list *lista, char *recursoPedido2)
{
    bool _archivoPorNombre(void *elemento)
    {
        return strcmp(((t_archivo_abierto_proceso *)elemento)->nombreArchivo, recursoPedido2) == 0;
    }

    t_archivo_abierto_proceso *archivoExistente;
    archivoExistente = list_find(lista, _archivoPorNombre);
    return archivoExistente;
}

t_archivo_global *crear_archivo_global(char *nombre, char lock)
{

    t_archivo_global *archivo = malloc(sizeof(t_archivo_global));
    archivo->nombreArchivo = string_new();
    string_append(&(archivo->nombreArchivo), nombre);
    archivo->lock = lock;
    archivo->contador = 1;
    archivo->colabloqueado = queue_create();
    list_add(lista_archivos_abiertos, archivo);
    return archivo;
}

t_archivo_global *crear_archivo_global_con_contador(char *nombre, char lock, int contador)
{

    t_archivo_global *archivo = malloc(sizeof(t_archivo_global));
    archivo->nombreArchivo = string_new();
    string_append(&(archivo->nombreArchivo), nombre);
    archivo->lock = lock;
    archivo->contador = contador;
    archivo->colabloqueado = queue_create();
    list_add(lista_archivos_abiertos, archivo);
    return archivo;
}

void sumar_contador_archivo_global(char *nombre_archivo)
{
    bool _encontrar_por_nombre(void *elemento)
    {
        return ((t_archivo_global *)elemento)->nombreArchivo == nombre_archivo;
    }
    t_archivo_global *archivo = buscarArchivoGlobal(lista_archivos_abiertos, nombre_archivo);
    archivo->contador++;
    list_replace_by_condition(lista_archivos_abiertos, _encontrar_por_nombre, archivo);
}

void restar_contador_archivo_global(char *nombre_archivo)
{
    bool _encontrar_por_nombre(void *elemento)
    {
        return ((t_archivo_global *)elemento)->nombreArchivo == nombre_archivo;
    }
    t_archivo_global *archivo = buscarArchivoGlobal(lista_archivos_abiertos, nombre_archivo);
    archivo->contador--;
    list_replace_by_condition(lista_archivos_abiertos, _encontrar_por_nombre, archivo);
}

t_archivo_global *buscarArchivoGlobal(t_list *lista_archivos, char *nombre_archivo)
{
    for (int i = 0; i < list_size(lista_archivos); i++)
    {
        t_archivo_global *archivo = (t_archivo_global *)list_get(lista_archivos, i);
        if (strcmp(archivo->nombreArchivo, nombre_archivo) == 0)
        {
            return archivo;
        }
    }
    return NULL;
}

void exec_block_fs()
{

    safe_pcb_remove(cola_exec, &mutex_cola_exec);
    proceso_en_ejecucion = NULL;
    log_info(kernel_logger_info, "mando a bloquear al proceso");
    set_pcb_block(pcbelegido);
    log_info(kernel_logger_info, "bloqueado");
    log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "BLOCKED");

    pthread_mutex_lock(&mutex_cola_block);
    pcbelegido->motivo_block = OP_FILESYSTEM;
    pthread_mutex_unlock(&mutex_cola_block);
}

int open_file(char *nombre_archivo, char lock)
{
    // lista_global_archivos: sacar
    t_archivo_global *archivo = buscarArchivoGlobal(lista_global_archivos, nombre_archivo);
    if (archivo == NULL)
    {
        return -1;
    }
    archivo->lock = lock;
    archivo->contador += 1;
    return 1;
}

void fs_interaction()
{

    // Usar instruccion_fs
    // enviar_instruccion(conexion_filesystem, proceso_en_ejecucion->contexto_ejecucion->instruccion_ejecutada);

    exec_block_fs();
    // Se espera respuesta en hilo
    sem_post(&sem_hilo_FS);
    log_warning(kernel_logger_info, "Se envio instruccion a FS");

    // sem_post(&sem_ready);
    // sem_post(&sem_exec);
}

void *recibir_op_FS()
{
    while (1)
    {
        t_pcb *pcb_bloqueado;
        sem_wait(&sem_hilo_FS);

        t_resp_file op = recibir_operacion(conexion_filesystem);
        int *pid = malloc(sizeof(int));
        recibir_pid(conexion_filesystem, pid);
        log_warning(kernel_logger_info, "Recibi operacion de fs %d", op);
        switch (op)
        {
        case F_ERROR:
            log_error(kernel_logger_info, "Obtuvimos codigo F_ERROR");
            break;
        case F_OPEN_SUCCESS:
            sem_post(&reanudaar_exec);
            break;
        case F_TRUNCATE_SUCCESS:
            pcb_bloqueado = buscarProceso(*(pid));
            remove_blocked(pcb_bloqueado->pid);
            log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb_bloqueado->pid, "BLOCKEADO", "READY");
            set_pcb_ready(pcb_bloqueado);
            sem_post(&sem_ready);
            sem_post(&sem_exec);
            break;
        case F_WRITE_SUCCESS:
            pcb_bloqueado = buscarProceso(*(pid));
            remove_blocked(pcb_bloqueado->pid);
            log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb_bloqueado->pid, "BLOCKEADO", "READY");
            set_pcb_ready(pcb_bloqueado);
            sem_post(&sem_ready);
            sem_post(&sem_exec);
            break;
        case F_READ_SUCCESS:
            pcb_bloqueado = buscarProceso(*(pid));
            remove_blocked(pcb_bloqueado->pid);
            log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb_bloqueado->pid, "BLOCKEADO", "READY");
            set_pcb_ready(pcb_bloqueado);
            sem_post(&sem_ready);
            sem_post(&sem_exec);
            break;
        case F_CREATE_SUCCESS:
            sem_post(&reanudaar_exec);
            break;
        case FILE_DOESNT_EXISTS:
            t_instruccion_fs *inst_f_open_fs = inicializar_instruccion_fs(proceso_en_ejecucion->contexto_ejecucion->instruccion_ejecutada, 1);
            inst_f_open_fs->estado = F_CREATE;
            enviarInstruccionFS(conexion_filesystem, inst_f_open_fs);
            sem_post(&sem_hilo_FS);
            break;
        default:
            log_info(kernel_logger_info, "Me llego un codigo de operacion de FS que no reconozco");
            finalizar_kernel();
            break;
        }
    }
}

void *manejar_pf(void *args)
{
    // sem_wait(&sem_PF);
    /*1.- Mover al proceso al estado Bloqueado. Este estado bloqueado será independiente de todos los
    demás ya que solo afecta al proceso y no compromete recursos compartidos.*/

    args_pf *args_pf = args;

    int pid = args_pf->pid;
    int nro_pf = args_pf->num_pf;

    log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pid, "EXEC", "BLOCKED");
    log_info(kernel_logger_info, "Pagina[%d]-Bloqueado por: PAGE_FAULT  \n", nro_pf);

    // todo AGREGAR SEMAFORO
    /*t_pcb *pcb = buscarProceso(pid);
    set_pcb_block(pcb);
    proceso_en_ejecucion = NULL;

    log_info(kernel_logger_info, "cola exec: %d", list_size(cola_exec));
    safe_pcb_remove(cola_exec, &mutex_cola_exec);*/

    if (frenado != 1)
    {
        if (list_size(lista_ready) > 0)
        {
            sem_post(&sem_ready);
            sem_post(&sem_exec);
        }
    }

    /*2.- Solicitar al módulo memoria que se cargue en memoria principal la página correspondiente, la
    misma será obtenida desde el mensaje recibido de la CPU.*/

    enviar_pf(conexion_memoria, PAGE_FAULT_KERNEL, nro_pf, pid);

    /*3.- Esperar la respuesta del módulo memoria.*/

    // TODO - GONZA - SUMAR MUTEX PARA EL SOCKET DE MEMORIA PARA ASEGURARSE QUE ESE HILO RECIBA PRIMERO LA RESPUESTA DEL PG.
    pthread_mutex_lock(&mutex_PF);
    op_code op = recibir_operacion(conexion_memoria);
    if (op != PAGINA_CARGADA)
    {
        log_error(kernel_logger_info, "No se puedo cargar la pagina.");
        pthread_mutex_unlock(&mutex_PF);
        return NULL;
    }

    int pid_memoria;

    recibir_pid(conexion_memoria, &pid_memoria);
    pthread_mutex_unlock(&mutex_PF);
    /*4.- Al recibir la respuesta del módulo memoria, desbloquear el proceso y colocarlo en la cola de
    ready.*/

    t_pcb *pcb_bloqueado = buscarProceso(pid_memoria);

    pcb_bloqueado->contexto_ejecucion->motivo_desalojado = PAGE_FAULT_RESUELTO;

    remove_blocked(pcb_bloqueado->pid);
    set_pcb_ready(pcb_bloqueado);
    log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb_bloqueado->pid, "BLOCKED", "READY");

    if (pcb_bloqueado->prioridad < pcbelegido->prioridad)
    {
        log_info(kernel_logger_info, "mando interrupt");
        t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
        interrupcion->motivo_interrupcion = INTERRUPT_NUEVO_PROCESO;
        interrupcion->pid = pcbelegido->pid;
        enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
        free(interrupcion);
    }

    if (frenado != 1)
    {
        sem_post(&sem_ready);
        sem_post(&sem_exec);
    }

    return NULL;
}

void enviar_pf(int socket, op_code code, int num_pag, int pid)
{
    t_paquete *paquete = crear_paquete_con_codigo_de_operacion(code);
    paquete->buffer->size += sizeof(int) * 2;
    paquete->buffer->stream = malloc(paquete->buffer->size);

    int offset = 0;

    memcpy(paquete->buffer->stream + offset, &(pid), sizeof(int));
    offset += sizeof(int);
    memcpy(paquete->buffer->stream + offset, &(num_pag), sizeof(int));

    enviar_paquete(paquete, socket);
    eliminar_paquete(paquete);
}

t_instruccion_fs *inicializar_instruccion_fs(t_instruccion *instr, uint32_t ptr)
{
    t_instruccion_fs *instr_fs = malloc(sizeof(t_instruccion_fs));

    instr_fs->estado = instr->codigo;
    instr_fs->pid = instr->pid;
    instr_fs->param1_length = instr->longitud_parametro1;
    instr_fs->param2_length = instr->longitud_parametro2;

    instr_fs->param1 = malloc(instr->longitud_parametro1);
    memcpy(instr_fs->param1, instr->parametro1, instr->longitud_parametro1);

    instr_fs->param2 = malloc(instr->longitud_parametro2);
    memcpy(instr_fs->param2, instr->parametro2, instr->longitud_parametro2);

    instr_fs->puntero = ptr;
    return instr_fs;
}

t_resp_file esperar_respuesta_file()
{

    t_resp_file respuesta = F_ERROR;

    respuesta = recibir_operacion(conexion_filesystem);
    switch (respuesta)
    {
    case 0:
        log_error(kernel_logger_info, "Fallo de serializacion de respuesta file");

        break;
    default:

        break;
    }
    // log_warning(kernel_logger_info, "El codigo que recibi es %s", obtener_nombre_resp_file(respuesta));
    return respuesta;
}

void hay_deadlock(char *recurso_deadlock)
{
    t_list *nombres_recursos = proceso_en_ejecucion->recursos_asignados;

    log_info(kernel_logger_info, "Deadlock detectado: %d - Recursos en posesión: ", proceso_en_ejecucion->pid);

    for (int i = 0; i < list_size(nombres_recursos); i++)
    {
        char *recurso = list_get(nombres_recursos, i);
        log_info(kernel_logger_info, "%s", recurso);
    }

    log_info(kernel_logger_info, "Recurso Requerido: %s ", recurso_deadlock);
}