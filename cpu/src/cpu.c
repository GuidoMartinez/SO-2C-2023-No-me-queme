#include "cpu.h"
#include "instructions.h"

t_contexto_ejecucion *contexto_actual;

t_log *cpu_logger_info;
t_config *config;

uint32_t tamano_pagina;
bool page_fault = false;

// interrupciones
bool interrupciones[3] = {0, 0, 0};
bool descartar_instruccion = false;

// conexiones
int socket_memoria, servidor_cpu_dispatch,
    servidor_cpu_interrupt, conexion_kernel_dispatch,
    conexion_kernel_interrupt;
op_code codigo_operacion;

pthread_t hiloInterrupt;
pthread_mutex_t mutex_interrupt;

arch_config config_valores_cpu;

void sighandler(int s)
{
    finalizar_cpu();
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    if (argc < 1)
    {
        // para ejecutar el cpu es: ./cpu ./cfg/cpu.config
        printf("No se ingresaron los parametros!! \n");
        return EXIT_FAILURE;
    }
    cpu_logger_info = log_create("./cfg/cpu.log", "CPU", true, LOG_LEVEL_INFO);
    cargar_configuracion(argv[1]);

    pthread_mutex_init(&mutex_interrupt, NULL);

    iniciar_conexiones();
    // cambiar a uno para recibir la operacion de kernel
    while (1)
    {

        codigo_operacion = recibir_operacion(conexion_kernel_dispatch);

        switch (codigo_operacion)
        {
        case CONTEXTO:
            log_info(cpu_logger_info, "Esperando contexto: ...");
            contexto_actual = recibir_contexto(conexion_kernel_dispatch);
            log_info(cpu_logger_info, "Recibo pid%d y pc%d", contexto_actual->pid, contexto_actual->program_counter);
            contexto_actual->codigo_ultima_instru = -1;

            while (!es_syscall() && !hay_interrupciones() && !page_fault)
            {
                log_warning(cpu_logger_info, "Ejecuto proxima instruccion");
                ejecutar_ciclo_instruccion();
            }
            log_info(cpu_logger_info, "Ultima instruccion: %s", obtener_nombre_instruccion(contexto_actual->codigo_ultima_instru));
            log_info(cpu_logger_info, "PC actual: %d", contexto_actual->program_counter);
            obtener_motivo_desalojo();
            enviar_contexto(conexion_kernel_dispatch, contexto_actual);

            pthread_mutex_lock(&mutex_interrupt);
            limpiar_interrupciones();
            pthread_mutex_unlock(&mutex_interrupt);

            break;
        default:
            log_error(cpu_logger_info, "El codigo que me llego es %d", codigo_operacion);
            log_warning(cpu_logger_info, "Operacion desconocida \n");
            finalizar_cpu();
            abort();
            break;
        }
    }

    return EXIT_SUCCESS;
}

void iniciar_conexiones()
{
    conectar_memoria();
    cargar_servidor(&servidor_cpu_dispatch, config_valores_cpu.puerto_escucha_dispatch, &conexion_kernel_dispatch, HANDSHAKE_CPU_DISPATCH, "DISPATCH");
    cargar_servidor(&servidor_cpu_interrupt, config_valores_cpu.puerto_escucha_interrupt, &conexion_kernel_interrupt, HANDSHAKE_CPU_INTERRUPT, "INTERRUPT");
    pthread_create(&hiloInterrupt, NULL, recibir_interrupt, NULL);
    pthread_detach(hiloInterrupt);
}

