#include "shared_utils.h"

char *mi_funcion_compartida()
{ // TODO -- BORRAR -- PARA PROBAR CONFIG DE SHARED
	return "Hice uso de la shared!";
}

void enviar_mensaje(char *mensaje, int socket_cliente)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void *a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}

void *serializar_paquete(t_paquete *paquete, int bytes)
{
	void *magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento += sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento += paquete->buffer->size;

	return magic;
}

void crear_buffer(t_paquete *paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}

t_paquete *crear_paquete(void)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = PAQUETE;
	crear_buffer(paquete);
	return paquete;
}

t_paquete *crear_paquete_con_codigo_de_operacion(uint32_t codigo)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codigo;
	crear_buffer(paquete);
	return paquete;
}

t_paquete *crear_paquete_con_codigo_de_estado(t_resp_file codigo)
{
	t_paquete *paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codigo;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete *paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2 * sizeof(int);
	void *a_enviar = serializar_paquete(paquete, bytes);
	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete *paquete)
{

	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

// CONEXION INICIAL

int crear_conexion(char *ip, char *puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);

		if (connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen) == -1)
	{
		printf("error al conectar a algun modulo"); // TODO // eliminar
		// freeaddrinfo(server_info); // TODO -- VER SI TIENE QUE ESTAR
		return -1;
	}
	/*if(setsockopt(socket_cliente, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))>0){
		printf("error al agregar configuracion extra al socket"); // TODO // eliminar
		// freeaddrinfo(server_info); // TODO -- VER SI TIENE QUE ESTAR
		return -1;
	}*/

	freeaddrinfo(server_info);

	return socket_cliente;
}

