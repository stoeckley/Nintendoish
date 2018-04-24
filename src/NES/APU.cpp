#include "APU.h"
#include <iostream>

using namespace std;
using namespace NES;

uint8_t APU::getAPURegister(uint16_t index) {
    switch (index) {
    case 0x4015:
        return ((triangle.lengthCounter > 0) << 0x2) | ((pulse2.lengthCounter > 0) << 0x1) | (pulse1.lengthCounter > 0);
        break;
    case 0x4017:
        return frameCounter;
        break;
    default:
        return 0x0;
        break;
    }
}

void APU::setAPURegister(uint16_t index, uint8_t value) {
    if (index >= 0x4000 && index <= 0x4003) pulse1.writeRegister(index, value);
    if (index >= 0x4004 && index <= 0x4007) pulse2.writeRegister(index - 0x4, value);
    if (index >= 0x4008 && index <= 0x400B) triangle.writeRegister(index, value);
    if (index >= 0x400C && index <= 0x400F) processNoise(index, value);
    if (index >= 0x4010 && index <= 0x4013) processDMC(index, value);
    if (index == 0x4015) processControl(index, value);
    if (index == 0x4017) frameCounter = value;
}

void APU::processNoise(uint16_t index, uint8_t value) {

}

void APU::processDMC(uint16_t index, uint8_t value) {

}

void APU::processControl(uint16_t index, uint8_t value) {
    //pulse1.enabled = ((value & 0x1) == 0x1);
    if (pulse1.enabled == false) pulse1.lengthCounter = 0x0;
    //pulse2.enabled = ((value & 0x2) == 0x2);
    if (pulse2.enabled == false) pulse2.lengthCounter = 0x0;
    triangle.enabled = ((value & 0x4) == 0x4);
    if (triangle.enabled == false) triangle.lengthCounter = 0x0;
}

float APU::sample() {
    //TODO Lookup instead of linear approximation
    float pulseOut = 0.00752f * (float)(pulse1.sample() + pulse2.sample());
    float tndOut = 0.00851 * triangle.sample();
    return pulseOut + tndOut;
}

void APU::step() {
    totalCycles++;
    triangle.stepTimer();

    if (totalCycles % 2 == 0) {
        stepFrameCounter();
    }
    
    static bool r = 0;
    static int nextClock = (console.CPU_CLOCK_RATE / console.AUDIO_SAMPLE_RATE);
    if (totalCycles == nextClock) {
        r = !r;
        nextClock += (console.CPU_CLOCK_RATE / console.AUDIO_SAMPLE_RATE) + r;
        if (console.audioBufferLength < console.AUDIO_BUFFER_SIZE) {
            console.audioBuffer[console.audioBufferLength++] = sample();
        }
    } 
}

void APU::stepFrameCounter() {
    
    currentCycle++;

    pulse1.stepTimer();
    pulse2.stepTimer();

    bool sequence = ((frameCounter & 0x80) == 0x80);
    bool irqEnabled = ((frameCounter & 0x40) == 0x40);
    
    if (sequence == true) {
        //5-Step Sequence
        if (currentCycle == 3729) {
            //Clock Envelopes and Triangle Linear Counter
            pulse1.stepEnvelope();
            pulse2.stepEnvelope();
            triangle.stepLinearCounter();
        }
        if (currentCycle == 7457) {
            //Clock Envelopes and Triangle Linear Counter
            //Clock Length Counters and Sweep Units 
            pulse1.stepLengthCounter();
            pulse1.stepEnvelope();
            pulse1.stepSweep();

            pulse2.stepLengthCounter();
            pulse2.stepEnvelope();
            pulse2.stepSweep();

            triangle.stepLinearCounter();
        }
        if (currentCycle == 11186) {
            //Clock Envelopes and Triangle Linear Counter
        }
        if (currentCycle == 18641) {
            //Clock Envelopes and Triangle Linear Counter
            //Clock Length Counters and Sweep Units 
            currentCycle = 0;
            pulse1.stepLengthCounter();
            pulse1.stepEnvelope();
            pulse1.stepSweep();

            pulse2.stepLengthCounter();
            pulse2.stepEnvelope();
            pulse2.stepSweep();

            triangle.stepLinearCounter();
        }
    } else {
        //4-Step Sequence
        if (currentCycle == 3729) {
            //Clock Envelopes and Triangle Linear Counter
            pulse1.stepEnvelope();
            pulse2.stepEnvelope();
            triangle.stepLinearCounter();
        }
        if (currentCycle == 7457) {
            //Clock Envelopes and Triangle Linear Counter
            //Clock Length Counters and Sweep Units 
            pulse1.stepLengthCounter();
            pulse1.stepEnvelope();
            pulse1.stepSweep();

            pulse2.stepLengthCounter();
            pulse2.stepEnvelope();
            pulse2.stepSweep();

            triangle.stepLinearCounter();
        }
        if (currentCycle == 11186) {
            //Clock Envelopes and Triangle Linear Counter
            pulse1.stepEnvelope();
            pulse2.stepEnvelope();
            triangle.stepLinearCounter();
        }
        if (currentCycle == 14914) {
            //IRQ
            if (irqEnabled) {
                printf("Triggering IRQ");
                console.cpu->requestIRQ = true;
            }
        }
        
        if (currentCycle == 14915) {
            //Clock Envelopes and Triangle Linear Counter
            //Clock Length Counters and Sweep Units 
            currentCycle = 0;
            pulse1.stepLengthCounter();
            pulse1.stepEnvelope();
            pulse1.stepSweep();

            pulse2.stepLengthCounter();
            pulse2.stepEnvelope();
            pulse2.stepSweep();

            triangle.stepLinearCounter();
        }
    }
}
