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
#ifndef __WS_WEBSOCKET_CLIENT_H
#define __WS_WEBSOCKET_CLIENT_H
#include <openssl/ssl.h>

#define WEBSOCKET_WS           80
#define WEBSOCKET_WSS          9443

struct data {
	unsigned char *message;
	int length;
};

struct wsclient {
	int socket;
	int type;
	char *host;
	char *room;
	char *guidkey;
	char *ip_origin;
	char error[255];
	SSL *ssl;
	const SSL_METHOD *method;
	SSL_CTX *ctx;

	struct data data;

};
/* указать данные для отправки */
void ws_set_data ( struct wsclient *ws, const char *data );
/* соединиться с сервером.
 * @site, указывать без ws://.
 * port это тип, либо WEBSOCKET_WS, либо WEBSOCKET_WSS.
 * @room это например /chat или /echo
 * @ip_origin обратный адрес
 */
struct wsclient ws_connect_to ( const char *site, const unsigned short port, const char *room, const char *ip_origin );
/*
 * отправить данные
 */
size_t ws_write ( struct wsclient *ws );
/*
 * прочитать данные от websocket сервера
 * @buffer сюда запишется строка.
 */
size_t ws_read ( struct wsclient *ws, char *buffer, const unsigned int max_size );

void ws_close ( struct wsclient *ws );
#endif