int iniciar_servidor(t_log *logger, char *IP, char *PUERTO)
{
	int socket_servidor;
	struct addrinfo hints;
	struct addrinfo *serverInfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int rv = getaddrinfo(IP, PUERTO, &hints, &serverInfo);
	if (rv != 0)
	{
		printf("getaddrinfo error: %s\n", gai_strerror(rv));
		return EXIT_FAILURE;
	}

	socket_servidor = socket(serverInfo->ai_family,
							 serverInfo->ai_socktype,
							 serverInfo->ai_protocol);

	if (socket_servidor == -1)
	{
		printf("Socket creation error\n%s", strerror(errno));
		return EXIT_FAILURE;
	}
	bind(socket_servidor, serverInfo->ai_addr, serverInfo->ai_addrlen);
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(serverInfo);
	return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log *logger)
{
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	// log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int crear_socket_escucha(char *puerto_escucha)
{
	struct sockaddr_in direccion_servidor;
	memset(&direccion_servidor, 0, sizeof(direccion_servidor));
	direccion_servidor.sin_family = AF_INET;
	direccion_servidor.sin_addr.s_addr = INADDR_ANY;
	direccion_servidor.sin_port = htons(atoi(puerto_escucha));

	int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_servidor == -1)
	{
		perror("Error al crear el socket");
		exit(EXIT_FAILURE);
	}

	int activado = 1;
	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)) == -1)
	{
		perror("Error al configurar el socket");
		exit(EXIT_FAILURE);
	}

	if (bind(socket_servidor, (void *)&direccion_servidor, sizeof(direccion_servidor)) != 0)
	{
		perror("Error al bindear el socket");
		exit(EXIT_FAILURE);
	}

	if (listen(socket_servidor, SOMAXCONN) == -1)
	{
		perror("Error al poner el servidor en modo escucha");
		exit(EXIT_FAILURE);
	}

	return socket_servidor;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void *recibir_buffer(int *size, int socket_cliente)
{
	void *buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

void *recibir_mensaje(int socket_cliente, t_log *logger)
{
	int size;
	char *buffer = recibir_buffer(&size, socket_cliente);
	log_info(logger, "Me llego el mensaje %s", buffer);
	return buffer;
}

void *recibir_mensaje_sin_log(int socket_cliente)
{
	int size;
	char *buffer = recibir_buffer(&size, socket_cliente);
	return buffer;
}

t_list *recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void *buffer;
	t_list *valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while (desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento += sizeof(int);
		char *valor = malloc(tamanio);
		memcpy(valor, buffer + desplazamiento, tamanio);
		desplazamiento += tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
	return NULL;
}

void realizar_handshake(int conexion, op_code codigo_op, t_log *logger)
{
	t_paquete *paquete_handshake = crear_paquete_con_codigo_de_operacion(codigo_op);

	paquete_handshake->buffer->size += sizeof(uint32_t);
	paquete_handshake->buffer->stream = realloc(paquete_handshake->buffer->stream, paquete_handshake->buffer->size);
	memcpy(paquete_handshake->buffer->stream, &(codigo_op), sizeof(codigo_op));
	enviar_paquete(paquete_handshake, conexion);
	eliminar_paquete(paquete_handshake);
}

op_code recibir_handshake(int conexion, t_log *logger)
{

	int size;
	void *buffer;
	buffer = recibir_buffer(&size, conexion);
	// printf("Size del stream a deserializar: %d \n", size); // TODO -- BORRAR

	op_code codigo_recibido;

	int offset = 0;

	memcpy(&(codigo_recibido), buffer + offset, sizeof(op_code));

	free(buffer);

	return codigo_recibido;
}

char *obtener_nombre_instruccion(nombre_instruccion instruccion)
{
	switch (instruccion)
	{
	case SET:
		return "SET";
	case SUM:
		return "SUM";
	case SUB:
		return "SUB";
	case JNZ:
		return "JNZ";
	case SLEEP:
		return "SLEEP";
	case MOV_IN:
		return "MOV_IN";
	case MOV_OUT:
		return "MOV_OUT";
	case WAIT:
		return "WAIT";
	case SIGNAL:
		return "SIGNAL";
	case F_OPEN:
		return "F_OPEN";
	case F_TRUNCATE:
		return "F_TRUNCATE";
	case F_SEEK:
		return "F_SEEK";
	case F_CREATE:
		return "F_CREATE";
	case F_WRITE:
		return "F_WRITE";
	case F_READ:
		return "F_READ";
	case F_CLOSE:
		return "F_CLOSE";
	case EXIT:
		return "EXIT";
	default:
		printf("imprimo el num %d", instruccion);
		return "DESCONOCIDO";
	}
}

uint32_t str_to_uint32(char *str)
{
	char *endptr;
	uint32_t result = (uint32_t)strtoul(str, &endptr, 10);

	// Comprobar si hubo errores durante la conversión
	if (*endptr != '\0')
	{
		fprintf(stderr, "Error en la conversión de '%s' a uint32_t.\n", str);
		exit(EXIT_FAILURE);
	}

	return result;
}

void serializar_contexto(t_paquete *paquete, t_contexto_ejecucion *ctx)
{

	paquete->buffer->size = sizeof(int) * 4 +
							sizeof(t_registros) +
							sizeof(nombre_instruccion) +
							sizeof(uint32_t) * 2 +
							ctx->instruccion_ejecutada->longitud_parametro1 +
							ctx->instruccion_ejecutada->longitud_parametro2 +
							sizeof(nombre_instruccion) +
							sizeof(motivo_desalojo);

	// printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int desplazamiento = 0;

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->pid), sizeof(int));
	desplazamiento += sizeof(int);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->program_counter), sizeof(int));
	desplazamiento += sizeof(int);

	memcpy(paquete->buffer->stream + desplazamiento, ctx->registros, sizeof(t_registros));
	desplazamiento += sizeof(t_registros);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->numero_marco), sizeof(int));
	desplazamiento += sizeof(int);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->nro_pf), sizeof(int));
	desplazamiento += sizeof(int);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->instruccion_ejecutada->codigo), sizeof(nombre_instruccion));
	desplazamiento += sizeof(nombre_instruccion);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->instruccion_ejecutada->longitud_parametro1), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->instruccion_ejecutada->longitud_parametro2), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, ctx->instruccion_ejecutada->parametro1, ctx->instruccion_ejecutada->longitud_parametro1);
	desplazamiento += ctx->instruccion_ejecutada->longitud_parametro1;

	memcpy(paquete->buffer->stream + desplazamiento, ctx->instruccion_ejecutada->parametro2, ctx->instruccion_ejecutada->longitud_parametro2);
	desplazamiento += ctx->instruccion_ejecutada->longitud_parametro2;

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->codigo_ultima_instru), sizeof(nombre_instruccion));
	desplazamiento += sizeof(nombre_instruccion);

	memcpy(paquete->buffer->stream + desplazamiento, &(ctx->motivo_desalojado), sizeof(motivo_desalojo));
	desplazamiento += sizeof(motivo_desalojo);
}

