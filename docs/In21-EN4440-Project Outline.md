![][image1]

**Department of Electronic & Telecommunication Engineering (EN)**  
**Department of Biomedical Engineering (BM)**  
**University of Moratuwa**

# Project Outline 

EN4440 \- Embedded Systems Engineering

Last Modified: Sep 16, 2025

TABLE OF CONTENTS

[1\. Project Overview	2](#project-overview)

[1.1. Background (in simple terms)	2](#background-\(in-simple-terms\))

[1.2. Core Objective	3](#core-objective)

[2\. Actors & Channels	3](#actors-&-channels)

[2.1. Inverter SIM Deployment Options	3](#inverter-sim-deployment-options)

[2.2. Transport Constraints	3](#transport-constraints)

[2.3. EcoWatt Device Functional Blocks	3](#ecowatt-device-functional-blocks)

[2.4. Cloud Components	4](#cloud-components)

[2.5. Key Workflows	5](#key-workflows)

[3\. Overview of Evaluation Criteria and Project Execution	5](#overview-of-evaluation-criteria-and-project-execution)

[3.1. Success Criteria / Acceptance Tests	5](#success-criteria-/-acceptance-tests)

[3.2. High-Level Project Phases	5](#high-level-project-phases)

[3.3. Milestone Breakdown and Marks Allocation	6](#milestone-breakdown-and-marks-allocation)

[4\. Milestone 1: System Modeling with Petri Nets and Scaffold Implementation	6](#milestone-1:-system-modeling-with-petri-nets-and-scaffold-implementation)

[4.1. Objective	6](#objective)

[4.2. Scope	6](#scope)

[4.2.1. Part 1: Petri Net Modeling	6](#part-1:-petri-net-modeling)

[4.2.2. Part 2: Scaffold Implementation	6](#part-2:-scaffold-implementation)

[4.2.3. Part 3: Demonstration Video	7](#part-3:-demonstration-video)

[4.2.4. What You Must Submit/Deliverables	7](#what-you-must-submit/deliverables)

[4.3. Evaluation Rubric	7](#evaluation-rubric)

[5\. Milestone 2: Inverter SIM Integration and Basic Acquisition	8](#milestone-2:-inverter-sim-integration-and-basic-acquisition)

[5.1. Objective	8](#objective-1)

[5.2. Scope	8](#scope-1)

[5.2.1. Part 1: Protocol Adapter Implementation	8](#part-1:-protocol-adapter-implementation)

[5.2.2. Part 2: Basic Data Acquisition	8](#part-2:-basic-data-acquisition)

[5.2.3. Part 3: Demonstration Video	8](#part-3:-demonstration-video-1)

[5.2.4. Resources Provided	9](#resources-provided)

[5.2.5. What You Must Submit: Deliverables	9](#what-you-must-submit:-deliverables)

[5.3. Evaluation Rubric	9](#evaluation-rubric-1)

[6\. Milestone 3: Local Buffering, Compression, and Upload Cycle	9](#milestone-3:-local-buffering,-compression,-and-upload-cycle)

[6.1. Objective	9](#objective-2)

[6.2. Scope	10](#scope-2)

[6.2.1. Part 1: Buffer Implementation	10](#part-1:-buffer-implementation)

[6.2.2. Part 2: Compression Algorithm and Benchmarking	10](#part-2:-compression-algorithm-and-benchmarking)

[6.2.3. Part 3: Uplink Packetizer and Upload Cycle	10](#part-3:-uplink-packetizer-and-upload-cycle)

[6.2.4. Part 4: Demonstration Video	10](#part-4:-demonstration-video)

[6.2.5. Resources Provided	11](#resources-provided-1)

[6.2.6. What You Must Submit: Deliverables	11](#what-you-must-submit:-deliverables-1)

[6.3. Evaluation Rubric	11](#evaluation-rubric-2)

[7\. Milestone 4: Remote Configuration, Security Layer & FOTA	12](#milestone-4:-remote-configuration,-security-layer-&-fota)

[7.1. Objective	12](#objective-3)

[7.2. Scope	12](#scope-3)

[7.2.1. Part 1: Remote Configuration	12](#part-1:-remote-configuration)

[7.2.2. Part 2 – Command Execution	12](#part-2-–-command-execution)

[7.2.3. Part 3: Security Implementation	13](#part-3:-security-implementation)

[7.2.4. Part 4: FOTA Module	13](#part-4:-fota-module)

[7.2.5. Part 5: Demonstration Video	13](#part-5:-demonstration-video)

[7.2.6. Resources Provided	13](#resources-provided-2)

[7.2.7. What You Must Submit: Deliverables	14](#what-you-must-submit:-deliverables-2)

[7.3. Evaluation Rubric	14](#evaluation-rubric-3)

[8\. Milestone 5: Fault Recovery, Power Optimization & Final Integration	14](#milestone-5:-fault-recovery,-power-optimization-&-final-integration)

[8.1. Objective	14](#objective-4)

[8.2. Scope	14](#scope-4)

[8.2.1. Part 1: Power Management and Measurement	14](#part-1:-power-management-and-measurement)

[8.2.2. Part 2: Fault Recovery	15](#part-2:-fault-recovery)

[8.2.3. Part 3: Final Integration and Fault Testing	15](#part-3:-final-integration-and-fault-testing)

[8.2.4. Part 4: Live Demonstration	15](#part-4:-live-demonstration)

[8.2.5. Resources Provided	15](#resources-provided-3)

[8.2.6. What You Must Submit: Deliverables	16](#what-you-must-submit:-deliverables-3)

[8.3. Evaluation Rubric	16](#evaluation-rubric-4)

1. # **Project Overview** {#project-overview}

   1. ## **Background (in simple terms)** {#background-(in-simple-terms)}

We are building a small embedded device called **EcoWatt Device** that pretends it is plugged into a real solar inverter. Because we don’t have a physical inverter for the project, the device will talk to an online **Inverter SIM** service that behaves just like a real inverter would: it answers register read requests and accepts configuration write commands.

What we want the system to do (plain language):

* **Read a lot, send a little:** EcoWatt Device can read data (voltage, current, and later things like frequency) as often as we like, but it is only allowed to send data to the cloud once every 15 minutes. So it must store, shrink (compress), and package everything before the upload window.  
* **Let EcoWatt Cloud stay in charge:** EcoWatt Cloud decides what to read, how often to read it, and can even push a new firmware image to the device. The device (“slave”) simply follows those instructions safely.  
* **Stay secure but lightweight:** We use small-footprint security (keys, message authentication, simple encryption) so the MCU can still run fast and save power.  
* **Save power smartly:** The device should slow down its clock (DVFS) or sleep between jobs to cut its own energy use.  
* **Be future-ready:** Later, we can swap the Inverter SIM for a real inverter with minimal code changes because we’ve cleanly separated the protocol layer.

This background sets the scene. The sections below provide a more detailed description of the project's scope and building blocks.

2. ## **Core Objective** {#core-objective}

Build a microcontroller device called **EcoWatt Device** (“slave”) that behaves like a field gateway. Instead of a physical inverter, it communicates with a **Simulated Inverter** called **Inverter SIM**. EcoWatt Device:

* Polls inverter registers (voltage, current, optional frequency, etc.) at a **configurable rate**.  
* Buffers and **compresses** data locally.  
* **Uploads once every 15 minutes** through a constrained link.  
* Accepts **remote configuration & firmware updates (FOTA)** from the cloud “master.”  
* Implements **lightweight security** (auth, integrity, confidentiality) suitable for MCUs.  
* Uses **power saving / DVFS & sleep** between tasks.

2. # **Actors & Channels** {#actors-&-channels}

Three main pieces interact:

* **EcoWatt Device** (NodeMCU/ESP8266 MCU – the “slave”)  
* **EcoWatt Cloud** (configuration, commands, FOTA, data ingestion – the “master”)  
* **Inverter SIM** (the inverter emulator EcoWatt Device talks to instead of real hardware) – provided to you.

  1. ## **Inverter SIM Deployment Options** {#inverter-sim-deployment-options}

* Cloud API Mode – Inverter SIM runs as an HTTP/MQTT service online. EcoWatt Device reaches it over Wi‑Fi just like it would reach EcoWatt Cloud. The inverter protocol is encapsulated inside these requests.  
* PC Modbus Mode – Inverter SIM runs locally on a PC as a Modbus simulator. EcoWatt Device connects to the PC over USB (UART). From the device’s point of view, it is still “speaking to an inverter” via a serial link.

**For this project, we will use the Cloud API Mode**. The other path should still be achievable later with limited changes—mainly in the transport/adapter layer.

2. ## **Transport Constraints** {#transport-constraints}

* **EcoWatt Device → EcoWatt Cloud:** bulk payload upload strictly once every 15 minutes on a request-response setup.  
* **EcoWatt Device ↔ Inverter SIM:** high‑frequency polling allowed (configurable). Responses are stored locally and only forwarded (compressed) at the 15‑minute.

  3. ## **EcoWatt Device Functional Blocks** {#ecowatt-device-functional-blocks}

* **Protocol Adapter Layer**  
  * Implements inverter serial semantics over network to Inverter SIM.  
  * Swappable for real RS‑485 later (not in project scope)  
* **Acquisition Scheduler**  
  * Periodic reads (configurable from EcoWatt Cloud).  
  * Aperiodic writes/controls triggered by EcoWatt Cloud.  
* **Local Buffer & Compression**  
  * Local buffer for samples.  
  * Lightweight compression (delta/RLE or time‑series scheme).  
  * Optional aggregation (min/avg/max) to meet payload cap.  
* **Uplink Packetizer**  
  * 15‑minute tick: finalize compressed block → encrypt/MAC → transmit.  
  * Retry/chunk logic for unreliable links.  
* **Remote Config & Command Handler**  
  * Receives new sampling frequency, additional register list (e.g., add phase angle, active power, etc.), compression level, etc.  
  * Applies changes without reflashing firmware (runtime config update).  
* **FOTA (Firmware Over-The-Air) Module**  
  * Secure, chunked firmware download over multiple intervals.  
  * Integrity check (hash/signature), dual-bank/rollback.  
* **Security Layer (Lightweight)**  
  * Mutual auth (PSK \+ HMAC).  
  * AES‑CCM / ChaCha20‑Poly1305 for payloads.  
  * Anti‑replay via nonces/sequence numbers.  
  * Secure key storage appropriate for MCU constraints.  
* **Power Management / DVFS**  
  * Clock scaling when idle; sleep between polls/uploads.  
  * Peripheral gating (disable comm blocks outside windows).  
  * Optional self-energy-use reporting.  
* **Diagnostics & Fault Handling**  
  * Handle Inverter SIM timeouts, malformed frames, buffer overflow.  
  * Local event log.

  4. ## **Cloud Components** {#cloud-components}

* **Inverter SIM Service**  
  * REST/MQTT endpoints mimicking register reads/writes, stateful behavior (ramps, faults) \- provided to you.  
* **EcoWatt Cloud Control Service**  
  * Device registry, config push, command queue.  
  * Receives compressed payloads, decrypts/decompresses, stores data.  
  * Schedules and monitors FOTA rollouts.  
* **Firmware Repository**  
  * Stores signed binaries \+ manifests (version, size, hash).  
* **(Optional) Analytics/Visualization**  
  * Dashboards for inverter metrics & device health.

  5. ## **Key Workflows** {#key-workflows}

* **Acquisition Loop**  
  * EcoWatt Device polls Inverter SIM at configured rate → buffers & compresses → sleeps/DVFS between polls.  
* **15‑Minute Upload Cycle**  
  * Wake → finalize block → encrypt/HMAC → upload to EcoWatt Cloud → receive ACK \+ pending configs/commands.  
* **Remote Configuration Update**  
  * EcoWatt Cloud sends new config (e.g., add frequency/phase angle/active power).  
  * EcoWatt Device validates & updates acquisition tables instantly (no reboot).  
* **Firmware Update (FOTA)**  
  * EcoWatt Cloud flags update → EcoWatt Device downloads chunks → verifies → swaps image → reports result.  
* **Command Execution (Writes)**  
  * EcoWatt Cloud queues write → delivered at next slot → EcoWatt Device forwards to Inverter SIM → returns status next slot.  
* **Fault & Recovery**  
  * Network/API errors → backoff & retry.  
  * Integrity failures/tampered payloads → discard and alert.

3. # **Overview of Evaluation Criteria and Project Execution** {#overview-of-evaluation-criteria-and-project-execution}

   1. ## **Success Criteria / Acceptance Tests** {#success-criteria-/-acceptance-tests}

* **Compression/Buffering:** All samples fit in the 15‑minute payload; target compression ratio achieved.  
* **Config Flexibility:** Sampling rate / signal list changed remotely without firmware rebuild.  
* **FOTA Reliability:** Update with rollback safety demonstrated.  
* **Security:** Replay/tamper tests fail cleanly.  
* **Power Saving:** Demonstrated reduction in average MCU current with DVFS/sleep vs. naive always-on mode.  
* **Protocol Abstraction:** Minimal code change to swap Inverter SIM for a real inverter later.

  2. ## **High-Level Project Phases** {#high-level-project-phases}

* **Scaffold & Transport:** Basic EcoWatt Device ↔ EcoWatt Cloud link, 15‑min upload.  
* **Inverter SIM Integration:** Protocol adapter \+ successful read/write round trips.  
* **Buffering & Compression:** Implement and benchmark.  
* **Config & Command Path:** Remote parameter change \+ write execution.  
* **Security Layer:** Auth, encryption, MAC, anti‑replay.  
* **FOTA Pipeline:** Chunked download, verify, swap.  
* **Power Optimization:** DVFS/sleep integration \+ measurement.  
* **Integration & Testing:** End-to-end with fault & attack injection.

  3. ## **Milestone Breakdown and Marks Allocation** {#milestone-breakdown-and-marks-allocation}

* **Milestone 1:** System Modeling with Petri Nets and Scaffold Implementation (10% of total project grade)  
* **Milestone 2:** Inverter SIM Integration and Basic Acquisition (20% of total project grade)  
* TBF

4. # **Milestone 1: System Modeling with Petri Nets and Scaffold Implementation** {#milestone-1:-system-modeling-with-petri-nets-and-scaffold-implementation}

   1. ## **Objective** {#objective}

This milestone introduces you to **formal modeling and early-stage system design** using Petri Nets and a simplified scaffold implementation. You must:

* Use a Petri Net to model the behavior of a key workflow in the EcoWatt Device.  
* Build a scaffold (basic prototype) of that workflow in software.

  2. ## **Scope** {#scope}

     1. ## Part 1: Petri Net Modeling {#part-1:-petri-net-modeling}

You must create a Petri Net diagram modeling the following workflow:

* **Periodic polling** of the Inverter SIM to read parameters (e.g., voltage, current).  
* **Buffering** of polled data in memory.  
* **Uploading** buffered data to the cloud every 15 minutes.

To simplify testing and demonstration:

* You are allowed to simulate the 15-minute upload cycle using a 15-second interval.  
* Your Petri Net should clearly show:  
  * Places (states or resources)  
  * Transitions (events or actions)  
  * Tokens (data or events in motion)  
  * Optional: Conditions/guards (e.g., timer expired, buffer full)  
* You may use any tool (e.g., draw.io, Yasper, PIPE) or scan a clear hand-drawn diagram.

  2. ## Part 2: Scaffold Implementation {#part-2:-scaffold-implementation}

You must implement a basic working prototype of the behavior you modeled. What the scaffold must do:

* **Poll the Inverter SIM (or simulate it):** Read or generate fake voltage/current values every few seconds.  
* **Buffer the data:** Store the values in memory (e.g., a list, queue, or array).  
* **Upload periodically:** Every 15 seconds (simulated), print or send the buffered data.  
* **Modular code structure:** Organize code to reflect separate responsibilities for acquisition, buffering, and uploading.

Language Requirements:

* The scaffold implementation may be written in any language, such as Python, to simulate the logic easily.  
* However, be aware that the final project will be implemented in C/C++ for NodeMCU/ESP8266.  
* The structure of the scaffold should be designed with this future porting in mind.

  3. ## Part 3: Demonstration Video {#part-3:-demonstration-video}

You must submit a 5–7 minute video that includes:

* **Petri Net walkthrough:**  
  * Explain the logic and flow of your model.  
  * Clearly describe places, transitions, and what tokens represent.  
* **Scaffold code walkthrough:**  
  * Explain how the code reflects your Petri Net model.  
  * Identify which parts of the code map to which model elements.  
* You must keep your video on throughout the demo.

  4. ## What You Must Submit/Deliverables {#what-you-must-submit/deliverables}

* **Petri Net Diagram** (PDF or image)  
* **Scaffold Code** (GitHub repo link or downloadable ZIP)  
* **Video or the Video Link** (clearly accessible without having to log-in)  
* Zip all these into a single file and upload to Moodle. 

Notes:

* No report is required.  
* You do **not** need to implement compression, encryption, cloud auth, or FOTA at this stage.  
* Focus on clear structure and accurate modeling of system behavior.

  3. ## **Evaluation Rubric** {#evaluation-rubric}

| Criterion | Weight |
| ----- | :---: |
| Accuracy and clarity of the Petri Net model | 30% |
| Mapping between model and scaffold code | 20% |
| Correct functioning of polling, buffering, and upload | 20% |
| Clarity and completeness of the video explanation | 15% |
| Code modularity and readability | 15% |

5. # **Milestone 2: Inverter SIM Integration and Basic Acquisition** {#milestone-2:-inverter-sim-integration-and-basic-acquisition}

   1. ## **Objective** {#objective-1}

This milestone integrates the EcoWatt Device with the Inverter SIM and establishes correct round-trip communication for reading and writing inverter registers. You must:

* Use the selected Inverter SIM deployment option to connect the EcoWatt Device to the Inverter SIM.  
* Implement the protocol adapter that formats requests and parses responses.  
* Build a basic acquisition scheduler that polls inverter registers and stores readings in EcoWatt Device memory.

  2. ## **Scope** {#scope-1}

     1. ## Part 1: Protocol Adapter Implementation {#part-1:-protocol-adapter-implementation}

You must implement the protocol adapter to allow the EcoWatt Device to speak to the Inverter SIM. The implementation must:

* Handle request formatting and response parsing.  
* Detect and recover from timeouts and malformed frames.  
* Log protocol errors and provide simple retry logic for transient failures.

You are allowed to simulate the Inverter SIM responses for parts of testing if the live SIM is unavailable.

2. ## Part 2: Basic Data Acquisition {#part-2:-basic-data-acquisition}

You must implement an acquisition scheduler that:

* Polls a minimum set of registers, including voltage and current, at a configurable rate. You may poll once every 5 seconds for this milestone.  
* The registers polled by the EcoWatt Device must also be based on the acquisition tables and configurable during runtime (see “Remote Configuration Update” under Section 2.5). This will be implemented in a later milestone.  
* Stores raw samples in memory using a clear, modular structure separate from protocol code.  
* Performs at least one write operation to the Inverter SIM.

  3. ## Part 3: Demonstration Video {#part-3:-demonstration-video-1}

You must submit a 2 to 4-minute video that includes:

* Protocol adapter walkthrough that explains message formats and error handling.  
* An acquisition code walkthrough that maps components to the design.  
* Live demo of a successful read and write round-trip between EcoWatt Device and Inverter SIM.  
* You must keep your video on throughout the demo.  
* The video must be done by another member of the group who did not do the first milestone video.

  4. ## Resources Provided {#resources-provided}

The following resources will be provided to assist with implementation:

* Inverter SIM cloud API endpoint and documentation (includes request/response formats and register map): [Link](https://docs.google.com/document/d/12Q4TQ9upTPrM49k_jNWgm9N3mCi92AyzJESqgAlyLc4/edit?usp=sharing)

  5. ## What You Must Submit: Deliverables {#what-you-must-submit:-deliverables}

* Source code for protocol adapter and acquisition scheduler (you can submit a GitHub link).  
* Short protocol documentation and test scenarios.  
* Video or video link accessible without logging in.  
* Zip all these into a single file and upload to Moodle.

  3. ## **Evaluation Rubric** {#evaluation-rubric-1}

| Criterion | Weight |
| ----- | :---: |
| Correct functioning of read and write operations with the Inverter SIM | 30% |
| Clarity and correctness of protocol adapter implementation | 25% |
| Code modularity and readiness for porting to MCU | 20% |
| Clarity and completeness of the video explanation and live demo | 15% |
| Quality of documentation and test scenarios | 10% |

6. # **Milestone 3: Local Buffering, Compression, and Upload Cycle** {#milestone-3:-local-buffering,-compression,-and-upload-cycle}

   1. ## **Objective** {#objective-2}

This milestone implements local buffering and a lightweight compression scheme on the EcoWatt Device so that it can meet the upload payload constraint. It also implements the periodic upload cycle from the EcoWatt Device to the EcoWatt Cloud. You must:

* Design and implement a buffer that holds acquired samples for the upload interval.  
* Implement a lightweight compression method and benchmark it (delta/RLE or time‑series scheme) against the uncompressed code.  
* Implement the upload packetizer and perform periodic uploads (every 15 minutes).

  2. ## **Scope** {#scope-2}

     1. ## Part 1: Buffer Implementation {#part-1:-buffer-implementation}

  You must implement a local buffer that:

* Stores the full set of samples expected within the upload interval without data loss.  
* It is modular and separable from the acquisition and transport code.

  2. ## Part 2: Compression Algorithm and Benchmarking {#part-2:-compression-algorithm-and-benchmarking}

You must implement at least one lightweight compression technique, such as delta/RLE or time‑series scheme, and:

* Measure compression ratio and CPU overhead on data acquired from Inverter SIM  
* Document test methodology and results in a short benchmark report. Fields to include;  
  1. Compression Method Used:  
  2. Number of Samples:  
  3. Original Payload Size:  
  4. Compressed Payload Size:  
  5. Compression Ratio:  
  6. CPU Time (if measurable):  
  7. Lossless Recovery Verification:  
* Verify lossless decompression and data integrity.  
* Aggregation (min/avg/max) to meet payload cap.

  3. ## Part 3: Uplink Packetizer and Upload Cycle {#part-3:-uplink-packetizer-and-upload-cycle}

  You must implement the packetizer that:

* Finalizes a compressed block at the upload window and prepares the payload for transmission.  
* Implements retry/chunk logic for unreliable links.  
* Uses a placeholder or stub for encryption at this stage since security implementation will be done in a future milestone.  
* You may simulate the 15-minute upload interval using a 15-second interval for demonstrations.  
* 15‑minute tick: finalize compressed block → encrypt/MAC → transmit.  
  * Overall workflow: Wake → finalize block → encrypt/HMAC → upload to EcoWatt Cloud → receive ACK \+ pending configs/commands.  
* You must implement the cloud API endpoint that accepts the uploaded data and stores it. Students are expected to define both the client and server sides and implement the cloud API using Flask, NodeJS, or similar tools.

  4. ## Part 4: Demonstration Video {#part-4:-demonstration-video}

  You must submit a max 10-minute video that includes:

* Walkthrough of buffer design and its interaction with acquisition.  
* Compression explanation and benchmark summary.  
* Live demo of a complete acquisition to upload cycle, including payload size measurement.  
* You must keep your video on throughout the demo. The demo video must be presented by a group member who has not presented in either of the previous milestone videos.

  5. ## Resources Provided {#resources-provided-1}

The following resources will be provided to assist with implementation:

* Information required for the implementation:  
  * Max payload size that can be uploaded to EcoWatt Cloud: No specific value is defined to reduce the complexity of the scope. Report the compression ratio achieved after implementing 6.2.2. Furthermore, you need to implement aggregation (min/avg/max) to meet the payload cap, but you are not required to use it when sending data.  
  * You can assume that the local memory available on the EcoWatt Device is the same as the hardware that you are using (e.g., NodeMCU/ESP8266 MCU)

    6. ## What You Must Submit: Deliverables {#what-you-must-submit:-deliverables-1}

* Source code for buffer, compression, and packetizer modules.  
* Cloud API endpoint and documentation (includes request/response formats and register map).  
* Compression benchmark report with methodology and measured results.  
  * You may use bullet points, a table, or a brief markdown-style report for the benchmark.  
* Video or video link accessible without logging in.  
* Zip all items into a single file for upload to Moodle.

  3. ## **Evaluation Rubric** {#evaluation-rubric-2}

| Criterion | Weight |
| ----- | :---: |
| Correct buffering implementation and data integrity | 20% |
| Compression efficiency and benchmark quality | 20% |
| Robustness and correctness of the packetizer and upload logic | 20% |
| Correct cloud API implementation | 20% |
| Clarity of video demonstration, including payload measurements | 10% |
| Code readability and modularity for porting to the MCU | 10% |

  # 

7. # **Milestone 4: Remote Configuration, Security Layer & FOTA** {#milestone-4:-remote-configuration,-security-layer-&-fota}

   1. ## **Objective** {#objective-3}

This milestone enables runtime remote configuration from EcoWatt Cloud to EcoWatt Device, introduces command execution in the Inverter SIM, implements a lightweight security layer, and establishes a secure firmware-over-the-air (FOTA) update mechanism. You must:

* Implement runtime configuration updates that take effect after the next upload without reboot.  
* Implement command execution, where commands are executed in the Inverter SIM and results are reported back to the EcoWatt cloud.  
* Implement a security layer for authentication, integrity, confidentiality, and anti-replay.  
* Implement a FOTA module with secure download, verification, controlled reboot, and rollback.

  2. ## **Scope** {#scope-3}

     1. ## Part 1: Remote Configuration {#part-1:-remote-configuration}

You must implement a runtime configuration mechanism that:

* Accepts update requests of acquisition parameters such as sampling frequency, additional register reads (e.g., phase angle, active power), and similar parameters.  
* Applies each validated configuration update to all relevant subsystems (e.g., acquisition scheduler, register list). The parameters must take effect after the next upload at runtime without reboot or reflashing.  
* It is expected that configuration updates are applied in a thread-safe or non-blocking manner that does not interrupt ongoing acquisition/other running processes.  
* Handles duplicate or previously applied configurations gracefully (idempotent behavior).  
* Stores accepted parameters in non-volatile memory to survive power cycles.  
* Validates all new parameters and rejects unsafe/infeasible/invalid values with error reporting.  
* Confirms to EcoWatt Cloud which parameters were successfully applied and which were rejected. The cloud must maintain reports/logs of when updates occurred and their outcomes (including which parameters were updated and which were not, etc.).  
* Implement the EcoWatt Cloud behavior that facilitates this sequence. Using a Node-RED dashboard for this is acceptable.

  2. ## Part 2 – Command Execution {#part-2-–-command-execution}

You must implement a command execution flow that:

* EcoWatt Cloud queues a write command.  
* At the next transmission slot, the EcoWatt Device receives the queued command.  
* The EcoWatt Device forwards the command to the Inverter SIM.  
* The Inverter SIM executes the command and generates a status response.  
* At the following slot, the EcoWatt Device reports the execution result back to EcoWatt Cloud.  
* EcoWatt Cloud must maintain logs of all commands and results for both EcoWatt Device and EcoWatt Cloud.

  3. ## Part 3: Security Implementation {#part-3:-security-implementation}

You must implement a practical, lightweight security layer suitable for MCU constraints that must not rely on heavyweight cryptographic libraries or hardware secure elements. It must include:

* Pre-shared key-based mutual authentication (PSK \+ HMAC).  
* Payload confidentiality and integrity using a suitable encryption scheme for payloads.  
* Anti‑replay via nonces/sequence numbers.  
* Secure key storage appropriate for MCU constraints.

  4. ## Part 4: FOTA Module {#part-4:-fota-module}

You must implement a secure firmware-over-the-air mechanism that:

* Supports chunked firmware download across multiple communication intervals and resumes after interruptions.  
* Verifies firmware integrity and authenticity.  
* Uses a rollback approach so that if verification fails or the new firmware fails to boot, the device reverts to the previous firmware.  
* Performs a controlled reboot only after successful verification.  
* Reports progress, verification results, reboot status, and final boot confirmation to EcoWatt Cloud.  
* Maintains detailed logs for all FOTA operations in the EcoWatt Cloud.  
* Implement the EcoWatt Cloud behavior that facilitates this sequence.

  5. ## Part 5: Demonstration Video {#part-5:-demonstration-video}

You must submit a max 10-minute video that includes:

* Configuration updates showing that the device verifies, correctly applies updates internally, and reports success/failure.  
* Command execution round-trip (EcoWatt Cloud → EcoWatt Device → Inverter SIM → EcoWatt Cloud).  
* Secure transmission demo showing authentication and integrity checks.  
* Full FOTA update including chunked download, verification, controlled reboot, and confirmation of new firmware.  
  * At least one FOTA demo must include a simulated failure and demonstrate a rollback to the previous version.  
* You must keep your video on throughout the demo.   
* The demo video must be presented by a group member who has not presented in either of the previous milestone videos.

  6. ## Resources Provided {#resources-provided-2}

The following resources will be provided to assist with implementation:

* [Milestone 4 - Resources - In21-EN4440-Project Outline](https://docs.google.com/document/d/1i6U_njSrn5oQf0JZs-8GpcQY5Ruh5C1NaT3PF8N_KGo/edit?usp=sharing)

  7. ## What You Must Submit: Deliverables {#what-you-must-submit:-deliverables-2}

* Source code implementing the functionalities.  
* Documentation detailing supported configuration parameters, how the device applies them internally, command execution flow, security design, and FOTA workflow, including rollback handling.  
* Video or video link accessible without requiring a login.  
* Zip all items into a single file for upload to Moodle.

  3. ## **Evaluation Rubric** {#evaluation-rubric-3}

| Criterion | Weight |
| ----- | :---: |
| Successful runtime application of remote configuration parameters | 15% |
| Correct and reliable command execution with reporting | 15% |
| Correctness and completeness of security implementation | 25% |
| Correctness and reliability of FOTA implementation, including rollback | 30% |
| Clarity and completeness of the video explanation and live demo | 10% |
| Quality of documentation and test scenarios | 5% |

# 

8. # **Milestone 5: Fault Recovery, Power Optimization & Final Integration** {#milestone-5:-fault-recovery,-power-optimization-&-final-integration}

   1. ## **Objective** {#objective-4}

This milestone integrates power-saving features and performs end-to-end integration testing, including fault handling and recovery. You must:

* Apply power management techniques to reduce average MCU current and document savings.  
* Perform full integration tests that demonstrate all major features working together.

  2. ## **Scope** {#scope-4}

     1. ## Part 1: Power Management and Measurement {#part-1:-power-management-and-measurement}

You must integrate power-saving mechanisms that include:

* Clock scaling when idle; sleep between polls/uploads.  
* Peripheral gating (disable comm blocks outside windows).  
* Optional self-energy-use reporting.  
* Prepare a short report comparing the before and after results.

You must apply as many of these mechanisms as are technically feasible, with justification for choices and measurement of resulting savings

2. ## Part 2: Fault Recovery {#part-2:-fault-recovery}

You must implement fault recovery mechanisms that:

* Handle Inverter SIM timeouts, malformed frames, and buffer overflow.  
* Maintains a local event log. Sample log shown below:

![][image2]

Faults can be simulated using the provided API tools or pre-crafted payloads (e.g., malformed SIM responses, corrupted JSON, or network timeouts).

3. ## Part 3: Final Integration and Fault Testing {#part-3:-final-integration-and-fault-testing}

   You must perform full system integration tests that cover:

* Acquisition, buffering, compression, secure upload, remote configuration, command execution, FOTA, and power management.  
* Fault injection tests for network errors, corrupted payloads, and interrupted FOTA, showing recovery behavior.  
* You must submit a unified firmware build that integrates all major features implemented in Milestones 1–4 (e.g., config, command, security, FOTA, etc.) and demonstrate their interoperability under typical operating conditions and during controlled fault scenarios (e.g., SIM timeout, network dropout, FOTA hash mismatch)

  4. ## Part 4: Live Demonstration {#part-4:-live-demonstration}

* Demonstrate the end-to-end system behavior based on a system integration checklist (provided), verifying each feature was tested under normal and fault conditions.   
* This checklist will be marked during the live demonstration

  5. ## Resources Provided {#resources-provided-3}

The following resources will be provided to assist with implementation:

* [Milestone 5 - Resources - In21-EN4440-Project Outline](https://docs.google.com/document/d/1w_bU9ZoMP-TubxwHeYtWWkH3hAuarNOC3LQeACf26ng/edit?usp=sharing)  
* Updated [In21-EN4440-API Service Documentation](https://docs.google.com/document/d/12Q4TQ9upTPrM49k_jNWgm9N3mCi92AyzJESqgAlyLc4/edit?usp=sharing) document

  6. ## What You Must Submit: Deliverables {#what-you-must-submit:-deliverables-3}

* Full source code for the integrated project, including FOTA and power management features.  
* Power measurement report summarizing methodology and before and after results.  
* Zip all items into a single file for upload to Moodle.

  3. ## **Evaluation Rubric** {#evaluation-rubric-4}

| Criterion | Weight |
| ----- | :---: |
| Measured power savings and quality of the measurement report | 20% |
| End-to-end system performance and fault recovery correctness | 40% |
| Clarity of video demonstration, including payload measurements | 10% |
| Quality of documentation and test scenarios | 5% |
| Code readability and modularity for porting to the MCU | 25% |

# 

[image1]: <data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAqQAAAALCAYAAACpk5+7AAAAVElEQVR4Xu3WIQEAIADAMFrSiFCUA08AbiZmnuBj7nUAAKAy3gAAAD8ZUgAAUoYUAICUIQUAIGVIAQBIGVIAAFKGFACAlCEFACBlSAEASBlSAABSF0wNIW5pFw2VAAAAAElFTkSuQmCC>

[image2]: <data:image/png;base64,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>