void *recibir_interrupt(void *arg)
{
    while (1)
    {
        codigo_operacion = recibir_operacion(conexion_kernel_interrupt);
        if (codigo_operacion != INTERRUPCION)
        {
            log_error(cpu_logger_info, "El codigo que me llego es %d", codigo_operacion);
            log_warning(cpu_logger_info, "Operacion desconocida \n");
            finalizar_cpu();
            abort();
        }
        t_interrupcion *interrupcion = malloc(sizeof(t_interrupcion));
        interrupcion = recibir_interrupcion(conexion_kernel_interrupt);
        switch (interrupcion->motivo_interrupcion)
        {
        case INTERRUPT_FIN_QUANTUM:
            log_info(cpu_logger_info, "Recibo fin de quantum");
            interrupciones[INTERRUPT_FIN_QUANTUM] = 1;
            pthread_mutex_lock(&mutex_interrupt);
            contexto_actual->motivo_desalojado = INTERRUPT_FIN_QUANTUM;
            pthread_mutex_unlock(&mutex_interrupt);
            break;
        case INTERRUPT_FIN_PROCESO:
            log_info(cpu_logger_info, "Recibo fin de proceso");
            if (!descartar_instruccion)
            {
                interrupciones[INTERRUPT_FIN_PROCESO] = 1;
                pthread_mutex_lock(&mutex_interrupt);
                contexto_actual->motivo_desalojado = INTERRUPT_FIN_PROCESO;
                pthread_mutex_unlock(&mutex_interrupt);
            }
            break;
        case INTERRUPT_NUEVO_PROCESO:
            log_info(cpu_logger_info, "Llego proceso con mayor prioridad");
            interrupciones[INTERRUPT_NUEVO_PROCESO] = 1;
            pthread_mutex_lock(&mutex_interrupt);
            contexto_actual->motivo_desalojado = INTERRUPT_NUEVO_PROCESO;
            pthread_mutex_unlock(&mutex_interrupt);
            break;
        default:
            log_warning(cpu_logger_info, "Operacion desconocida \n");
            finalizar_cpu();
            abort();
            break;
        }
    }

    // free(interrupcion);
    return NULL;
}

bool hay_interrupciones()
{
    if (interrupciones[INTERRUPT_FIN_QUANTUM] || interrupciones[INTERRUPT_FIN_PROCESO] || interrupciones[INTERRUPT_NUEVO_PROCESO])
    {
        return true;
    }
    else
        return false;
}

void obtener_motivo_desalojo()
{
    if (interrupciones[INTERRUPT_FIN_PROCESO])
        contexto_actual->motivo_desalojado = INTERRUPT_FIN_PROCESO;
    if (interrupciones[INTERRUPT_FIN_QUANTUM])
        contexto_actual->motivo_desalojado = INTERRUPT_FIN_QUANTUM;
    if (interrupciones[INTERRUPT_NUEVO_PROCESO])
        contexto_actual->motivo_desalojado = INTERRUPT_NUEVO_PROCESO;
    if (page_fault)
        contexto_actual->motivo_desalojado = PAGE_FAULT;
}

bool descartar_interrupcion(int pid)
{
    if (pid != contexto_actual->pid)
        return true;
    else
        return false;
}

void limpiar_interrupciones()
{
    interrupciones[INTERRUPT_FIN_QUANTUM] = 0;
    interrupciones[INTERRUPT_FIN_PROCESO] = 0;
    interrupciones[INTERRUPT_NUEVO_PROCESO] = 0;
    page_fault = false;
}

void conectar_memoria()
{
    socket_memoria = crear_conexion(config_valores_cpu.ip_memoria, config_valores_cpu.puerto_memoria);

    realizar_handshake(socket_memoria, HANDSHAKE_CPU, cpu_logger_info);

    op_code respuesta = recibir_operacion(socket_memoria);
    if (respuesta == HANDSHAKE_MEMORIA)
    {
        op_code prueba = recibir_handshake(socket_memoria, cpu_logger_info);
        log_info(cpu_logger_info, "Deserialice el codigo de operacion %d", prueba);
        log_info(cpu_logger_info, "HANDSHAKE EXITOSO CON MEMORIA");
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }
    receive_page_size(socket_memoria);
    log_info(cpu_logger_info, "El tamano de pagina recibido de memoria es %d ", tamano_pagina);
}

void receive_page_size(int socket)
{
    op_code codigo_op = recibir_operacion(socket);

    if (codigo_op == TAMANO_PAGINA)
    {
        int size;
        void *buffer = recibir_buffer(&size, socket);
        memcpy(&tamano_pagina, buffer, sizeof(uint32_t));
        free(buffer);
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la respuesta de la memoria.");
    }
}

