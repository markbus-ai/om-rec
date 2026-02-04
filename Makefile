CC = gcc
CFLAGS = -O3 -fstack-protector-strong
TARGET = om-rec
BIN_DIR = $(HOME)/.local/bin
APP_DIR = $(HOME)/.local/share/applications

all: $(TARGET)

$(TARGET): om-rec.c
	$(CC) $(CFLAGS) om-rec.c -o $(TARGET)
	strip $(TARGET)

install: all
	@echo "Instalando Omarchy Recorder..."
	mkdir -p $(BIN_DIR)
	mkdir -p $(APP_DIR)
	cp $(TARGET) $(BIN_DIR)/
	cp om-toggle-rec $(BIN_DIR)/
	chmod +x $(BIN_DIR)/om-toggle-rec
	cp om-rec.desktop $(APP_DIR)/
	
	# Asegurar rutas absolutas en el script wrapper instalado
	sed -i 's|BIN=.*|BIN="$(BIN_DIR)/$(TARGET)"|' $(BIN_DIR)/om-toggle-rec
	
	# Asegurar rutas absolutas en el desktop file instalado
	sed -i 's|Exec=.*|Exec=$(BIN_DIR)/om-toggle-rec|' $(APP_DIR)/om-rec.desktop
	
	@echo "------------------------------------------------"
	@echo "¡Instalación completa!"
	@echo "Añade esto a tu hyprland.conf:"
	@echo "bind = SUPER, R, exec, uwsm-app -- $(BIN_DIR)/om-toggle-rec"
	@echo "------------------------------------------------"

clean:
	rm -f $(TARGET)

uninstall:
	rm -f $(BIN_DIR)/$(TARGET)
	rm -f $(BIN_DIR)/om-toggle-rec
	rm -f $(APP_DIR)/om-rec.desktop
