#include "kernel.h"
#include "plani.h"
#include "instructions.h"

void exit_pcb(void)
{
    while (1)
    {
        sem_wait(&sem_exit);
        t_pcb *pcb = safe_pcb_remove(cola_exit, &mutex_cola_exit);
        // char *motivo = motivo_exit_to_string(pcb->motivo_exit);
        // log_info(kernel_logger_info, "Le cambie el estado");

        liberar_recursos(pcb);

        pcb_destroy(pcb);

        if (list_size(lista_ready_detenidos)> 0){
        t_pcb* pcb_ready_detenido =list_remove(lista_ready_detenidos,0);
        set_pcb_ready(pcb_ready_detenido);
        }
        
        sem_post(&sem_ready);
        if (frenado != 1)
        {
            if (list_size(lista_ready) > 0)
                sem_post(&sem_exec);
        }
    }
}

void pcb_destroy(t_pcb *pcb)
{
    log_info(kernel_logger_info, "Entre al destroy ");

    if (list_size(pcb->archivos_abiertos) > 0)
    {
        for (int i = 0; i <= list_size(pcb->archivos_abiertos); i++)
        {
            t_archivo_global *archivo_global = buscarArchivoGlobal(lista_archivos_abiertos, list_get(pcb->archivos_abiertos, i));
            if (archivo_global == NULL)
            {
                continue;
            }
            archivo_global->contador--;

            if (archivo_global->contador == 0)
            {
                list_remove_element(lista_archivos_abiertos, archivo_global);
                free(archivo_global->nombreArchivo);
                free(archivo_global);
            }
        }
    }

    list_destroy(pcb->archivos_abiertos);

    free(pcb);
    log_info(kernel_logger_info, "Hice free ");
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
     log_info(kernel_logger_info, "rEMOVI PID %d", pcb->pid);
    return pcb;
}
void pcb_create(int prio, int tamano, int pid_ok)
{
    t_pcb *pcb = malloc(sizeof(t_pcb));
    t_contexto_ejecucion *contexto = malloc(sizeof(t_contexto_ejecucion));
    pcb->archivos_abiertos = list_create();
    pcb->pid = pid_ok;
    pcb->prioridad = prio;
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
    pcb->recurso_instruccion = strdup("");
    pcb->recursos_asignados = cargar_recursos_totales();
    safe_pcb_add(cola_listos_para_ready, pcb, &mutex_cola_listos_para_ready);
    list_add(lista_global, pcb);
    log_info(kernel_logger_info, "Llegue hasta PCB %d", pcb->pid);
    sem_post(&sem_listos_ready);
}

