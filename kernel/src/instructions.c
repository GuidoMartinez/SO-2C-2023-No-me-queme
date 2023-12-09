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
    //log_info(kernel_logger_info, "ENTRE AL SLEEP");
    pthread_t hilo_sleep;
    pthread_create(&hilo_sleep, NULL, (void *)sleeper, args);
    pthread_detach(hilo_sleep);

    if (frenado)
    {
        sem_wait(&sem_detener);
    }
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
    if (frenado)
    {
        sem_wait(&sem_detener_sleep);
    }
    safe_pcb_add(lista_ready, _args->pcb, &mutex_cola_ready);
    sem_post(&sem_ready);
    sem_post(&sem_exec);
}

void kwait()
{
    pcbelegido->recurso_instruccion = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;
    //log_info(kernel_logger_info, "ESTOY EN WAIT %s", pcbelegido->recurso_instruccion);

    recurso_instancia *recurso_kernel = (recurso_instancia *)malloc(sizeof(recurso_instancia));
    recurso_kernel = buscar_recurso(recursos_kernel, pcbelegido->recurso_instruccion);

    if (recurso_kernel != NULL)
    {
        log_info(kernel_logger_info, "El recurso [%s] existe en mi lista de recursosKernel", pcbelegido->recurso_instruccion);
        if (recurso_kernel->cantidad <= 0)
        {
            pthread_mutex_lock(&mutex_cola_exec);
            list_remove(cola_exec, 0);
            pthread_mutex_unlock(&mutex_cola_exec);

            log_info(kernel_logger_info, "No tengo instancias disponibles del recurso: [%s] -instancias %d", pcbelegido->recurso_instruccion, recurso_kernel->cantidad);

            if (frenado)
            {
                sem_wait(&sem_detener);
            }

            pthread_mutex_lock(&mutex_cola_block);

            pcbelegido->estado = BLOCK;
            queue_push(recurso_kernel->colabloqueado, pcbelegido);
            //log_info(kernel_logger_info, "Tamaño de la cola: %d", queue_size(recurso_kernel->colabloqueado));
            // TODO: Revisar colas de bloqueo por recurso
            list_add(cola_block, pcbelegido);

            pthread_mutex_unlock(&mutex_cola_block);

            log_info(kernel_logger_info, "PID[%d] bloqueado por %s \n", pcbelegido->pid, recurso_kernel->nombre);

            log_info(kernel_logger_info, "ANÁLISIS DE DETECCIÓN DE DEADLOCK");
            if (queue_size(recurso_kernel->colabloqueado) > 1)
            {
                hay_deadlock(pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);
            }

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
   // log_info(kernel_logger_info, "ESTOY EN SIGNAL %s", pcbelegido->recurso_instruccion);
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
                t_pcb *proceso_desbloqueado = queue_pop(recurso_signal->colabloqueado);
                recurso_instancia *recurso_proceso = buscar_recurso(proceso_desbloqueado->recursos_asignados, recurso_signal->nombre);
                recurso_proceso->cantidad += 1;
                sem_post(&sem_blocked_w);
                remove_blocked(proceso_desbloqueado->pid);
                set_pcb_ready(proceso_desbloqueado);
            }
            else
            {
                log_info(kernel_logger_info, "NO QUEDAN RECURSOS BLOQUEADOS");
            }
            sem_post(&sem_ready);
            sem_post(&sem_exec);
        }
        else
        {
            log_info(kernel_logger_info, "El pcb: %d liberó el recurso: %s y no lo tenia asignado", pcbelegido->pid, pcbelegido->recurso_instruccion);

            finalizar_proceso_en_ejecucion();

            // TODO: Hacer funcion de enum a char* para hacer el log de los estados
            log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");

            sem_post(&sem_exit);
            sem_post(&sem_ready);
            if (list_size(lista_ready) > 0)
                sem_post(&sem_exec);
        }
        // sem_post(&sem_ready);
    }
    else
    {
        log_info(kernel_logger_info, "El recurso [%s] pedido por PID [%d] no existe. Se manda proceso a exit", pcbelegido->recurso_instruccion, pcbelegido->pid);

        // TODO: Hacer funcion de enum a char* para hacer el log de los estados
        log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");
        log_info(kernel_logger_info, "Finaliza el proceso <%d> Motivo <%s> \n", pcbelegido->pid, "INVALID_RESOURCE");

        proceso_en_ejecucion = NULL;

        pthread_mutex_lock(&mutex_cola_exit);

        pcbelegido->estado = FINISH_EXIT;
        list_add(cola_exit, pcbelegido);

        pthread_mutex_unlock(&mutex_cola_exit);

        sem_post(&sem_exit);
    }
}

