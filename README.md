# What is this project about

A SAMW25-based IoT device which can use a mini thermal printer to print out the handwriting inputted from an embedded touchscreen or downloaded from the cloud. We name it "IoT Memo Printer". Addtionally, with a pair of such printers, two users can communicate remotely in an old-fashioned way.

It has a 2.4-inch touchscreen for users to write down texts (using a finger or a resistive touchscreen pen) like writing a letter or to draw a picture. The text/drawing can be printed locally, or sent through Wi-Fi and Internet to another user that has the same device. Before printing or sending out the text/drawing, the user can preview it on the touchscreen display.

We also plan to add an additional feature: handwriting recognition. For the characters input by touchscreen, the user can choose the recognition mode that can transform the handwritings to texts. This is done by sending the handwriting to a cloud server running a handwriting recognition program, and receiving the recognized texts in real time.

# How we built it

Yes. We built it from the scratch! Everything from designing the circuit board to writing the code is done by my partner and me.

## System Diagram

![system-diagram](https://user-images.githubusercontent.com/55012776/162537519-8bff6a82-c7ea-4335-869f-d5a9da1c383b.png)

## PCB Design

We used Altium Designer to architect the circuit layout, draw the schematics, and route the PCB.

### Main Schematic

![main-schematic](https://user-images.githubusercontent.com/55012776/162539013-0c5d6ab4-57d9-44e6-9b66-cbda211a9aa8.png)

### PCBA Model Top View

![pcba-model-top-view](https://user-images.githubusercontent.com/55012776/162539799-f190728a-ba7f-4552-95b8-7313485f47c6.png)

### 3D Model

(To be added)

### More Details

Draftsman documentation and project files under Altium-Project/ directory

## Firmware Development

We used Atmel Studio as the IDE to develop the firmware for our device.

### Command Line Tool

We implemented a command line tool to interact with FreeRTOS kernel for debugging purpose.

### Peripheral Drivers

We developed FreeRTOS-compatible I2C, SPI, and UART drivers for sensors and other peripherals.

### Interacting with Cloud

We Implemented a Node-RED instance on the MCU to send data to and receive data from the cloud using MQTT.

### Bootloader for OTAFU

We developed a bootloader for the MCU and generated a complete OTA(on-the-air) firmware update solution.

### More details to be added...
