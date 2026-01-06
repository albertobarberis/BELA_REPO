#include <unistd.h>
#include "u8g2/U8g2LinuxI2C.h"
#include <Bela.h>
#include <libraries/Trill/Trill.h>
#include <libraries/OscSender/OscSender.h>
#include "button/button.hpp"
#include <cmath>
#include <atomic>

// ============ Constants ============
const int MAX_SENSORS = 6;
const int MAX_BUTTONS = 2;
const int remotePort = 6901;
const char* remoteIp = "192.168.7.1";
const float gSendInterval = 0.05f;
const unsigned int i2cBus = 1;
const unsigned int gI2cBus = 1;

const unsigned int buttonPin_0 = 0;
const unsigned int buttonPin_1 = 1;
const unsigned int buttonPin_2 = 2;
const unsigned int buttonPin_3 = 3;

// ============ Global Variables ============
Trill* gTouchSensors[MAX_SENSORS] = {nullptr};
int gNumSensors = 0;

OscSender oscSender;
std::vector<Button> vectorButton;

unsigned int gSampleCount = 0;
unsigned int gSendIntervalSamples = 0;

// Display
struct Display {
	U8G2 d;
	int mux;
};
Display gDisplay = {U8G2_SSD1306_128X64_NONAME_F_HW_I2C_LINUX(U8G2_R0, gI2cBus, 0x3c), -1};

// Current sensor values for fader visualization
float sensorValues[MAX_SENSORS] = {0};

// Thread-safe flags and state
std::atomic<bool> displayNeedsUpdate(false);

enum DisplayMode {
	DISPLAY_FADERS,
	DISPLAY_BUTTON
};

std::atomic<DisplayMode> currentDisplayMode(DISPLAY_FADERS);
std::atomic<int> displayedButtonNumber(-1);

// ============ Function Declarations ============
void readLoop(void*);
void updateDisplay();
void displayLoop(void*);
void handleButtons(BelaContext* context, unsigned int n);
void handleTrills();

// ============ Sensor Functions ============
void readLoop(void*)
{
	while(!Bela_stopRequested())
	{
		for(int n = 0; n < gNumSensors; ++n)
		{
			if(gTouchSensors[n])
				gTouchSensors[n]->readI2C();
		}
		usleep(50000);  // 50ms
	}
}

void handleTrills()
{
	for(int t = 0; t < gNumSensors && t < MAX_SENSORS; ++t)
	{
		const float touchSize = gTouchSensors[t]->compoundTouchSize();
		const float touchLocation = gTouchSensors[t]->compoundTouchLocation();
		const float touchHorizontal = gTouchSensors[t]->compoundTouchHorizontalLocation();
		
		sensorValues[t] = touchLocation;
		
		rt_printf("[%d] %2.3f %.3f %.3f  ", t, touchSize, touchLocation, touchHorizontal);
		oscSender.newMessage("/trill")
			.add(t)
			.add(touchSize)
			.add(touchLocation)
			.add(touchHorizontal)
			.send();
	}
	rt_printf("\n");
}

// ============ Display Functions ============
void updateDisplay()
{
	U8G2& u8g2 = gDisplay.d;
	u8g2.clearBuffer();
	
	const DisplayMode mode = currentDisplayMode.load();
	
	// Display button number if in button mode
	if(mode == DISPLAY_BUTTON)
	{
		// Draw white background (fill entire screen)
		u8g2.setDrawColor(1);
		u8g2.drawBox(0, 0, 128, 64);  // Full screen white box
		
		// Draw black number on white background
		u8g2.setDrawColor(0);  // Black color
		u8g2.setFont(u8g2_font_logisoso32_tn);  // Original 32px font that was centered
		char numStr[4];
		const int buttonNum = displayedButtonNumber.load();
		snprintf(numStr, sizeof(numStr), "%d", buttonNum);
		
		// Center horizontally
		const int width = u8g2.getStrWidth(numStr);
		const int x = (128 - width) / 2;
		
		// Original y position that was working
		u8g2.drawStr(x, 45, numStr);
		
		u8g2.sendBuffer();
		return;
	}
	
	// Show faders (normal black background)
	if(gNumSensors == 0)
	{
		u8g2.setDrawColor(1);
		u8g2.setFont(u8g2_font_6x10_tf);
		u8g2.drawStr(10, 32, "No sensors");
		u8g2.sendBuffer();
		return;
	}
	
	u8g2.setDrawColor(1);
	u8g2.setFont(u8g2_font_4x6_tf);
	u8g2.setFontRefHeightText();
	
	const int sectionHeight = 64 / gNumSensors;
	
	// Draw fader bars for each sensor
	for(int s = 0; s < gNumSensors; ++s)
	{
		const int yBase = s * sectionHeight;
		
		// Draw bar background frame
		u8g2.drawFrame(0, yBase + 2, 128, sectionHeight - 4);
		
		// Draw filled bar based on sensor value
		const int barWidth = static_cast<int>(sensorValues[s] * 126.0f);
		if(barWidth > 0)
		{
			u8g2.drawBox(1, yBase + 3, barWidth, sectionHeight - 6);
		}
	}
	
	u8g2.sendBuffer();
}