void kf_open()
{

    //log_info(kernel_logger_info, "ESTOY EN F_OPEN");

    char *nombre_archivo = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;
    char lock = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro2[0];

    log_info(kernel_logger_info, "PID[%d] Abrir Archivo %s", pcbelegido->pid, nombre_archivo);
    t_archivo_global *archivo_global_pedido = buscarArchivoGlobal(lista_archivos_abiertos, nombre_archivo);

    if (archivo_global_pedido == NULL)
    {
        // Mnadar a filesystem si existe el archivo
        t_instruccion_fs *inst_f_open_fs = inicializar_instruccion_fs(pcbelegido->contexto_ejecucion->instruccion_ejecutada, 1);

        enviarInstruccionFS(conexion_filesystem, inst_f_open_fs);

        sem_post(&sem_hilo_FS);
        sem_wait(&reanudaar_exec);

        if (open_file(nombre_archivo, lock) == -1)
        {
            log_warning(kernel_logger_info, "Se creo archivo");
            crear_archivo_global(nombre_archivo, lock);
        }

        crear_archivo_proceso(nombre_archivo, pcbelegido);
        log_warning(kernel_logger_info, "Hice open file");
    }
    else
    {
        if (archivo_global_pedido->lock == 'W' || lock == 'W')
        {
            log_warning(kernel_logger_info, "Tenia lock de escritura");
            queue_push(archivo_global_pedido->colabloqueado, pcbelegido);
            exec_block_fs();

            pthread_mutex_lock(&mutex_cola_block);
            pcbelegido->motivo_block = ARCHIVO_BLOCK;
            pthread_mutex_unlock(&mutex_cola_block);

            log_info(kernel_logger_info, "PID[%d] bloqueado por %s \n", pcbelegido->pid, archivo_global_pedido->nombreArchivo);
            sem_post(&sem_ready);
            if (list_size(lista_ready) > 0)
            sem_post(&sem_exec);
            return;
        }

        else if (lock == 'R')
        {
            log_warning(kernel_logger_info, "Tenia lock de lectura");
            archivo_global_pedido->contador++;
        }
    }
    sem_post(&sem_ready);
    sem_post(&sem_exec);
}

