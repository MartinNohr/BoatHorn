/*
 Name:		BoatHorn.ino
 Created:	4/27/2022 8:42:56 PM
 Author:	Martin Nohr
*/
#include <stack>
#include <M5ez.h>
#include <M5Stack.h>
#include <EEPROM.h>
#include <ezTime.h>
//#include <SPIFFS.h>
#include <SD.h>
#include <Preferences.h>
#define IR_RECEIVE_PIN 22
#define IR_SEND_PIN 21
#include <IRremote.h>

bool HandleMenuInteger(ezMenu* menu);
bool ToggleBool(ezMenu* menu);
String FormatInteger(int num, int decimals);

char* prefsName = "remotecontrol";
#define PREFS_DISPLAYTIME "displaytime"
ezMenu mainMenu("Main Menu");
int nSlideDisplayTime;  // in seconds
// timer for pause
#define PREFS_PAUSE_TIME "pausetime"
int nPauseTime;    // seconds before unpausing
// touch buttons
#define PREFS_TOUCH_1 "touch1level"
#define PREFS_TOUCH_2 "touch2level"
#define PREFS_TOUCH_3 "touch3level"
// pins
#define TOUCH_PIN1 2
#define TOUCH_PIN2 13
#define TOUCH_PIN3 15
int touchPinList[3] = { TOUCH_PIN1,TOUCH_PIN2,TOUCH_PIN3 };
// store the logic level in here
int touch1Level, touch2Level, touch3Level;

// this is set if any integer values changed by int or bool handlers
bool bValueChanged = false;
ezHeader header;

#include <BleKeyboard.h>
BleKeyboard bleKeyboard("ESP32 Slide Player");

// command lists
struct CMD_ITEM {
    char* cmd;
    uint32_t ir;
    const MediaKeyReport* btTwoKeys;
    uint8_t btSingleKey;
};
typedef CMD_ITEM CMD_ITEM;
#define CMDNAME_PLAY "Play/Pause"
#define CMDNAME_PREV "Previous"
#define CMDNAME_NEXT "Next"
#define CMDNAME_STOP "Stop"
CMD_ITEM cmdList[] = {
    {CMDNAME_PLAY,  0,&KEY_MEDIA_PLAY_PAUSE},
    {CMDNAME_PREV,  0,&KEY_MEDIA_PREVIOUS_TRACK},
    {CMDNAME_NEXT,  0,&KEY_MEDIA_NEXT_TRACK},
    {CMDNAME_STOP,  0,&KEY_MEDIA_STOP},
    {"Left Arrow",  0,NULL,KEY_LEFT_ARROW},
    {"Right Arrow", 0,NULL,KEY_RIGHT_ARROW},
    {"Up Arrow",    0,NULL,KEY_UP_ARROW},
    {"Down Arrow",  0,NULL,KEY_DOWN_ARROW},
    {"Return/Enter",0,NULL,KEY_RETURN},
    {"Mute",        0,&KEY_MEDIA_MUTE},
    {"Volume+",     0,&KEY_MEDIA_VOLUME_UP},
    {"Volume-",     0,&KEY_MEDIA_VOLUME_DOWN},
};
//#include <map>
//struct SEND_CMD_ITEM {
//    uint32_t ir;
//    const MediaKeyReport* btTwoKeys;
//    const uint8_t btSingleKey;
//};
//typedef SEND_CMD_ITEM SEND_CMD_ITEM;
//std::map<char*, SEND_CMD_ITEM>commandMap;
//std::map<char*, SEND_CMD_ITEM>commandIt;

// mode IR or BT, false is BT
bool bIRmode = true;
#define PREFS_IRMODE "irmode"

