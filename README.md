# IoT Scent Reproduction Firmware (ESP8266)

This repository contains the embedded **C++ firmware** for the **MoldSEF Scent Reproduction System**. 

The device acts as an edge node that maintains a persistent WebSocket connection to the central backend. It receives real-time AI-generated scent inference commands and triggers physical hardware actuators (pumps/atomizers) to reproduce the target scent.

Note: *the hardware is still under development*

### Related Repositories
- **Backend (FastAPI/AI):** [github.com/maximbenea/fastapi-backend](https://github.com/maximbenea/fastapi-backend)
- **Frontend (Vite/React):** [github.com/maximbenea/vite-frontend](https://github.com/maximbenea/vite-frontend)

## System Architecture

```mermaid
graph LR
    A[Video Input] --> B(Frontend)
    B --> C{FastAPI Backend}
    C -->|Gemini API| D[Scent Inference]
    D --> C
    C -->|WebSocket JSON| E[ESP8266 Firmware]
    E -->|GPIO Trigger| F[Scent Hardware]
```