void cargar_servidor(int *servidor, char *puerto_escucha, int *conexion, op_code handshake, char *nombre_servidor)
{
    *(servidor) = iniciar_servidor(cpu_logger_info, config_valores_cpu.ip_escucha, puerto_escucha);
    *(conexion) = esperar_cliente(*(servidor), cpu_logger_info);
    codigo_operacion = recibir_operacion(*(conexion));
    if (codigo_operacion == handshake)
    {
        log_info(cpu_logger_info, "Handshake exitoso con KERNEL para la conexion %s", nombre_servidor);
        recibir_handshake(*(conexion), cpu_logger_info);
        realizar_handshake(*(conexion), handshake, cpu_logger_info);
    }
    else
    {
        log_error(cpu_logger_info, "Fallo la comunicacion. Abortando \n");
        finalizar_cpu();
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
    config_valores_cpu.ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    config_valores_cpu.puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    config_valores_cpu.puerto_escucha_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    config_valores_cpu.puerto_escucha_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");
    config_valores_cpu.ip_escucha = config_get_string_value(config, "IP_ESCUCHA");
}

void ejecutar_ciclo_instruccion()
{
    t_instruccion *instruccion = fetch(contexto_actual->pid, contexto_actual->program_counter);
    decode(instruccion);
    if (!page_fault)
        contexto_actual->program_counter++; // TODO -- chequear que en los casos de instruccion con memoria logica puede dar PAGE FAULT y no hay que aumentar el pc (restarlo dentro del decode en esos casos)
}

// Pide a memoria la siguiente instruccion a ejecutar
// Recibe por parametro el pid, el pc de la instruccion a pedir
t_instruccion *fetch(int pid, int pc)
{
    ask_instruccion_pid_pc(pid, pc, socket_memoria);

    op_code codigo_op = recibir_operacion(socket_memoria);

    t_instruccion *instruccion;

    if (codigo_op == INSTRUCCION)
    {
        instruccion = deserializar_instruccion(socket_memoria);
        contexto_actual->instruccion_ejecutada = instruccion;
    }
    else
    {
        log_warning(cpu_logger_info, "Operación desconocida. No se pudo recibir la instruccion de memoria.");
        abort();
    }

    log_info(cpu_logger_info, "PID: %d - FETCH - Program Counter: %d", pid, pc);

    return instruccion;
}

// Ejecuta instrucciones
void decode(t_instruccion *instruccion)
{
    char* param1 = string_new();
    char* param2 = string_new();
    if(instruccion->parametro1 != NULL) {strcpy(param1, instruccion->parametro1);}
    if(instruccion->parametro2 != NULL) {strcpy(param2, instruccion->parametro2);}
    log_info(cpu_logger_info, "PID: %d - DECODE - Instruccion: %s - %s - %s", contexto_actual->pid, cod_inst_to_str(instruccion->codigo), param1, param2);

    switch (instruccion->codigo)
    {
    case SET:
        _set(instruccion->parametro1, instruccion->parametro2);
        break;
    case SUM:
        _sum(instruccion->parametro1, instruccion->parametro2);
        break;
    case SUB:
        _sub(instruccion->parametro1, instruccion->parametro2);
        break;
    case JNZ:
        _jnz(instruccion->parametro1, instruccion->parametro2);
        break;
    case SLEEP:
        _sleep();
        break;
    case WAIT:
        _wait();
        break;
    case SIGNAL:
        _signal();
        break;
    case MOV_IN:
        _mov_in(instruccion->parametro1, instruccion->parametro2);
        break;
    case MOV_OUT:
        _mov_out(instruccion->parametro1, instruccion->parametro2);
        break;
    case F_OPEN:
        _f_open(instruccion->parametro1, instruccion->parametro2);
        break;
    case F_CLOSE:
        _f_close(instruccion->parametro1);
        break;
    case F_SEEK:
        _f_seek(instruccion->parametro1, instruccion->parametro2);
    case F_READ:
        _f_read(instruccion->parametro1, instruccion->parametro2);
        break;
    case F_WRITE:
        _f_write(instruccion->parametro1, instruccion->parametro2);
        break;
    case F_TRUNCATE:
        _f_truncate(instruccion->parametro1, instruccion->parametro2);
        break;
    case EXIT:
        __exit(contexto_actual);
        break;
    default:
        log_error(cpu_logger_info, "Instruccion no reconocida");
        break;
    }
}

bool es_syscall()
{
    nombre_instruccion ultima_instruccion = contexto_actual->codigo_ultima_instru;
    switch (ultima_instruccion)
    {
    case WAIT:
    case SIGNAL:
    case SLEEP:
    case F_OPEN:
    case F_CLOSE:
    case F_READ:
    case F_WRITE:
    case F_SEEK:
    case F_TRUNCATE:
    case EXIT:
        return true;
    default:
        return false;
    }
}

uint32_t obtener_valor_dir(uint32_t dl){

    int df = traducir_dl(dl);
    int valor = 0;

    if(df == -1) {return df;}

    enviar_op_con_int(socket_memoria, MOV_IN_CPU, df);

    op_code codigo_operacion = recibir_operacion(socket_memoria);

    uint32_t resultado = recibir_valor_memoria(socket_memoria);

    if (codigo_operacion == (op_code)MOV_IN_CPU) {valor = resultado;}
    else {log_error(cpu_logger_info, "Error al hacer el MOV_IN");}

    return valor;
} 

void escribir_memoria(uint32_t dl, uint32_t valor){
    int df = traducir_dl(dl);

    log_info(cpu_logger_info, "El valor a enviar por MOV_OUT ES %d",valor);

    if(df != -1) {enviar_mov_out(df, valor);}
    else {log_info(cpu_logger_info, "Hubo page fault");}
}


void enviar_mov_out(int df, uint32_t valor)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(MOV_OUT_CPU);
	paquete->buffer->size += sizeof(int) + sizeof(uint32_t);
	paquete->buffer->stream = malloc(paquete->buffer->size);

    log_info(cpu_logger_info,"El size del paquete mov out es %d", paquete->buffer->size);

	int offset = 0;

    memcpy(paquete->buffer->stream + offset, &(valor), sizeof(uint32_t));
    offset += sizeof(uint32_t);
	memcpy(paquete->buffer->stream + offset, &(df), sizeof(int));

	enviar_paquete(paquete, socket_memoria);
	eliminar_paquete(paquete);
}

