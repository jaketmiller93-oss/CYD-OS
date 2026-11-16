#include "control_boot.h"
static esp_err_t command(const cyd_control_boot_io_t *io,uint8_t value) {
    for(unsigned attempt=1;attempt<=3;attempt++) {
        esp_err_t err=io->write(io->context,value);
        if(err==ESP_OK)return ESP_OK;
        if(attempt==3 || (err!=ESP_ERR_TIMEOUT && err!=ESP_ERR_INVALID_STATE))return err;
        if(io->retry_note)io->retry_note(io->context,value,attempt,err);
        esp_err_t recovery=io->reset_bus(io->context);
        if(recovery!=ESP_OK)return recovery;
        io->wait_ms(io->context,100);
    }
    return ESP_ERR_INVALID_STATE;
}
esp_err_t cyd_control_boot(const cyd_control_boot_io_t *io) {
    if(!io || !io->write || !io->reset_bus || !io->wait_ms)return ESP_ERR_INVALID_ARG;
    /* esp_restart does not power-cycle the external controllers. Clear a bus
     * left mid-transaction by the previous app before talking to the STC. */
    io->wait_ms(io->context,50);
    esp_err_t err=io->reset_bus(io->context);
    if(err!=ESP_OK)return err;
    err=command(io,250);
    if(err!=ESP_OK)return err;
    io->wait_ms(io->context,100);
    return command(io,0);
}
