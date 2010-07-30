/*
 * functions.h - Headers for function.s.
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

float BassBoosta(float sample);
void opamp(float *in, float *out, long nframes, long SR);
char key_pressed(void); 
void update_parameters(void);
float calculate_ir(float x); 