void enviar_contexto(int socket, t_contexto_ejecucion *contexto_a_enviar)
{

	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(CONTEXTO);
	serializar_contexto(paquete, contexto_a_enviar);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

t_contexto_ejecucion *recibir_contexto(int socket)
{
	int size;
	void *buffer;

	buffer = recibir_buffer(&size, socket);
	// printf("Size del stream a deserializar: %d \n", size);

	t_contexto_ejecucion *contexto_recibido = malloc(sizeof(t_contexto_ejecucion));

	int offset = 0;

	memcpy(&(contexto_recibido->pid), buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&(contexto_recibido->program_counter), buffer + offset, sizeof(int));
	offset += sizeof(int);

	contexto_recibido->registros = malloc(sizeof(t_registros));
	memcpy(contexto_recibido->registros, buffer + offset, sizeof(t_registros));
	offset += sizeof(t_registros);

	memcpy(&(contexto_recibido->numero_marco), buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(&(contexto_recibido->nro_pf), buffer + offset, sizeof(int));
	offset += sizeof(int);

	contexto_recibido->instruccion_ejecutada = malloc(sizeof(t_instruccion));

	memcpy(&(contexto_recibido->instruccion_ejecutada->codigo), buffer + offset, sizeof(nombre_instruccion));
	offset += sizeof(nombre_instruccion);
	memcpy(&(contexto_recibido->instruccion_ejecutada->longitud_parametro1), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);
	memcpy(&(contexto_recibido->instruccion_ejecutada->longitud_parametro2), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	contexto_recibido->instruccion_ejecutada->parametro1 = malloc(contexto_recibido->instruccion_ejecutada->longitud_parametro1);
	memcpy(contexto_recibido->instruccion_ejecutada->parametro1, buffer + offset, contexto_recibido->instruccion_ejecutada->longitud_parametro1);
	offset += contexto_recibido->instruccion_ejecutada->longitud_parametro1;

	contexto_recibido->instruccion_ejecutada->parametro2 = malloc(contexto_recibido->instruccion_ejecutada->longitud_parametro2);
	memcpy(contexto_recibido->instruccion_ejecutada->parametro2, buffer + offset, contexto_recibido->instruccion_ejecutada->longitud_parametro2);
	offset += contexto_recibido->instruccion_ejecutada->longitud_parametro2;

	memcpy(&(contexto_recibido->codigo_ultima_instru), buffer + offset, sizeof(nombre_instruccion));
	offset += sizeof(nombre_instruccion);

	memcpy(&(contexto_recibido->motivo_desalojado), buffer + offset, sizeof(motivo_desalojo));
	offset += sizeof(motivo_desalojo);

	free(buffer);

	return contexto_recibido;
}

void enviar_instruccion(int socket, t_instruccion *instruccion)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(INSTRUCCION);
	serializar_instruccion(paquete, instruccion);
	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

void serializar_instruccion(t_paquete *paquete, t_instruccion *instruccion)
{
	paquete->buffer->size = sizeof(nombre_instruccion) +
							sizeof(uint32_t) * 2 +
							instruccion->longitud_parametro1 +
							instruccion->longitud_parametro2;
	// printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int desplazamiento = 0;

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->codigo), sizeof(nombre_instruccion));
	desplazamiento += sizeof(nombre_instruccion);

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->longitud_parametro1), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->longitud_parametro2), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, instruccion->parametro1, instruccion->longitud_parametro1);
	desplazamiento += instruccion->longitud_parametro1;

	memcpy(paquete->buffer->stream + desplazamiento, instruccion->parametro2, instruccion->longitud_parametro2);
	desplazamiento += instruccion->longitud_parametro2;
}

