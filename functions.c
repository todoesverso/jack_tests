/*
 * functions.c - This is just a simple library of silly functions.
 *
 * Copyright (C) 2010  todoesverso (todoesverso at gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <curses.h>

#include "functions.h"

/* Global variables for effects */
short int type_func = 0;
float gain_wet = 1.0;
float gain_dry = 1.0;
float c = 1.0;
float d = 1.0;
float DIST = 0.9;
bool curses_started = false;



float __max(float a, float b)
{
	if (a > b)
		return a;
	else
		return b;
}

float __min(float a, float b)
{
	if (a < b)
		return a;
	else
		return b;
}

#define saturate(x) __min(__max(-1.0,x),1.0)

float BassBoosta(float sample)
{
	static float selectivity, gain1, gain2, ratio, cap;
	selectivity = 140;
	ratio = 0.9;
	gain2 = 0.9;
	gain1 = 1.0 / (selectivity + 1.0);

	cap = (sample + cap * selectivity) * gain1;
	sample = saturate((sample + cap * ratio) * gain2);

	return sample;
}

void opamp(float *in, float *out, long nframes, long SR)
{
	long i;
	float A = 1 / ((104.7 - (100 * DIST)) * 1E+6);
	float B = 1 / ((25 * DIST) * 1E-6);
	float C = 1 / ((25 * DIST) * (104.7 - (100 * DIST)));

	float A2 = 1 - 2 * (2 * A + B) / SR + 4 * C / (SR * SR);
	float A1 = 2 - 8 * C / (SR * SR);
	float A0 = 1 + 2 * (2 * A + B) / SR + 4 * C / (SR * SR);

	float B2 = 1 - 2 * (A + B) / SR + 4 * C / (SR * SR);
	/* float B1 = A1; */
	float B0 = 1 + 2 * (A + B) / SR + 4 * C / (SR * SR);

	for (i = 0; i < nframes; i++) {
		out[i] =
		    (float) (B0 * A0 * in[i] + B1 * A0 * in[i - 1] +
			     B2 * A0 * in[i - 2] - (B1 / A0) * out[i - 1] -
			     (A2 / A0) * out[i - 2]);
	}

}

void stop_curses()
{
	if (curses_started && !isendwin())
		endwin();
}

void start_curses()
{
	if (curses_started) {
		refresh();
	} else {
		initscr();
		cbreak();
		noecho();
		intrflush(stdscr, false);
		keypad(stdscr, true);
		atexit(stop_curses);
		curses_started = true;
	}
}

char key_pressed()
{
	char ch;
	start_curses();
	timeout(0);
	ch = getch();
	stop_curses();
	if (ch != ERR)
		return ch;
	else
		return 'p';
}

void update_parameters()
{
	char key;
	float inc = 0.01;

	key = key_pressed();
	switch (key) {
	case 'a':
		gain_dry = inc;
		break;
	case 'z':
		gain_dry -= inc;
		break;
	case 's':
		gain_wet += inc;
		break;
	case 'x':
		gain_wet -= inc;
		break;
	case 'd':
		c += inc;
		DIST += inc;
		break;
	case 'c':
		c -= inc;
		DIST -= inc;
		break;
	case 'f':
		d += inc;
		break;
	case 'v':
		d -= inc;
		break;
	case '1':
		type_func = 1;
		break;
	case '2':
		type_func = 2;
		break;
	case '3':
		type_func = 3;
		break;
	case 'm':
		gain_dry = 0;
		break;
	case '0':
		gain_wet = 0;
		break;

	default:
		break;
	}
}

float calculate_ir(float x)
{
	float ir;

	switch (type_func) {
	case 1:
		ir = atanf(c * x);
		break;
	case 2:
		ir = c * (x * x * x) + d * (x * x);
		break;
	case 3:
		//ir = atanf(c * x);
		ir = sqrt(c * x);
		break;
	default:
		ir = 0;
	}

	return ir;

}
