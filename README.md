# GDx (Ground Drone Experimental)

**GDx** is a dual-MCU robotics platform designed for localized surveillance and remote exploration. It features a high-bandwidth MJPEG video stream using the **OV3660** sensor and a custom 2-DOF Pan-Tilt camera system.

---

## 📸 Project Gallery

### Hardware Overview
The drone utilizes a 4WD aluminum chassis designed for versatile power delivery. The "Brain" consists of an **Arduino Uno** for locomotion and an **ESP32-CAM** for high-level networking and vision.

![Ground Drone Hardware](assets/GDx_Photo.png)

---

### Control Interface
The GDx is operated via a custom mobile-responsive web application. The interface provides real-time movement controls, granular camera gimbal adjustment, and a dedicated toggle for the onboard flash LED.

| **Normal View** | **Flash Active** |
| :---: | :---: |
| ![Web App Dark](assets/WebAppDark.jpg) | ![Web App Flash](assets/WebAppFlash.jpg) |
| *Standard MJPEG stream in low-light.* | *Integrated LED Flash enabled for high-visibility.* |

---

## ⚙️ Hardware Specifications
* **Camera Module**: **OmniVision OV3660** (3MP).
    * *Firmware is specifically optimized for OV3660 registers to ensure stable MJPEG streaming.*
* **Microcontrollers**: 
    * **ESP32-CAM**: Handles the WebSocket server, MJPEG streaming, and WiFi AP logic.
    * **Arduino Uno**: Manages the L298N motor driver and SG90 servo PWM signals.
* **Power Delivery**: **7.4V Battery**.
    * **Supported Chemistries**: The system is compatible with both **Li-Ion** and **LiPo** power sources.
    * *A stable 7.4V nominal output is required to ensure motor torque and MCU logic stability.*

---

## 📡 Networking & Dashboard Access

To access the GDx dashboard, you must manually connect to the drone's localized network. For security and to reduce spectral noise, the **Access Point (AP) is hidden**.

1.  **Search for Hidden Network**: Open your device's WiFi settings and select "Add Network" or "Other".
2.  **Input SSID**: Manually type the SSID defined in the `ESP32-CAM_Control.ino` code.
3.  **Input Password**: Enter the WPA2 passphrase.
4.  **Set Hidden Status**: You must toggle the **"Hidden Network"** option to **Yes** in your device settings to successfully authenticate.
5.  **Access URL**: Once connected, navigate to the ESP32-CAM's gateway IP (default `192.168.4.1`) in your browser.

---

## 🛠 Deployment Notes

* **Programming Protocol**: When uploading code to the Arduino, disconnect the ESP32-CAM to avoid Serial bus conflicts. If the connection is permanent, hold the **Reset** button on the ESP32-CAM during the Arduino upload to put it in a cutoff state.
* **RF Environment**: Outdoor operation is recommended to maximize range. Nearby 2.4GHz devices can cause interference that degrades the MJPEG video feed.
* **Range Extension**: The ESP32-CAM range can be extended by attaching an external antenna. This requires moving the 0-ohm resistor on the board to the external antenna pads.

---

## 🔍 Troubleshooting

| Issue | Solution |
| :--- | :--- |
| **Cannot connect to AP** | Ensure you have enabled "Connect to hidden network" and press the **Reset** button on the ESP32-CAM. |
| **No SSID visible** | This is intended as the network is hidden. Manually add the credentials. If it still fails, check the battery voltage. |
| **Video feed lost** | Refresh the web page. If the issue persists, reset the ESP32-CAM. Ensure no physical obstructions are blocking the link. |
| **Continuous Spinning** | If the drone spins after an ESP32-CAM reset, press the **Arduino Reset** button to clear the motor buffer. |

---

## **License**
This project is released under the **MIT License**.