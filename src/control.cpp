#include "control.h"

#include "config.h"

static QueueHandle_t g_commands = nullptr;
static Status g_status = {};
static portMUX_TYPE g_statusMux = portMUX_INITIALIZER_UNLOCKED;

void controlBegin() {
    g_commands = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(Command));
    configASSERT(g_commands != nullptr);
}

bool controlSend(const Command& command) { return xQueueSend(g_commands, &command, 0) == pdTRUE; }

bool controlReceive(Command& command, TickType_t wait) { return xQueueReceive(g_commands, &command, wait) == pdTRUE; }

void controlPublish(const Status& status) {
    portENTER_CRITICAL(&g_statusMux);
    g_status = status;
    portEXIT_CRITICAL(&g_statusMux);
}

Status controlSnapshot() {
    portENTER_CRITICAL(&g_statusMux);
    const Status status = g_status;
    portEXIT_CRITICAL(&g_statusMux);
    return status;
}
