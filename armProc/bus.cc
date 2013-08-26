/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * uARM
 *
 * Copyright (C) 2013 Marco Melletti
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef UARM_SYSTEMBUS_CC
#define UARM_SYSTEMBUS_CC

#include "bus.h"

systemBus::systemBus(){
	ram = new ramMemory();
	cycles = 0;
}

systemBus::~systemBus(){
	delete ram;
}

void systemBus::fetch(Word *address){
	pipeline[PIPELINE_EXECUTE] = pipeline[PIPELINE_DECODE];
	pipeline[PIPELINE_DECODE] = pipeline[PIPELINE_FETCH];
	pipeline[PIPELINE_FETCH] = ram->readW(address);
}

#endif //UARM_SYSTEMBUS_CC