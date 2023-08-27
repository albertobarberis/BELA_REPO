/*

Connecting and Detecting Multiple Trill
And send data via OSC 
Alberto Barberis

From the example: Communication/OSC/render.cpp

=========================================

NOTE: as this example scans several addresses on the i2c bus
it could cause non-Trill peripherals connected to it to malfunction.

OSC

It is designed to be run alongside resources/osc/osc.js.
For the example to work, run in a terminal on the board
```
node /root/Bela/resources/osc/osc.js

```
HAND SHAKE

Open Max befor running the code on the Bela board. 

In `setup()` an OSC message to address `/osc-setup`, it then waits
1 second for a reply on `/osc-setup-reply`.

After that, OSC communication takes place in the on_receive() callback,
which is called every time a new message comes in.

METODI di conversione
da: http://gruntthepeon.free.fr/oscpkt/html/oscpkt_8hh_source.html

// parse messages received by the OSC receiver
// msg is Message class of oscpkt: http://gruntthepeon.free.fr/oscpkt/

popInt32(i)
popInt64(h)
popFloat(f)
popDouble(d)
popStr(s)
popBlob(b)

*/

#include <Bela.h>
#include <cmath>
#include <libraries/Trill/Trill.h>

#include <libraries/OscSender/OscSender.h>
#include <libraries/OscReceiver/OscReceiver.h>

std::vector<Trill*> gTouchSensors;

OscReceiver oscReceiver;
OscSender oscSender;

int localPort = 6900; // PORTA LOCALE
int remotePort = 6901; // PORTA DI RICEZIONE


// To communicate with Bela connected with the PC 
// this is the IP of the PC 192.168.7.1 
// this is the IP of the Bela connected via USB 192.168.7.2 
const char* remoteIp = "192.168.7.1"; 

// Alternatively to communicate with Ethernet the IP has to be 192.168.10.1

bool handshakeReceived;

unsigned int gSampleCount = 0;
float gSendInterval = 0.1;
unsigned int gSendIntervalSamples;


void readLoop(void*)
{
	while(!Bela_stopRequested())
	{
		for(unsigned int n = 0; n < gTouchSensors.size(); ++n)
		{
			Trill* t = gTouchSensors[n];
			t->readI2C();
		}
		usleep(50000);
	}
}

void on_receive(oscpkt::Message* msg, const char* addr, void* arg)
{
	printf("From %s\n", addr);
	if(msg->match("/osc-setup-reply"))
		handshakeReceived = true;
	else if(msg->match("/osc-test")){
		int intArg;
		float floatArg;
		msg->match("/osc-test").popInt32(intArg).popFloat(floatArg).isOkNoMoreArgs();
		
		printf("received a message with int %i and float %f\n", intArg, floatArg);
		
	} else if(msg->match("/osc-data")){
		std::string data;
		msg->match("/osc-data").popStr(data).isOkNoMoreArgs();
		
		printf("received a message with string %s\n", data.c_str());
		
	}
}

bool setup(BelaContext *context, void *userData)
{
	
	oscReceiver.setup(localPort, on_receive);
	oscSender.setup(remotePort, remoteIp);

	// the following code sends an OSC message to address /osc-setup
	// then waits 1 second for a reply on /osc-setup-reply
	oscSender.newMessage("/osc-setup").send();
	
	int count = 0;
	int timeoutCount = 10;
	printf("Waiting for handshake ....\n");
	
	while(!handshakeReceived && ++count != timeoutCount)
	{
		usleep(100000);
	}
	if (handshakeReceived) {
		printf("handshake received!\n");
	} else {
		printf("timeout! : did you start the node server? `node /root/Bela/resources/osc/osc.js\n");
		return false;
	}
	
	unsigned int i2cBus = 1;
	for(uint8_t addr = 0x20; addr <= 0x50; ++addr)
	{
		Trill::Device device = Trill::probe(i2cBus, addr);
		if(Trill::NONE != device && Trill::CRAFT != device)
		{
			gTouchSensors.push_back(new Trill(i2cBus, device, addr));
			gTouchSensors.back()->printDetails();
		}
	}
	Bela_runAuxiliaryTask(readLoop);

	gSendIntervalSamples = context->audioSampleRate * gSendInterval;
	return true;
}

void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; ++n)
	{
		gSampleCount++;
		if(gSampleCount == gSendIntervalSamples)
		{
			gSampleCount = 0;
			float arr[2];
			for(unsigned int t = 0; t < gTouchSensors.size(); ++t) {
				
			arr[0] = gTouchSensors[t]->compoundTouchLocation();
			arr[1] = gTouchSensors[t]->compoundTouchSize();
			
			rt_printf("[%d] %.3f %.3f ", t, arr[0], arr[1]);
			
			oscSender.newMessage("/trill").add((int)t).add((float) arr[0]).add((float)arr[1]).send();
				
				
			}
			rt_printf("\n");
		}
	}
}

void cleanup(BelaContext *context, void *userData)
{
	for(auto t : gTouchSensors)
		delete t;
}
