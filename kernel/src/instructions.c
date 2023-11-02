#include "kernel.h"
#include "plani.h"
#include "instructions.h"

void kexit()
{
    log_info(kernel_logger_info, "Entre al exit");
    proceso_en_ejecucion = NULL;
    pthread_mutex_lock(&mutex_cola_exit);
    pcbelegido->estado = FINISH_EXIT;
    list_add(cola_exit, pcbelegido);
    pthread_mutex_unlock(&mutex_cola_exit);
    sem_post(&sem_exit);
}

void ksleep()
{
    safe_pcb_remove(cola_exec, &mutex_cola_exec);
    set_pcb_block(pcbelegido);
    proceso_en_ejecucion = NULL;
    args_sleep *args = malloc(sizeof(args_sleep));
    args->pcb = pcbelegido;
    args->tiempo = atoi(pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);
    log_info(kernel_logger_info, "ENTRE AL SLEEP");
    pthread_t hilo_sleep;
    pthread_create(&hilo_sleep, NULL, (void *)sleeper, args);
    pthread_detach(hilo_sleep);

    sem_post(&sem_ready);
    if (list_size(lista_ready) > 0)
        sem_post(&sem_exec);
    // bloquear proceso que mando el sleep
    // Cargar semaforos (replanificar) y cargar en lista de ready proceso en ejecucion
}

void sleeper(void *args)
{
    args_sleep *_args = (args_sleep *)args;
    log_warning(kernel_logger_info, "Inicio sleep de %d segundos para proceso: %d,", _args->tiempo, _args->pcb->pid);
    sleep(_args->tiempo);
    log_warning(kernel_logger_info, "Paso proceso: %d a ready", _args->pcb->pid);
    safe_pcb_add(lista_ready, _args->pcb, &mutex_cola_ready);
    sem_post(&sem_ready);
    sem_post(&sem_exec);
}

void kwait()
{
    pcbelegido->recurso_instruccion = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;
    log_info(kernel_logger_info, "ESTOY EN WAIT %s", pcbelegido->recurso_instruccion);

    recurso_instancia *recurso_kernel = (recurso_instancia *)malloc(sizeof(recurso_instancia));
    recurso_kernel = buscar_recurso(recursos_kernel, pcbelegido->recurso_instruccion);

    if (recurso_kernel != NULL)
    {
        log_info(kernel_logger_info, "El recurso [%s] existe en mi lista de recursosKernel", pcbelegido->recurso_instruccion);
        if (recurso_kernel->cantidad <= 0)
        {
            pthread_mutex_lock(&mutex_cola_exec);
            t_pcb *proceso_aux = list_remove(cola_exec, 0);
            pthread_mutex_unlock(&mutex_cola_exec);
            log_info(kernel_logger_info, "No tengo instancias disponibles del recurso: [%s] -instancias %d", pcbelegido->recurso_instruccion, recurso_kernel->cantidad);
            pthread_mutex_lock(&mutex_cola_block);
            pcbelegido->estado = BLOCK;
            queue_push(recurso_kernel->colabloqueado, pcbelegido);
            log_info(kernel_logger_info, "Tamaño de la cola: %d", queue_size(recurso_kernel->colabloqueado));
            // TODO: Revisar colas de bloqueo por recurso
            list_add(cola_block, pcbelegido);
            pthread_mutex_unlock(&mutex_cola_block);
            log_info(kernel_logger_info, "PID[%d] bloqueado por %s \n", pcbelegido->pid, recurso_kernel->nombre);
            proceso_en_ejecucion = NULL;
            sem_post(&sem_ready);
            if (list_size(lista_ready) > 0)
                sem_post(&sem_exec);
        }
        else
        {

            recurso_kernel->cantidad -= 1;
            log_info(kernel_logger_info, "PID: <%d> - Wait: <%s> - Instancias restantes: <%d>", pcbelegido->pid, recurso_kernel->nombre, recurso_kernel->cantidad);
            // TODO: Esto no anda, no encuentra el recurso en la lista de recursos global
            recurso_instancia *recurso_proceso = buscar_recurso(pcbelegido->recursos_asignados, recurso_kernel->nombre);
            if (recurso_proceso == NULL)
                log_info(kernel_logger_info, "NO ENCONTRE EL RECURSO");
            // TODO: revisar si hay que actualizar la cantidad de recursos dentro del pcb
            recurso_proceso->cantidad += 1;
            log_info(kernel_logger_info, "PID[%d] se asigno recurso %s - INSTANCIAS %d en PROCESO", pcbelegido->pid, recurso_proceso->nombre, recurso_proceso->cantidad);
            sem_post(&sem_ready);
            sem_post(&sem_exec);
        }
    }
    else
    {

        pthread_mutex_lock(&mutex_cola_exec);
        t_pcb *proceso_aux = list_remove(cola_exec, 0);
        pthread_mutex_unlock(&mutex_cola_exec);
        log_info(kernel_logger_info, "El recurso [%s] pedido por PID [%d] no existe. Se manda proceso a exit", proceso_aux->recurso_instruccion, proceso_aux->pid);

        // TODO: Hacer funcion que pase de enum a char* para hacer el log de los estados
        log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual  <%s>\n", proceso_aux->pid, "EXEC", "EXIT");

        for (int i = 0; i < list_size(proceso_aux->recursos_asignados); i++)
        {

            recurso_signal = list_get(recursos_kernel, i);
            recurso_proceso = list_get(proceso_aux->recursos_asignados, i);

            if (strcmp(recurso_signal->nombre, recurso_proceso->nombre) == 0)
            {
                log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n", recurso_signal->cantidad);

                recurso_signal->cantidad += recurso_proceso->cantidad;
                log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n", recurso_signal->cantidad);

                recurso_proceso->cantidad -= recurso_proceso->cantidad;
            }
        }
        log_info(kernel_logger_info, "Finaliza el proceso <%d> Motivo <%s> \n", pcbelegido->pid, "INVALID_RESOURCE");

        proceso_en_ejecucion = NULL;
        pthread_mutex_lock(&mutex_cola_exit);
        proceso_aux->estado = FINISH_EXIT;
        list_add(cola_exit, proceso_aux);
        pthread_mutex_unlock(&mutex_cola_exit);
        sem_post(&sem_exit);
        //  sem_post(&sem_ready);
    }
}