int traducir_dl(uint32_t dl){
    uint32_t num_pag = dl/tamano_pagina;

    log_error(cpu_logger_info, "Numero de pagina: %d", num_pag);

    uint32_t desplazamiento = dl - num_pag * tamano_pagina;

    enviar_pedido_marco(socket_memoria, num_pag, contexto_actual->pid);

    op_code operacion = recibir_operacion(socket_memoria);

    int marco = recibir_marco(socket_memoria);

    if(operacion == MARCO_PAGE_FAULT) {
        log_info(cpu_logger_info, "Page Fault PID: %d - Página: %d", contexto_actual->pid, num_pag);
        page_fault = true;
        contexto_actual->nro_pf = num_pag;
        return marco;
    }
    else{
        log_info(cpu_logger_info, "“PID: %d - OBTENER MARCO - Página: %d - Marco:%d”",contexto_actual->pid, num_pag, marco);
        int df = marco * tamano_pagina + desplazamiento;
        log_info(cpu_logger_info, "Dirección física: %d", df);
        return df;
    }
}

void finalizar_cpu()
{
    log_info(cpu_logger_info, "Finalizando el modulo CPU");
    log_destroy(cpu_logger_info);
    config_destroy(config);
    close(socket_memoria);
    close(servidor_cpu_dispatch);
    close(servidor_cpu_interrupt);
    close(conexion_kernel_dispatch);
    close(conexion_kernel_interrupt);
}