t_instruccion *deserializar_instruccion(int socket)
{
	int size;
	void *buffer;

	buffer = recibir_buffer(&size, socket);
	//printf("Size del stream a deserializar: %d \n", size);

	t_instruccion *instruccion_recibida = malloc(sizeof(t_instruccion));

	int offset = 0;

	memcpy(&(instruccion_recibida->codigo), buffer + offset, sizeof(nombre_instruccion));
	offset += sizeof(nombre_instruccion);

	memcpy(&(instruccion_recibida->longitud_parametro1), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	memcpy(&(instruccion_recibida->longitud_parametro2), buffer + offset, sizeof(uint32_t));
	offset += sizeof(uint32_t);

	instruccion_recibida->parametro1 = malloc(instruccion_recibida->longitud_parametro1);
	memcpy(instruccion_recibida->parametro1, buffer + offset, instruccion_recibida->longitud_parametro1);
	offset += instruccion_recibida->longitud_parametro1;

	instruccion_recibida->parametro2 = malloc(instruccion_recibida->longitud_parametro2);
	memcpy(instruccion_recibida->parametro2, buffer + offset, instruccion_recibida->longitud_parametro2);
	offset += instruccion_recibida->longitud_parametro2;

	free(buffer);

	return instruccion_recibida;
}

char *cod_inst_to_str(nombre_instruccion nomb)
{
	switch (nomb)
	{
	case SET:
		return "SET";
	case SUM:
		return "SUM";
	case SUB:
		return "SUB";
	case JNZ:
		return "JNZ";
	case SLEEP:
		return "SLEEP";
	case WAIT:
		return "WAIT";
	case SIGNAL:
		return "SIGNAL";
	case MOV_IN:
		return "MOV_IN";
	case MOV_OUT:
		return "MOV_OUT";
	case F_OPEN:
		return "F_OPEN";
	case F_CLOSE:
		return "F_CLOSE";
	case F_SEEK:
		return "F_SEEK";
	case F_READ:
		return "F_READ";
	case F_WRITE:
		return "F_WRITE";
	case F_TRUNCATE:
		return "F_TRUNCATE";
	case EXIT:
		return "EXIT";
	default:
		return "ERROR";
	}
}

// pedido de CPU a MEMORIA para la instruccion segun PC y PID
void ask_instruccion_pid_pc(int pid, int pc, int socket)
{

	t_paquete *paquete_instruccion = crear_paquete_con_codigo_de_operacion(PEDIDO_INSTRUCCION);
	paquete_instruccion->buffer->size += sizeof(int) * 2;
	paquete_instruccion->buffer->stream = malloc(paquete_instruccion->buffer->size);
	memcpy(paquete_instruccion->buffer->stream, &(pid), sizeof(int));
	memcpy(paquete_instruccion->buffer->stream + sizeof(int), &(pc), sizeof(int));
	enviar_paquete(paquete_instruccion, socket);
	eliminar_paquete(paquete_instruccion);
}

// RECIBO PEDIDO DE CPU EN MEMORIA SOBRE LA INSTRUCCION CON PC Y PID INDICADO
void pedido_instruccion(uint32_t *pid, uint32_t *pc, int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	// printf("size del stream a deserializar \n%d", size);
	memcpy(pid, buffer + offset, sizeof(int));
	offset += sizeof(int);
	memcpy(pc, buffer + offset, sizeof(int));

	free(buffer);
}

void enviar_interrupcion(int socket, t_interrupcion *interrupcion)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(INTERRUPCION);

	paquete->buffer->size = sizeof(motivo_desalojo) +
							sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int desplazamiento = 0;

	memcpy(paquete->buffer->stream + desplazamiento, &(interrupcion->motivo_interrupcion), sizeof(motivo_desalojo));
	desplazamiento += sizeof(motivo_desalojo);

	memcpy(paquete->buffer->stream + desplazamiento, &(interrupcion->pid), sizeof(int));
	desplazamiento += sizeof(int);

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

t_interrupcion *recibir_interrupcion(int socket)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	int offset = 0;

	//printf("size del stream a deserializar \n%d", size);
	t_interrupcion *interrupcion_recibida = malloc(sizeof(t_interrupcion));

	memcpy(&(interrupcion_recibida->motivo_interrupcion), buffer + offset, sizeof(motivo_desalojo));
	offset += sizeof(motivo_desalojo);

	memcpy(&(interrupcion_recibida->pid), buffer + offset, sizeof(int));
	offset += sizeof(int);

	free(buffer);
	return interrupcion_recibida;
}

