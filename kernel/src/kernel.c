#include "kernel.h"

int main(){
    t_log* logger = log_create("./cfg/kernel.log", "KERNEL", true, LOG_LEVEL_INFO);
    log_info(logger, "Soy el kernel %s", mi_funcion_compartida());
    log_destroy(logger);
}
