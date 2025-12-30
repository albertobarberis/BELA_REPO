#include "button.hpp"

Button::Button(unsigned int digitalPin_) {
    digitalPin = digitalPin_;
} // Default constructor

void Button::setState(unsigned int state_){
    state = state_;
}

unsigned int Button::getState(){
    return state;
}

unsigned int Button::getPin(){
    return digitalPin;
}