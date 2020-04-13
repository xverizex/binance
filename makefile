all:
	gcc   ./src/websocket_client.c ./src/main.c -D_REENTRANT -pthread -I/usr/include/json-c -I/usr/include/libappindicator3-0.1 -I/usr/include/libdbusmenu-glib-0.4 -I/usr/include/gtk-3.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/gtk-3.0 -I/usr/include/gio-unix-2.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/fribidi -I/usr/include/harfbuzz -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/uuid -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -ljson-c -lssl -lappindicator3 -ldbusmenu-glib -lcanberra-gtk3 -lX11 -lcanberra -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -pthread -o binance
install:
	install binance /usr/local/bin
	mkdir -p /usr/local/share/binance
	cp data/binance.png /usr/local/share/pixmaps
	cp data/com.xverizex.binance.desktop /usr/local/share/applications
	cp data/clear.png /usr/local/share/binance
clean:
	rm binance
uninstall:
	rm /usr/local/bin/binance