void ksignal()
{
    pcbelegido->recurso_instruccion = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;
    log_info(kernel_logger_info, "ESTOY EN SIGNAL %s", pcbelegido->recurso_instruccion);
    recurso_instancia *recurso_signal = (recurso_instancia *)malloc(sizeof(recurso_instancia));
    recurso_signal = buscar_recurso(recursos_kernel, pcbelegido->recurso_instruccion);
    if (recurso_signal != NULL)
    {
        if (tiene_recurso(pcbelegido->recursos_asignados, pcbelegido->recurso_instruccion))
        {
            log_info(kernel_logger_info, "El recurso [%s] existe en mi lista de recursos_kernel y le devuelvo una instancia del PID %d", pcbelegido->recurso_instruccion, pcbelegido->pid);

            recurso_signal->cantidad = recurso_signal->cantidad + 1;
            recurso_instancia *recurso_proceso = buscar_recurso(pcbelegido->recursos_asignados, recurso_signal->nombre);
            log_info(kernel_logger_info, "PID: <%d> - Signal: <%s> - Instancias restantes: <%d>", pcbelegido->pid, recurso_signal->nombre, recurso_signal->cantidad);
            recurso_proceso->cantidad -= 1;

            if (queue_size(recurso_signal->colabloqueado) > 0)
            {
                log_info(kernel_logger_info, "popie proceso bloquedo por %s", recurso_signal->nombre);
                sem_post(&sem_blocked_w);
                remove_blocked(pcbelegido->pid);
                set_pcb_ready(pcbelegido);
            }
            else
            {
                log_info(kernel_logger_info, "NO QUEDAN RECURSOS BLOQUEADOS");
            }
            sem_post(&sem_exec);
        }
        else
        {
            log_info(kernel_logger_info, "El pcb: %d liberó el recurso: %s y no lo tenia asignado", pcbelegido->pid, pcbelegido->recurso_instruccion);
            pthread_mutex_lock(&mutex_cola_exec);
            // No es necesario guardar el proceso aux porq esta en una global el ultimo en ejecucion
            proceso_aux = list_remove(cola_exec, 0);
            pthread_mutex_unlock(&mutex_cola_exec);

            if (ocupa_recursos(proceso_aux->recursos_asignados))
            {
                for (int i = 0; i < list_size(proceso_aux->recursos_asignados); i++)
                {

                    recurso_signal = list_get(recursos_kernel, i);
                    recurso_proceso = list_get(proceso_aux->recursos_asignados, i);

                    if (strcmp(recurso_signal->nombre, recurso_proceso->nombre) == 0)
                    {
                        log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n", recurso_signal->cantidad);

                        recurso_signal->cantidad += recurso_proceso->cantidad;
                        log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n", recurso_signal->cantidad);

                        recurso_proceso->cantidad -= recurso_proceso->cantidad;
                    }
                }
            }
            proceso_en_ejecucion = NULL;
            pthread_mutex_lock(&mutex_cola_exit);
            pcbelegido->estado = FINISH_EXIT;
            list_add(cola_exit, pcbelegido);
            pthread_mutex_unlock(&mutex_cola_exit);
            // TODO: Hacer funcion de enum a char* para hacer el log de los estados
            log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");

            sem_post(&sem_exit);
            sem_post(&sem_ready);
            if (list_size(lista_ready))
                sem_post(&sem_exec);
        }
        // sem_post(&sem_ready);
    }
    else
    {
        pthread_mutex_lock(&mutex_cola_exec);
        // No es necesario guardar el proceso aux porq esta en una global el ultimo en ejecucion
        proceso_aux = list_remove(cola_exec, 0);
        pthread_mutex_unlock(&mutex_cola_exec);
        log_info(kernel_logger_info, "El recurso [%s] pedido por PID [%d] no existe. Se manda proceso a exit", pcbelegido->recurso_instruccion, pcbelegido->pid);

        for (int i = 0; i < list_size(proceso_aux->recursos_asignados); i++)
        {

            recurso_signal = list_get(recursos_kernel, i);
            recurso_proceso = list_get(proceso_aux->recursos_asignados, i);

            if (strcmp(recurso_signal->nombre, recurso_proceso->nombre) == 0)
            {
                log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n", recurso_signal->cantidad);

                recurso_signal->cantidad += recurso_proceso->cantidad;
                log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n", recurso_signal->cantidad);

                recurso_proceso->cantidad -= recurso_proceso->cantidad;
            }
        }
        proceso_en_ejecucion = NULL;
        pthread_mutex_lock(&mutex_cola_exit);
        pcbelegido->estado = FINISH_EXIT;
        list_add(cola_exit, pcbelegido);
        pthread_mutex_unlock(&mutex_cola_exit);
        // TODO: Hacer funcion de enum a char* para hacer el log de los estados
        log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");

        sem_post(&sem_exit);
        sem_post(&sem_ready);
    }
}