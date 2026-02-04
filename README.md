# Omarchy Recorder (om-rec)

Una herramienta de grabación de pantalla minimalista, nativa y ultrarrápida diseñada para entornos Wayland (Hyprland/Omarchy).

## Características

- **Cero Bloat:** Escrito en C puro. Usa `fork+exec` (sin llamadas a shell lentas).
- **Seguro:** Locking atómico para prevenir instancias múltiples.
- **Eficiente:** Usa `wf-recorder` con preset `ultrafast` y formato `yuv420p` (compatible con WhatsApp/Telegram).
- **UX Nativa:** Notificaciones asíncronas e integración con el menú de aplicaciones (`.desktop`).
- **Resiliente:** Detecta cierres inesperados y se auto-repara.

## Requisitos

- `wf-recorder`
- `slurp`
- `libnotify`
- `gcc` (para compilar)

## Instalación Rápida

```bash
git clone https://github.com/TU_USUARIO/om-rec.git
cd om-rec
make install
```

## Configuración (Hyprland)

Agrega esto a tu `~/.config/hypr/hyprland.conf` o `bindings.conf`:

```ini
bind = SUPER, R, exec, uwsm-app -- ~/.local/bin/om-toggle-rec
```

## Uso

- **Teclado:** Presiona `Super + R` para iniciar/detener.
- **Menú:** Busca "Omarchy Recorder".
- **Comportamiento:**
    1. Al iniciar, selecciona el área con el mouse.
    2. Al detener, recibirás una notificación.
    3. Los videos se guardan en `~/Videos/rec_YYYYMMDD_HHMMSS.mp4`.
