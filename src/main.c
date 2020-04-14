/*
 * binance - программа для отслеживания курса криптовалюты
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
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include <json-c/json.h>
#include <canberra-gtk.h>
#include "vars.h"
#include <libappindicator/app-indicator.h>
#include "websocket_client.h"

#define BUF_SIZE_DATA           16384
#define BUF_SEND_SIGNAL          2048

struct wsclient ws;
GtkApplication *app;
GtkWidget *window;
GtkWidget *window_settings;
GtkWidget *label_btc_update;
GtkWidget *label_eth_update;

char *config_dir;
char *config_file;
char *config_file_error;

double price_btc_upper_percent;
double price_btc_lower;
double btcusd_percent_point;

double price_eth_upper_percent;
double price_eth_lower;
double eth_percent_point;

#define TYPE_OF_COIN_BTC               0
#define TYPE_OF_COIN_ETH               1

GNotification *notify;

enum {
	ID,
	NOTIFY,
	TIME,
	SYMBOL,
	START,
	CLOSE,
	SYMBOL_TRADE,
	INTERVAL,
	FIRST_TRADE_ID,
	LAST_TRADE_ID,
	OPEN_PRICE,
	CLOSE_PRICE,
	HIGH_PRICE,
	LOW_PRICE,
	BASE_ASSET_VOLUME,
	NUMBER_OF_TRADER,
	IS_KLINE_CLOSED,
	QUOTE_ASSET_VOLUME,
	TAKER_BASE_ASSET_VOLUME,
	TAKER_QUOTE_ASSET_VOLUME,
	N_COUNT
};

struct settings_symbol {
	int btc_update;
	char btcusd[64];
	char btcusd_lower[64];
	char btcusd_point[64];
	int btcusd_switch;

	int eth_update;
	char eth[64];
	char eth_lower[64];
	char eth_point[64];
	int eth_switch;
}s_s;


GtkTreeStore *store;
ca_context *ca;

int overflow_btc = 0;
int overflow_eth = 0;

pthread_t thread_read;
int current = 0;
struct active {
	char symbol[255];
	char kline[255];
}act;

int id = 1;
char unsubscribe[BUF_SEND_SIGNAL];
char subscribe[BUF_SEND_SIGNAL];
int btc_update_mutex = 0;
char btc_update[64];
int eth_update_mutex = 0;
char eth_update[64];

static void sig_handler ( int sig ) {
	ws_set_data ( &ws, unsubscribe );
	ws_write ( &ws );
}

int id_line = 0;

static gboolean update_cb ( gpointer data ) {
	if ( btc_update_mutex ) {
		gtk_label_set_markup ( ( GtkLabel * ) label_btc_update, btc_update );
		btc_update_mutex = 0;
	}
	return FALSE;
}

static gboolean update_eth_cb ( gpointer data ) {
	if ( eth_update_mutex ) {
		gtk_label_set_markup ( ( GtkLabel * ) label_eth_update, eth_update );
		eth_update_mutex = 0;
	}
	return FALSE;
}

static void parse_line ( char *buffer ) {
	int length = strlen ( buffer );
	int in = 0, out = 0;
	for ( int i = 0; i < length; i++ ) {
		if ( buffer[i] == '{' ) in++;
		if ( buffer[i] == '}' ) out++;
	}
	if ( out < in ) {
		buffer[length] = '}';
		buffer[length + 1] = 0;
	}

	char *s = strstr ( buffer, "{" );
	json_object *root = json_tokener_parse ( s );
	json_object *result;
	json_object_object_get_ex ( root, "id", &result );
	if ( result ) {
		json_object_put ( root );
		return;
	}
	json_object *obj_time;
	json_object *obj_symbol;
	json_object *obj_data;

	json_object_object_get_ex ( root, "s", &obj_symbol );
	json_object_object_get_ex ( root, "E", &obj_time );
	json_object_object_get_ex ( root, "k", &obj_data );

	json_object *obj_start_time;
	json_object *obj_close_time;
	json_object *obj_symbol_trade;
	json_object *obj_interval;
	json_object *obj_first_trade_id;
	json_object *obj_last_trade_id;
	json_object *obj_open_price;
	json_object *obj_close_price;
	json_object *obj_high_price;
	json_object *obj_low_price;
	json_object *obj_base_asset_volume;
	json_object *obj_number_of_trade;
	json_object *obj_close;
	json_object *obj_quote_asset_volume;
	json_object *obj_taker_buy_base;
	json_object *obj_taker_buy_quote;

	json_object_object_get_ex ( obj_data, "t", &obj_start_time );
	json_object_object_get_ex ( obj_data, "T", &obj_close_time );
	json_object_object_get_ex ( obj_data, "s", &obj_symbol_trade );
	json_object_object_get_ex ( obj_data, "i", &obj_interval );
	json_object_object_get_ex ( obj_data, "f", &obj_first_trade_id );
	json_object_object_get_ex ( obj_data, "L", &obj_last_trade_id );
	json_object_object_get_ex ( obj_data, "o", &obj_open_price );
	json_object_object_get_ex ( obj_data, "c", &obj_close_price );
	json_object_object_get_ex ( obj_data, "h", &obj_high_price );
	json_object_object_get_ex ( obj_data, "l", &obj_low_price );
	json_object_object_get_ex ( obj_data, "v", &obj_base_asset_volume );
	json_object_object_get_ex ( obj_data, "n", &obj_number_of_trade );
	json_object_object_get_ex ( obj_data, "x", &obj_close );
	json_object_object_get_ex ( obj_data, "q", &obj_quote_asset_volume );
	json_object_object_get_ex ( obj_data, "V", &obj_taker_buy_base );
	json_object_object_get_ex ( obj_data, "Q", &obj_taker_buy_quote );

	double price_item = atof ( json_object_get_string ( obj_open_price ) );
	double res = 0;
	double res_lower = 0;

	int type_of_coin = -1;
	if ( !strncmp ( json_object_get_string ( obj_symbol_trade ), "BTCUSDT", 8 ) ) type_of_coin = TYPE_OF_COIN_BTC;
	else if ( !strncmp ( json_object_get_string ( obj_symbol_trade ), "ETHUSDT", 8 ) ) type_of_coin = TYPE_OF_COIN_ETH;

	switch ( type_of_coin ) {
		case TYPE_OF_COIN_BTC:
			if ( price_btc_lower < price_item ) {
				res = ( ( price_item - price_btc_lower ) / price_btc_lower ) * 100;
			} else {
				res = 0;
			}

			if ( price_btc_lower > price_item ) {
				res_lower = ( ( price_btc_lower - price_item ) / price_item ) * 100;
			} 
			if ( res_lower > btcusd_percent_point && s_s.btcusd_switch ) overflow_btc = 1;
			break;
		case TYPE_OF_COIN_ETH:
			if ( price_eth_lower < price_item ) {
				res = ( ( price_item - price_eth_lower ) / price_eth_lower ) * 100;
			} else {
				res = 0;
			}

			if ( price_eth_lower > price_item ) {
				res_lower = ( ( price_eth_lower - price_item ) / price_item ) * 100;
			} 
			if ( res_lower > eth_percent_point && s_s.eth_switch ) overflow_eth = 1;
			break;
	}




	//printf ( "%f %f %f %f\n", res_lower, btcusd_percent_point, price_btc_lower, price_item );

	GtkTreeIter iter;
	time_t time_time = atoi ( json_object_get_string ( obj_time ) );
	time_t time_start = atoi ( json_object_get_string ( obj_start_time ) );
	time_t time_close = atoi ( json_object_get_string ( obj_close_time ) );

	struct tm *tt = gmtime ( &time_time );
	struct tm *ts = gmtime ( &time_start );
	struct tm *tc = gmtime ( &time_close );

	char tt_time[32];
	char ts_time[32];
	char tc_time[32];
	snprintf ( tt_time, 32, "%02d/%02d %02d:%02d", tt->tm_mday, tt->tm_mon, tt->tm_hour, tt->tm_min );
	snprintf ( ts_time, 32, "%02d/%02d %02d:%02d", ts->tm_mday, ts->tm_mon, ts->tm_hour, ts->tm_min );
	snprintf ( tc_time, 32, "%02d/%02d %02d:%02d", tc->tm_mday, tc->tm_mon, tc->tm_hour, tc->tm_min );

	switch ( type_of_coin ) {
		case TYPE_OF_COIN_BTC:
			{
				const char *curs = gtk_label_get_text ( ( GtkLabel * ) label_btc_update );
				if ( curs[0] == 0 && !btc_update_mutex ) {
					snprintf ( btc_update, 64, "%.0f", price_item );
					btc_update_mutex = 1;
					g_idle_add ( update_cb, NULL );
				} else if ( curs[0] >= '0' && curs[0] <= '9' && !btc_update_mutex ) {
					double cu = atof ( curs );
					if ( cu > price_item ) {
						snprintf ( btc_update, 64, "<span foreground='red'>%.0f</span>", price_item );
					} else {
						snprintf ( btc_update, 64, "<span foreground='green'>%.0f</span>", price_item );
					}
					btc_update_mutex = 1;
					g_idle_add ( update_cb, NULL );
				}
				if ( !s_s.btc_update ) break;
				if ( price_item >= price_btc_lower && overflow_btc ) {
					ca_context_play ( ca, 1, CA_PROP_EVENT_ID, "desktop-login", NULL );
					overflow_btc = 0;
					g_notification_set_body ( notify, "Точка входа BTC" );
					g_application_send_notification ( ( GApplication * ) app, "com.xverizex.binance", notify );
					gtk_tree_store_append ( store, &iter, NULL );
					gtk_tree_store_set ( store, &iter,
						ID, id_line++,
						NOTIFY, "ТОЧКА ВХОДА",
						TIME, tt_time,
						SYMBOL, json_object_get_string ( obj_symbol ),
						START, ts_time,
						CLOSE, tc_time,
						SYMBOL_TRADE, json_object_get_string ( obj_symbol_trade ),
						INTERVAL, json_object_get_string ( obj_interval ),
						FIRST_TRADE_ID, json_object_get_string ( obj_first_trade_id ),
						LAST_TRADE_ID, json_object_get_string ( obj_last_trade_id ),
						OPEN_PRICE, json_object_get_string ( obj_open_price ),
						CLOSE_PRICE, json_object_get_string ( obj_close_price ),
						HIGH_PRICE, json_object_get_string ( obj_high_price ),
						LOW_PRICE, json_object_get_string ( obj_low_price ),
						BASE_ASSET_VOLUME, json_object_get_string ( obj_base_asset_volume ),
						NUMBER_OF_TRADER, json_object_get_string ( obj_number_of_trade ),
						IS_KLINE_CLOSED, json_object_get_string ( obj_close ),
						QUOTE_ASSET_VOLUME, json_object_get_string ( obj_quote_asset_volume ),
						TAKER_BASE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_base ),
						TAKER_QUOTE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_quote ),
						-1
					);
				} else
				if ( res >= price_btc_upper_percent && price_item > price_btc_lower ) {
					ca_context_play ( ca, 1, CA_PROP_EVENT_ID, "desktop-login", NULL );
					g_notification_set_body ( notify, "Подъем курса BTC" );
					g_application_send_notification ( ( GApplication * ) app, "com.xverizex.binance", notify );
					gtk_tree_store_append ( store, &iter, NULL );
					gtk_tree_store_set ( store, &iter,
						ID, id_line++,
						NOTIFY, "ПОДЪЕМ КУРСА",
						TIME, tt_time,
						SYMBOL, json_object_get_string ( obj_symbol ),
						START, ts_time,
						CLOSE, tc_time,
						SYMBOL_TRADE, json_object_get_string ( obj_symbol_trade ),
						INTERVAL, json_object_get_string ( obj_interval ),
						FIRST_TRADE_ID, json_object_get_string ( obj_first_trade_id ),
						LAST_TRADE_ID, json_object_get_string ( obj_last_trade_id ),
						OPEN_PRICE, json_object_get_string ( obj_open_price ),
						CLOSE_PRICE, json_object_get_string ( obj_close_price ),
						HIGH_PRICE, json_object_get_string ( obj_high_price ),
						LOW_PRICE, json_object_get_string ( obj_low_price ),
						BASE_ASSET_VOLUME, json_object_get_string ( obj_base_asset_volume ),
						NUMBER_OF_TRADER, json_object_get_string ( obj_number_of_trade ),
						IS_KLINE_CLOSED, json_object_get_string ( obj_close ),
						QUOTE_ASSET_VOLUME, json_object_get_string ( obj_quote_asset_volume ),
						TAKER_BASE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_base ),
						TAKER_QUOTE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_quote ),
						-1
					);
				}
			}
			break;
		case TYPE_OF_COIN_ETH:
			{
				const char *curs = gtk_label_get_text ( ( GtkLabel * ) label_eth_update );
				if ( curs[0] == 0 && !eth_update_mutex ) {
					snprintf ( eth_update, 64, "%.0f", price_item );
					eth_update_mutex = 1;
					g_idle_add ( update_eth_cb, NULL );
				} else if ( curs[0] >= '0' && curs[0] <= '9' && !eth_update_mutex ) {
					double cu = atof ( curs );
					if ( cu > price_item ) {
						snprintf ( eth_update, 64, "<span foreground='red'>%.0f</span>", price_item );
					} else {
						snprintf ( eth_update, 64, "<span foreground='green'>%.0f</span>", price_item );
					}
					eth_update_mutex = 1;
					g_idle_add ( update_eth_cb, NULL );
				}
				if ( !s_s.eth_update ) break;
				if ( price_item >= price_eth_lower && overflow_eth ) {
					ca_context_play ( ca, 1, CA_PROP_EVENT_ID, "desktop-login", NULL );
					overflow_eth = 0;
					g_notification_set_body ( notify, "Точка входа ETH" );
					g_application_send_notification ( ( GApplication * ) app, "com.xverizex.binance", notify );
					gtk_tree_store_append ( store, &iter, NULL );
					gtk_tree_store_set ( store, &iter,
						ID, id_line++,
						NOTIFY, "ТОЧКА ВХОДА",
						TIME, tt_time,
						SYMBOL, json_object_get_string ( obj_symbol ),
						START, ts_time,
						CLOSE, tc_time,
						SYMBOL_TRADE, json_object_get_string ( obj_symbol_trade ),
						INTERVAL, json_object_get_string ( obj_interval ),
						FIRST_TRADE_ID, json_object_get_string ( obj_first_trade_id ),
						LAST_TRADE_ID, json_object_get_string ( obj_last_trade_id ),
						OPEN_PRICE, json_object_get_string ( obj_open_price ),
						CLOSE_PRICE, json_object_get_string ( obj_close_price ),
						HIGH_PRICE, json_object_get_string ( obj_high_price ),
						LOW_PRICE, json_object_get_string ( obj_low_price ),
						BASE_ASSET_VOLUME, json_object_get_string ( obj_base_asset_volume ),
						NUMBER_OF_TRADER, json_object_get_string ( obj_number_of_trade ),
						IS_KLINE_CLOSED, json_object_get_string ( obj_close ),
						QUOTE_ASSET_VOLUME, json_object_get_string ( obj_quote_asset_volume ),
						TAKER_BASE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_base ),
						TAKER_QUOTE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_quote ),
						-1
					);
				} else
				if ( res >= price_eth_upper_percent && price_item > price_eth_lower ) {
					ca_context_play ( ca, 1, CA_PROP_EVENT_ID, "desktop-login", NULL );
					g_notification_set_body ( notify, "Подъем курса ETH" );
					g_application_send_notification ( ( GApplication * ) app, "com.xverizex.binance", notify );
					gtk_tree_store_append ( store, &iter, NULL );
					gtk_tree_store_set ( store, &iter,
						ID, id_line++,
						NOTIFY, "ПОДЪЕМ КУРСА",
						TIME, tt_time,
						SYMBOL, json_object_get_string ( obj_symbol ),
						START, ts_time,
						CLOSE, tc_time,
						SYMBOL_TRADE, json_object_get_string ( obj_symbol_trade ),
						INTERVAL, json_object_get_string ( obj_interval ),
						FIRST_TRADE_ID, json_object_get_string ( obj_first_trade_id ),
						LAST_TRADE_ID, json_object_get_string ( obj_last_trade_id ),
						OPEN_PRICE, json_object_get_string ( obj_open_price ),
						CLOSE_PRICE, json_object_get_string ( obj_close_price ),
						HIGH_PRICE, json_object_get_string ( obj_high_price ),
						LOW_PRICE, json_object_get_string ( obj_low_price ),
						BASE_ASSET_VOLUME, json_object_get_string ( obj_base_asset_volume ),
						NUMBER_OF_TRADER, json_object_get_string ( obj_number_of_trade ),
						IS_KLINE_CLOSED, json_object_get_string ( obj_close ),
						QUOTE_ASSET_VOLUME, json_object_get_string ( obj_quote_asset_volume ),
						TAKER_BASE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_base ),
						TAKER_QUOTE_ASSET_VOLUME, json_object_get_string ( obj_taker_buy_quote ),
						-1
					);
				}
			}
			break;
	}
//	printf ( "%f %f:%f %f\n", price_btc_upper_percent, res, price_btc_lower, price_item );


#if 0
	json_object_put ( obj_start_time );
	json_object_put ( obj_close_time );
	json_object_put ( obj_symbol_trade );
	json_object_put ( obj_interval );
	json_object_put ( obj_first_trade_id );
	json_object_put ( obj_last_trade_id );
	json_object_put ( obj_open_price );
	json_object_put ( obj_close_price );
	json_object_put ( obj_high_price );
	json_object_put ( obj_low_price );
	json_object_put ( obj_base_asset_volume );
	json_object_put ( obj_number_of_trade );
	json_object_put ( obj_close );
	json_object_put ( obj_quote_asset_volume );
	json_object_put ( obj_taker_buy_base );
	json_object_put ( obj_taker_buy_quote );

	json_object_put ( obj_time );
	json_object_put ( obj_symbol );
	json_object_put ( obj_data );
#endif
	json_object_put ( root );

}

void *read_data_cb ( void *data ) {
	char *buffer = calloc ( BUF_SIZE_DATA, 1 );
	while ( 1 ) {
		memset ( buffer, 0, BUF_SIZE_DATA );
		int ret = ws_read ( &ws, buffer, BUF_SIZE_DATA );
		if ( ret <= 0 ) {
			fprintf ( stderr, "ret: %d\n", ret );
			exit ( EXIT_FAILURE );
		}

		parse_line ( buffer );
	}
}


static void init_strings ( ) {
	int n;
	int total;
	char *s;

	const char *sym[] = {
		"btcusdt",
		"ethusdt"
	};
#define TOTAL_SYM                   2

	for ( int i = 0; i < 2; i++ ) {
		switch ( i ) {
			case 0:
				{
					s = &unsubscribe[0];
					snprintf ( s, BUF_SEND_SIGNAL,
							"{ \"method\": \"UNSUBSCRIBE\","
							"\"params\": [%n"
							,
							&n
		 					);
					s += n;
					total = BUF_SEND_SIGNAL - n;
				}
				break;
			case 1:
				{
					s = &subscribe[0];
					snprintf ( s, BUF_SEND_SIGNAL,
							"{ \"method\": \"SUBSCRIBE\","
							"\"params\": [%n"
							,
							&n
		 					);
					s += n;
					total = BUF_SEND_SIGNAL - n;
				}
				break;
			default:
				break;
		}

		for ( int i = 0; i < TOTAL_SYM; i++ ) {
			snprintf ( s, total,
				"\"%s@kline_1m\","
				"\"%s@kline_3m\","
				"\"%s@kline_5m\","
				"\"%s@kline_15m\","
				"\"%s@kline_30m\","
				"\"%s@kline_1h\","
				"\"%s@kline_2h\","
				"\"%s@kline_4h\","
				"\"%s@kline_6h\","
				"\"%s@kline_8h\","
				"\"%s@kline_12h\","
				"\"%s@kline_1d\","
				"\"%s@kline_3d\","
				"\"%s@kline_1w\","
				"\"%s@kline_1M\"%s%n",
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				sym[i],
				i + 1 == TOTAL_SYM ? "" : ",",
				&n
			 	);
			s += n;
			total -= n;
		}

		snprintf ( s, total,
				"],"
				"\"id\": %d"
				"}",
				id++
			 );
	}
}

static gboolean window_delete_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	gtk_widget_hide ( widget );
	return TRUE;
}

static void button_clear_clicked_cb ( GtkButton *button, gpointer data ) {
	id_line = 0;
	gtk_tree_store_clear ( store );
}

static void action_connect_cb ( GSimpleAction *action, GVariant *parameter, gpointer data ) {
	gtk_tree_store_clear ( store );

	if ( thread_read > 0 ) {
		ws_set_data ( &ws, unsubscribe );
		ws_write ( &ws );
		pthread_cancel ( thread_read );
		ws_close ( &ws );
		id_line = 0;
	}
	ws = ws_connect_to ( "stream.binance.com", WEBSOCKET_WSS, "ws", "10.10.10.10" );
	if ( ws.socket > 0 ) {
		g_notification_set_body ( notify, "Успешное подключение" );
		g_application_send_notification ( ( GApplication * ) app, "com.xverizex.binance", notify );
	}

	pthread_create ( &thread_read, NULL, read_data_cb, NULL );

	ws_set_data ( &ws, subscribe );
	ws_write ( &ws );
}

const char *style = "box#box_select { background-color: #4c4c4c; } label#label_symbol { color: #e1ff5a; } label#label_kline { color: #e1ff5a; } frame#frame_top { background-color: #3c3c3c; border-radius: 6px; } button#button_accept { border-radius: 6px; } button#button_clear { border-radius: 6px; } frame#group { } box#box_item { background-color: #cccccc; } label#curs_up { color: #3aff9d; } label#curs_down { color: #ff1317; } button#button_left { border-top-left-radius: 10px; border-bottom-left-radius: 10px; } button#button_right { border-top-right-radius: 10px; border-bottom-right-radius: 10px; }";

static void get_tree_store ( GtkWidget *tree_view ) {
	store = gtk_tree_store_new ( N_COUNT,
			G_TYPE_INT,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_STRING
			);

	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	const char *cells[] = {
		"id",
		"оповещение",
		"время",
		"тип",
		"начало",
		"конец",
		"валютная пара",
		"интервал",
		"первый id сделки",
		"последний id сделки",
		"open price",
		"close price",
		"high_price",
		"low_price",
		"base_volume",
		"number trade",
		"closed",
		"quote volume",
		"taker buy base",
		"taker buy quote"
	};


	for ( int i = 0; i < N_COUNT; i++ ) {
		renderer = gtk_cell_renderer_text_new ( );
		column = gtk_tree_view_column_new_with_attributes ( cells[i], renderer, "text", i, NULL );
		gtk_tree_view_column_set_resizable ( ( GtkTreeViewColumn * ) column, TRUE );
		gtk_tree_view_append_column ( ( GtkTreeView * ) tree_view, column );
	}

	gtk_tree_view_set_model ( ( GtkTreeView * ) tree_view, ( GtkTreeModel * ) store );
}

static void read_config_file ( ) {
	FILE *fp = fopen ( config_file, "r" );
	fread ( &s_s, sizeof ( struct settings_symbol ), 1, fp );
	fclose ( fp );
}

static void write_config_file ( ) {
	FILE *fp = fopen ( config_file, "w" );
	fwrite ( &s_s, sizeof ( struct settings_symbol ), 1, fp );
	fclose ( fp );
}

GtkWidget *entry_btcusd;
GtkWidget *entry_btcusd_lower;
GtkWidget *entry_btcusd_percent_point;
GtkWidget *switch_btcusd_point;
GtkWidget *entry_eth_point;
GtkWidget *switch_eth_point;
GtkWidget *entry_eth_up;
GtkWidget *check_eth_lower;
GtkWidget *check_btcusd_lower;
GtkWidget *entry_eth_lower;

static void action_settings_cb ( GSimpleAction *action, GVariant *parameter, gpointer data ) {
	read_config_file ( );
	gtk_entry_set_text ( ( GtkEntry * ) entry_btcusd, s_s.btcusd );
	gtk_entry_set_text ( ( GtkEntry * ) entry_btcusd_lower, s_s.btcusd_lower );
	gtk_entry_set_text ( ( GtkEntry * ) entry_btcusd_percent_point, s_s.btcusd_point );
	gtk_switch_set_active ( ( GtkSwitch * ) switch_btcusd_point, s_s.btcusd_switch );

	gtk_toggle_button_set_active ( ( GtkToggleButton * ) check_btcusd_lower, s_s.btc_update );
	gtk_toggle_button_set_active ( ( GtkToggleButton * ) check_eth_lower, s_s.eth_update );

	gtk_entry_set_text ( ( GtkEntry * ) entry_eth_up, s_s.eth );
	gtk_entry_set_text ( ( GtkEntry * ) entry_eth_lower, s_s.eth_lower );
	gtk_entry_set_text ( ( GtkEntry * ) entry_eth_point, s_s.eth_point );
	gtk_switch_set_active ( ( GtkSwitch * ) switch_eth_point, s_s.eth_switch );

	gtk_widget_show_all ( window_settings );
}

static void action_quit_cb ( GSimpleAction *action, GVariant *parameter, gpointer data ) {
	g_application_quit ( ( GApplication * ) app );
	exit ( EXIT_SUCCESS );
}

const GActionEntry entries[] = {
	{ "action_settings", action_settings_cb, NULL, NULL, NULL },
	{ "action_connect", action_connect_cb, NULL, NULL, NULL },
	{ "action_quit", action_quit_cb, NULL, NULL, NULL }
};

static void create_actions ( GtkWidget *window ) {
	g_action_map_add_action_entries ( ( GActionMap * ) window, entries, G_N_ELEMENTS ( entries ), NULL );
}

static void button_save_clicked_cb ( GtkButton *btn, gpointer data ) {
	const char *btcusd_str = gtk_entry_get_text ( ( GtkEntry * ) entry_btcusd );
	const char *btcusd_lower_str = gtk_entry_get_text ( ( GtkEntry * ) entry_btcusd_lower );
	const char *btcusd_point_str = gtk_entry_get_text ( ( GtkEntry * ) entry_btcusd_percent_point );
	const char *eth_str = gtk_entry_get_text ( ( GtkEntry * ) entry_eth_up );
	const char *eth_lower_str = gtk_entry_get_text ( ( GtkEntry * ) entry_eth_lower );
	const char *eth_point_str = gtk_entry_get_text ( ( GtkEntry * ) entry_eth_point );
	s_s.btcusd_switch = gtk_switch_get_active ( ( GtkSwitch * ) switch_btcusd_point );
	s_s.eth_switch = gtk_switch_get_active ( ( GtkSwitch * ) switch_eth_point );

	strncpy ( &s_s.btcusd[0], btcusd_str, strlen ( btcusd_str ) + 1 );
	strncpy ( &s_s.btcusd_lower[0], btcusd_lower_str, strlen ( btcusd_lower_str ) + 1 );
	strncpy ( &s_s.btcusd_point[0], btcusd_point_str, strlen ( btcusd_point_str ) + 1 );

	strncpy ( &s_s.eth[0], eth_str, strlen ( eth_str ) + 1 );
	strncpy ( &s_s.eth_lower[0], eth_lower_str, strlen ( eth_lower_str ) + 1 );
	strncpy ( &s_s.eth_point[0], eth_point_str, strlen ( eth_point_str ) + 1 );

	price_btc_upper_percent = atof ( s_s.btcusd );
	price_btc_lower = atof ( s_s.btcusd_lower );
	btcusd_percent_point = atof ( s_s.btcusd_point );

	s_s.btc_update = gtk_toggle_button_get_active ( ( GtkToggleButton * ) check_btcusd_lower );
	s_s.eth_update = gtk_toggle_button_get_active ( ( GtkToggleButton * ) check_eth_lower );

	write_config_file ( );
}

GdkRGBA rgb_item;

static gboolean window_settings_delete_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	gtk_widget_hide ( widget );
	return TRUE;
}

static void init_window_settings ( GtkWidget *window ) {
	window_settings = gtk_application_window_new ( app );
	g_signal_connect ( window_settings, "delete-event", G_CALLBACK ( window_settings_delete_event_cb ), NULL );

	gtk_window_set_default_size ( ( GtkWindow * ) window_settings, 800, 600 );

	gtk_window_set_modal ( ( GtkWindow * ) window_settings, TRUE );
	gtk_window_set_transient_for ( ( GtkWindow * ) window_settings, ( GtkWindow * ) window );
	gtk_application_window_set_show_menubar ( ( GtkApplicationWindow * ) window_settings, FALSE );

	GtkWidget *header_bar = gtk_header_bar_new ( );
	gtk_header_bar_set_title ( ( GtkHeaderBar * ) header_bar, "Настройки" );
	gtk_header_bar_set_show_close_button ( ( GtkHeaderBar * ) header_bar, TRUE );
	gtk_window_set_titlebar ( ( GtkWindow * ) window_settings, header_bar );

	GtkWidget *label_group_upper = gtk_label_new ( "Подъем в цене в процентах" );
	GtkWidget *label_btcusd = gtk_label_new ( "BTC" );
	entry_btcusd = gtk_entry_new ( );
	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_btcusd, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_btcusd, 1 );

	GtkWidget *label_group_lower = gtk_label_new ( "Нижний порог" );
	check_btcusd_lower = gtk_check_button_new_with_label ( "BTC" );
	entry_btcusd_lower = gtk_entry_new ( );
	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_btcusd_lower, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_btcusd_lower, 1 );

	GtkWidget *button_save = gtk_button_new_with_label ( "Сохранить" );

	gtk_widget_set_margin_start ( label_group_upper, 0 );
	gtk_widget_set_margin_end ( label_group_upper, 10 );
	gtk_widget_set_margin_top ( label_group_upper, 10 );
	gtk_widget_set_margin_bottom ( label_group_upper, 10 );

	gtk_widget_set_margin_start ( label_btcusd, 10 );
	gtk_widget_set_margin_end ( label_btcusd, 10 );
	gtk_widget_set_margin_top ( label_btcusd, 10 );
	gtk_widget_set_margin_bottom ( label_btcusd, 10 );

	gtk_widget_set_margin_start ( check_btcusd_lower, 10 );
	gtk_widget_set_margin_end ( check_btcusd_lower, 10 );
	gtk_widget_set_margin_top ( check_btcusd_lower, 10 );
	gtk_widget_set_margin_bottom ( check_btcusd_lower, 10 );

	gtk_widget_set_margin_start ( entry_btcusd_lower, 10 );
	gtk_widget_set_margin_end ( entry_btcusd_lower, 10 );
	gtk_widget_set_margin_top ( entry_btcusd_lower, 10 );
	gtk_widget_set_margin_bottom ( entry_btcusd_lower, 10 );

	gtk_widget_set_margin_start ( entry_btcusd, 10 );
	gtk_widget_set_margin_end ( entry_btcusd, 10 );
	gtk_widget_set_margin_top ( entry_btcusd, 10 );
	gtk_widget_set_margin_bottom ( entry_btcusd, 10 );

	gtk_widget_set_margin_start ( button_save, 10 );
	gtk_widget_set_margin_end ( button_save, 10 );
	gtk_widget_set_margin_top ( button_save, 10 );
	gtk_widget_set_margin_bottom ( button_save, 10 );

	GtkWidget *frame_upper = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, "name", "group", NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) frame_upper, GTK_SHADOW_IN );
	
	GtkWidget *upper_group_box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 1 );
	gtk_widget_set_margin_start ( frame_upper, 64 );
	gtk_widget_set_margin_end ( frame_upper, 64 );
	gtk_widget_set_margin_top ( frame_upper, 0 );
	gtk_widget_set_margin_bottom ( frame_upper, 0 );

	GtkWidget *box_lower = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 1 );
	GtkWidget *box_group_lower = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 1 );
	GtkWidget *box_btcusd_lower = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_name ( box_btcusd_lower, "box_item" );
	GtkWidget *box_label_lower = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_label_lower, label_group_lower, FALSE, FALSE, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_btcusd_lower, check_btcusd_lower, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_btcusd_lower, entry_btcusd_lower, FALSE, FALSE, 0 );

	check_eth_lower = gtk_check_button_new_with_label ( "ETH" );
	entry_eth_lower = gtk_entry_new ( );
	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_eth_lower, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_eth_lower, 1 );
	gtk_widget_set_margin_start ( check_eth_lower, 10 );
	gtk_widget_set_margin_end ( check_eth_lower, 10 );
	gtk_widget_set_margin_top ( check_eth_lower, 10 );
	gtk_widget_set_margin_bottom ( check_eth_lower, 10 );
	gtk_widget_set_margin_start ( entry_eth_lower, 10 );
	gtk_widget_set_margin_end ( entry_eth_lower, 10 );
	gtk_widget_set_margin_top ( entry_eth_lower, 10 );
	gtk_widget_set_margin_bottom ( entry_eth_lower, 10 );
	GtkWidget *box_eth_lower = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_eth_lower, check_eth_lower, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_eth_lower, entry_eth_lower, FALSE, FALSE, 0 );
	gtk_widget_set_name ( box_eth_lower, "box_item" );

	gtk_box_pack_start ( ( GtkBox * ) box_lower, box_label_lower, FALSE, FALSE, 0 );

	GtkWidget *box_btcusd = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_name ( box_btcusd, "box_item" );
	gtk_box_pack_start ( ( GtkBox * ) box_btcusd, label_btcusd, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_btcusd, entry_btcusd, FALSE, FALSE, 0 );

	GtkWidget *box_button_save = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_button_save, button_save, FALSE, FALSE, 0 );

	GtkWidget *box_main = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_box_pack_start ( ( GtkBox * ) upper_group_box, box_btcusd, FALSE, FALSE, 0 );
	gtk_container_add ( ( GtkContainer * ) frame_upper, upper_group_box );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) frame_upper, GTK_SHADOW_IN );

	GtkWidget *label_eth_up = gtk_label_new ( "ETH" );
	entry_eth_up = gtk_entry_new ( );
	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_eth_up, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_eth_up, 1 );
	gtk_widget_set_margin_start ( label_eth_up, 10 );
	gtk_widget_set_margin_end ( label_eth_up, 10 );
	gtk_widget_set_margin_top ( label_eth_up, 10 );
	gtk_widget_set_margin_bottom ( label_eth_up, 10 );
	gtk_widget_set_margin_start ( entry_eth_up, 10 );
	gtk_widget_set_margin_end ( entry_eth_up, 10 );
	gtk_widget_set_margin_top ( entry_eth_up, 10 );
	gtk_widget_set_margin_bottom ( entry_eth_up, 10 );

	GtkWidget *box_eth_up = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 1 );
	gtk_widget_set_name ( box_eth_up, "box_item" );
	gtk_box_pack_start ( ( GtkBox * ) box_eth_up, label_eth_up, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_eth_up, entry_eth_up, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) upper_group_box, box_eth_up, FALSE, FALSE, 0 );

	GtkWidget *frame_lower = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, "name", "group", NULL );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) frame_lower, GTK_SHADOW_IN );
	gtk_widget_set_margin_start ( frame_lower, 64 );
	gtk_widget_set_margin_end ( frame_lower, 64 );
	gtk_widget_set_margin_top ( frame_lower, 10 );
	gtk_widget_set_margin_bottom ( frame_lower, 0 );
	gtk_frame_set_shadow_type ( ( GtkFrame * ) frame_lower, GTK_SHADOW_IN );
	gtk_box_pack_start ( ( GtkBox * ) box_group_lower, box_btcusd_lower, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_group_lower, box_eth_lower, FALSE, FALSE, 0 );
	gtk_container_add ( ( GtkContainer * ) frame_lower, box_group_lower );

	GtkWidget *box_label_group_upper = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_margin_start ( box_label_group_upper, 64 );
	gtk_widget_set_margin_end ( box_label_group_upper, 64 );
	gtk_widget_set_margin_top ( box_label_group_upper, 64 );
	gtk_widget_set_margin_bottom ( box_label_group_upper, 0 );

	gtk_widget_set_margin_start ( box_label_lower, 64 );
	gtk_widget_set_margin_end ( box_label_lower, 64 );
	gtk_widget_set_margin_top ( box_label_lower, 10 );
	gtk_widget_set_margin_bottom ( box_label_lower, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_label_group_upper, label_group_upper, FALSE, FALSE, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_main, box_label_group_upper, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_main, frame_upper, FALSE, FALSE, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_main, box_lower, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_main, frame_lower, FALSE, FALSE, 0 );


	GtkWidget *label_point = gtk_label_new ( "Точки входа" );
	GtkWidget *label_btcusd_point_item = gtk_label_new ( "Процент включения BTC" );
	entry_btcusd_percent_point = gtk_entry_new ( );
	switch_btcusd_point = gtk_switch_new ( );
	
	GtkWidget *box_points = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 1 );
	GtkWidget *box_label_point = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *box_btcusd_point_item = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *frame_point = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, "name", "group", NULL );

	gtk_widget_set_margin_bottom ( label_point, 10 );
	gtk_widget_set_margin_top ( label_point, 10 );
	gtk_box_pack_start ( ( GtkBox * ) box_label_point, label_point, FALSE, FALSE, 0 );
	gtk_widget_set_margin_start ( box_label_point, 64 );
	gtk_widget_set_margin_end ( box_label_point, 64 );
	gtk_widget_set_margin_bottom ( label_point, 0 );
	gtk_widget_set_margin_top ( label_point, 10 );

	gtk_widget_set_margin_start ( label_btcusd_point_item, 10 );
	gtk_widget_set_margin_top ( label_btcusd_point_item, 10 );
	gtk_widget_set_margin_end ( label_btcusd_point_item, 10 );
	gtk_widget_set_margin_bottom ( label_btcusd_point_item, 10 );
	gtk_box_pack_start ( ( GtkBox * ) box_btcusd_point_item, label_btcusd_point_item, FALSE, FALSE, 0 );
	gtk_widget_set_name ( box_btcusd_point_item, "box_item" );

	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_btcusd_percent_point, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_btcusd_percent_point, 1 );
	gtk_widget_set_margin_start ( entry_btcusd_percent_point, 10 );
	gtk_widget_set_margin_top ( entry_btcusd_percent_point, 10 );
	gtk_widget_set_margin_end ( entry_btcusd_percent_point, 10 );
	gtk_widget_set_margin_bottom ( entry_btcusd_percent_point, 10 );
	gtk_box_pack_end ( ( GtkBox * ) box_btcusd_point_item, entry_btcusd_percent_point, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_btcusd_point_item, switch_btcusd_point, FALSE, FALSE, 0 );

	gtk_widget_set_margin_start ( switch_btcusd_point, 10 );
	gtk_widget_set_margin_top ( switch_btcusd_point, 10 );
	gtk_widget_set_margin_end ( switch_btcusd_point, 10 );
	gtk_widget_set_margin_bottom ( switch_btcusd_point, 10 );

	gtk_box_pack_start ( ( GtkBox * ) box_points, box_btcusd_point_item, FALSE, FALSE, 0 );

	gtk_widget_set_margin_start ( frame_point, 64 );
	gtk_widget_set_margin_top ( frame_point, 10 );
	gtk_widget_set_margin_end ( frame_point, 64 );
	gtk_widget_set_margin_bottom ( frame_point, 10 );

	GtkWidget *label_eth_point = gtk_label_new ( "Процент включения ETH" );
	entry_eth_point = gtk_entry_new ( );
	switch_eth_point = gtk_switch_new ( );
	gtk_entry_set_input_purpose ( ( GtkEntry * ) entry_eth_point, GTK_INPUT_PURPOSE_DIGITS );
	gtk_entry_set_alignment ( ( GtkEntry * ) entry_eth_point, 1 );
	GtkWidget *box_eth_point = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	gtk_widget_set_name ( box_eth_point, "box_item" );
	gtk_widget_set_margin_start ( label_eth_point, 10 );
	gtk_widget_set_margin_end ( label_eth_point, 10 );
	gtk_widget_set_margin_top ( label_eth_point, 10 );
	gtk_widget_set_margin_bottom ( label_eth_point, 10 );
	gtk_widget_set_margin_start ( entry_eth_point, 10 );
	gtk_widget_set_margin_end ( entry_eth_point, 10 );
	gtk_widget_set_margin_top ( entry_eth_point, 10 );
	gtk_widget_set_margin_bottom ( entry_eth_point, 10 );
	gtk_widget_set_margin_start ( switch_eth_point, 10 );
	gtk_widget_set_margin_end ( switch_eth_point, 10 );
	gtk_widget_set_margin_top ( switch_eth_point, 10 );
	gtk_widget_set_margin_bottom ( switch_eth_point, 10 );

	gtk_box_pack_start ( ( GtkBox * ) box_eth_point, label_eth_point, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_eth_point, entry_eth_point, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_eth_point, switch_eth_point, FALSE, FALSE, 0 );

	gtk_box_pack_start ( ( GtkBox * ) box_points, box_eth_point, FALSE, FALSE, 0 );

	gtk_container_add ( ( GtkContainer * ) frame_point, box_points );

	gtk_box_pack_start ( ( GtkBox * ) box_main, box_label_point, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_main, frame_point, FALSE, FALSE, 0 );

	GtkWidget *latest_box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	GtkWidget *scroll = gtk_scrolled_window_new ( NULL, NULL );
	gtk_container_add ( ( GtkContainer * ) scroll, box_main );
	gtk_box_pack_start ( ( GtkBox * ) latest_box, scroll, TRUE, TRUE, 0 );

	gtk_box_pack_end ( ( GtkBox * ) latest_box, box_button_save, FALSE, FALSE, 0 );

	g_signal_connect ( button_save, "clicked", G_CALLBACK ( button_save_clicked_cb ), NULL );

	gtk_container_add ( ( GtkContainer * ) window_settings, latest_box );
}

static void ai_show_window ( GtkMenuItem *item, gpointer data ) {
	gtk_widget_show_all ( window );
}

static void ai_exit_program ( GtkMenuItem *item, gpointer data ) {
	g_application_quit ( ( GApplication * ) app );
	exit ( EXIT_SUCCESS );
}

#define PAGE_MAIN             0
#define PAGE_STATISTICS       1
#define PAGE_GRAPHIC          2
int current_page = 0;
GtkWidget *main_box;
GtkWidget *statistics_box;
GtkWidget *graphic_box;

static void button_main_clicked_cb ( GtkButton *button, gpointer data ) {
	if ( current_page == PAGE_STATISTICS ) {
		g_object_ref ( statistics_box );
		gtk_container_remove ( ( GtkContainer * ) window, statistics_box );
		gtk_container_add ( ( GtkContainer * ) window, main_box );
	} else
	if ( current_page == PAGE_GRAPHIC ) {
		g_object_ref ( graphic_box );
		gtk_container_remove ( ( GtkContainer * ) window, graphic_box );
		gtk_container_add ( ( GtkContainer * ) window, main_box );
	}
	current_page = PAGE_MAIN;
	
}

static void button_statistics_clicked_cb ( GtkButton *button, gpointer data ) {
	if ( current_page == PAGE_MAIN ) {
		g_object_ref ( main_box );
		gtk_container_remove ( ( GtkContainer * ) window, main_box );
		gtk_container_add ( ( GtkContainer * ) window, statistics_box );
	} else
	if ( current_page == PAGE_GRAPHIC ) {
		g_object_ref ( graphic_box );
		gtk_container_remove ( ( GtkContainer * ) window, graphic_box );
		gtk_container_add ( ( GtkContainer * ) window, statistics_box );
	}
	current_page = PAGE_STATISTICS;
	
}

static void button_graphic_clicked_cb ( GtkButton *button, gpointer data ) {
	if ( current_page == PAGE_MAIN ) {
		g_object_ref ( main_box );
		gtk_container_remove ( ( GtkContainer * ) window, main_box );
		gtk_container_add ( ( GtkContainer * ) window, graphic_box );
	} else
	if ( current_page == PAGE_STATISTICS ) {
		g_object_ref ( statistics_box );
		gtk_container_remove ( ( GtkContainer * ) window, statistics_box );
		gtk_container_add ( ( GtkContainer * ) window, graphic_box );
	}
	
	current_page = PAGE_GRAPHIC;
	
}

static void g_startup_cb ( GtkApplication *app, gpointer data ) {
	
	GMainLoop *loop = g_main_loop_new ( NULL, FALSE );

	window = gtk_application_window_new ( app );


	GtkWidget *action_bar = gtk_action_bar_new ( );
	GtkWidget *label_action_btc = gtk_label_new ( "BTC" );
	GtkWidget *label_action_eth = gtk_label_new ( "ETH" );
	label_btc_update = gtk_label_new ( "" );
	label_eth_update = gtk_label_new ( "" );
	gtk_label_set_use_markup ( ( GtkLabel * ) label_btc_update, TRUE );
	gtk_label_set_use_markup ( ( GtkLabel * ) label_eth_update, TRUE );

	gtk_action_bar_pack_start ( ( GtkActionBar * ) action_bar, label_action_btc );
	gtk_action_bar_pack_start ( ( GtkActionBar * ) action_bar, label_btc_update );

	gtk_action_bar_pack_start ( ( GtkActionBar * ) action_bar, label_action_eth );
	gtk_action_bar_pack_start ( ( GtkActionBar * ) action_bar, label_eth_update );

	ca = ca_gtk_context_get ( );

	const char *paths[] = {
		DEFAULT_SHARE_PATH,
		"/usr/share/icons"
	};

	GtkIconTheme *theme_icon;
	theme_icon = gtk_icon_theme_get_default ( );
	gtk_icon_theme_set_search_path ( theme_icon, paths, 2 );

	AppIndicator *ai;
	GtkWidget *menu, *item;
	ai = app_indicator_new ( "com.xverizex.binance", "security-low", APP_INDICATOR_CATEGORY_APPLICATION_STATUS );
	menu = gtk_menu_new ( );
	item = gtk_menu_item_new_with_label ( "Показать окно" );
	gtk_menu_shell_append ( ( GtkMenuShell * ) menu, item );
	g_signal_connect ( item, "activate", G_CALLBACK ( ai_show_window ), NULL );

	item = gtk_menu_item_new_with_label ( "Выход" );
	gtk_menu_shell_append ( ( GtkMenuShell * ) menu, item );
	g_signal_connect ( item, "activate", G_CALLBACK ( ai_exit_program ), NULL );
	
	app_indicator_set_menu ( ai, ( GtkMenu * ) menu );
	app_indicator_set_status ( ai, APP_INDICATOR_STATUS_ACTIVE );
	gtk_widget_show_all ( menu );
	
	notify = g_notification_new ( "binance bot" );
	g_notification_set_priority ( notify, G_NOTIFICATION_PRIORITY_URGENT );

	create_actions ( window );
	init_window_settings ( window );

	GMenu *gmenu_menu = g_menu_new ( );
	g_menu_append ( gmenu_menu, "Параметры", "win.action_settings" );
	g_menu_append ( gmenu_menu, "Подключиться", "win.action_connect" );
	g_menu_append ( gmenu_menu, "Завершить программу", "win.action_quit" );

	gtk_application_set_app_menu ( app, ( GMenuModel * ) gmenu_menu );

	GtkWidget *header_bar = gtk_header_bar_new ( );
	gtk_header_bar_set_title ( ( GtkHeaderBar * ) header_bar, "Бинанс бот" );
	gtk_header_bar_set_show_close_button ( ( GtkHeaderBar * ) header_bar, TRUE );
	gtk_header_bar_set_decoration_layout ( ( GtkHeaderBar * ) header_bar, "menu:minimize,maximize,close" );

	gtk_window_set_titlebar ( ( GtkWindow * ) window, header_bar );

	GdkDisplay *display = gdk_display_get_default ( );
	GdkScreen *screen = gdk_display_get_default_screen ( display );
	GtkCssProvider *css_provider = gtk_css_provider_new ( );

	gtk_style_context_add_provider_for_screen ( screen, ( GtkStyleProvider * ) css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );
	gtk_css_provider_load_from_data ( ( GtkCssProvider * ) css_provider, style, strlen ( style ), NULL );

	GtkStyleContext *style_context = gtk_style_context_new ( );
	gtk_style_context_add_provider ( ( GtkStyleContext * ) style_context, ( GtkStyleProvider * ) css_provider, GTK_STYLE_PROVIDER_PRIORITY_USER );

	g_signal_connect ( window, "delete-event", G_CALLBACK ( window_delete_event_cb ), NULL );

	gtk_window_set_default_size ( ( GtkWindow * ) window, 1024, 600 );

	GtkWidget *button_clear = gtk_button_new_from_icon_name ( "clear", GTK_ICON_SIZE_BUTTON );
	gtk_header_bar_pack_end ( ( GtkHeaderBar * ) header_bar, button_clear );

	g_signal_connect ( button_clear, "clicked", G_CALLBACK ( button_clear_clicked_cb ), NULL );

	main_box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	GtkWidget *tree_view = gtk_tree_view_new ( );
	GtkWidget *scroll = gtk_scrolled_window_new ( NULL, NULL );
	gtk_scrolled_window_set_policy ( ( GtkScrolledWindow * ) scroll, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_container_add ( ( GtkContainer * ) scroll, tree_view );

	get_tree_store ( tree_view );

	gtk_box_pack_start ( ( GtkBox * ) main_box, scroll, TRUE, TRUE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) main_box, action_bar, FALSE, FALSE, 0 );

	GtkWidget *box_header = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );

	GtkWidget *button_main = gtk_button_new_with_label ( "Главная" );
	gtk_widget_set_name ( button_main, "button_left" );
	gtk_box_pack_start ( ( GtkBox * ) box_header, button_main, FALSE, FALSE, 0 );
	g_signal_connect ( button_main, "clicked", G_CALLBACK ( button_main_clicked_cb ), NULL );

	GtkWidget *button_statistics = gtk_button_new_with_label ( "Статистика" );
	gtk_box_pack_start ( ( GtkBox * ) box_header, button_statistics, FALSE, FALSE, 0 );
	g_signal_connect ( button_statistics, "clicked", G_CALLBACK ( button_statistics_clicked_cb ), NULL );

	GtkWidget *button_graphic = gtk_button_new_with_label ( "График" );
	gtk_widget_set_name ( button_graphic, "button_right" );
	gtk_box_pack_start ( ( GtkBox * ) box_header, button_graphic, FALSE, FALSE, 0 );
	g_signal_connect ( button_graphic, "clicked", G_CALLBACK ( button_graphic_clicked_cb ), NULL );

	gtk_header_bar_set_custom_title ( ( GtkHeaderBar * ) header_bar, box_header );

	statistics_box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	graphic_box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );

	gtk_container_add ( ( GtkContainer * ) window, main_box );

	gtk_widget_show_all ( window );
	gtk_widget_hide ( window_settings );
	g_main_loop_run ( loop );

}

static void init_config ( ) {
	config_dir = calloc ( 255, 1 );
	config_file = calloc ( 255, 1 );
	config_file_error = calloc ( 255, 1 );

	const char *home = getenv ( "HOME" );

	snprintf ( config_dir, 255, "%s/.binance_bot", home );
	snprintf ( config_file, 255, "%s/binance.data", config_dir );
	snprintf ( config_file_error, 255, "%s/%s", home, "binance.error" );

	if ( access ( config_dir, F_OK ) ) {
		mkdir ( config_dir, 0770 );
	} else {
		struct stat st;
		stat ( config_dir, &st );
		switch ( st.st_mode & S_IFMT ) {
			case S_IFDIR: break;
			default:
				       {
					       FILE *fp = fopen ( config_file_error, "a" );
					       time_t cur_time = time ( NULL );
					       fprintf ( fp, "%s %s\n", ctime ( &cur_time ), ".binance_bot должен быть каталогом. Можете удалить каталог и программа сама его создаст." );
					       fclose ( fp );
					       exit ( EXIT_FAILURE );
				       }
				       break;
		}
	}

	if ( access ( config_file, F_OK ) ) {
		FILE *fp = fopen ( config_file, "w" );
		snprintf ( s_s.btcusd, 64, "10.0" );
		snprintf ( s_s.btcusd_lower, 64, "10.0" );
		snprintf ( s_s.btcusd_point, 64, "10.0" );
		snprintf ( s_s.eth, 64, "10.0" );
		snprintf ( s_s.eth_lower, 64, "10.0" );
		snprintf ( s_s.eth_point, 64, "10.0" );

		price_btc_upper_percent = atof ( s_s.btcusd );
		price_btc_lower = atof ( s_s.btcusd_lower );
		btcusd_percent_point = atof ( s_s.btcusd_point );

		price_eth_upper_percent = atof ( s_s.eth );
		price_eth_lower = atof ( s_s.eth_lower );
		eth_percent_point = atof ( s_s.eth_point );

		s_s.btc_update = 0;
		s_s.eth_update = 0;

		fwrite ( &s_s, sizeof ( struct settings_symbol ), 1, fp );
		fclose ( fp );
	} else {
		FILE *fp = fopen ( config_file, "r" );

		fread ( &s_s, sizeof ( struct settings_symbol ), 1, fp );
		fclose ( fp );

		price_btc_upper_percent = atof ( s_s.btcusd );
		price_btc_lower = atof ( s_s.btcusd_lower );
		btcusd_percent_point = atof ( s_s.btcusd_point );

		price_eth_upper_percent = atof ( s_s.eth );
		price_eth_lower = atof ( s_s.eth_lower );
		eth_percent_point = atof ( s_s.eth_point );
	}

	snprintf ( config_file_error, 255, "%s/%s", config_dir, "binance.error" );
}

int main ( int argc, char **argv ) {

	init_config ( );

	init_strings ( );

	signal ( SIGINT, sig_handler );

	app = gtk_application_new ( "com.xverizex.binance", G_APPLICATION_FLAGS_NONE );
	g_application_register ( ( GApplication * ) app, NULL, NULL );
	g_signal_connect ( app, "activate", G_CALLBACK ( g_startup_cb ), NULL );
	return g_application_run ( ( GApplication * ) app, argc, argv );
}
