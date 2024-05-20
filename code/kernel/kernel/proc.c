#include "proc.h"
#include "risv.h"

int cpuid() {
    int id = r_tp();
    return id;
}