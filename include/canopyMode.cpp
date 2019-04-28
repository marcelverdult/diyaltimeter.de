#include "canopyMode.h"

// mode = 1
void canopyMode(){
    // buttons disabled
    // Set CanopyTime in JumpLog
    // read both pressure sensors
    // compare -> ERROR if different
    // smooth sensor readings
    // display altitude in 5m steps -> font: u8g2_font_7Segments_26x42_mn 
    // display descent rate -> font: u8g2_font_ncenR18_tf -> 18 Pixel
    // check altitude -> if stays same for xx secs -> set mode to ground (3)
}