# Project 2 – Sistemas Ciberfísicos

## Overview

This repository contains the source code and supporting files developed for Project 2 of the "Sistemas Ciberfísicos" (Cyber-Physical Systems - CPS) course. 
The project combines an Arduino-based embedded system with a Python application that communicates through Bluetooth Low Energy (BLE). 
It also includes the dataset used during development and the final project report.

## The Project

The final implementation allows a player to play Tic-Tac-Toe against the camera. Due to the mechanical limitations of the robotic arm used in the project, only one game configuration was implemented: the human player always plays with **O**, while the camera-controlled opponent always plays with **X**.

## Contents

- **arduino_ttt_ble.cpp**  
  Arduino source code implementing the embedded application and BLE communication.

- **nicla_ttt_ble.py**  
  Python application responsible for connecting to the Arduino device, receiving data, and executing the application logic.

- **dataset_cells/**  
  Image dataset used for training, testing, and validating the project.

- **P2_SC.pdf**  
  Final report describing the project objectives, implementation, results, and conclusions.

## Requirements

- Arduino IDE
- OpenMV
- Python 3.x
- Required Python libraries (see source code imports)

## Authors

Rodrigo Freitas, Leonardo Gonçalves

## Final Note

This implementation was presented at Rock in Rio 2026 with slight changes to improve the communication and other aspects
