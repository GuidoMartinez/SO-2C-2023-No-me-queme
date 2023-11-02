#ifndef PLANI_H
#define PLANI_H

#include "shared_utils.h"

char *motivo_exit_to_string(motivo_exit);
void exit_pcb(void);
void pcb_destroy(t_pcb*);
void safe_pcb_add(t_list*, t_pcb*, pthread_mutex_t*);
t_pcb *safe_pcb_remove(t_list*, pthread_mutex_t*);
void pcb_create(int, int, int);
t_pcb *elegir_pcb_segun_algoritmo();
char *estado_to_string(estado_proceso);
void cambiar_estado(t_pcb*, estado_proceso);
void procesar_cambio_estado(t_pcb*, estado_proceso);
void planificar_largo_plazo();
void planificar_corto_plazo();
void ready_pcb(void);
void exec_pcb();
void proceso_admitido(t_pcb*);
void block();
void asignar_algoritmo(char*);
void set_pcb_ready(t_pcb*);
void set_pcb_block(t_pcb*);
void remove_blocked(int);
void remove_ready(int);


bool maximo_RR(t_pcb*, t_pcb*);
bool maximo_PRIORIDAD(t_pcb*, t_pcb*);
t_pcb *obtener_pcb_PRIORIDAD();
t_pcb *obtener_pcb_RR();

#endif