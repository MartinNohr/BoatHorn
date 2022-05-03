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

bool HandleMenuInteger(ezMenu* menu);
bool ToggleBool(ezMenu* menu);
String FormatInteger(int num, int decimals);

char* prefsName = "boathorn";
ezMenu mainMenu("Horn Blower");
// timer for pause
#define PREFS_PAUSE_TIME "pausetime"
int nPauseTime;    // seconds before unpausing

// this is set if any integer values changed by int or bool handlers
bool bValueChanged = false;
ezHeader header;

// horn action list
enum HORN_ACTION_TYPE { HORN_ACTION_ON, HORN_ACTION_OFF };
typedef HORN_ACTION_TYPE HornActionType;
HornActionType HaAlarm[] = { HORN_ACTION_ON,HORN_ACTION_OFF,HORN_ACTION_ON,HORN_ACTION_OFF, HORN_ACTION_ON,HORN_ACTION_OFF };

// command lists
enum HORN_TYPE {HORN_ALARM, HORN_WARNING, HORN_PORT, HORN_STARBOARD};
struct Horn_Action {
    char* title;
    HornActionType* actionList;
};
typedef Horn_Action HornAction;
HornAction Horns[] = {
    {"ALARM",HaAlarm},
    {"WARNING",NULL},
    {"PORT",NULL},
    {"STARBOARD",NULL},
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
    nPauseTime = prefs.getInt(PREFS_PAUSE_TIME, 60);
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
}

void loop() {
    mainMenu.buttons("up # # Horn # Set # down #");
    mainMenu.runOnce();
    String str = mainMenu.pickButton();
    if (str == "Set") {
        PlayerSettings();
        ez.header.show("Horn Blower");
    }
    else if (str == "Horn") {
        // run the horn action
    }
}

void PlayerSettings()
{
    ezMenu menuPlayerSettings("Player Settings");
    menuPlayerSettings.txtSmall();
    menuPlayerSettings.buttons("up # # Go # Back # down #");
    menuPlayerSettings.addItem("Pause Timer", &nPauseTime, 10, 600, 0, HandleMenuInteger);
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
