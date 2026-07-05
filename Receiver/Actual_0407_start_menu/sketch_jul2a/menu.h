#pragma once

#include <Arduino.h> 
#include "app_state.h"

void Menu_init();
void Menu_update();

bool Menu_isOpen();
uint8_t Menu_getSelected();
AppMode Menu_getMode();