void setup() {
#include <themes/default.h>
#include <themes/dark.h>
    ezt::setDebug(INFO);
    ez.begin();
    Wire.begin();

    // get saved values
    Preferences prefs;
    prefs.begin(prefsName, true);
    nSlideDisplayTime = prefs.getInt(PREFS_DISPLAYTIME, 10);
    nPauseTime = prefs.getInt(PREFS_PAUSE_TIME, 60);
    touch1Level = prefs.getInt(PREFS_TOUCH_1, 15);
    touch2Level = prefs.getInt(PREFS_TOUCH_2, 15);
    touch3Level = prefs.getInt(PREFS_TOUCH_3, 15);
    bIRmode = prefs.getBool(PREFS_IRMODE, true);
    // load the command map
    for (auto& item : cmdList) {
        //Serial.print("item: " + String(item.cmd));
        // see if an IR was stored
        uint32_t tmp = prefs.getLong(item.cmd, 0);
        //Serial.println(String(item.cmd) + " " + String(tmp, 16));
        if (tmp) {
            item.ir = tmp;
        }
        //Serial.print(" IR: " + String(item.ir));
        //if (item.btSingleKey)
        //    Serial.print(" Single " + String(item.btSingleKey));
        //if (item.btTwoKeys)
        //    Serial.print(" Double " + String(item.btTwoKeys[0][0]) + " " + String(item.btTwoKeys[0][1]));
        //Serial.println();
    }
    prefs.end();

    mainMenu.txtSmall();
    mainMenu.addItem("Player Settings", PlayerSettings);
    mainMenu.addItem("Record IR Command", RecordCommand);
    mainMenu.addItem("Send IR Command", SendCommand);
    mainMenu.addItem("BlueTooth Keyboard", SendBlueTooth);
    mainMenu.addItem("Touch Buttons", TestTouch);
    mainMenu.addItem("System Settings", ez.settings.menu);
    mainMenu.addItem("Restart", Restart);
    mainMenu.addItem("Power Off", Shutdown);

    Serial.println("START " __FILE__ " from " __DATE__);
    // In case the interrupt driver crashes on setup, give a clue
    // to the user what's going on.
    Serial.println("Enabling IRin");
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);
    IrSender.begin(IR_SEND_PIN, false);

    Serial.print("Ready to receive IR signals at pin ");
    Serial.println(IR_RECEIVE_PIN);
    Serial.print("Ready to transmit IR signals at pin ");
    Serial.println(IR_SEND_PIN);
    header.show("Running Slides");
    bleKeyboard.begin();
}

void loop() {
    static uint32_t secTimer;
    static int slideTimer;
    static int count;
    static int pauseTimer;
    ez.buttons.show("Prev ## Play/Pause ## Next #### Menu");
    String str = ez.buttons.poll();
    static String pendingStr;
    if (pendingStr.length() == 0 && ReadTouch(0)) {
        pendingStr = "Prev";
    }
    else if (pendingStr.length() == 0 && ReadTouch(1)) {
        pendingStr = "Play/Pause";
    }
    else if (pendingStr.length() == 0 && ReadTouch(2)) {
        pendingStr = "Next";
    }
    // see if the button is released before letting it go
    if (pendingStr.length() != 0) {
        // check all the buttons
        int ix;
        for (ix = 0; ix < 3; ++ix) {
            if (ReadTouch(ix))
                break;
        }
        // if all buttons off, go...
        if (ix >= 3) {
            str = pendingStr;
            pendingStr.clear();
        }
    }
    if (str == "Menu") {
        mainMenu.buttons("up # # Go # Back # down #");
        while (true) {
            mainMenu.runOnce();
            if (mainMenu.pickButton() == "Back") {
                header.show("Running Slides");
                break;
            }
        }
    }
    else if (str == "Prev") {
        // send command here
        TxCommand(CMDNAME_PREV, bIRmode);
        slideTimer = nSlideDisplayTime;
        if (pauseTimer)
            pauseTimer = 1;
        secTimer = 0;
        --count;
    }
    else if (str == "Next") {
        // send command here
        secTimer = slideTimer = 0;
        if (pauseTimer)
            pauseTimer = 1;
    }
    else if (str == "Play/Pause") {
        // send command here, and start the restart timer
        if (pauseTimer)
            pauseTimer = 1;
        else {
            TxCommand(CMDNAME_PLAY, bIRmode);
            pauseTimer = nPauseTime;
        }
        //Serial.println("pause timer: " + String(pauseTimer));
    }
    // do this every second
    if (secTimer == 0 || millis() > secTimer + 1000) {
        if (pauseTimer) {
            slideTimer = 0;
            ez.canvas.font(&FreeSans12pt7b);
            ez.canvas.x(10);
            ez.canvas.y(40);
            if (--pauseTimer == 0) {
                // we just hit zero, unpause
                TxCommand(CMDNAME_PLAY, bIRmode);
                Serial.println("unpausing");
                ez.canvas.print("                                       ");
            }
            else {
                ez.canvas.print("Time to unpause: " + String(pauseTimer) + "    ");
            }
        }
        else {
            if (slideTimer == 0) {
                ++count;
                slideTimer = nSlideDisplayTime;
                ez.canvas.font(&FreeSans12pt7b);
                ez.canvas.x(10);
                ez.canvas.y(100);
                ez.canvas.print("Slide count: " + String(count) + "      ");
                TxCommand(CMDNAME_NEXT, bIRmode);
            }
            ez.canvas.font(&FreeSans12pt7b);
            ez.canvas.x(10);
            ez.canvas.y(130);
            ez.canvas.print("Time to next slide: " + String(slideTimer) + "      ");
            --slideTimer;
        }
        secTimer = millis();
    }
}

