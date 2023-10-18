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
    sem_init(&sem_detener, 0, 0);
    sem_init(&sem_blocked_w, 0, 0);
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
            frenado = 0;
            iniciar_planificacion();
            

            //  free(linea);
            // break;
        }

        if (!strncmp(linea, "detener_planificacion", 21))
        {

            // log_info(kernel_logger_info, "FInalice proceso %s ",palabras[1]);
            detener_planificacion();

            // break;
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
    asignar_algoritmo(config_valores_kernel.algoritmo_planificacion);
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
    t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
    interrupcion->motivo_interrupcion = INTERRUPT_FIN_PROCESO;
    interrupcion->pid = pid;
    enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
    t_pcb *proceso_encontrado;
    proceso_encontrado = buscarProceso(pid);
    pthread_mutex_lock(&mutex_cola_exit);
    list_add(cola_exit, proceso_encontrado);
    pthread_mutex_unlock(&mutex_cola_exit);
    cambiar_estado(proceso_encontrado, FINISH_EXIT);
    // list_add(lista_global, proceso_encontrado);
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
        log_info(kernel_logger_info, "Entre al exit pcb");
        t_pcb *pcb = safe_pcb_remove(cola_exit, &mutex_cola_exit);
        log_info(kernel_logger_info, "Saque al proceso ");
        // char *motivo = motivo_exit_to_string(pcb->motivo_exit);
        // log_info(kernel_logger_info, "Le cambie el estado");
        pcb_destroy(pcb);
        /*   for (int i = 0; i < list_size(proceso->recursosAsignados); i++)
           {

               recurso_signal = list_get(recursosKernel, i);
               recursoProceso = list_get(proceso->recursosAsignados, i);

               if (strcmp(recurso_signal->nombre, recursoProceso->nombre) == 0)
               {
                   // log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n",recurso_signal->cantidad);

                   recurso_signal->cantidad += recursoProceso->cantidad;
                   // log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n",recurso_signal->cantidad);

                   recursoProceso->cantidad -= recursoProceso->cantidad;
               }
           }*/
        sem_post(&sem_ready);
        sem_post(&sem_exec);
    }
}
void pcb_destroy(t_pcb *pcb)
{
    log_info(kernel_logger_info, "Entre al destroy ");
    list_destroy(pcb->archivos_abiertos);
    // contexto_destroyer(pcb->contexto_ejecucion);
    free(pcb);
    log_info(kernel_logger_info, "Hice free ");
}
void detener_planificacion()
{
    frenado = 1;
    log_info(kernel_logger_info, "Cambie el frenado ");
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
    pcb->recursos_asignados =cargar_recursos_totales();
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

    pthread_create(&hilo_exit, NULL, (void *)exit_pcb, NULL);
    pthread_create(&hilo_ready, NULL, (void *)ready_pcb, NULL);
    // pthread_create(&hilo_block, NULL, (void *)block, NULL);

    pthread_detach(hilo_exit);
    pthread_detach(hilo_ready);
    // pthread_detach(hilo_block);
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

void quantum_interrupter(void)
{
    t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
    interrupcion->motivo_interrupcion = INTERRUPT_FIN_QUANTUM;
    interrupcion->pid = -1;
    while (1)
    {
        usleep(config_valores_kernel.quantum);
        if (ALGORITMO_PLANIFICACION == RR)
        {
            enviar_interrupcion(conexion_cpu_interrupt, interrupcion);
        }
    }
    free(interrupcion);
}

void sleeper(void *args)
{
    args_sleep *_args = (args_sleep *)args;
    while (1)
    {
        usleep(_args->tiempo);
        set_pcb_ready(_args->pcb);
    }
}

void ready_pcb(void)
{
    while (1)
    {
        sem_wait(&sem_listos_ready);
        if (list_is_empty(cola_listos_para_ready))
        {
            log_info(kernel_logger_info, "No hay procesos para admitir");
            //break;
        }
        else
        {
            t_pcb *pcb = safe_pcb_remove(cola_listos_para_ready, &mutex_cola_listos_para_ready);
            log_info(kernel_logger_info, "Pase a READY el PCB: %d", pcb->pid);
            pthread_mutex_lock(&leer_grado);
            int procesos_ready = list_size(lista_ready);
            int procesos_exec = list_size(cola_exec);
            int procesos_bloqueado = list_size(cola_block);
            // TODO: Cerrar semaforo acá?

            int procesos_activos = procesos_ready + procesos_exec + procesos_bloqueado;

            if (procesos_activos < sem.g_multiprog_ini)
            {

                procesos_activos = procesos_activos + 1;
                pthread_mutex_unlock(&leer_grado);
                // log_info(kernel_logger_info, "Voy a pasar al ready %d", pcb->pid);
                set_pcb_ready(pcb);
                if (ALGORITMO_PLANIFICACION == PRIORIDADES)
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
                 if (frenado!=1){
                sem_post(&sem_ready);
                 } 
                 //else {
                // sem_wait(&sem_detener);
                // log_info(kernel_logger_info,"Me frene");
                //   }
            }
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
        if(list_size(lista_ready)<1){
            log_info(kernel_logger_info, "LIsta ready vacia");
            continue;
        }
        if (proceso_en_ejecucion== NULL){
            pcbelegido = elegir_pcb_segun_algoritmo();
            prceso_admitido(pcbelegido);
        } else {
            pcbelegido=proceso_en_ejecucion;
        }
        enviar_contexto(conexion_cpu_dispatch, pcbelegido->contexto_ejecucion);
        log_info(kernel_logger_info, "Envie PID %d con PC %d a CPU", pcbelegido->pid, pcbelegido->contexto_ejecucion->program_counter);
        //proceso_en_ejecucion = pcbelegido;

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

        int codigo_instruccion = pcbelegido->contexto_ejecucion->codigo_ultima_instru;
        log_info(kernel_logger_info, "Volvio PID %d con codigo inst %d ", ultimo_contexto->pid, pcbelegido->contexto_ejecucion->codigo_ultima_instru);

        proceso_en_ejecucion = pcbelegido;
        // TODO: Guardar pcb en una lista segun el tipo de desalojo (poner dentro de los switches)
        switch (codigo_instruccion)
        {

        case EXIT:
            log_info(kernel_logger_info, "Entre al exit");
            proceso_en_ejecucion= NULL;
            pthread_mutex_lock(&mutex_cola_exit);
            pcbelegido->estado = FINISH_EXIT;
            list_add(cola_exit, pcbelegido);
            pthread_mutex_unlock(&mutex_cola_exit);
            sem_post(&sem_exit);
            break;
        case SLEEP:
            set_pcb_block(pcbelegido);
            args_sleep args;
            args.pcb = pcbelegido;
            args.tiempo = atoi(pcbelegido->contexto_ejecucion->instruccion_ejecutada->parametro1);
            pthread_t hilo_sleep;
            pthread_create(&hilo_sleep, NULL, (void *)sleeper, &args);
            pthread_detach(hilo_sleep);
            // bloquear proceso que mando el sleep
            break;
        case WAIT:
            pcbelegido->recurso_instruccion = ultimo_contexto->instruccion_ejecutada->parametro1;
            log_info(kernel_logger_info, "ESTOY EN WAIT %s", pcbelegido->recurso_instruccion);

            recurso_instancia *recurso_kernel = (recurso_instancia *)malloc(sizeof(recurso_instancia));
            recurso_kernel = buscar_recurso_w(recursos_kernel, pcbelegido->recurso_instruccion);

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
                    proceso_aux->estado = BLOCK;
                    queue_push(recurso_kernel->colabloqueado, proceso_aux);
                    // TODO: Revisar colas de bloqueo por recurso
                    list_add(cola_block, proceso_aux);
                    pthread_mutex_unlock(&mutex_cola_block);
                    log_info(kernel_logger_info, "PID[%d] bloqueado por %s \n", pcbelegido->pid, recurso_kernel->nombre);
                    sem_post(&sem_ready);
                }
                else
                {

                    recurso_kernel->cantidad -= 1;
                    log_info(kernel_logger_info, "PID: <%d> - Wait: <%s> - Instancias restantes: <%d>", pcbelegido->pid, recurso_kernel->nombre, recurso_kernel->cantidad);
                    //TODO: Esto no anda, no encuentra el recurso en la lista de recursos global
                    recurso_instancia *recurso_proceso = buscar_recurso_w(pcbelegido->recursos_asignados, recurso_kernel->nombre);
                    if(recurso_proceso == NULL) log_info(kernel_logger_info, "NO ENCONTRE EL RECURSO");
                    //TODO: revisar si hay que actualizar la cantidad de recursos dentro del pcb
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
                pthread_mutex_lock(&mutex_cola_exit);
                proceso_aux->estado = FINISH_EXIT;
                list_add(cola_exit, proceso_aux);
                pthread_mutex_unlock(&mutex_cola_exit);

                //TODO: Hacer funcion que pase de enum a char* para hacer el log de los estados
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

                sem_post(&sem_exit);
                sem_post(&sem_ready);
            }
            break;
        case SIGNAL:
            pcbelegido->recurso_instruccion = ultimo_contexto->instruccion_ejecutada->parametro1;
            log_info(kernel_logger_info, "ESTOY EN SIGNAL %s", pcbelegido->recurso_instruccion);
            recurso_instancia *recurso_signal = (recurso_instancia *)malloc(sizeof(recurso_instancia));
            recurso_signal = buscar_recurso_w(recursos_kernel, pcbelegido->recurso_instruccion);
            if (recurso_signal != NULL)
            {
                log_info(kernel_logger_info, "El recurso [%s] existe en mi lista de recursos_kernel y le devuelvo una instancia del PID %d", pcbelegido->recurso_instruccion, pcbelegido->pid);

                recurso_signal->cantidad = recurso_signal->cantidad + 1;
                recurso_instancia *recurso_proceso = buscar_recurso_w(pcbelegido->recursos_asignados, recurso_signal->nombre);
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
                sem_post(&sem_ready);
            }
            else
            {
                pthread_mutex_lock(&mutex_cola_exec);
                //No es necesario guardar el proceso aux porq esta en una global el ultimo en ejecucion
                proceso_aux = list_remove(cola_exec, 0);
                pthread_mutex_unlock(&mutex_cola_exec);
                log_info(kernel_logger_info, "El recurso [%s] pedido por PID [%d] no existe. Se manda proceso a exit", pcbelegido->recurso_instruccion, pcbelegido->pid);

                for (int i = 0; i < list_size(proceso_aux->recursos_asignados); i++)
                 {

                     recurso_signal = list_get(recursos_kernel, i);
                     recurso_proceso = list_get(proceso_aux->recursos_asignados, i);

                     if (strcmp(recurso_signal->nombre, recurso_proceso->nombre) == 0)
                     {
                        log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] antes  \n",recurso_signal->cantidad);

                        recurso_signal->cantidad += recurso_proceso->cantidad;
                        log_info(kernel_logger_info, "LIBERE RECURSO de proceso[%d] despues \n",recurso_signal->cantidad);

                        recurso_proceso->cantidad -= recurso_proceso->cantidad;
                     }
                 } 
                pthread_mutex_lock(&mutex_cola_exit);
                pcbelegido->estado = FINISH_EXIT;
                list_add(cola_exit, proceso_aux);
                pthread_mutex_unlock(&mutex_cola_exit);
                //TODO: Hacer funcion de enum a char* para hacer el log de los estados
                log_info(kernel_logger_info, "PID[%d] Estado Anterior: <%s> Estado Actual:<%s>  \n", pcbelegido->pid, "EXEC", "EXIT");

                sem_post(&sem_exit);
                sem_post(&sem_ready);
            }
            break;

        default:
            log_info(kernel_logger_info, "Entre al default");

            break;
        }
    }

    //     sem_post(&sem_exec);
    // TODO: Chequear estos free
    // free(pcb);
    // free(ultimo_contexto);
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

void prceso_admitido(t_pcb *pcb)
{
    cambiar_estado(pcb, EXEC);
    safe_pcb_add(cola_exec, pcb, &mutex_cola_exec);
    //sem_post(&sem_exec);
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
    pthread_mutex_lock(&mutex_cola_block);
    list_remove_element(cola_block, proceso_elegido);
    pthread_mutex_unlock(&mutex_cola_block);
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

/*int comparar(const void *a, const void *b) {
    return ((MiStruct*)a)->enum_field - ((MiStruct*)b)->enum_field;
}*/

bool maximo_RR(t_pcb *pcb1, t_pcb *pcb2)
{
    return pcb1->tiempo_ingreso_ready <= pcb2->tiempo_ingreso_ready;
}

bool maximo_PRIORIDAD(t_pcb *pcb1, t_pcb *pcb2)
{
    return pcb1->prioridad <= pcb2->prioridad;
}

recurso_instancia *buscar_recurso_w(t_list *lista_recursos, char *nombre_recurso)
{
    bool _recursoPorNombre(void *elemento)
    {
        return strcmp(((recurso_instancia *)elemento)->nombre, nombre_recurso) == 0;
    }
    recurso_instancia *recurso_existente;
    recurso_existente = list_find(lista_recursos, _recursoPorNombre);
    return recurso_existente;
}