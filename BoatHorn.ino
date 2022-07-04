/*
 Name:		BoatHorn.ino
 Created:	4/27/2022 8:42:56 PM
 Author:	Martin Nohr
*/

#include <M5ez.h>
#include <M5Stack.h>
#include <EEPROM.h>
#include <ezTime.h>
#include <Update.h>
//#include <SPIFFS.h>
#include <SD.h>
#include <Preferences.h>

//#define _countof(array) (sizeof(array) / sizeof(array[0]))

const char* Version = "Version 0.5";
bool HandleMenuInteger(ezMenu* menu);
bool ToggleBool(ezMenu* menu);
String FormatInteger(int num, int decimals);

char* prefsName = "boathorn";
ezMenu mainMenu("Horn Signals");
// timer for pause between blasts
constexpr auto PREFS_PAUSE_TIME = "pausetime";
int nPauseTime;    // milliseconds before next blast
// enable beep sound
constexpr auto PREFS_BEEP_SOUND = "beepsound";
bool bBeepSound = false;

// this is set if any integer values changed by int or bool handlers
bool bValueChanged = false;
ezHeader header;

// horn command lists
struct Horn_Action {
    char* title;
    unsigned long repeatTime;
    std::vector<bool> actionList;   // true for long horn
};
typedef Horn_Action HornAction;
std::vector<HornAction> HornVector;

//HornActionType BellOne[] = { HORN_ACTION_SHORT };
//HornActionType BellTwo[] = { HORN_ACTION_SHORT, HORN_ACTION_SHORT };
//HornAction Bells[] = {
//    {"One Bell",BellOne,_countof(BellOne)},
//    {"Two Bells",BellTwo,_countof(BellTwo)},
//};

constexpr auto RELAY1 = 21;
constexpr auto RELAY2 = 22;
constexpr bool HORN_SHORT = false;
constexpr bool HORN_LONG = true;

void setup() {
#include <themes/default.h>
#include <themes/dark.h>
	HornVector.push_back(HornAction{ "Short Blast", 0, { HORN_SHORT } });
	HornVector.push_back(HornAction{ "Long Blast", 0, { HORN_LONG } });
    HornVector.push_back(HornAction{ "Danger", 0, { HORN_SHORT,HORN_SHORT, HORN_SHORT, HORN_SHORT, HORN_SHORT } });
    HornVector.push_back(HornAction{ "Pass Port Side", 0, { HORN_SHORT } });
    HornVector.push_back(HornAction{ "Pass Starboard Side", 0, { HORN_SHORT,HORN_SHORT } });
    HornVector.push_back(HornAction{ "Backing Up", 0, { HORN_SHORT,HORN_SHORT,HORN_SHORT } });
    HornVector.push_back(HornAction{ "Blind Bend", 0, { HORN_LONG } });
    HornVector.push_back(HornAction{ "Backing Out of Dock", 0, {HORN_LONG,HORN_SHORT,HORN_SHORT,HORN_SHORT} });
    HornVector.push_back(HornAction{ "Blind/Fog Power Vessel", 60000, { HORN_LONG } });
    HornVector.push_back(HornAction{ "Blind/Fog Sailing Vessel", 120000, { HORN_LONG, HORN_SHORT, HORN_SHORT } });
	HornVector.push_back(HornAction{ "Channel Passing Port", 0, { HORN_LONG,HORN_SHORT } });
	HornVector.push_back(HornAction{ "Channel Passing Starboard", 0,  { HORN_LONG,HORN_SHORT,HORN_SHORT } });

    ezt::setDebug(INFO);
	ez.begin();
    Wire.begin();

    // get saved values
    Preferences prefs;
    prefs.begin(prefsName, true);
    nPauseTime = prefs.getInt(PREFS_PAUSE_TIME, 1000);
    bBeepSound = prefs.getBool(PREFS_BEEP_SOUND, false);
    prefs.end();

    mainMenu.txtSmall();
    for (HornAction ha : HornVector) {
        // build the string
		String menuStr = ha.title + String(" (");
		if (ha.repeatTime)
			menuStr += "R" + String(ha.repeatTime / 60000);
		for (auto hix = ha.actionList.begin(); hix != ha.actionList.end(); ++hix) {
            menuStr += *hix ? "L" : "S";
        }
        menuStr += ')';
        mainMenu.addItem(menuStr);
    }
    //ez.header.title("Horn Blower");
    //ez.header.show();
    //ez.canvas.font(&FreeSans12pt7b);
    //ez.canvas.pos(0, 50);
    //ez.canvas.print("Horn");

    Serial.println("START " __FILE__ " from " __DATE__);
    pinMode(RELAY1, OUTPUT);
    digitalWrite(RELAY1, LOW);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY2, LOW);
    // check for update bin file
    CheckUpdateBin();
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
    // get the repeat timer, 0 for no repeat
    unsigned long repeatTime = HornVector[hix].repeatTime;
    bool bRun = true;
    bool bCancel = false;
    ez.header.show(HornVector[hix].title);
    ez.buttons.show(" #  # Cancel #  #  # ");
    while (bRun) {
        auto actionList = HornVector[hix].actionList.begin();
        int count = HornVector[hix].actionList.size();
		while (!bCancel && count--) {
			uint32_t hornlen = *actionList ? 5000 : 1000;
            if (hornlen) {
				digitalWrite(RELAY1, HIGH);
				if (bBeepSound) {
                    m5.Speaker.begin();
                    m5.Speaker.setBeep(500, hornlen);
					m5.Speaker.beep();
				}
				bCancel = CheckCancel(hornlen, "horn on", "Second(s) Horn");
                digitalWrite(RELAY1, LOW);
            }
			if (!bCancel && count) {
				if (hornlen)
					bCancel = CheckCancel(nPauseTime, "pause", "Second(s) Off");
                // get the next one
                ++actionList;
            }
        }
        //Serial.println(ez.buttons.poll());
        // exit if no repeats
		if (repeatTime == 0) {
			bRun = false;
		}
		// first check if cancel
		else if (bCancel) {
			bRun = false;
		}
		else {
			// wait the prescribed amount of time and repeat
			bRun = !CheckCancel(repeatTime, "waiting to repeat", "Repeat");
        }
    }
}