int ReadTouchAverage(int which)
{
#define READ_TRIES 10
    int pin = touchPinList[which];
    int sum = 0;
    for (int ix = 0; ix < READ_TRIES; ++ix) {
        sum += touchRead(pin);
        delay(2);
    }
    return sum / READ_TRIES;
}

// read the touch button
// call with 0, 1, or 2
bool ReadTouch(int which)
{
    bool retval = false;
    // get average
    int val = ReadTouchAverage(which);
    switch (which) {
    case 0:
        retval = val < touch1Level;
        break;
    case 1:
        retval = val < touch2Level;
        break;
    case 2:
        retval = val < touch3Level;
        break;
    }
    return retval;
}

// test touch buttons
void TestTouch()
{
    bool oldVals[3];
    ez.canvas.clear();
    ez.canvas.font(&FreeSans12pt7b);
    ez.buttons.show("# Exit # Calibrate");
    for (int ix = 0; ix < sizeof(oldVals) / sizeof(*oldVals); ++ix) {
        // make sure it will update
        oldVals[ix] = !ReadTouch(ix);
    }
    while (true) {
        String str = ez.buttons.poll();
        if (str == "Exit")
            break;
        else if (str == "Calibrate") {
            CalibrateTouch();
            ez.canvas.clear();
            ez.canvas.font(&FreeSans12pt7b);
            ez.buttons.show("# Exit # Calibrate");
            memset(oldVals, -1, sizeof(oldVals));
        }
        ez.canvas.font(&FreeSans12pt7b);
        // read and display the touch
        int y = 20;
        char* btnNames[] = { "Left","Middle","Right" };
        for (int ix = 0; ix < sizeof(oldVals) / sizeof(*oldVals); ++ix, y += 30) {
            int val = ReadTouch(ix);
            if (val != oldVals[ix]) {
                oldVals[ix] = val;
                ez.canvas.x(5);
                ez.canvas.y(y);
                ez.canvas.print(String(btnNames[ix]) + " " + val + "   ");
            }
        }
        delay(10);
    }
}

