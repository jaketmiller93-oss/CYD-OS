#include "cyd.h"
#include "esp_app_desc.h"
#include <stdatomic.h>
static atomic_bool maintenance;
const char *cyd_system_version(void) {
    return esp_app_get_description()->version;
}
bool cyd_maintenance_active(void) {
    return atomic_load(&maintenance);
}
bool cyd_maintenance_begin(void) {
    bool expected = false;
    return atomic_compare_exchange_strong(&maintenance, &expected, true);
}
void cyd_maintenance_end(void) {
    atomic_store(&maintenance, false);
}