void serializar_respuesta_file_kernel(int socket_cliente, t_resp_file respuesta)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_estado(respuesta);
	enviar_paquete(paquete, socket_cliente);
	eliminar_paquete(paquete);
}

void deserializar_header(t_paquete *paquete, int socket_cliente)
{
	recv(socket_cliente, &(paquete->codigo_operacion), sizeof(uint32_t), MSG_WAITALL);
	recv(socket_cliente, &(paquete->lineas), sizeof(uint32_t), MSG_WAITALL);
	recv(socket_cliente, &(paquete->buffer->size), sizeof(uint32_t), MSG_WAITALL);
	paquete->buffer->stream = malloc(paquete->buffer->size);
	recv(socket_cliente, paquete->buffer->stream, paquete->buffer->size, MSG_WAITALL);
}

t_instruccion_fs *deserializar_instruccion_fs(int socket)
{
	int size;
	void *buffer;

	buffer = recibir_buffer(&size, socket);
	//printf("Size del stream a deserializar: %d \n", size);

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
	free(buffer);

	return instruccion;
}

void serializar_instruccion_fs(t_paquete *paquete, t_instruccion_fs *instruccion)
{
	paquete->buffer->size = sizeof(nombre_instruccion) +
							sizeof(uint32_t) * 4 +
							instruccion->param1_length +
							instruccion->param2_length;

	//printf("Size del stream a serializar: %d \n", paquete->buffer->size); // TODO - BORRAR LOG
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int desplazamiento = 0;

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->estado), sizeof(nombre_instruccion));
	desplazamiento += sizeof(nombre_instruccion);

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->pid), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->param1_length), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->param2_length), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);

	memcpy(paquete->buffer->stream + desplazamiento, instruccion->param1, instruccion->param1_length);
	desplazamiento += instruccion->param1_length;

	memcpy(paquete->buffer->stream + desplazamiento, instruccion->param2, instruccion->param2_length);
	desplazamiento += instruccion->param2_length;

	memcpy(paquete->buffer->stream + desplazamiento, &(instruccion->puntero), sizeof(uint32_t));
	desplazamiento += sizeof(uint32_t);
}

// en la lista debo guardar los punteros de int ya que las listas manejan punteros a estructuras. @ TOMAS
void serializar_lista_swap(t_list *bloques_swap, t_paquete *paquete)
{
	int offset = paquete->buffer->size;

	for (int i = 0; i < list_size(bloques_swap); i++)
	{
		// int *ptr_bloque = list_get(bloques_swap, i);
		int id_bloque = list_get(bloques_swap, i);

		// int id_bloque = *ptr_bloque;

		paquete->buffer->size += sizeof(int);
		paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size);
		memcpy(paquete->buffer->stream + offset, &(id_bloque), sizeof(int));
		offset += sizeof(int);
	}
	// list_destroy(bloques_swap); TODO chequear donde se libera
}