// calibrate the touch buttons
void CalibrateTouch()
{
    Preferences prefs;
    prefs.begin(prefsName);
    int vals[3], oldvals[3], avgs[3];
    oldvals[0] = vals[0] = prefs.getInt(PREFS_TOUCH_1, 15);
    oldvals[1] = vals[1] = prefs.getInt(PREFS_TOUCH_2, 15);
    oldvals[2] = vals[2] = prefs.getInt(PREFS_TOUCH_3, 15);
    String btn = ez.msgBox("Calibration", "Select Button", "Left # Middle # Right");
    int which = 0;
    if (btn == "Left") {
        which = 0;
    }
    else if (btn == "Middle") {
        which = 1;
    }
    else if (btn == "Right") {
        which = 2;
    }
    int pin = touchPinList[which];
    ez.canvas.clear();
    ez.canvas.font(&FreeSans12pt7b);
    ez.buttons.show("Lower # # OK # Cancel # Higher #");
    ez.canvas.x(5);
    ez.canvas.y(10);
    ez.canvas.print("Current value: " + String(oldvals[which]) + " for " + btn);
    bool redraw = true;
    unsigned long tm = millis();
    while (true) {
        if (redraw) {
            ez.canvas.x(5);
            ez.canvas.y(10);
            ez.canvas.print("Current value: " + String(oldvals[which]) + " for " + btn);
            ez.canvas.x(5);
            ez.canvas.y(35);
            ez.canvas.print("New value: " + String(vals[which]) + "    ");
            redraw = false;
        }
        if (millis() > tm + 250) {
            // redraw the average
            ez.canvas.x(5);
            ez.canvas.y(80);
            ez.canvas.print("Button Read Value: " + String(ReadTouchAverage(which)) + "    ");
            tm = millis();
        }
        String str = ez.buttons.poll();
        if (str == "Cancel")
            break;
        else if (str == "Lower") {
            redraw = true;
            vals[which] -= 1;
        }
        else if (str == "Higher") {
            redraw = true;
            vals[which] += 1;
        }
        else if (str == "OK") {
            // save them
            prefs.putInt(PREFS_TOUCH_1, vals[0]);
            prefs.putInt(PREFS_TOUCH_2, vals[1]);
            prefs.putInt(PREFS_TOUCH_3, vals[2]);
            break;
        }
    }
    prefs.end();
}

// send bluetooth commands
void SendBlueTooth()
{
    ezMenu menu("Choose BT Command");
    menu.txtSmall();
    for (auto& item : cmdList) {
        menu.addItem(item.cmd);
    }
    menu.buttons("up # # Send # Exit # down #");
    int which = 1;
    if (bleKeyboard.isConnected()) {
        while (true) {
            //bleKeyboard.print("Hello world");
            //bleKeyboard.write(KEY_RETURN);
            menu.setItem(which);
            which = menu.runOnce();
            String str = menu.pickButton();
            if (str == "Exit") {
                break;
            }
            else if (str == "Send") {
                if (cmdList[which - 1].btSingleKey) {
                    bleKeyboard.write(cmdList[which - 1].btSingleKey);
                }
                else if (cmdList[which - 1].btTwoKeys) {
                    bleKeyboard.write(*(cmdList[which - 1].btTwoKeys));
                }
            }
        }
    }
    else {
        ez.msgBox("BlueTooth", "BlueTooth not connected");
    }
}

void PlayerSettings()
{
    ezMenu menuPlayerSettings("Player Settings");
    menuPlayerSettings.txtSmall();
    menuPlayerSettings.buttons("up # # Go # Back # down #");
    menuPlayerSettings.addItem("IR/BT", &bIRmode, "IR", "BlueTooth", ToggleBool);
    menuPlayerSettings.addItem("Slide Timer", &nSlideDisplayTime, 1, 120, 0, HandleMenuInteger);
    menuPlayerSettings.addItem("Pause Timer", &nPauseTime, 10, 600, 0, HandleMenuInteger);
    menuPlayerSettings.addItem("Clear Stored Values", ClearStoredValues);
    while (true) {
        menuPlayerSettings.runOnce();
        if (menuPlayerSettings.pickButton() == "Back") {
            // see if anything changed
            if (bValueChanged) {
                Serial.println("values changed");
                bValueChanged = false;
                Preferences prefs;
                prefs.begin(prefsName);
                prefs.putInt(PREFS_DISPLAYTIME, nSlideDisplayTime);
                prefs.putInt(PREFS_PAUSE_TIME, nPauseTime);
                prefs.putBool(PREFS_IRMODE, bIRmode);
                prefs.end();
            }
            break;
        }
    }
}

