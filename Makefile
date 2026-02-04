CC = gcc
CFLAGS = -O3 -fstack-protector-strong
TARGET = om-rec

# Defaults para instalación local (make install a secas)
PREFIX ?= $(HOME)/.local
BIN_DIR = $(DESTDIR)$(PREFIX)/bin
APP_DIR = $(DESTDIR)$(PREFIX)/share/applications

all: $(TARGET)

$(TARGET): om-rec.c
	$(CC) $(CFLAGS) om-rec.c -o $(TARGET)
	strip $(TARGET)

install: all
	@echo "Instalando Omarchy Recorder en $(BIN_DIR)..."
	mkdir -p $(BIN_DIR)
	mkdir -p $(APP_DIR)
	install -m 755 $(TARGET) $(BIN_DIR)/$(TARGET)
	install -m 755 om-toggle-rec $(BIN_DIR)/om-toggle-rec
	install -m 644 om-rec.desktop $(APP_DIR)/om-rec.desktop
	
	# Ajustar rutas en el script instalado para apuntar al binario correcto
	# Si PREFIX es /usr (instalación sistema), el binario estará en /usr/bin/om-rec
	sed -i 's|BIN=.*|BIN="$(PREFIX)/bin/$(TARGET)"|' $(BIN_DIR)/om-toggle-rec
	
	# Ajustar ruta en el desktop file
	sed -i 's|Exec=.*|Exec=$(PREFIX)/bin/om-toggle-rec|' $(APP_DIR)/om-rec.desktop

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(BIN_DIR)/$(TARGET)
	rm -f $(BIN_DIR)/om-toggle-rec
	rm -f $(APP_DIR)/om-rec.desktop