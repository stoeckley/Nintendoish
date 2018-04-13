#include "glfw3.h"
#include "Display.h"
#include "CHIP8/Chip8.h"
#include <iostream>
#include <stdio.h>
#include <chrono>
#include <thread>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "NES/NES.h"
#include "NES/PPU.h"
#include "wavefile.h"

using namespace std::chrono;
using namespace std;

static Display *display;
static NES::Console *nes;

bool pause = false;
bool showLog = false;
bool timeSynch = false;
int debugStartLineNumber = 2000;
int lineNumber = 0x0;

const double cyclesPerSecond = 1789773;
const double audioSamplesPerSecond = 1789773;
const double targetFPS = 60.0988; //NTSC Vertical Scan Rate

Wave wav = makeWave(audioSamplesPerSecond, 1, 32);

void updateDisplay() {
    
    glClear(GL_COLOR_BUFFER_BIT);
    for (int x = 0; x < 256; x++) {
        for (int y = 0; y < 240; y++) {
            unsigned int color = nes->displayBuffer[x + (y * 256)];
            if (color) {
                float blue = (float)(color & 0x000000FF);
                float green = (float)((color & 0x0000FF00) >> 8);
                float red = (float)((color & 0x00FF0000) >> 16);
                display->plotPixel(x, y, red / 255.0, green / 255.0, blue / 255.0);
            }
        }
    }

    glEnd();
    glfwSwapBuffers(display->window);
}

void updateAudio() {
    for (unsigned int i = 0; i < nes->audioBufferLength; i += (cyclesPerSecond / audioSamplesPerSecond)) {
        waveAddSample(&wav, &nes->audioBuffer[i]);
    }
    nes->audioBufferLength = 0;
}

void updateNES() {
    //Update one frame
    int currentFrame = nes->ppu->totalFrames;
    while (nes->ppu->totalFrames == currentFrame) nes->emulateCycle();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    bool isKeyDown = action == GLFW_PRESS;
    bool isKeyUp = action == GLFW_RELEASE;
    if (!isKeyUp && !isKeyDown) return;
    switch (key) {
        case GLFW_KEY_A:
            nes->controllerOne->b = isKeyDown;
            break;
        case GLFW_KEY_S:
            nes->controllerOne->a = isKeyDown;
            break;
        case GLFW_KEY_UP:
            nes->controllerOne->up = isKeyDown;
            break;
        case GLFW_KEY_DOWN:
            nes->controllerOne->down = isKeyDown;
            break;
        case GLFW_KEY_LEFT:
            nes->controllerOne->left = isKeyDown;
            break;
        case GLFW_KEY_RIGHT:
            nes->controllerOne->right = isKeyDown;
            break;
        case GLFW_KEY_RIGHT_SHIFT:
            nes->controllerOne->select = isKeyDown;
            break;
        case GLFW_KEY_ENTER:
            nes->controllerOne->start = isKeyDown;
            break;
        case GLFW_KEY_P:
            if (isKeyDown) pause = !pause;
            break;
        default:
            break;
    }
}

