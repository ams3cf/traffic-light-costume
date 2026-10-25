// ============================================================================
// Web core: the HTTP UI and API, plus the network (network.h), running in
// a task pinned to WEB_TASK_CORE. It only enqueues Commands and
// reads Status snapshots; it never touches LEDs or LightState directly.
// API reference: docs/WEB_API.md.
// ============================================================================
#pragma once

void webBegin();