t_pcb *elegir_pcb_segun_algoritmo()
{

    if (list_size(lista_ready) == 0)
    {
        exit(1);
    }
    switch (ALGORITMO_PLANIFICACION)
    {
    case FIFO:
        log_info(kernel_logger_info, "ELEGI FIFO");
        return safe_pcb_remove(lista_ready, &mutex_cola_ready);
    case RR:
        log_info(kernel_logger_info, "ELEGI RR");
        return obtener_pcb_RR();
    case PRIORIDADES:
        log_info(kernel_logger_info, "ELEGI PRIORIDADES");
        return obtener_pcb_PRIORIDAD();
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
        // char* nuevo_estado_string = strdup(estado_to_string(nuevo_estado));
        // char* estado_anterior_string = strdup(estado_to_string(pcb->estado));
        // log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb->pid, strdup(estado_to_string(pcb->estado)), strdup(estado_to_string(nuevo_estado)));
        pcb->estado = nuevo_estado;
        // log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcb->pid, estado_anterior_string, nuevo_estado_string);
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

    pthread_create(&hilo_exit, NULL, (void *)exit_pcb, NULL);
    pthread_create(&hilo_ready, NULL, (void *)ready_pcb, NULL);

    pthread_detach(hilo_exit);
    pthread_detach(hilo_ready);
}

void planificar_corto_plazo()
{
    log_info(kernel_logger_info, "Entre corto plazo");
    pthread_t hilo_corto_plazo, hilo_quantum;

    pthread_create(&hilo_corto_plazo, NULL, (void *)exec_pcb, NULL);
    pthread_detach(hilo_corto_plazo);
    pthread_create(&hilo_quantum, NULL, (void *)quantum_interrupter, NULL);
    pthread_detach(hilo_quantum);
}

void ready_pcb(void)
{
    while (1)
    {
        sem_wait(&sem_listos_ready);
        if (list_is_empty(cola_listos_para_ready))
        {
            log_info(kernel_logger_info, "No hay procesos para admitir");
            // break;
        }
        else
        {
            t_pcb *pcb = safe_pcb_remove(cola_listos_para_ready, &mutex_cola_listos_para_ready);
           // log_info(kernel_logger_info, "Pase a READY el PCB: %d", pcb->pid);
            pthread_mutex_lock(&leer_grado);
            int procesos_ready = list_size(lista_ready);
           // int procesos_exec = list_size(cola_exec);
            int procesos_bloqueado = list_size(cola_block);

            int procesos_activos = procesos_ready + procesos_bloqueado;//+ procesos_exec 

          //  log_info(kernel_logger_info, "READY %d EXEC %d BLOQUEADOS %d",procesos_ready,procesos_exec,procesos_bloqueado);
            
            if (procesos_activos < sem.g_multiprog_ini)
            {

                procesos_activos= procesos_activos + 1;
               
                set_pcb_ready(pcb);
                if (ALGORITMO_PLANIFICACION == PRIORIDADES) //detener_planificacion
                {
                    if (proceso_en_ejecucion != NULL)
                    {
                        if (pcb->prioridad > proceso_en_ejecucion->prioridad)
                        {
                            t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
                            interrupcion->motivo_interrupcion = INTERRUPT_NUEVO_PROCESO;
                            interrupcion->pid = pcb->pid;
                            enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
                            free(interrupcion);
                        }
                    }
                }
                if (frenado != 1)
                {
                    sem_post(&sem_ready);
                }
               
            }
            else
            {
                log_info(kernel_logger_info, "Excede grado de multiprogramacion");
                pthread_mutex_lock(&mutex_cola_ready);
                list_add(lista_ready_detenidos, pcb);
                pthread_mutex_unlock(&mutex_cola_ready);
                log_info(kernel_logger_info, "Tamaño lista ready despues de exceder grado %d",list_size(lista_ready));
                log_info(kernel_logger_info, "Tamaño lista ready detenidos %d despues de exceder grado - PID %d",list_size(lista_ready_detenidos),pcb->pid);
                
               
            }
            pthread_mutex_unlock(&leer_grado);
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
        if (list_size(lista_ready) < 1 && proceso_en_ejecucion == NULL)
        {
            log_info(kernel_logger_info, "Lista ready vacia");
            continue;
        }
        if(proceso_en_ejecucion == NULL){
            log_warning(kernel_logger_info, "Proceso en ejecucion es NULL");
        }
        else {
            log_warning(kernel_logger_info, "Motivo de desalojo %d", proceso_en_ejecucion->contexto_ejecucion->motivo_desalojado);
        }
        if (proceso_en_ejecucion == NULL || proceso_en_ejecucion->contexto_ejecucion->motivo_desalojado != SYSCALL)
        {
            log_warning(kernel_logger_info, "Tengo que elegir pcb segun algoritmo");
            pcbelegido = elegir_pcb_segun_algoritmo();
            proceso_admitido(pcbelegido);
        }
        else
        {
            log_warning(kernel_logger_info, "Proceso en ejecucion es el pcb elegido");
            pcbelegido = proceso_en_ejecucion;
        }
        log_warning(kernel_logger_info, "Envie contexto");
        enviar_contexto(conexion_cpu_dispatch, pcbelegido->contexto_ejecucion);
        log_info(kernel_logger_info, "Envie PID %d con PC %d a CPU", pcbelegido->pid, pcbelegido->contexto_ejecucion->program_counter);

        // TODO: Liberar contexto desactualizado (0 prioritario)
        codigo_operacion = recibir_operacion(conexion_cpu_dispatch);
        log_info(kernel_logger_info, "Recibi operacion %d", codigo_operacion);
        if (codigo_operacion != CONTEXTO)
        {
            log_info(kernel_logger_info, "Error al recibir contexto");
            abort();
        } 
        t_contexto_ejecucion *ultimo_contexto = malloc(sizeof(t_contexto_ejecucion));
        ultimo_contexto = recibir_contexto(conexion_cpu_dispatch);
        pcbelegido->contexto_ejecucion = ultimo_contexto;

        proceso_en_ejecucion = pcbelegido; // TODO -- CHEQUEAR QUE NO ROMPA EN OTRO LADO GONZA

        if (pcbelegido->contexto_ejecucion->motivo_desalojado == PAGE_FAULT)
        {
            log_info(kernel_logger_info, "El proceso %d fue desalojado por PAGE FAULT", proceso_en_ejecucion->pid);

            args_pf *args = malloc(sizeof(args_pf));
            args->pid = proceso_en_ejecucion->pid;
            args->num_pf = pcbelegido->contexto_ejecucion->nro_pf;

            pthread_t hilo_page_fault;
            pthread_create(&hilo_page_fault, NULL, (void *)manejar_pf, args);
            pthread_detach(hilo_page_fault);

            continue;
        }

        int codigo_instruccion = pcbelegido->contexto_ejecucion->codigo_ultima_instru;
        log_info(kernel_logger_info, "Volvio PID %d con codigo inst %d ", ultimo_contexto->pid, pcbelegido->contexto_ejecucion->codigo_ultima_instru);
        pcbelegido->contexto_ejecucion->instruccion_ejecutada->pid = pcbelegido->pid;
        switch (codigo_instruccion)
        {
        case EXIT:
            kexit();
            break;
        case SLEEP:
            ksleep();
            break;
        case WAIT:
            kwait();
            break;
        case SIGNAL:
            ksignal();
            break;
        case F_OPEN:
            kf_open();
            break;
        case F_CLOSE:
            kf_close();
            break;
        case F_SEEK:
            kf_seek();
        case F_READ:
            kf_read();
            break;
        case F_WRITE:
            kf_write();
            break;
        case F_TRUNCATE:
            kf_truncate();
            break;
        default:
            log_info(kernel_logger_info, "Entre al default");
            safe_pcb_remove(cola_exec, &mutex_cola_exec);
            safe_pcb_add(lista_ready, pcbelegido, &mutex_cola_ready);
            sem_post(&sem_ready);
            sem_post(&sem_exec);
            break;
        }
    }

    // TODO: Chequear estos free
    // free(pcb);
    // free(ultimo_contexto);
}

void proceso_admitido(t_pcb *pcb)
{
    cambiar_estado(pcb, EXEC);
    safe_pcb_add(cola_exec, pcb, &mutex_cola_exec);
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
    pcb->tiempo_ingreso_ready = time(NULL);
    log_info(kernel_logger_info, "SET PCB READY %d", pcb->pid);
}

void set_pcb_block(t_pcb *pcb)
{
    pthread_mutex_lock(&mutex_cola_block);
    cambiar_estado(pcb, BLOCK);
    list_add(cola_block, pcb);
    pthread_mutex_unlock(&mutex_cola_block);
    log_info(kernel_logger_info, "SET PCB BLOCK %d", pcb->pid);
}

void remove_blocked(int pid)
{
    bool _proceso_id(void *elemento)
    {
        return ((t_pcb *)elemento)->pid == pid;
    }
    t_pcb *proceso_elegido;
    proceso_elegido = list_find(cola_block, _proceso_id);
    if (proceso_elegido != NULL)
    {
        pthread_mutex_lock(&mutex_cola_block);
        list_remove_element(cola_block, proceso_elegido);
        pthread_mutex_unlock(&mutex_cola_block);
    }
}

void remove_ready(int pid)
{
    bool _proceso_id(void *elemento)
    {
        return ((t_pcb *)elemento)->pid == pid;
    }
    t_pcb *proceso_elegido;
    proceso_elegido = list_find(lista_ready, _proceso_id);
    if (proceso_elegido != NULL)
    {
        pthread_mutex_lock(&mutex_cola_block);
        list_remove_element(lista_ready, proceso_elegido);
        pthread_mutex_unlock(&mutex_cola_block);
    }
}

bool maximo_RR(t_pcb *pcb1, t_pcb *pcb2)
{
    return pcb1->tiempo_ingreso_ready <= pcb2->tiempo_ingreso_ready;
}

bool maximo_PRIORIDAD(t_pcb *pcb1, t_pcb *pcb2)
{
    return pcb1->prioridad <= pcb2->prioridad;
}

t_pcb *obtener_pcb_RR()
{
    pthread_mutex_lock(&mutex_cola_ready);
    list_sort(lista_ready, (void *)maximo_RR);
    t_pcb *pcb = list_remove(lista_ready, 0);
    log_info(kernel_logger_info, "Se eligio el proceso %d por RR", pcb->pid);
    pthread_mutex_unlock(&mutex_cola_ready);
    return pcb;
}

t_pcb *obtener_pcb_PRIORIDAD()
{
    pthread_mutex_lock(&mutex_cola_ready);
    list_sort(lista_ready, (void *)maximo_PRIORIDAD);
    t_pcb *pcb = list_remove(lista_ready, 0);
    log_info(kernel_logger_info, "Se eligio el proceso %d por Prioridad", pcb->pid);
    pthread_mutex_unlock(&mutex_cola_ready);
    return pcb;
}
