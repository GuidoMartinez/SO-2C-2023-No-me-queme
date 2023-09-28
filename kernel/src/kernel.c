#include "kernel.h"
#include "shared_utils.h"
#include <readline/readline.h>
#include <readline/history.h>

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

    /*// conexion FILESYSTEM
    conexion_filesystem = crear_conexion(config_valores_kernel.ip_filesystem, config_valores_kernel.puerto_filesystem);
    realizar_handshake(conexion_filesystem, HANDSHAKE_KERNEL, kernel_logger_info);
*/
    // log_warning(kernel_logger_info, "me conecte OK A TODOS LADOS, NO TENGO NADA QUE HACER");

    cola_block = list_create();
    cola_exit = list_create();
    cola_exec = list_create();
    cola_listos_para_ready = list_create();
    lista_ready = list_create();
    lista_global = list_create();

    pthread_mutex_init(&mutex_cola_ready, NULL);
    pthread_mutex_init(&mutex_cola_listos_para_ready, NULL);
    pthread_mutex_init(&mutex_cola_exit, NULL);
    pthread_mutex_init(&mutex_cola_exec, NULL);
    pthread_mutex_init(&mutex_cola_block, NULL);
    pthread_mutex_init(&mutex_generador_pid, NULL);

    sem_init(&sem_listos_ready, 0, 0);
    sem_init(&sem_ready, 0, 0);
    sem_init(&sem_exec, 0, 1);
    sem_init(&sem_exit, 0, 0);
    /*sem_init(&sem_block_return, 0, 0);*/

    while (1)
    {
        char *linea;
        int indice_split = 0;

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

            // log_info(kernel_logger_info, "FInalice proceso %s ",palabras[1]);
            finalizar_proceso(pid);

            // free(linea);
            // break;
        }

        if (!strncmp(linea, "iniciar_planificacion", 21))
        {

            // log_info(kernel_logger_info, "Inicie plani");
            iniciar_planificacion();

            //  free(linea);
            // break;
        }

        if (!strncmp(linea, "detener_planificacion", 21))
        {
            // char **palabras = string_split(linea, " ");
            // int pid = palabras[1];

            // log_info(kernel_logger_info, "FInalice proceso %s ",palabras[1]);
            //  finalizar_proceso(pid);

            free(linea);
            break;
        }
        if (!strncmp(linea, "modificar_grado", 15))
        {
            char **palabras = string_split(linea, " ");
            int grado = atoi(palabras[1]);

            sem.g_multiprog_ini = grado;

            //    int valor =sem.g_multiprog_ini;

            log_info(kernel_logger_info, " Modifique %d ", grado);
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
    config_valores_kernel.grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION_INI");
    sem.g_multiprog_ini = config_valores_kernel.grado_multiprogramacion; // esto es una struct
    config_valores_kernel.recursos = config_get_array_value(config, "RECURSOS");
    config_valores_kernel.instancias_recursos = config_get_array_value(config, "INSTANCIAS_RECURSOS");
}