void enviar_pid(int pid, int socket, op_code codigo)
{
	t_paquete *paquete_pid = crear_paquete_con_codigo_de_operacion(codigo);
	paquete_pid->buffer->size += sizeof(int);
	paquete_pid->buffer->stream = realloc(paquete_pid->buffer->stream, paquete_pid->buffer->size);
	memcpy(paquete_pid->buffer->stream, &(pid), sizeof(int));
	enviar_paquete(paquete_pid, socket);
	eliminar_paquete(paquete_pid);
}

void recibir_pid(int socket, int *pid)
{
	int size;
	void *buffer = recibir_buffer(&size, socket);
	memcpy(pid, buffer, sizeof(int));
	free(buffer);
}

// MEMORIA - FS

t_list *recibir_listado_id_bloques(int socket)
{

	int size;
	void *buffer;

	buffer = recibir_buffer(&size, socket);
	
	t_list *lista = list_create();

	int offset = 0;

	while (offset < size)
	{
		int bloque_swap;
		memcpy(&bloque_swap, buffer + offset, sizeof(int));
		offset += sizeof(int);
		list_add(lista, bloque_swap);
	}
	free(buffer);

	return lista;
}

void enviar_pedido_marco(int socket, int pagina, int pid)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(MARCO);
	paquete->buffer->size += sizeof(int) * 2;
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int offset = 0;

	memcpy(paquete->buffer->stream + offset, &(pid), sizeof(int));
	offset += sizeof(int);

	memcpy(paquete->buffer->stream + offset, &(pagina), sizeof(int));

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recibir_marco(int socket)
{

	int size;
	void *buffer;
	buffer = recibir_buffer(&size, socket);
	// printf("Size del stream a deserializar: %d \n", size); // TODO -- BORRAR

	int marco;

	int offset = 0;

	memcpy(&(marco), buffer + offset, sizeof(int));

	free(buffer);

	return marco;
}

void enviar_op_con_int(int socket, op_code code, int entero)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(code);
	paquete->buffer->size += sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int offset = 0;

	memcpy(paquete->buffer->stream + offset, &(entero), sizeof(int));

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

int recibir_int(int socket)
{
	int size;
	void *buffer;
	buffer = recibir_buffer(&size, socket);

	int entero;
	memcpy(&(entero), buffer, sizeof(int));
	free(buffer);
	return entero;
}

uint32_t recibir_valor_memoria(int socket)
{
	int size;
	void *buffer;
	buffer = recibir_buffer(&size, socket);

	uint32_t valor_memoria;

	int offset = 0;

	memcpy(&(valor_memoria), buffer + offset, sizeof(uint32_t));

	free(buffer);

	return valor_memoria;
}

void liberar_instruccion(t_instruccion *instr)
{
	if (strcmp(instr->parametro1, "") != 0)
	{
		free(instr->parametro1);
	}
	if (strcmp(instr->parametro2, "") != 0)
	{
		free(instr->parametro2);
	}
	free(instr);
}

void liberar_contexto(t_contexto_ejecucion *contexto){
	free(contexto->registros);
	free(contexto->instruccion_ejecutada->parametro1);
	free(contexto->instruccion_ejecutada->parametro2);
	free(contexto->instruccion_ejecutada);
	free(contexto);
}


void liberar_lista_instrucciones(t_list *instrucciones)
{
	list_destroy_and_destroy_elements(instrucciones, (void (*)(void *))liberar_instruccion);
}

void enviar_op(int socket, op_code code, int entero)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(code);
	paquete->buffer->size += sizeof(int);
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int offset = 0;

	memcpy(paquete->buffer->stream + offset, &(entero), sizeof(int));

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}

void enviar_bloque(int socket, bloque_t bloque, int tamano_bloque)
{
	t_paquete *paquete = crear_paquete_con_codigo_de_operacion(VALOR_BLOQUE);
	paquete->buffer->size += tamano_bloque;
	paquete->buffer->stream = malloc(paquete->buffer->size);

	int offset = 0;

	memcpy(paquete->buffer->stream + offset, bloque.datos, tamano_bloque);

	enviar_paquete(paquete, socket);
	eliminar_paquete(paquete);
}