void ClearStoredValues()
{
    String ret = ez.msgBox("Stored Values", "Clear All Values?", "Cancel # OK #");
    if (ret == "OK") {
        Preferences prefs;
        prefs.begin(prefsName);
        prefs.clear();
        prefs.end();
        ez.canvas.font(&FreeSans12pt7b);
        ez.canvas.clear();
        ez.canvas.x(10);
        ez.canvas.y(100);
        ez.canvas.print("Memory Cleared");
        delay(2000);
    }
}

void Shutdown()
{
    m5.powerOFF();
}

void Restart()
{
    ESP.restart();
}

// record a command
void RecordCommand()
{
    // first pick which one
    String str;
    ezMenu menu("Choose Button");
    menu.txtSmall();
    for (auto& item : cmdList) {
        menu.addItem(item.cmd);
    }
    while (true) {
        menu.buttons("up # # Go # Exit # down #");
        int which = menu.runOnce();
        str = menu.pickButton();
        if (str == "Exit") {
            break;
        }
        else if (str == "Go") {
            ez.canvas.font(&FreeSans12pt7b);
            ez.canvas.x(5);
            ez.canvas.y(50);
            ez.canvas.print(String("Button: ") + cmdList[which - 1].cmd);
            ez.buttons.show("Cancel # Waiting #");
            bool done = false;
            uint32_t rxValue = 0;
            while (!done) {
                str = ez.buttons.poll();
                if (str == "Cancel") {
                    done = true;
                    break;
                }
                if (str == "Save") {
                    // save it here
                    if (rxValue) {
                        Preferences prefs;
                        prefs.begin(prefsName);
                        prefs.putLong(cmdList[which - 1].cmd, rxValue);
                        // save in list also
                        cmdList[which - 1].ir = rxValue;
                        done = true;
                        prefs.end();
                        break;
                    }
                }
                if (IrReceiver.decode()) {
                    ez.buttons.show("Cancel # Save #");
                    IrReceiver.printIRResultShort(&Serial);
                    Serial.println();
                    IRData* res = IrReceiver.read();
                    res->flags;
                    if ((res->flags & IRDATA_FLAGS_IS_REPEAT) == 0) {
                        //Serial.println("Protocol: " + String(res.decode_type));
                        rxValue = res->decodedRawData;
                        //Serial.println("Result: " + String(rxValue, 16));
                        ez.canvas.x(5);
                        ez.canvas.y(80);
                        ez.canvas.print("Value: " + String(rxValue, 16) + "                ");
                    }
                    IrReceiver.resume(); // Receive the next value
                }
                delay(100);
            }
        }
    }
}

// send a command using the name
void TxCommand(char* cmd, bool ir)
{
    int which = -1;
    int ix = 0;
    for (auto& val : cmdList) {
        if (strcmp(val.cmd, cmd) == 0) {
            which = ix;
            break;
        }
        ++ix;
    }
    if (which == -1) {
        return;
    }
    //Serial.println("tx which: " + String(which));
    if (ir) {
        uint32_t txValue = cmdList[which].ir;
        if (txValue) {
            Serial.println("IR: " + String(cmdList[which].cmd) + " " + String(txValue, 16));
            IrSender.sendNECRaw(txValue);
        }
    }
    else {
        Serial.println("BT: " + String(cmdList[which].cmd));
        if (cmdList[which].btSingleKey) {
            bleKeyboard.write(cmdList[which].btSingleKey);
        }
        else if (cmdList[which].btTwoKeys) {
            bleKeyboard.write(*cmdList[which].btTwoKeys);
        }
    }
}