void displayLoop(void*)
{
	while(!Bela_stopRequested())
	{
		if(displayNeedsUpdate.load())
		{
			displayNeedsUpdate.store(false);
			updateDisplay();
		}
		usleep(50000);  // 50ms
	}
}

// ============ Input Functions ============
void handleButtons(BelaContext* context, unsigned int n)
{
	const size_t numButtons = vectorButton.size();
	for(size_t i = 0; i < numButtons; ++i)
	{
		const int previousState = vectorButton[i].getState();
		const int currentState = digitalRead(context, n, vectorButton[i].getPin());
		vectorButton[i].setState(currentState);
		
		// Check for button press (transition from 0 to 1)
		if(previousState == 0 && currentState == 1)
		{
			currentDisplayMode.store(DISPLAY_BUTTON);
			displayedButtonNumber.store(static_cast<int>(i));
			displayNeedsUpdate.store(true);
		}
		// Check for button release (transition from 1 to 0)
		else if(previousState == 1 && currentState == 0)
		{
			currentDisplayMode.store(DISPLAY_FADERS);
			displayNeedsUpdate.store(true);
		}
		
		oscSender.newMessage("/buttons")
			.add(static_cast<int>(i))
			.add(currentState)
			.send();
	}
}

// ============ Main Bela Functions ============
bool setup(BelaContext *context, void *userData)
{
	rt_printf("Starting setup...\n");
	
	// Reserve space in vector to avoid reallocation
	vectorButton.reserve(MAX_BUTTONS);
	
	// Create and configure buttons
	Button button_0(buttonPin_0);
	pinMode(context, 0, buttonPin_0, INPUT);
	vectorButton.push_back(button_0);

	Button button_1(buttonPin_1);
	pinMode(context, 0, buttonPin_1, INPUT);
	vectorButton.push_back(button_1);

	Button button_2(buttonPin_2);
	pinMode(context, 0, buttonPin_2, INPUT);
	vectorButton.push_back(button_2);

	Button button_3(buttonPin_3);
	pinMode(context, 0, buttonPin_3, INPUT);
	vectorButton.push_back(button_3);

	// Initialize display
	U8G2& u8g2 = gDisplay.d;
	u8g2.initDisplay();
	u8g2.setPowerSave(0);
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_6x10_tf);
	u8g2.drawStr(10, 32, "Initializing...");
	u8g2.sendBuffer();
	
	// Setup OSC
	oscSender.setup(remotePort, remoteIp);
	rt_printf("OSC sender configured to %s:%d\n", remoteIp, remotePort);
	
	// Probe for Trill sensors (skip OLED address 0x3c)
	rt_printf("Probing for Trill sensors...\n");
	for(uint8_t addr = 0x20; addr <= 0x50 && gNumSensors < MAX_SENSORS; ++addr)
	{
		if(addr == 0x3c)  // Skip OLED display
			continue;
			
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device && Trill::CRAFT != device)
		{
			gTouchSensors[gNumSensors] = new Trill(i2cBus, device, addr);
			if(gTouchSensors[gNumSensors] == nullptr)
			{
				rt_printf("WARNING: Failed to create sensor at address 0x%02x\n", addr);
				continue;
			}
			gTouchSensors[gNumSensors]->printDetails();
			gNumSensors++;
		}
	}
	rt_printf("Found %d Trill sensor(s)\n", gNumSensors);
	
	// Start auxiliary tasks
	Bela_runAuxiliaryTask(readLoop);
	Bela_runAuxiliaryTask(displayLoop);
	rt_printf("Auxiliary tasks started\n");
	
	// Calculate send interval
	gSendIntervalSamples = static_cast<unsigned int>(context->audioSampleRate * gSendInterval);
	rt_printf("Send interval: %u samples (%.1fms)\n", gSendIntervalSamples, gSendInterval * 1000.0f);
	
	// Trigger initial display update
	displayNeedsUpdate.store(true);
	
	rt_printf("Setup complete!\n");
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; ++n)
	{
		++gSampleCount;
		
		if(gSampleCount >= gSendIntervalSamples)
		{
			gSampleCount = 0;
			
			handleButtons(context, n);
			handleTrills();
			
			displayNeedsUpdate.store(true);
		}
	}
}

void cleanup(BelaContext *context, void *userData)
{
	rt_printf("Cleaning up...\n");
	
	// Clear the display
	U8G2& u8g2 = gDisplay.d;
	u8g2.clearBuffer();
	u8g2.sendBuffer();
	u8g2.setPowerSave(1);
	
	// Clean up sensors
	for(int i = 0; i < gNumSensors && i < MAX_SENSORS; ++i)
	{
		if(gTouchSensors[i])
		{
			delete gTouchSensors[i];
			gTouchSensors[i] = nullptr;
		}
	}
	
	rt_printf("Cleanup complete\n");
}