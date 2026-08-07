# Signaloid SoC application
This directory holds the application that runs on the Signaloid C0-microSD.

## How to flash
1. Modify the `TOOLKIT` flag in the `Makefile` to point to your C0-microSD toolkit, and the `DEVICE` flag to point to your C0-microSD device path.
2. Run `make` to build the application.
3. Run `make flash` and `make switch` (the green LED should blink). Both targets use `sudo`.
4. Power cycle the C0-microSD (the green LED should light up).