void kf_close()
{

    //log_info(kernel_logger_info, "ESTOY EN F_CLOSE");

    char *nombre_archivo = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;

    log_info(kernel_logger_info, "PID[%d] Cerrar Archivo %s", pcbelegido->pid, nombre_archivo);

    t_archivo_global *archivo_global_pedido = buscarArchivoGlobal(lista_archivos_abiertos, nombre_archivo);
    t_archivo_abierto_proceso *archivo_proceso = buscar_archivo_proceso(proceso_en_ejecucion->archivos_abiertos, nombre_archivo);

    if (archivo_global_pedido == NULL)
    {
        log_warning(kernel_logger_info, "El archivo global no estaba abierto");
    }
    else
    {
        log_error(kernel_logger_info, "%c", archivo_global_pedido->lock);
    }

    if (archivo_proceso == NULL)
    {
        log_warning(kernel_logger_info, "El archivo proceso no estaba abierto");
    }

    if (archivo_global_pedido != NULL && archivo_proceso != NULL)
    {

        if (archivo_global_pedido->lock == 'R')
        {
            archivo_global_pedido->contador--;
        }
        if (archivo_global_pedido->lock == 'W' || archivo_global_pedido->contador == 0)
        {
            log_error(kernel_logger_info, "TENGO QUE DESBLOQUEAR SI HAY BLOQUEADOS");
            archivo_global_pedido->lock = 'N';
            archivo_global_pedido->contador = 0;
        }
        if(archivo_global_pedido->lock != 'R' && archivo_global_pedido->lock != 'W')
        {
            log_info(kernel_logger_info, "El archivo no tenia lock (no estaba abierto)");
        }
    }
    else
    {
        log_info(kernel_logger_info, "El archivo no estaba abierto");
    }

    list_remove_element(proceso_en_ejecucion->archivos_abiertos, archivo_proceso);
    // Mutex! capaz no? solo un proceso en ejecucion...
    bool continuar = true;
    bool crear = true;
    list_remove_element(lista_archivos_abiertos, archivo_global_pedido);
    t_queue *cola_temporal_de_escritura = queue_create();

    //log_warning(kernel_logger_info, "cola d bloqueados: %d", queue_size(archivo_global_pedido->colabloqueado));

    if (archivo_global_pedido->lock == 'N' && queue_size(archivo_global_pedido->colabloqueado) > 0)
    {
        while (continuar)
        {
            t_pcb *pcb_desbloqueado = queue_pop(archivo_global_pedido->colabloqueado);
            log_warning(kernel_logger_info, "PID[%d] Desbloqueado por %s \n", pcb_desbloqueado->pid, archivo_global_pedido->nombreArchivo);

            char lock = pcb_desbloqueado->contexto_ejecucion->instruccion_ejecutada->parametro2[0];
            if (lock == 'W' && !crear)
            {
                queue_push(cola_temporal_de_escritura, pcb_desbloqueado);
                if (queue_size(archivo_global_pedido->colabloqueado) == 0)
                    continuar = false;
                continue;
            }
            log_warning(kernel_logger_info, "Lock %c \n", lock);

            // Se abre el archivo para ese proceso
            crear_archivo_proceso(nombre_archivo, pcb_desbloqueado);
            open_file(nombre_archivo, lock);

            if (crear)
            {
                crear_archivo_global_con_contador(nombre_archivo, lock, archivo_global_pedido->contador + 1);
                crear = false;
            }
            else
            {
                sumar_contador_archivo_global(nombre_archivo);
            }

            set_pcb_ready(pcb_desbloqueado);
            if (queue_size(archivo_global_pedido->colabloqueado) == 0)
                continuar = false;
        }

        bool agregar = false;

        while (queue_size(cola_temporal_de_escritura) > 0)
        {
            agregar = true;
            list_remove_element(lista_archivos_abiertos, archivo_global_pedido);
           // log_error(kernel_logger_info, "VOY A METER UN PROCESO CON W A LA COLA D BLOQUAEDOS DEL ARCHIVO");
            t_pcb *pcb_desbloqueado = queue_pop(cola_temporal_de_escritura);
            queue_push(archivo_global_pedido->colabloqueado, pcb_desbloqueado);
        }

        if(agregar){
            archivo_global_pedido->lock = 'W';
            list_add(lista_archivos_abiertos, archivo_global_pedido);
        }
      //  log_error(kernel_logger_info, "TAMAÑO DE LA COLA DE BLOQUEADOS %d", queue_size(archivo_global_pedido->colabloqueado));
    }

    else if (archivo_global_pedido->contador > 0)
    {
        crear_archivo_global_con_contador(nombre_archivo, 'R', archivo_global_pedido->contador + 1);
    }

    /*free(archivo_global_pedido->nombreArchivo);
    free(archivo_global_pedido);*/

   // log_warning(kernel_logger_info, "termino fclose");
   // log_warning(kernel_logger_info, "tamaño de la tabla de archivos abiertos %d", list_size(lista_archivos_abiertos));
    sem_post(&sem_ready);
    sem_post(&sem_exec);
};

