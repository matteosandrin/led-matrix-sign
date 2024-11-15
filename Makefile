ARDUINO_CLI=arduino-cli
BOARD_PORT=/dev/tty.usbserial-0001
BOARD_NAME=esp32:esp32:esp32da
BUILD_DIR=./build
ARDUINO_LIB_INSTALL_CMD=${ARDUINO_CLI} lib install
ARDUINO_COMPILE_OPTIONS=-v --fqbn ${BOARD_NAME} --build-path ${BUILD_DIR} --port ${BOARD_PORT}

dependencies:
	${ARDUINO_CLI} core install esp32:esp32@2.0.14
	${ARDUINO_LIB_INSTALL_CMD} "Adafruit GFX Library"@1.11.9
	${ARDUINO_LIB_INSTALL_CMD} "ESP Async WebServer"@3.3.22
	${ARDUINO_LIB_INSTALL_CMD} "ESP32 HUB75 LED MATRIX PANEL DMA Display"@3.0.10
	${ARDUINO_LIB_INSTALL_CMD} ArduinoJson@6.19.4
	${ARDUINO_LIB_INSTALL_CMD} AsyncTCP@1.1.1 
	${ARDUINO_LIB_INSTALL_CMD} Button2@2.0.3 
	${ARDUINO_LIB_INSTALL_CMD} TJpg_Decoder@1.0.5 

upload:
	touch ${BUILD_DIR}/file_opts
	${ARDUINO_CLI} compile --upload ${ARDUINO_COMPILE_OPTIONS}

build:
	touch ${BUILD_DIR}/file_opts
	${ARDUINO_CLI} compile ${ARDUINO_COMPILE_OPTIONS}

clean:
	rm -rf ${BUILD_DIR}

.PHONY: dependencies upload build clean