void finalizar_kernel()
{ // Chequear todo lo que deberia finalizarse al tirar Control + C
    log_info(kernel_logger_info, "Finalizando el modulo Kernel");
    log_destroy(kernel_logger_info);
    close(conexion_cpu_dispatch);
    close(conexion_cpu_interrupt);
    close(conexion_memoria);
    close(conexion_filesystem);
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
    t_pcb *proceso_encontrado;
    // list_add(lista_global, proceso_encontrado);
    proceso_encontrado = buscarProceso(pid);
    pthread_mutex_lock(&mutex_cola_exit);
    list_add(cola_exit, proceso_encontrado);
    pthread_mutex_unlock(&mutex_cola_exit);
    cambiar_estado(proceso_encontrado, FINISH_EXIT);
    // list_add(lista_global, proceso_encontrado);
    // char *motivo = motivo_exit_to_string(proceso_encontrado->motivo_exit);
    //  sem_post(&sem_exit);
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

void iniciar_planificacion()
{

    planificar_largo_plazo();
    planificar_corto_plazo();
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

void exit_pcb(void)
{
    while (1)
    {
        sem_wait(&sem_exit);
        t_pcb *pcb = safe_pcb_remove(cola_exit, &mutex_cola_exit);
        char *motivo = motivo_exit_to_string(pcb->motivo_exit);
        pcb_destroy(pcb);
    }
}
void pcb_destroy(t_pcb *pcb)
{
    list_destroy(pcb->archivos_abiertos);
    // contexto_destroyer(pcb->contexto_ejecucion);
    free(pcb);
}
void detener_planificacion()
{
}

void safe_pcb_add(t_list *list, t_pcb *pcb, pthread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
    list_add(list, pcb);
    pthread_mutex_unlock(mutex);
}

t_pcb *safe_pcb_remove(t_list *list, pthread_mutex_t *mutex)
{
    t_pcb *pcb;
    pthread_mutex_lock(mutex);
    pcb = list_remove(list, 0);
    pthread_mutex_unlock(mutex);
    return pcb;
}
void pcb_create(int prio, int tamano, int pid_ok)
{
    t_pcb *pcb = malloc(sizeof(t_pcb));
    t_contexto_ejecucion *contexto = malloc(sizeof(t_contexto_ejecucion));
    pcb->archivos_abiertos = list_create();
    pcb->pid = pid_ok;
    pcb->contexto_ejecucion = contexto;
    pcb->contexto_ejecucion->pid = pid_ok;
    pcb->contexto_ejecucion->program_counter = 0;
    pcb->contexto_ejecucion->instruccion_ejecutada = malloc(sizeof(t_instruccion));
    pcb->contexto_ejecucion->instruccion_ejecutada->longitud_parametro1 = 1;
    pcb->contexto_ejecucion->instruccion_ejecutada->longitud_parametro2 = 1;
    pcb->contexto_ejecucion->instruccion_ejecutada->parametro1 = string_new();
    pcb->contexto_ejecucion->instruccion_ejecutada->parametro2 = string_new();
    pcb->contexto_ejecucion->registros = malloc(sizeof(t_registros));
    pcb->estado = NEW;
    safe_pcb_add(cola_listos_para_ready, pcb, &mutex_cola_listos_para_ready);
    list_add(lista_global, pcb);
    log_info(kernel_logger_info, "Llegue hasta PCB %d", pcb->pid);
    sem_post(&sem_listos_ready);
}

t_pcb *elegir_pcb_segun_algoritmo()
{
    switch (ALGORITMO_PLANIFICACION)
    {
    case FIFO:
        log_info(kernel_logger_info, "ELEGI FIFO");
        return safe_pcb_remove(lista_ready, &mutex_cola_ready);
    case RR:
        return 0;
    case PRIORIDADES:
        return 0;
    default:
        exit(1);
    }
}
char *estado_to_string(estado_proceso estado)
{
    switch (estado)
    {
    case NEW:
        return "NEW";
        break;
    case READY:
        return "READY";
        break;
    case BLOCK:
        return "BLOCK";
        break;
    case EXEC:
        return "EXEC";
        break;
    case FINISH_EXIT:
        return "EXIT";
        break;
    case FINISH_ERROR:
        return "EXIT_ERROR";
        break;
    default:
        return "UNKNOWN";
        break;
    }
}
void cambiar_estado(t_pcb *pcb, estado_proceso nuevo_estado)
{
    if (pcb->estado != nuevo_estado)
    {
        char *nuevo_estado_string = strdup(estado_to_string(nuevo_estado));
        char *estado_anterior_string = strdup(estado_to_string(pcb->estado));
        pcb->estado = nuevo_estado;
        // free(estado_anterior_string);
        // free(nuevo_estado_string);
    }
}

void procesar_cambio_estado(t_pcb *pcb, estado_proceso estado_nuevo)
{

    switch (estado_nuevo)
    {
    case READY:

        break;
    case FINISH_EXIT:

        break;
    case FINISH_ERROR:

        break;

    case BLOCK:

        break;
    default:

        break;
    }
}

void planificar_largo_plazo()
{
    log_info(kernel_logger_info, "Entre al largo plazo");
    pthread_t hilo_ready;
    pthread_t hilo_exit;
    pthread_t hilo_block;

    // pthread_create(&hilo_exit, NULL, (void *)exit_pcb, NULL);
    pthread_create(&hilo_ready, NULL, (void *)ready_pcb, NULL);
    // pthread_create(&hilo_block, NULL, (void *)block, NULL);

    // pthread_detach(hilo_exit);
    pthread_detach(hilo_ready);
    // pthread_detach(hilo_block);
}

void planificar_corto_plazo()
{
    log_info(kernel_logger_info, "Entre corto plazo");
    pthread_t hilo_corto_plazo;
    pthread_create(&hilo_corto_plazo, NULL, (void *)exec_pcb, NULL);
    pthread_detach(hilo_corto_plazo);
}

void ready_pcb(void)
{
    while (1)
    {
        sem_wait(&sem_listos_ready);
        t_pcb *pcb = safe_pcb_remove(cola_listos_para_ready, &mutex_cola_listos_para_ready);
        log_info(kernel_logger_info, "pASE AL READY PCB %d", pcb->pid);
        pthread_mutex_lock(&leer_grado);
        int procesos_activos = list_size(lista_ready);
        //+ list_size(cola_exec) + list_size(cola_block);

        if (procesos_activos < sem.g_multiprog_ini)
        {

            procesos_activos = procesos_activos + 1;
            pthread_mutex_unlock(&leer_grado);
            log_info(kernel_logger_info, "VOy a pasar al ready %d", pcb->pid);
            set_pcb_ready(pcb);
            sem_post(&sem_ready);
        }
    }
}

void exec_pcb()
{
    while (1)
    {

        sem_wait(&sem_ready);
        sem_wait(&sem_exec);
        log_info(kernel_logger_info, "Entre a hilo exec");
        t_pcb *pcb = elegir_pcb_segun_algoritmo();
        log_info(kernel_logger_info, "Sale pcb  %d", pcb->pid);
        prceso_admitido(pcb);
        enviar_contexto(conexion_cpu_dispatch, pcb->contexto_ejecucion);
        log_info(kernel_logger_info, "Envie PID %d con PC %d a CPU", pcb->pid,pcb->contexto_ejecucion->program_counter);
     

        codigo_operacion = recibir_operacion(conexion_cpu_dispatch);
        log_info(kernel_logger_info, "Recibi el codigo de operacion de CPU %d", codigo_operacion);

        //     sem_post(&sem_exec);
    }
}

void prceso_admitido(t_pcb *pcb)
{
    cambiar_estado(pcb, EXEC);
    safe_pcb_add(cola_exec, pcb, &mutex_cola_exec);
    sem_post(&sem_exec);
}

void block()
{
    while (1)
    {

        t_pcb *pcb = safe_pcb_remove(cola_block, &mutex_cola_block);
        set_pcb_ready(pcb);
    }
}

void asignar_algoritmo(char *algoritmo)
{
    if (strcmp(algoritmo, "FIFO") == 0)
    {
        ALGORITMO_PLANIFICACION = FIFO;
    }
    else if (strcmp(algoritmo, "RR") == 0)
    {
        ALGORITMO_PLANIFICACION = RR;
    }
    else
    {
        ALGORITMO_PLANIFICACION = PRIORIDADES;
    }
}

void set_pcb_ready(t_pcb *pcb)
{
    pthread_mutex_lock(&mutex_cola_ready);
    cambiar_estado(pcb, READY);
    list_add(lista_ready, pcb);
    pthread_mutex_unlock(&mutex_cola_ready);
    log_info(kernel_logger_info, "SET PCB READY %d", pcb->pid);
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