// wait mS for cancel
bool CheckCancel(unsigned long nWait, String strTitle, String strLabel)
{
	ezProgressBar pb(strTitle, String(nWait / 1000) + " " + strLabel, " #  # Cancel #  #  # ");
    uint32_t until = millis() + nWait;
    while (millis() < until) {
		pb.value(100.0 - (((until - millis()) * 100.0) / nWait));
        if (ez.buttons.poll() == "Cancel")
            return true;
    }
    return false;
}

void Settings()
{
    ezMenu menuPlayerSettings("Settings");
    menuPlayerSettings.txtSmall();
    menuPlayerSettings.buttons("up # # Go # Back # down #");
	menuPlayerSettings.addItem("Custom Sounds", HandleMenuCustom);
    menuPlayerSettings.addItem("Time Between Blasts (mS)", &nPauseTime, 250, 2000, 0, HandleMenuInteger);
    menuPlayerSettings.addItem("Beep Sound", &bBeepSound, "On", "Off", ToggleBool);
    menuPlayerSettings.addItem("Clear Stored Values", ClearStoredValues);
    menuPlayerSettings.addItem("System Settings", ez.settings.menu);
    menuPlayerSettings.addItem("Restart", Restart);
    menuPlayerSettings.addItem("Power Off", Shutdown);
    menuPlayerSettings.addItem("Download New Firmware", HandleOTA);
	menuPlayerSettings.addItem(String(Version) + " " + __DATE__/* + " " + __TIME__*/);
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
                prefs.putBool(PREFS_BEEP_SOUND, bBeepSound);
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

// edit custom horn sequences
void HandleMenuCustom()
{
    ezMenu menuPlayerSettings("Custom Sounds");
    menuPlayerSettings.txtSmall();
    menuPlayerSettings.buttons("up # Del # Add # Back # down #");
    // list the custom sounds
    menuPlayerSettings.addItem("NONE");
    while (true) {
        menuPlayerSettings.runOnce();
        String mstr = menuPlayerSettings.pickButton();
        if (mstr == "Back") {
            break;
        }
        else if (mstr == "Del") {
        }
        else if (mstr == "Add") {
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

// OTA handler
void HandleOTA()
{
	String ret = ez.msgBox("New Firmware", "Download New Firmware?", "Cancel # OK #");
	if (ret == "OK") {
		ezProgressBar pb("OTA update in progress", "Downloading ...", "Abort");
		String url = "https://raw.githubusercontent.com/MartinNohr/BoatHorn/master/Release/BoatHorn.bin";
#include"raw_githubusercontent_com.h"
		if (ez.wifi.update(url, root_cert, &pb)) {
			ez.msgBox("Over The Air updater", "OTA download successful. Reboot to new firmware", "Reboot");
			ESP.restart();
		}
		else {
			ez.msgBox("OTA error", ez.wifi.updateError(), "OK");
		}
	}
}

ezProgressBar* pUpdateProgress = NULL;
// display updating progress
void ProgressDisplay(size_t done, size_t total)
{
	pUpdateProgress->value((float)done * 100.0 / total);
    //Serial.println("done: " + String(done) + " total: " + String(total));
}

// see if there is an update bin file in the SD slot
void CheckUpdateBin()
{
	const char* binFileName = "/BoatHorn.bin";
    if (SD.exists(binFileName)) {
		String str = ez.msgBox("Update File", "Load New Firmware From SD?", "Cancel # OK # Cancel");
		if (str == "OK") {
            File binFile = SD.open(binFileName);
            if (binFile) {
                pUpdateProgress = new ezProgressBar("Updating", "Progress");
                size_t binSize = binFile.size();
                Serial.println("size: " + String(binSize));
                Update.begin(binSize);
                Update.onProgress(ProgressDisplay);
				size_t bytesWritten = Update.writeStream(binFile);
                Serial.println("written: " + String(bytesWritten));
                Update.end();
                binFile.close();
                str = ez.msgBox("Update File", "Delete BIN file?", "No # Yes # No");
                if (str == "Yes") {
					SD.remove(binFileName);
                }
                delete pUpdateProgress;
				ESP.restart();
            }
		}
    }
    //else {
    //    ez.msgBox("file missing", "no bin file", "Cancel # OK # Cancel");
    //}
}
