#pragma once

class Button {
	public:
        Button(unsigned int digitalPin);
        void setState(unsigned int state);
        unsigned int getState();
        unsigned int getPin();
        ~Button(); 
    private:
        unsigned int digitalPin;
        unsigned int state;
};

