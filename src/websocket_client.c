/*
 * binance - программа для отслеживания курса криптовалюты.
 *
 * Copyright (C) 2020 Naidolinsky Dmitry <naidv88@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY of FITNESS for A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * -------------------------------------------------------------------/
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include "websocket_client.h"

#define SIZE_OUT      16384
#define SIZE_IN       16384
const char *GUID_KEY = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char *sec_websocket_key = "dGhlIHNhbXBsZSBub25jZQ==";

static void handshake ( struct wsclient *ws ) {
	char *out = calloc ( SIZE_OUT, 1 );
	char *in = calloc ( SIZE_IN, 1 );
	snprintf ( out, SIZE_OUT - 1,
			"GET /%s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: %s\r\n"
			"Origin: %s\r\n"
			"Sec-WebSocket-Protocol: chat, superchat\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"\r\n"
			,
			ws->room,
			ws->host,
			sec_websocket_key,
			ws->ip_origin
		 );
	switch ( ws->type ) {
		case WEBSOCKET_WS:
			write ( ws->socket, out, strlen ( out ) );
			read ( ws->socket, in, SIZE_IN - 1 );
			break;
		case WEBSOCKET_WSS:
			SSL_write ( ws->ssl, out, strlen ( out ) );
			SSL_read ( ws->ssl, in, SIZE_IN - 1 );
			break;
	}

	free ( out );
	free ( in );

}


static void set_fragment ( struct data *gl, unsigned char n ) {
	unsigned char f_byte = 0;
	f_byte = ( n << 7 );
	gl->message[0] |= f_byte;
}

static void set_opcode ( struct data *gl, unsigned char n ) {
	unsigned char f_byte = 0;
	f_byte = ( n << 0 );
	gl->message[0] |= f_byte;
}
#define SIZE_MESSAGE          16384

static void set_mask_pre ( struct data *gl, const char *data, const int length_buf ) {
	unsigned char *s = &gl->message[2];
	unsigned char *ss = s;
	for ( int i = 0; i < 4 && i < length_buf; i++ ) {
		s [ i ] = data[ i ] ^ data[ i % 4 ];
	}
	s += 4;
	for ( int i = 0; i < length_buf; i++ ) {
		s [ i ] = data[ i ] ^ ss[ i % 4 ];
	}

}
static void set_mask_post ( struct data *gl, const char *data, const int length_buf ) {
	gl->length += 2;
	unsigned short len = length_buf;
	gl->message[2] = len >>  8 & 0xff;
	gl->message[3] = len >>  0 & 0xff;
	char *s = &gl->message[4];
	char *ss = s;
	for ( int i = 0; i < 4; i++ ) {
		ss [ i ] = data[ i ] ^ data [ i % 4 ];
	}
	s += 4;
	for ( int i = 0; i < length_buf; i++ ) {
		s [ i ] = data[ i ] ^ ss[ i % 4 ];
	}
}
void ws_set_data ( struct wsclient *ws, const char *data ) {
	if ( !ws->data.message ) ws->data.message = calloc ( SIZE_MESSAGE, 1 );
	else memset ( ws->data.message, 0, SIZE_MESSAGE );

	struct data *gl = &ws->data;
	set_fragment ( gl, 1 );
	set_opcode ( gl, 1 );

	gl->length = 2;
	int length_buf = strlen ( data );
	gl->length += length_buf + 4;
	unsigned char s_byte = 0;
	s_byte |= ( 1 << 7 );
	if ( length_buf < 126 ) {
		s_byte |= length_buf;
		gl->message[1] = s_byte;
		set_mask_pre ( gl, data, length_buf );
	} else {
		s_byte |= 126;
		gl->message[1] = s_byte;
		set_mask_post ( gl, data, length_buf );
	}
}


struct wsclient ws_connect_to ( const char *site, const unsigned short port, const char *room, const char *ip_origin ) {
	struct hostent *ht = gethostbyname ( site );

	struct wsclient ws;
	memset ( &ws, 0, sizeof ( ws ) );

	struct sockaddr_in s;
	memset ( &s, 0, sizeof ( s ) );

	s.sin_family = AF_INET;
	s.sin_port = htons ( port );
	s.sin_addr.s_addr = *( ( in_addr_t * ) ht->h_addr );
	ws.type = port;

	ws.socket = socket ( AF_INET, SOCK_STREAM, 0 );
	if ( ws.socket == -1 ) {
		perror ( "socket" );
		exit ( EXIT_FAILURE );
	}
	ws.room = strdup ( room );
	ws.ip_origin = calloc ( 255, 1 );
	switch ( ws.type ) {
		case WEBSOCKET_WS:
			snprintf ( ws.ip_origin, 254, "http://%s", ip_origin );
			break;
		case WEBSOCKET_WSS:
			snprintf ( ws.ip_origin, 254, "https://%s", ip_origin );
			break;
	}
	int len = strlen ( ws.ip_origin );
	ws.ip_origin = realloc ( ws.ip_origin, len + 1 );

	if ( connect ( ws.socket, ( struct sockaddr * ) &s, sizeof ( s ) ) == -1 ) {
		perror ( "connect" );
		exit ( EXIT_FAILURE );
	}


	ws.host = calloc ( strlen ( site ), 1 );
	snprintf ( ws.host, 511, "%s", site );

	len = strlen ( GUID_KEY );
	ws.guidkey = calloc ( len + 1, 1 );
	strncpy ( ws.guidkey, GUID_KEY, len );

	switch ( ws.type ) {
		case WEBSOCKET_WS: 
			handshake ( &ws );
			return ws;
		case WEBSOCKET_WSS:
				   ws.method = SSLv23_client_method ( );
				   ws.ctx = SSL_CTX_new ( ws.method );
				   ws.ssl = SSL_new ( ws.ctx );
				   SSL_set_fd ( ws.ssl, ws.socket );
				   SSL_connect ( ws.ssl );
				   handshake ( &ws );
				   return ws;
				   break;
		default:
				   snprintf ( ws.error, 254, "Не правильный порт указан.\n" );
				   return ws;
				   break;
	}
	return ws;
}


size_t ws_write ( struct wsclient *ws ) { 
	size_t ret = -1;

	switch ( ws->type ) { 
		case WEBSOCKET_WS:
			ret = write ( ws->socket, ws->data.message, ws->data.length );
			return ret;
		case WEBSOCKET_WSS:
			ret = SSL_write ( ws->ssl, ws->data.message, ws->data.length );
			return ret;
	}
	return ret;
}

size_t ws_read ( struct wsclient *ws, char *data, const unsigned int max_size ) {
	unsigned char *dt = calloc ( SIZE_IN, 1 );
	int ret = -1;
	unsigned char pong[2] = { 0x8a, 0x00 };

	while ( 1 ) {
		memset ( dt, 0, SIZE_IN );
		switch ( ws->type ) {
			case WEBSOCKET_WS:
				ret = read ( ws->socket, dt, max_size );
				break;
			case WEBSOCKET_WSS:
				ret = SSL_read ( ws->ssl, dt, max_size );
				break;
			default:
				free ( dt );
				return ret;
		}

		if ( ret == 0 || ret == -1 ) return -1;

		if ( dt[0] == 0x89 ) {
			switch ( ws->type ) {
				case WEBSOCKET_WS:
					write ( ws->socket, pong, 2 );
					break;
				case WEBSOCKET_WSS:
					SSL_write ( ws->ssl, pong, 2 );
					break;
			}
		}
		if ( dt[0] == 0x88 ) {
			if ( dt[1] == 126 ) {
				int size = ( dt[2] << 8 ) | dt[3];
				int size_valid = max_size < size ? max_size : size;
				char *s = &dt[3];
				strncpy ( data, s, size_valid );
				free ( dt );
				return size;
			}
			int size = dt[1];
			char *s = &dt[2];
			int size_valid = max_size < size ? max_size : size;
			strncpy ( data, s, size );
			free ( dt );
			return -1;
		}
		unsigned char *s;
		unsigned short size = 0;
		if ( dt[0] == 0x81 ) {
			if ( dt[1] == 126 ) {
				size = ( dt[2] << 8 ) | dt[3];
				int size_valid = max_size < size ? max_size : size;
				s = &dt[3];
				strncpy ( data, s, size_valid );
				free ( dt );
				return size;
			}
			size = dt[1];
			s = &dt[2];
			int size_valid = max_size < size ? max_size : size;
			strncpy ( data, s, size );
			free ( dt );
			return size;
		}
	}
	free ( dt );
	return ret;
}

void ws_close ( struct wsclient *ws ) {
	if ( ws->data.message ) free ( ws->data.message );
	if ( ws->host ) free ( ws->host );
	if ( ws->room ) free ( ws->room );
	if ( ws->guidkey ) free ( ws->guidkey );
	if ( ws->ip_origin ) free ( ws->ip_origin );
	SSL_CTX_free ( ws->ctx );
	SSL_free ( ws->ssl );
	close ( ws->socket );

}
