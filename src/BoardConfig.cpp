#include "BoardConfig.h"

uint8_t kMatrixRgbPins[] = {7, 8, 9, 10, 11, 12};
uint8_t kMatrixAddrPins[] = {17, 18, 19, 20, 21};

const uint8_t kButtonPin = 2; // "UP" button on MatrixPortal M4

Adafruit_Protomatter matrix(
  kMatrixWidth,
  kMatrixBitplanes,
  kMatrixChains,
  kMatrixRgbPins,
  kMatrixAddrLines,
  kMatrixAddrPins,
  kMatrixClockPin,
  kMatrixLatchPin,
  kMatrixOePin,
  kMatrixDoubleBuffer
);
