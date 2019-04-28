#include "menuMode.h"

// mode = 4
void menuMode(){
    // MainMenu -> _menu = 0
    // -----------------------
    // read buttons
    // display main menu
    // mark selected entry
    // on ENTER set submenu
    
    // SubMenu -> _menu = X / _item = 0
    // ---------------------------------
    // hold ENTER to return to MainMenu -> _menu = 0
    // display submenu items
    // on ENTER set displayItem -> _item = X

    // DisplayItem -> _menu = X / _item = X
    // -------------------------------------
    // hold ENTER to return to SubMenu -> _item = 0
    // do stuff depending on item

    // Ideas:
    // -------
    // Sync Log / Download Log
    // Settings ( WiFi, ... )
    // -- WIFI: AP Mode / Client Mode (after credentials set via AP Mode)
    // -- WIFI: Change settings with browser on http://IP-ADDRESS/
    // OTA Update: set mode to update (5)
    
}