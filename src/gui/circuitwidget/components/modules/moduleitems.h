/***************************************************************************
 *   Copyright (C) 2026 SimulIDE-AI-Sloped contributors                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODULEITEMS_H
#define MODULEITEMS_H

class ItemLibrary;

// Registers the sensors, power parts, switches, driver / breakout modules and
// the named semiconductor / regulator / buzzer presets in the library
void addModuleItems(ItemLibrary *lib);

#endif
