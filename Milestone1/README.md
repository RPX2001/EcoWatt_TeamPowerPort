# EcoWatt Milestone 1 – Petri Net Model & Scaffold Implementation

This repository contains the **Milestone 1** deliverable for the EcoWatt project:  
- **Petri Net system model** for periodic polling → buffering → periodic upload.  
- **Python scaffold implementation** simulating device logic, inverter, and cloud.  
- Logs directly map Petri Net transitions to code execution for traceability.

---

## 📋 Overview

The system models a device that:
1. Periodically **polls** an inverter for voltage/current.
2. **Buffers** readings locally (bounded buffer).
3. **Uploads** all buffered samples to the cloud every 15 seconds (simulated; 15 min in real device).
4. **Clears** buffer on cloud acknowledgment.
5. Handles transient faults: inverter unavailable, cloud NACK.

This design is captured in a **Petri Net** for correctness and then implemented in a **Python scaffold**.

---

## 📈 Petri Net Model

**Places (P):**
- **P0** Idle  
- **P1** PollTimerReady  
- **P2** Polling  
- **P3** SampleReady  
- **P4** Buffer [cap=N]  
- **P5** UploadTimerReady  
- **P6** Uploading  
- **P7** Acked  
- **P8** PollTimerArmed  
- **P9** UploadTimerArmed  

**Transitions (T):**
- **T0** ArmPollTimer  
- **Ttick_Poll** (timed, poll period)  
- **T1** DoPoll `{SIM_OK}`  
- **T2** BufferPush `{BUF_HAS_SPACE}`  
- **T3** ArmUploadTimer  
- **Ttick_Upload** (timed, upload period)  
- **T4** UploadBatch `{BUF_NOT_EMPTY}`  
- **T5** AckAndFlush  

The Petri Net ensures:
- **Boundedness**: Buffer never exceeds capacity.  
- **Liveness**: With SIM & network OK, all buffered samples are eventually uploaded.  
- **Safety**: Guards prevent invalid state transitions.

---

## 🗂 Folder Structure
Milestone1/
    app.py                  # Main runner, wires modules together
    core/
        timers.py              # Periodic timers (poll/upload)
        acquisition.py         # Poll inverter + push to buffer
        buffer.py              # Bounded buffer (ring queue)
        uploader.py            # Upload to cloud + ack handling
    sim/
        inverter_sim.py        # Fake inverter readings (randomized V/I)
        cloud_stub.py          # Fake cloud (ACK/NACK simulation)
    README.md
---

## ⚙️ Requirements

Python 3.8+  
Standard library only (no external dependencies).  
Optional extras for nicer output / type checking:
```bash
pip install rich mypy
```

🚀 Running the Scaffold

1.	Clone repo & create virtual environment:
```bash
    git clone https://github.com/yourusername/ecowatt_m1.git
    cd ecowatt_m1
    python3 -m venv .venv
    source .venv/bin/activate  # Linux/macOS
    # .venv\Scripts\Activate.ps1  # Windows PowerShell
```
2.	Run:
```bash
python app.py
```
3.	Observe logs:
You will see periodic [Ttick_Poll], [P3 SampleReady], [T2_BufferPush] messages,
and every 15 seconds: [Ttick_Upload], [T4_UploadBatch], cloud ACK/NACK, [T5_AckAndFlush].
