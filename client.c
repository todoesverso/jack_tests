/*
 * client.c - This is just a simple jack client that makes some noise ;).
 * Based on a jack example.
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <curses.h>
#include <signal.h>
#include <jack/jack.h>

jack_port_t **input_ports;
jack_port_t **output_ports;
jack_client_t *client;
/*The current sample rate*/
jack_nframes_t SAMPLE_RATE;
typedef jack_default_audio_sample_t sample_t;
bool curses_started = false;

/* Global variables for effects */
//float b = 0.636;
//float c = 1.572;
//int a = 1;
short int type_func = 0; 
float gain_wet = 1.0;
float gain_dry = 1.0;
float c = 1.0;
float d = 1.0;
float DIST = 0.9;

float __max(float a, float b)
{
        if ( a > b ) 
                return a;
        else
                return b;
}

float __min(float a, float b)
{
        if ( a < b ) 
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
	gain1 = 1.0/(selectivity + 1.0);

	cap = (sample + cap*selectivity )*gain1;
	sample = saturate((sample + cap*ratio)*gain2);

	return sample;
}

static void opamp(float *in, float *out, long nframes)
{
        long j;
        float temp;
        float A = 1/((104.7 - (100*DIST))*1E+6);
        float B = 1/((25*DIST)*1E-6);
        float C = 1/((25*DIST)*(104.7 - (100*DIST)));

        float A2 = 1 - 2*(2*A + B)/SAMPLE_RATE + 4*C/(SAMPLE_RATE*SAMPLE_RATE);
        float A1 = 2 - 8*C/(SAMPLE_RATE*SAMPLE_RATE);
        float A0 = 1 + 2*(2*A + B)/SAMPLE_RATE + 4*C/(SAMPLE_RATE*SAMPLE_RATE);

        float B2 = 1 - 2*(A + B)/SAMPLE_RATE + 4*C/(SAMPLE_RATE*SAMPLE_RATE);
        float B1 = A1;
        float B0 = 1 + 2*(A + B)/SAMPLE_RATE + 4*C/(SAMPLE_RATE*SAMPLE_RATE);

        //printf("A =%E  B =%E C =%E\n",A,B,C);
        //printf("A2 =%E  B1 =%E B0 =%E\n",A2,A1,A0);
        //printf("B2 =%E  B1 =%E B0 =%E\n",B2,B1,B0);
        //
        //
        for (j = 0; j < nframes; j++) {
                        out[j] = (float)(B0*A0*in[j] + B1*A0*in[j-1] + B2*A0*in[j-2] - (A1/A0)*out[j-1] - (A2/A0)*out[j-2]);
//                printf("out =%E  \n",in[j]);
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

static void signal_handler(int sig)
{
	jack_client_close(client);
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
}

static void update_parameters() {
	char key;
        float inc = 0.01;

	key = key_pressed(); 
	switch(key) {
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
                DIST +=inc;
		break;
	case 'c':
		c -= inc;
                DIST -=inc;
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

static float calculate_ir(float x) 
{
        float ir;

	switch(type_func) {
                case 1:
			ir = atanf(c * x);
                        break;
                case 2:
			ir = c*(x * x * x) + d*(x * x);
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

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */
int process(jack_nframes_t nframes, void *arg)
{
	int i, j; 
	//long double ir;
	sample_t *in, *out;
	
	update_parameters();
	
	for (i = 0; i < 2; i++) {
		out = jack_port_get_buffer(output_ports[i], nframes);
		in = jack_port_get_buffer(input_ports[i], nframes);

	/*	for (j = 0; j < nframes; j++) {
				//ir = atanf(1.572*in[j])*0.636;
				//ir = atanf(c*in[j])*b;
				//in[j] = a*sqrt(sqrt(in[j])) + c*c*(in[j]);
				//ir = calculate_ir(in[j]);
				//ir = BassBoosta(ir);				
                                if ((j>2) && (j<nframes-2))
                                        ir = opamp(in[j], in[j-1], in[j-2], out[j], out[j-1], out[j-2]);
                                else
                                        ir = 0.1;
				in[j] = (gain_dry * in[j]) + (gain_wet * ir);
		}
        */
		memcpy(out, in, nframes * sizeof(sample_t));
                opamp(in, out, nframes);
	}
	return 0;
}

int srate(jack_nframes_t nframes, void *arg)
{
	printf("the sample rate is now %lu/sec\n", nframes);
	SAMPLE_RATE = nframes;
	return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown(void *arg)
{
	free(input_ports);
	free(output_ports);
	exit(1);
}

int main(int argc, char *argv[])
{
	int i;
	const char **ports;
	const char *client_name;
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	if (argc >= 2) {	/* client name specified? */
		client_name = argv[1];
		if (argc >= 3) {	/* server name specified? */
			server_name = argv[2];
			options |= JackServerName;
		}
	} else {		/* use basename of argv[0] */

		client_name = strrchr(argv[0], '/');
		if (client_name == 0) {
			client_name = argv[0];
		} else {
			client_name++;
		}
	}

	/* open a client connection to the JACK server */

	client =
	    jack_client_open(client_name, options, &status, server_name);
	if (client == NULL) {
		fprintf(stderr, "jack_client_open() failed, "
			"status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf(stderr,
				"Unable to connect to JACK server\n");
		}
		exit(1);
	}
	if (status & JackServerStarted) {
		fprintf(stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf(stderr, "unique name `%s' assigned\n",
			client_name);
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	 */
	jack_set_process_callback(client, process, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	 */
	jack_set_sample_rate_callback(client, srate, 0);

	printf("engine sample rate: %lu\n", jack_get_sample_rate(client));

	jack_on_shutdown(client, jack_shutdown, 0);

	/* create two ports pairs */
	input_ports = (jack_port_t **) calloc(2, sizeof(jack_port_t *));
	output_ports = (jack_port_t **) calloc(2, sizeof(jack_port_t *));

	char port_name[16];
	for (i = 0; i < 2; i++) {
		sprintf(port_name, "input_%d", i + 1);
		input_ports[i] =
		    jack_port_register(client, port_name,
				       JACK_DEFAULT_AUDIO_TYPE,
				       JackPortIsInput, 0);
		sprintf(port_name, "output_%d", i + 1);
		output_ports[i] =
		    jack_port_register(client, port_name,
				       JACK_DEFAULT_AUDIO_TYPE,
				       JackPortIsOutput, 0);
		if ((input_ports[i] == NULL) || (output_ports[i] == NULL)) {
			fprintf(stderr, "no more JACK ports available\n");
			exit(1);
		}
	}

	/* Tell the JACK server that we are ready to roll.  
	 * Our process() callback will start running now. 
	 */
	if (jack_activate(client)) {
		fprintf(stderr, "cannot activate client");
		exit(1);
	}

	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */
	ports =
	    jack_get_ports(client, NULL, NULL,
			   JackPortIsPhysical | JackPortIsOutput);
	if (ports == NULL) {
		fprintf(stderr, "no physical capture ports\n");
		exit(1);
	}

	for (i = 0; i < 2; i++)
		if (jack_connect
		    (client, ports[i], jack_port_name(input_ports[i])))
			fprintf(stderr, "cannot connect input ports\n");

	free(ports);

	ports =
	    jack_get_ports(client, NULL, NULL,
			   JackPortIsPhysical | JackPortIsInput);
	if (ports == NULL) {
		fprintf(stderr, "no physical playback ports\n");
		exit(1);
	}

	for (i = 0; i < 2; i++)
		if (jack_connect
		    (client, jack_port_name(output_ports[i]), ports[i]))
			fprintf(stderr, "cannot connect input ports\n");

	free(ports);

	/* install a signal handler to properly quits jack client */
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);

	/* keep running until the transport stops */
	while (1) {
		sleep(1);
	}

	jack_client_close(client);
	exit(0);
}
