# LED Matrix Sign

![led-matrix-sign](https://sandr.in/assets/img/led-matrix/led-matrix-02.jpg)

This repository contains the code for the replica of the MBTA's arrival board 
that I built. See more info at https://sandr.in/objects/led-matrix

## Build

1. Install the [`arduino-cli` command line tools](https://arduino.github.io/arduino-cli/1.1/installation/).

    ```
    brew install arduino-cli
    ```

2. Run `make dependencies` in order to:
    * Install the Arduino esp32 board core.
    * Install the Arduino library dependencies for this project.

3. Find the port and name corresponding to your board, and set it in the `Makefile`:
    ```
    BOARD_PORT=/dev/tty.usbserial-0001
    BOARD_NAME=esp32:esp32:esp32da
    ```