void kf_seek()
{

    char *nombre_archivo = proceso_en_ejecucion->contexto_ejecucion->instruccion_ejecutada->parametro1;
    int nueva_ubicacion_puntero = atoi(proceso_en_ejecucion->contexto_ejecucion->instruccion_ejecutada->parametro2);
    log_info(kernel_logger_info, "PID[%d] Actualizar puntero Archivo %s - Puntero %d\n", proceso_en_ejecucion->pid, nombre_archivo, nueva_ubicacion_puntero);
    if(proceso_en_ejecucion->archivos_abiertos == NULL) {
        log_error(kernel_logger_info, "El proceso no tiene archivos abiertos y deberia");
    }
    t_list *lista_archivos = proceso_en_ejecucion->archivos_abiertos;

    t_archivo_abierto_proceso *archivo_proceso = buscar_archivo_proceso(lista_archivos, nombre_archivo);
    if(archivo_proceso == NULL) {
        log_error(kernel_logger_info, "El proceso en archivo no se encuentra");
    }
    archivo_proceso->puntero = nueva_ubicacion_puntero;


    //log_error(kernel_logger_info, "Termino fseek");
    sem_post(&sem_ready);
    sem_post(&sem_exec);
};

void kf_read()
{

    t_archivo_abierto_proceso *archivo_proceso = buscar_archivo_proceso(proceso_en_ejecucion->archivos_abiertos, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);

    log_info(kernel_logger_info, "PID[%d] - Leer Archivo %s - Direccion memoria %s \n", proceso_en_ejecucion->pid, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro2);

    t_instruccion_fs *inst_f_read_fs = inicializar_instruccion_fs(pcbelegido->contexto_ejecucion->instruccion_ejecutada, archivo_proceso->puntero);
    enviarInstruccionFS(conexion_filesystem, inst_f_read_fs);
    fs_interaction();
};

void kf_write()
{
    char *nombre_archivo = proceso_en_ejecucion->contexto_ejecucion->instruccion_ejecutada->parametro1;

    log_info(kernel_logger_info, "PID[%d] - Escribir Archivo %s - Direccion memoria %s \n", proceso_en_ejecucion->pid, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro2);

    t_archivo_global *archivo_global_pedido = buscarArchivoGlobal(lista_archivos_abiertos, nombre_archivo);
    t_archivo_abierto_proceso *archivo_proceso = buscar_archivo_proceso(proceso_en_ejecucion->archivos_abiertos, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);

    if (archivo_global_pedido->lock == 'W')
    {
        t_instruccion_fs *inst_f_open_fs = inicializar_instruccion_fs(pcbelegido->contexto_ejecucion->instruccion_ejecutada, archivo_proceso->puntero);
        enviarInstruccionFS(conexion_filesystem, inst_f_open_fs);
        fs_interaction();
    }
    else
    {
        archivo_global_pedido->contador--;
        finalizar_proceso_en_ejecucion();
        log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");
    }
};

void kf_truncate()
{

    char *nombre_archivo = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1;
    char *tamanio = pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro2;
   // log_warning(kernel_logger_info, "La lista de proceso en ejecucion tiene %d elementos", list_size(proceso_en_ejecucion->archivos_abiertos));
    //log_warning(kernel_logger_info, "La lista de pcbelegidos tiene %d elementos", list_size(pcbelegido->archivos_abiertos));
    t_archivo_abierto_proceso *archivo_proceso = buscar_archivo_proceso(proceso_en_ejecucion->archivos_abiertos, pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);

    log_info(kernel_logger_info, "PID[%d] - Archivo %s - TAMAÑO %s", pcbelegido->pid, nombre_archivo, tamanio);
    if (archivo_proceso != NULL)
    {
       // log_warning(kernel_logger_info, "El archivo proceso no es NULL");
    }
   // log_warning(kernel_logger_info, "Voy a inicializar instruccion fs, valor del puntero %s", archivo_proceso->nombreArchivo);
    t_instruccion_fs *inst_f_open_fs = inicializar_instruccion_fs(pcbelegido->contexto_ejecucion->instruccion_ejecutada, archivo_proceso->puntero);
    //log_warning(kernel_logger_info, "El inicialice intruccion fs");
    enviarInstruccionFS(conexion_filesystem, inst_f_open_fs);
    fs_interaction();
};