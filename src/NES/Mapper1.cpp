#include "Mapper1.h"
#include <stdlib.h> 
#include <stdio.h>

using namespace NES;

Mapper1::Mapper1(Cartridge &cartridge) : Mapper(cartridge) {
    prgOffset1 = 0x0;
    prgOffset2 = cartridge.prgSize - 1;

    chrOffset1 = 0x0;
    chrOffset2 = cartridge.chrSize - 1;
}

uint8_t Mapper1::getTileData(uint16_t index) {
    if (index < 0x1000) {
        return cartridge.chr[index + (chrOffset1 * 0x1000)];
    } else {
        return cartridge.chr[(index - 0x1000) + (chrOffset2 * 0x1000)];
    }
}

void Mapper1::setTileData(uint16_t index, uint8_t value) {
    if (chrBankMode == 0x0) cartridge.chr[index] = value;
    if (index < 0x1000) {
        cartridge.chr[index + (chrOffset1 * 0x1000)] = value;
    } else {
        cartridge.chr[(index - 0x1000) + (chrOffset2 * 0x1000)] = value;
    }
}

uint8_t Mapper1::getProgramData(uint16_t index) {
    if (index >= 0xC000) {
        return cartridge.prg[(index - 0xC000) + (prgOffset2 * 0x4000)]; // Calculation done inline to prevent 16 bit overflow
    } else if (index >= 0x8000) {
        return cartridge.prg[(index - 0x8000) + (prgOffset1 * 0x4000)]; // Calculation done inline to prevent 16 bit overflow
    } else if (index >= 0x6000) {
        return prgRAM[index - 0x6000];
    }
    return 0x0;
}

void Mapper1::setProgramData(uint16_t index, uint8_t value) {

    if (index < 0x8000) {
        prgRAM[index - 0x6000] = value;
        return;
    }

    if ((value & 0x80) == 0x80) {
        clearLoadRegister();
        return;
    } 

    bool copyData = ((loadRegister & 0x1) == 0x1); //Shift register is full. Copy t0 control.
    
    //Shift load register.
    loadRegister >>= 1;
    loadRegister |= ((value & 0x1) << 4);
    if (copyData) { 
        if (index < 0xA000) {
            //Load Control
            chrBankMode = (loadRegister & 0x10) == 0x10; // 0: switch 8 KB at a time; 1: switch two separate 4 KB banks
            prgBankMode = ((loadRegister & 0xC) >> 2);
            switch (loadRegister & 0x3) {
            case 0:
            case 1:
                cartridge.horizontalMirroring = true;
                cartridge.verticalMirroring = true;
                break;
            case 2:
                cartridge.horizontalMirroring = false;
                cartridge.verticalMirroring = true;
                break;
            case 3:
                cartridge.horizontalMirroring = true;
                cartridge.verticalMirroring = false;
                break;
            }
        } else if (index < 0xC000) {
            //  Select 4 KB or 8 KB CHR bank at PPU $0000 (low bit ignored in 8 KB mode)
            if (chrBankMode == 0x0) {
                chrOffset1 = loadRegister & 0xE;
                chrOffset2 = chrOffset1 + 0x1;
            } else if (chrBankMode == 0x1) {
                chrOffset1 = loadRegister;
            }
        } else if (index < 0xE000) {
            // Select 4 KB CHR bank at PPU $1000 (ignored in 8 KB mode)
            if (chrBankMode == 0x1) {
                chrOffset2 = loadRegister;
            }
        } else if (index <= 0xFFFF) {
            if (prgBankMode == 0x0 || prgBankMode == 0x1) {
                // 0, 1: switch 32 KB at $8000, ignoring low bit of bank number;
                prgOffset1 = loadRegister & 0xE;
                prgOffset2 = prgOffset1 + 0x1;
            } else if (prgBankMode == 0x2) {
                // 2: fix first bank at $8000 and switch 16 KB bank at $C000;
                prgOffset1 = 0x0;
                prgOffset2 = loadRegister & 0xF;
            } else if (prgBankMode == 0x3) {
                // 3: fix last bank at $C000 and switch 16 KB bank at $8000
                prgOffset1 = loadRegister & 0xF;
                prgOffset2 = cartridge.prgSize - 1;
            }
        }
        clearLoadRegister();
    }
}

void Mapper1::clearLoadRegister() {
    loadRegister = 0x10;
}


