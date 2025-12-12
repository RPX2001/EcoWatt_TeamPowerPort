# 📚 EcoWatt Documentation Index

Welcome to the comprehensive documentation for the EcoWatt Smart Inverter Monitoring System.

---

## 📖 Documentation Structure

### 🌟 [Features Guide](FEATURES.md)
**Comprehensive feature explanations with implementation details**

Covers:
- Multi-layer security architecture (Upload & FOTA)
- Intelligent compression system (4 algorithms)
- Power management strategies
- Remote control system
- Web dashboard capabilities
- Modbus communication architecture

**Best for**: Understanding what the system does and how features work internally

---

### 🏛️ [System Architecture](ARCHITECTURE.md)
**Complete system design, data flows, and technical architecture**

Covers:
- High-level system overview
- ESP32 firmware architecture (Petri Net state machine)
- Backend architecture (Flask handlers & routes)
- Frontend architecture (React components)
- Database schema and design
- Communication protocols (Modbus, HTTPS, MQTT, WebSocket)
- Performance optimizations and scalability

**Best for**: Understanding system design and technical implementation

---

### 🔐 [Security Guide](SECURITY.md)
**Cryptographic implementations, threat model, and security best practices**

Covers:
- Complete threat model and attack vectors
- Upload security (3-layer defense: Nonce, HMAC, AES)
- FOTA security (AES-256-CBC, SHA-256, RSA-2048)
- Key management and rotation strategies
- Attack mitigations (replay, MITM, forgery)
- Security best practices for deployment

**Best for**: Understanding security implementations and hardening the system

---

### ⚙️ [Implementation Notes](IMPLEMENTATION.md)
**Simulation vs Production architecture differences**

Covers:
- Current WiFi simulation implementation
- Production Modbus RTU architecture
- Power management differences
- Hardware requirements
- Configuration parameters
- Migration path from development to production
- Performance expectations

**Best for**: Understanding implementation context and deployment planning

---

### 🚀 [Setup Guide](SETUP.md)
**Complete installation and configuration guide**

Covers:
- Prerequisites and requirements
- One-command setup process
- Detailed manual setup (Backend, Frontend, ESP32)
- Network configuration
- Security key generation and deployment
- Database initialization
- Troubleshooting common issues
- Production deployment strategies

**Best for**: Getting the system up and running

---

### 📡 [API Reference](API.md)
**Complete REST API documentation**

Covers:
- Device management endpoints
- Data upload endpoints
- Power data queries
- Command system API
- OTA update endpoints
- Diagnostics API
- Security validation
- Error codes and rate limiting

**Best for**: Integrating with the backend or developing new features

---

## 🗺️ Quick Navigation

### For New Users
1. Start with **[Setup Guide](SETUP.md)** to install the system
2. Read **[Features Guide](FEATURES.md)** to understand capabilities
3. Explore **[System Architecture](ARCHITECTURE.md)** for technical details

### For Developers
1. Review **[System Architecture](ARCHITECTURE.md)** for design patterns
2. Study **[API Reference](API.md)** for endpoint details
3. Check **[Security Guide](SECURITY.md)** for security implementation

### For Security Auditors
1. Read **[Security Guide](SECURITY.md)** for threat model
2. Review **[Features Guide](FEATURES.md)** for security features
3. Check **[API Reference](API.md)** for attack surfaces

---

## 📊 Documentation Metrics

| Document | Size | Topics | Difficulty |
|:---------|:-----|:-------|:-----------|
| **Features** | 17 KB | 7 major features | Intermediate |
| **Architecture** | 23 KB | System design | Advanced |
| **Security** | 24 KB | Cryptography | Advanced |
| **Setup** | 9 KB | Installation | Beginner |
| **API** | 13 KB | REST endpoints | Intermediate |

**Total**: ~86 KB of comprehensive documentation

---

## 🔍 Topics Index

### Security Topics
- [HMAC-SHA256 Authentication](SECURITY.md#hmac-sha256-authentication)
- [Nonce Anti-Replay Protection](SECURITY.md#nonce-management)
- [RSA-2048 Firmware Signing](SECURITY.md#rsa-key-generation)
- [AES Encryption](SECURITY.md#aes-128-cbc-encryption)
- [Threat Model](SECURITY.md#threat-model)

### Architecture Topics
- [ESP32 State Machine](ARCHITECTURE.md#petri-net-state-machine)
- [Backend Structure](ARCHITECTURE.md#flask-server-structure)
- [Database Schema](ARCHITECTURE.md#database-schema)
- [Communication Protocols](ARCHITECTURE.md#communication-protocols)
- [Data Flow Diagrams](ARCHITECTURE.md#data-flow-diagrams)

### Features Topics
- [Compression Algorithms](FEATURES.md#intelligent-algorithm-selection)
- [Power Gating](FEATURES.md#peripheral-gating-strategy)
- [Command System](FEATURES.md#command-queue-architecture)
- [FOTA Updates](FEATURES.md#fota-updates)
- [Ring Buffer](FEATURES.md#ring-buffer-architecture)

### Setup Topics
- [Quick Setup](SETUP.md#quick-setup)
- [Backend Configuration](SETUP.md#backend-configuration)
- [ESP32 Flashing](SETUP.md#3-esp32-firmware-setup)
- [Troubleshooting](SETUP.md#troubleshooting)
- [Production Deployment](SETUP.md#production-deployment)

### API Topics
- [Device Management](API.md#device-management)
- [Data Upload](API.md#data-upload)
- [Commands](API.md#commands)
- [OTA Updates](API.md#ota-updates)
- [Error Codes](API.md#error-codes)

---

## 🎯 Use Case Guides

### "I want to understand the compression system"
1. Read [Compression System](FEATURES.md#intelligent-algorithm-selection)
2. See [Compression Handler](ARCHITECTURE.md#backend-architecture)
3. Check compression-related [API endpoints](API.md#data-upload)

### "I want to secure my deployment"
1. Study [Security Guide](SECURITY.md) completely
2. Follow [Security Setup](SETUP.md#security-setup)
3. Review [Security Best Practices](SECURITY.md#security-best-practices)

### "I want to add a new command type"
1. Understand [Command System](FEATURES.md#command-queue-architecture)
2. Review [Command API](API.md#commands)
3. Study [Command Handler](ARCHITECTURE.md#backend-architecture)

### "I want to deploy to production"
1. Follow [Setup Guide](SETUP.md) for initial setup
2. Implement [Security Best Practices](SECURITY.md#security-best-practices)
3. Use [Production Deployment](SETUP.md#production-deployment) guide

---

## 📝 Additional Resources

### In Repository
- `README.md` - Main project overview
- `flask/` - Backend source code with inline comments
- `PIO/ECOWATT/` - ESP32 firmware with detailed comments
- `front-end/` - React dashboard source code

### External Resources
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [React Documentation](https://react.dev/)
- [Modbus Protocol Specification](https://modbus.org/specs.php)

---

## 🤝 Contributing to Documentation

If you find errors or want to improve documentation:

1. Documentation uses Markdown format
2. Follow existing structure and style
3. Include code examples where helpful
4. Add diagrams for complex concepts
5. Keep language clear and concise

---

## 📧 Support

For questions or issues:
- Check relevant documentation section
- Review [Troubleshooting Guide](SETUP.md#troubleshooting)
- Consult source code comments
- Contact project maintainers

---

<div align="center">

**[← Back to Main README](../README.md)**

---

**EcoWatt Documentation** | University of Moratuwa | EN4440 Project  
Last Updated: December 12, 2024

</div>