// manually send a command
void SendCommand()
{
    ezMenu menu("Choose Button");
    menu.txtSmall();
    for (auto& item : cmdList) {
        menu.addItem(item.cmd);
    }
    while (true) {
        menu.buttons("up # # Go # Exit # down #");
        int which = menu.runOnce();
        String str = menu.pickButton();
        if (str == "Exit") {
            break;
        }
        else if (str == "Go" && which > 0) {
            // get the button from eeprom
            uint32_t txValue = cmdList[which - 1].ir;
            if (txValue) {
                ez.canvas.font(&FreeSans12pt7b);
                ez.canvas.x(5);
                ez.canvas.y(50);
                ez.canvas.print(String("Button: ") + cmdList[which - 1].cmd);
                ez.buttons.show("Cancel # Send #");
                ez.canvas.x(5);
                ez.canvas.y(80);
                ez.canvas.print("Value: " + String(txValue, 16));
                while (true) {
                    String response = ez.buttons.poll();
                    if (response == "Send") {
                        IrSender.sendNECRaw(txValue);
                        break;
                    }
                    if (response == "Cancel") {
                        break;
                    }
                }
            }
            else {
                ez.canvas.font(&FreeSans12pt7b);
                ez.canvas.clear();
                ez.canvas.x(10);
                ez.canvas.y(100);
                ez.canvas.print("Nothing to Send");
                delay(2000);
            }
        }
    }
}

bool HandleMenuInteger(ezMenu* menu)
{
    int minVal = menu->getIntMinVal();
    int maxVal = menu->getIntMaxVal();
    int decimals = menu->getIntDecimals();
    int value = *menu->getIntValue();
    int inc = 1;
    int originalVal = value;
    ezProgressBar bl(menu->pickCaption(), "From " + FormatInteger(minVal, decimals) + " to " + FormatInteger(maxVal, decimals), "left # - # OK # Cancel # right # +");
    ez.canvas.font(&FreeSans12pt7b);
    int lastVal = value;
    int lastInc = inc;
    ez.canvas.x(140);
    ez.canvas.y(180);
    ez.canvas.print("value: " + FormatInteger(value, decimals) + "   ");
    while (true) {
        String b = ez.buttons.poll();
        if (b == "right")
            value += inc;
        else if (b == "left")
            value -= inc;
        else if (b == "+") {
            inc *= 10;
        }
        else if (b == "-") {
            inc /= 10;
        }
        else if (b == "OK") {
            break;
        }
        else if (b == "Cancel") {
            if (ez.msgBox("Restore original", "Cancel?", "Cancel # OK #") == "OK") {
                value = originalVal;
                break;
            }
            ez.buttons.show("left # - # OK # Cancel # right # +");
            ez.canvas.font(&FreeSans12pt7b);
        }
        inc = constrain(inc, 1, maxVal);
        value = constrain(value, minVal, maxVal);
        bl.value(((float)value / ((float)(maxVal - minVal) / 100.0)));
        if (lastInc != inc) {
            ez.canvas.x(0);
            ez.canvas.y(180);
            uint16_t oldcolor = ez.canvas.color();
            ez.canvas.color(TFT_CYAN);
            ez.canvas.print("+/- " + FormatInteger(inc, decimals) + "   ");
            ez.canvas.color(oldcolor);
            lastInc = inc;
        }
        if (lastVal != value) {
            ez.canvas.x(140);
            ez.canvas.y(180);
            ez.canvas.print("value: " + FormatInteger(value, decimals) + "   ");
            lastVal = value;
        }
    }
    String caption = menu->pickCaption();
    caption = caption.substring(0, caption.lastIndexOf('\t') + 1);
    menu->setCaption(menu->pickName(), caption + FormatInteger(value, decimals));
    // set the flag is the new value is different
    if (*menu->getIntValue() != value) {
        bValueChanged = true;
        // store the new value
        *menu->getIntValue() = value;
    }
    return true;
}

// handle boolean toggles
bool ToggleBool(ezMenu* menu)
{
    *menu->getBoolValue() = !*menu->getBoolValue();
    String caption = menu->pickCaption();
    caption = caption.substring(0, caption.lastIndexOf('\t') + 1);
    menu->setCaption(menu->pickName(), caption + (*menu->getBoolValue() ? menu->getBoolTrue() : menu->getBoolFalse()));
    // we changed it, let everybody know
    bValueChanged = true;
    return true;
}

String FormatInteger(int num, int decimals)
{
    String str;
    if (decimals) {
        str = String(num / (int)pow10(decimals)) + "." + String(num % (int)pow10(decimals));
    }
    else {
        str = String(num);
    }
    return str;
}
