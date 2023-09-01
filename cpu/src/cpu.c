#include "cpu.h"

int main(){
    t_log* logger = log_create("./cfg/cpu.log", "CPU", true, LOG_LEVEL_INFO);
    log_info(logger, "Soy el cpu %s", mi_funcion_compartida());
    log_destroy(logger);
}
