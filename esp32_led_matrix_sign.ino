
// Example sketch which shows how to display some patterns
// on a 64x32 LED matrix
//

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "MBTASans.h"


#define PANEL_RES_X 32      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 5      // Total number of panels chained one to another

#define SIGN_MODE_MBTA 0
uint8_t SIGN_MODE = SIGN_MODE_MBTA;
 
//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);
uint16_t AMBER = dma_display->color565(255, 191, 0);

void setup() {

  // Module configuration
  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN    // Chain length
  );

  // This is essential to avoid artifacts on the display
  mxconfig.clkphase = false;
  
  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90); //0-255
  dma_display->clearScreen();
}

void loop() {
  switch (SIGN_MODE)
  {
  case SIGN_MODE_MBTA:
    mbta_sign_mode_loop();
    break;
  default:
    break;
  }

}

void mbta_sign_mode_loop() {
  dma_display->setFont(&MBTASans);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(AMBER);

  dma_display->setCursor(0, 15);
  dma_display->print("Alewife");
  dma_display->setCursor(0, 31);
  dma_display->print("Braintree");

  dma_display->setCursor(110, 15);
  dma_display->print("1 min");
  dma_display->setCursor(110, 31);
  dma_display->print("5 min");
  delay(1000);
}