void logLoop() {
    while (!glfwWindowShouldClose(display->window)) {
        if (!pause) {
            if (nes->cpu->stallCycles == 0x0) {
                
                std::stringstream ss;
                
                uint8_t nextInstruction = nes->memory->get(nes->cpu->reg.PC);
                
                ss << uppercase << setfill('0') << setw(4) << hex << nes->cpu->reg.PC << "  " << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nextInstruction;
                /*if (reg.PC - startingPC > 1) {
                 debugBuffer << " " << uppercase << setfill('0') << setw(2) << hex << (uint16_t)lo;
                 }
                 else {
                 debugBuffer << "   ";
                 }
                 if (reg.PC - startingPC > 2) {
                 debugBuffer << " " << uppercase << setfill('0') << setw(2) << hex << (uint16_t)hi;
                 }
                 else {
                 debugBuffer << "   ";
                 }*/
                ss << "      ";
                ss << "  " << nes->cpu->debugTable[nextInstruction];
                ss << "                             A:" << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nes->cpu->reg.A;
                ss << " X:" << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nes->cpu->reg.X;
                ss << " Y:" << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nes->cpu->reg.Y;
                ss << " P:" << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nes->cpu->reg.P.byte;
                ss << " SP:" << uppercase << setfill('0') << setw(2) << hex << (uint16_t)nes->cpu->reg.S;
                ss << " CYC:" << setfill(' ') << dec << setw(3) << nes->ppu->currentCycle << " SL:" << nes->ppu->currentScanline;
                
                lineNumber++;
                
                string emuLine = ss.str();
                if (lineNumber >= debugStartLineNumber) {
                    cout << lineNumber << " " << emuLine << endl;
                }
                
            }
            
            int currentFrame = nes->ppu->totalFrames;
            nes->emulateCycle();
            if (nes->ppu->totalFrames != currentFrame) updateDisplay();
        }
        glfwPollEvents();
    }
    glfwTerminate();
}

void gameLoop() {
    
    waveSetDuration(&wav, 60);
    double targetFrameLength = 1000.0/targetFPS;
    double startTime = glfwGetTime() * 1000;
    
    int frameCount = 0;
    int droppedFrames = 0;
    int sleptFrames = 0;
    
    while (!glfwWindowShouldClose(display->window)) {
        bool skipFrame = false;
        bool sleepFrame = false;
        
        long realTime = (glfwGetTime() * 1000) - startTime; //How much time we've taken
        long gameTime = (frameCount * targetFrameLength);   //How much time in the game
        
        if (gameTime - realTime > targetFrameLength) sleepFrame = true; //We are behind. Sleep a frame.
        if (realTime - gameTime > targetFrameLength) skipFrame = true; //We are ahead. Skip rendeirng.
        
        if (!pause) {
            if (sleepFrame && timeSynch) {
                printf("Sleeping\n");
                std::this_thread::sleep_for(std::chrono::milliseconds(realTime - gameTime));
                sleptFrames++;
            }
            updateNES();
            updateAudio();
            if (!skipFrame || !timeSynch){
                updateDisplay();
            } else {
                printf("Skipping frame\n");
                droppedFrames++;
            }
            frameCount++;
        } else {
            startTime = glfwGetTime() * 1000;
            frameCount = 0;
        }
        glfwPollEvents();
    }
    waveToFile(&wav, "output.wav");
    glfwTerminate();
}

int main(int argc, char** argv) {
    display = new Display(4, 256, 240);
    display->initialize(argc, argv);
    //glfwSetInputMode(display->window, GLFW_STICKY_KEYS, 1);
    glfwSetKeyCallback(display->window, keyCallback);
    nes = new NES::Console();
    
    
    
    //nes->loadProgram("../roms/1-clocking.nes");
    //nes->loadProgram("../roms/PunchOut.nes");
    //nes->loadProgram("../roms/test/scanline.nes");
    //nes->loadProgram("../roms/Battletoads.nes");
    //nes->loadProgram("../roms/Gradius.nes");
    //nes->loadProgram("../roms/Contra.nes");
    //nes->loadProgram("../roms/Metroid.nes");
    //nes->loadProgram("../roms/IceClimber.nes");
    //nes->loadProgram("../roms/Megaman3.nes");
    //nes->loadProgram("../roms/Castlevania.nes");
    nes->loadProgram("../roms/Zelda.nes");
    //nes->loadProgram("../roms/SuperMario3.nes");
    //nes->loadProgram("../roms/Excitebike.nes");
    //nes->loadProgram("../roms/DonkeyKong.nes");
    //nes->loadProgram("../roms/sprite_ram.nes");

    if (showLog) {
        logLoop();
    } else {
        gameLoop();
    }
    return 0;
}
