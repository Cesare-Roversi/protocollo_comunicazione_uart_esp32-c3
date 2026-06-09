# Molecubes - Modular Robotic Arm

<div align="center">
<img src="assets/logo.png" alt="Molecubes Logo">
</div>

---

<div align="center">

  ![License](https://img.shields.io/github/license/EmbeddedDreamTeam/molecubes?style=for-the-badge&logoSize=auto&labelColor=blue&color=black)  ![Version](https://img.shields.io/badge/Pre-RELEASE?style=for-the-badge&logoSize=auto&label=Molecubes%20version&labelColor=white&color=black)  ![Stars](https://img.shields.io/github/stars/EmbeddedDreamTeam/molecubes?style=for-the-badge&logo=github&logoColor=black&logoSize=auto&labelColor=gold&color=black)  ![Contributors](https://img.shields.io/github/contributors/EmbeddedDreamTeam/molecubes?style=for-the-badge&logo=github&logoColor=white&logoSize=auto&label=contributors&labelColor=green&color=black)  ![Issues](https://img.shields.io/github/issues/EmbeddedDreamTeam/molecubes?style=for-the-badge&logoSize=auto&label=Issues&labelColor=red&color=black) 
</div>

---

## Table of Contents

<details>
  
- [Molecubes - Modular Robotic Arm](#molecubes---modular-robotic-arm)
  - [Table of Contents](#table-of-contents)
  - [Idea of the project](#idea-of-the-project)
  - [Requirements](#requirements)
    - [Hardware Requirements](#hardware-requirements)
    - [Software Requirements](#software-requirements)
  - [Getting Started](#getting-started)
    - [Setting up the Base Module](#setting-up-the-base-module)
    - [Setting up the Cube Modules](#setting-up-the-cube-modules)
    - [Setting up Wiring](#setting-up-wiring)
    - [Setting up ESP32 Software](#setting-up-esp32-software)
    - [Setting up Python Communication](#setting-up-python-communication)
  - [Project Layout](#project-layout)
  - [User guide](#user-guide)
  - [Team Members](#team-members)
  - [Resources](#resources)
  - [License](#license)

</details>

## Idea of the project 



The main idea of the Molecubes project is to build a modular robotic arm that utilizes a unique approach to movement through motor twisting. This design provides significantly more freedom of movement, allowing the arm to achieve various positions with different twists. The arm consists of interconnected cubes, each containing a servomotor, connected via magnetic connectors for easy assembly and reconfiguration.Thanks to the modular nature of the arm we can make use of differents attachment and cube layout that best suites a precise situation. 

The system is controlled by an ESP32-C3 microcontroller in the base module, with communication handled through Python scripts for user interaction and control.

## Requirements 


### Hardware Requirements 

- <u>**ESP32-C3**</u> microcontroller for the base module
- 3D printed housing for the base module
- Full set of cables (male-to-male, male-to-female)
- For each cube module:
  - 1x <u>**3D printed housing**</u>
  - 1x <u>**Servomotor**</u>
  - <u>**Set of cables**</u>
  - 2x <u>**Headed pin magnetic connectors**</u>

- 1x <u>**Breadboard**</u> (optional for prototyping)

<div style="display: flex; justify-content: space-between; align-items: center;">
    <img src="assets/esp32.png" alt="ESP32-S3" width="150" style="float: right; margin-left: 20px;">
    <img src="assets/servomotor.png" alt="Servomotor" width="100" style="float: right; margin-left: 20px;">
    <img src="assets/magnetic_connector.png" alt="Magnetic Connector" width="100" style="float: right; margin-left: 20px;">
    <img src="assets/cube_1.png" alt="Cube 1" width="100" style="float: right; margin-left: 20px;">
    <img src="assets/cube_2.png" alt="Cube 2" width="100" style="float: right; margin-left: 20px;">
</div>
<div style="display: flex; justify-content: space-between; align-items: center;">
    <img src="assets/timelapse_3d_model.gif" alt="Creation"  >
</div>


### Software Requirements 

- [PlatformIO IDE](https://platformio.org/)
- Python IDE (e.g. [PyCharm](https://www.jetbrains.com/pycharm/), [VS Code](https://code.visualstudio.com/))

#### Setting Up Python 

To run this project, you need to have PlatformIO IDE and Python installed along with the required dependencies.

## Getting Started 

---

### Setting up the Base Module 

The project begins with the assembly of the base module:

1. **3D Print the Base Housing**: Print the base housing using PLA material.
2. **Install ESP32-C3 and servo motor**: Mount the ESP32-C3 microcontroller, servomotor and connectors inside the housing.
3. **Flash the base esp32 with the root code**: Run PlatformIO esp32-c3-root task to upload the firmware.
3. **Connect Power and Interfaces**: Ensure proper connections for power supply, do not power more than a cube with usb.

### Setting up the Cube Modules 

Each cube module requires the following assembly:

1. **3D Print the Cube Housing**: Print the cube housing using PLA material.
2. **Install Servomotor and ESP32-C3**: Mount the servomotor,ESP32-C3 and connectors inside the housing, ensuring proper alignment for twisting motion.
3. **Attach Magnetic Connectors**: Install the 2 headed pin magnetic connectors on opposite faces of the cube for modular connection.
4. **Wire the Components**: Connect the servomotor to the appropriate cables for power and signal transmission.

### Setting up Wiring 

---

The Molecubes system uses a **daisy-chain UART communication topology**:
- The **base module (ROOT)** is the primary master
- Each **cube module** acts as both a **SLAVE** to its predecessor and a **MASTER** to the next cube
- This allows commands to propagate through the chain: ROOT → Cube 1 → Cube 2 → Cube 3, etc.

#### Pin Configuration

```c
// ROOT Module (Base) - Only acts as MASTER
#define U_WITH_MASTER 0
#define FROM_MASTER_RX 10    // Receive data from Cube 1 on pin 10
#define TO_MASTER_TX 9       // Transmit data to Cube 1 on pin 9

// CUBE Module - Acts as both SLAVE and MASTER
// SLAVE Configuration (receives from previous module)
#define U_WITH_SLAVE 1
#define FROM_SLAVE_RX 2      // Receive data from previous module on pin 2
#define TO_SLAVE_TX 3        // Transmit data to previous module on pin 3

// MASTER Configuration (transmits to next module)
#define FROM_MASTER_RX 10    // Receive data from next cube on pin 10
#define TO_MASTER_TX 9       // Transmit data to next cube on pin 9
```

#### Communication Chain Diagram

```
Root (Master Only)
       ↓↑ (pins 9,10)
    Cube 1 (Slave ↓, Master ↑)
       ↓↑ (pins 3,2) ← (pins 9,10)
    Cube 2 (Slave ↓, Master ↑)
       ↓↑ (pins 3,2) ← (pins 9,10)
    Cube 3 (Slave ↓, Master ↑)
```

#### Wiring Steps

1. **Power Supply**: 
   - Connect the ESP32-C3 base to a stable power source (5V).
   - Ensure each cube module receives proper power through the magnetic connectors.
   - Do not power more than one cube via USB simultaneously to avoid power supply issues.

2. **Root Module (BASE) UART Connections**:
   - Pin 10 (FROM_MASTER_RX): Receives serial data from Cube 1's TO_SLAVE_TX
   - Pin 9 (TO_MASTER_TX): Transmits serial data to Cube 1's FROM_SLAVE_RX

3. **Cube Module UART Connections** (as Slave to previous module):
   - Pin 2 (FROM_SLAVE_RX): Receives serial data from the previous module (chained)
   - Pin 3 (TO_SLAVE_TX): Transmits serial data to the previous module (chained)

4. **Cube Module UART Connections** (as Master to next module):
   - Pin 10 (FROM_MASTER_RX): Receives serial data from the next cube (if connected)
   - Pin 9 (TO_MASTER_TX): Transmits serial data to the next cube (if connected)

5. **Module Connections**: 
   - Use the magnetic connectors to link cube modules in series to the base.
   - Connection pattern: Root TX (pin 9) → Cube 1 RX (pin 2), Root RX (pin 10) ← Cube 1 TX (pin 3)
   - Each subsequent cube follows the same pattern: Cube N TX (pin 3) → Cube N+1 RX (pin 2), Cube N RX (pin 10) ← Cube N+1 TX (pin 9)

6. **Servomotor Signal Cables**: 
   - Route signal cables from each servomotor to the appropriate GPIO pins on the ESP32-C3.
   - Ensure proper power delivery to servomotors through dedicated power lines.

### Setting up ESP32 Software 

---

To program the ESP32-C3, follow these steps:

1. **Install PlatformIO IDE**
   - Download and install PlatformIO IDE from the official website: [PlatformIO Download](https://platformio.org/platformio-ide).
   - Open the project in PlatformIO.

2. **Build and Upload**
   - For the base module, upload the `esp32-c3-root` configuration.
   - For cube modules, upload the `esp32-c3-devkitm-1` configuration.


#### Project Layout 

```
molecubes/
├── platformio.ini                    # PlatformIO configuration
├── src/
│   ├── CMakeLists.txt                # Build configuration for ESP32 targets
│   ├── esp32-c3/
│   │   └── main.cpp                  # Root firmware for ESP32-C3 base module
│   └── esp32-s3/
│       └── main.cpp                  # Optional ESP32-S3 target firmware
├── lib/
│   ├── cinematics/                   # Kinematic control and motion helpers
│   ├── uart_code/                    # UART communication and command handling
│   └── wifi_code/                    # WiFi initialization and network management
├── include/
│   ├── cinematics_header/            # Servo and motion headers
│   ├── uart_headers/                 # UART and protocol headers
│   └── wifi_headers/                 # WiFi and TCP server headers
├── python client/
│   ├── esp32_client.py               # Python communication script
│   └── requirements.txt              # Python dependencies
├── esp32-c3/                         # ESP32-C3-specific board configuration files
├── esp32-s3/                         # ESP32-S3 board configuration defaults
```

* **`platformio.ini`**: Project configuration for PlatformIO and build environments.
* **`src/esp32-c3/main.cpp`**: Main firmware for the ESP32-C3 base/root module.
* **`lib/`**: Contains source implementation for kinematics, UART, and WiFi modules.
* **`include/`**: Contains header declarations for the firmware modules in `lib/`.
* **`python client/`**: Python-side communication tools for controlling the arm.

## User guide

1. **Power Up the Root Module**: Connect power to the root ESP32-C3 module and wait for it to initialize.
2. **Connect to WiFi**: The root module creates a WiFi access point. Connect your computer to this WiFi network.
3. **Start Python Script**: Run the Python client script (`esp32_client.py`) to establish a connection with the root module.
4. **Control Modes**:
   - **Manual Mode**: Input specific parameters including angles, speed, acceleration, and jerk for precise control. While in manual mode, you can hot-swap cubes and various attachments without interrupting operation.
   - **Auto Mode**: Run predefined sequences that execute automatically, following programmed movement patterns.

## Team Members 

- **Marco Adami** — `marco.adami@studenti.unitn.it`
  - Major developer of the WiFi connectivity section
- **Matteo Ballardin** — `matteo.ballardin@studenti.unitn.it`
  - Minor developer of the WiFi connectivity section
  - Minor developer of kinematics controls
  - Slide and documentation mantainer
- **Mattia Pistollato** — `mattia.pistollato@studenti.unitn.it`
  - 3D modelling of the cubes
  - Hand construction of the cubes
  - Major developer of kinematics controls
  - Camera man and video editor
- **Cesare Roversi** — `cesare.roversi@studenti.unitn.it`
  - Major developer of inter-cube connectivity

## Resources 

- YouTube: https://www.youtube.com/watch?v=TO-N0g9IO8k
- Presentation: https://docs.google.com/presentation/d/1EzeCRdcIliSEbOmkrEr7zNEghcpyGPThB2PobICRaiw/edit?usp=sharing

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

*For more information, please refer to the project documentation or contact the development team.*
