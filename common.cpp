#include "common.h"

char *sign_mode_to_str(SignMode sign_mode) {
  switch (sign_mode) {
    case SIGN_MODE_TEST:
      return "SIGN_MODE_TEST";
    case SIGN_MODE_MBTA:
      return "SIGN_MODE_MBTA";
    case SIGN_MODE_CLOCK:
      return "SIGN_MODE_CLOCK";
    case SIGN_MODE_MUSIC:
      return "SIGN_MODE_MUSIC";
  }
  return "SIGN_MODE_UNKNOWN";
}
