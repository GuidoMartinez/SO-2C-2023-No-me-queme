#include "filesystem.h"

int main(){
    t_log* logger = log_create("./cfg/filesystem.log", "FILESYSTEM", true, LOG_LEVEL_INFO);
    log_info(logger, "Soy el fs %s", mi_funcion_compartida());
    log_destroy(logger);
}
