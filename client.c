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
int a = 1;
typedef jack_default_audio_sample_t sample_t;
bool curses_started = false;

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
		return '0';
}

static void signal_handler(int sig)
{
	jack_client_close(client);
	fprintf(stderr, "signal received, exiting ...\n");
	exit(0);
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
	char k;
	sample_t *in, *out;

	k = key_pressed(); 
	if (k == 'a') {
		a++;
	}
	if (k == 'z') {
		a--;
	}

	for (i = 0; i < 2; i++) {
		in = jack_port_get_buffer(input_ports[i], nframes);
		for (j = 0; j < nframes; j++) {
			if (in[j] < 0) {
				in[j] = in[j] * (-90.0f) * a;
			}
		}

		out = jack_port_get_buffer(output_ports[i], nframes);
		for (j = 0; j < nframes; j++) {
			if (out[j] > 0) {
				out[j] = out[j] * (-10.0f) * a;
			}
		}

		memcpy(out, in, nframes * sizeof(sample_t));
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
