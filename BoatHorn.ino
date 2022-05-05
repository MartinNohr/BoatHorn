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
#define _countof(array) (sizeof(array) / sizeof(array[0]))

bool HandleMenuInteger(ezMenu* menu);
bool ToggleBool(ezMenu* menu);
String FormatInteger(int num, int decimals);

char* prefsName = "boathorn";
ezMenu mainMenu("Horn Signals");
// timer for pause between blasts
#define PREFS_PAUSE_TIME "pausetime"
int nPauseTime;    // milliseconds before next blast

// this is set if any integer values changed by int or bool handlers
bool bValueChanged = false;
ezHeader header;

// horn action list
enum HORN_ACTION_TYPE {
    HORN_ACTION_SHORT,
    HORN_ACTION_LONG,
    HORN_ACTION_REPEAT_1MIN,
    HORN_ACTION_REPEAT_2MIN,
};
typedef HORN_ACTION_TYPE HornActionType;
HornActionType HaShort[] = { HORN_ACTION_SHORT };
HornActionType HaLong[] = { HORN_ACTION_LONG };
HornActionType HaDanger[] = { HORN_ACTION_SHORT,HORN_ACTION_SHORT, HORN_ACTION_SHORT, HORN_ACTION_SHORT, HORN_ACTION_SHORT };
HornActionType HaPassPort[] = { HORN_ACTION_SHORT };
HornActionType HaPassStarboard[] = { HORN_ACTION_SHORT,HORN_ACTION_SHORT };
HornActionType HaBackingUp[] = { HORN_ACTION_SHORT,HORN_ACTION_SHORT,HORN_ACTION_SHORT };
HornActionType HaBlindBend[] = { HORN_ACTION_LONG };
HornActionType HaBackingOutOfDock[] = {HORN_ACTION_LONG,HORN_ACTION_SHORT,HORN_ACTION_SHORT,HORN_ACTION_SHORT};
HornActionType HaBlindFogPower[] = { HORN_ACTION_REPEAT_1MIN, HORN_ACTION_LONG };
HornActionType HaBlindFogSailing[] = { HORN_ACTION_REPEAT_2MIN, HORN_ACTION_LONG, HORN_ACTION_SHORT, HORN_ACTION_SHORT };
HornActionType HaChannelPassingPort[] = { HORN_ACTION_LONG,HORN_ACTION_SHORT };
HornActionType HaChannelPassingStarboard[] = { HORN_ACTION_LONG,HORN_ACTION_SHORT,HORN_ACTION_SHORT };
// command lists
struct Horn_Action {
    char* title;
    HornActionType* actionList;
    int actionCount;
};
typedef Horn_Action HornAction;
HornAction Horns[] = {
    {"Short Blast",HaShort,_countof(HaShort)},
    {"Long Blast",HaLong,_countof(HaLong)},
    {"Danger",HaDanger,_countof(HaDanger)},
    {"Pass Port Side",HaPassPort,_countof(HaPassPort)},
    {"Pass Starboard Side",HaPassStarboard,_countof(HaPassStarboard)},
    {"Backing Up",HaBackingUp,_countof(HaBackingUp)},
    {"Blind Bend",HaBlindBend,_countof(HaBlindBend)},
    {"Backing Out of Dock",HaBackingOutOfDock,_countof(HaBackingOutOfDock)},
    {"Blind/Fog Power Vessel (repeats)",HaBlindFogPower,_countof(HaBlindFogPower)},
    {"Blind/Fog Sailing Vessel (repeats)",HaBlindFogSailing,_countof(HaBlindFogSailing)},
    {"Channel Passing Port",HaChannelPassingPort,_countof(HaChannelPassingPort)},
    {"Channel Passing Starboard",HaChannelPassingStarboard,_countof(HaChannelPassingStarboard)},
};

void setup() {
#include <themes/default.h>
#include <themes/dark.h>
    ezt::setDebug(INFO);
    ez.begin();
    Wire.begin();

    // get saved values
    Preferences prefs;
    prefs.begin(prefsName, true);
    nPauseTime = prefs.getInt(PREFS_PAUSE_TIME, 1000);
    prefs.end();

    mainMenu.txtSmall();
    for (HornAction ha : Horns) {
        mainMenu.addItem(ha.title);
    }
    //ez.header.title("Horn Blower");
    //ez.header.show();
    //ez.canvas.font(&FreeSans12pt7b);
    //ez.canvas.pos(0, 50);
    //ez.canvas.print("Horn");

    Serial.println("START " __FILE__ " from " __DATE__);
    //m5.Speaker.setBeep(500, 1000);
    //m5.Speaker.beep();
    //delay(1000);
    //m5.Speaker.setBeep(2000, 100000);
    //m5.Speaker.beep();
}

void loop() {
    mainMenu.buttons("up # # Horn # Set # down #");
    mainMenu.runOnce();
    String str = mainMenu.pickButton();
    if (str == "Set") {
        Settings();
        ez.header.show("Horn Blower");
    }
    else if (str == "Horn") {
        // run the horn action
        int ix = mainMenu.pick() - 1;
        PlayHorn(ix);
    }
}

// play the horn sequence
void PlayHorn(int hix)
{
    ez.buttons.show(" #  #  # Cancel #  # ");
    HornActionType* actionList = Horns[hix].actionList;
    int count = Horns[hix].actionCount;
    while (count--) {
        switch (*actionList) {
        case HORN_ACTION_SHORT:
            m5.Speaker.setBeep(500, 1);
            m5.Speaker.beep();
            delay(1000);
            m5.Speaker.end();
            break;
        case HORN_ACTION_LONG:
            m5.Speaker.setBeep(500, 1);
            m5.Speaker.beep();
            delay(5000);
            m5.Speaker.end();
            break;
        default:
            break;
        }
        if (count) {
            delay(nPauseTime);
            // get the next one
            ++actionList;
        }
    }
}

void Settings()
{
    ezMenu menuPlayerSettings("Settings");
    menuPlayerSettings.txtSmall();
    menuPlayerSettings.buttons("up # # Go # Back # down #");
    menuPlayerSettings.addItem("Time Between Blasts (mS)", &nPauseTime, 250, 2000, 0, HandleMenuInteger);
    menuPlayerSettings.addItem("Clear Stored Values", ClearStoredValues);
    menuPlayerSettings.addItem("System Settings", ez.settings.menu);
    menuPlayerSettings.addItem("Restart", Restart);
    menuPlayerSettings.addItem("Power Off", Shutdown);
    while (true) {
        menuPlayerSettings.runOnce();
        if (menuPlayerSettings.pickButton() == "Back") {
            // see if anything changed
            if (bValueChanged) {
                Serial.println("values changed");
                bValueChanged = false;
                Preferences prefs;
                prefs.begin(prefsName);
                prefs.putInt(PREFS_PAUSE_TIME, nPauseTime);
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
        ez.canvas.pos(10, 100);
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
