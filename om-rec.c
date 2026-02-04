#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PID_FILE "/tmp/om-rec.pid"

// --- Helpers Seguros ---

// Verifica si un proceso está corriendo realmente
int is_process_running(pid_t pid) {
    if (pid <= 0) return 0;
    // Signal 0 no hace nada, pero verifica si tenemos permiso o si el proceso existe
    return (kill(pid, 0) == 0 || errno == EPERM);
}

// Genera ruta segura y crea directorio si falta
void get_safe_filepath(char *buffer, size_t len) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp"; // Fallback extremo

    char dir[256];
    snprintf(dir, sizeof(dir), "%s/Videos", home);

    // Intentamos crear el directorio (si ya existe, falla y no importa)
    mkdir(dir, 0755);

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

    snprintf(buffer, len, "%s/rec_%s.mp4", dir, timestamp);
}

// Ejecuta notificación sin bloquear y sin shell
void notify_user(const char *title, const char *body, const char *path) {
    if (fork() == 0) {
        // En el hijo (daemonizado levemente para no bloquear padre)
        if (fork() == 0) {
            // Nieto: hace el trabajo y muere
            char action_arg[512];
            // Construimos action de forma segura
            snprintf(action_arg, sizeof(action_arg), "files=Abrir Carpeta");
            
            // Usamos setsid para desvincularnos totalmente
            setsid();
            
            // Redirigir std a null
            int nullfd = open("/dev/null", O_RDWR);
            dup2(nullfd, 1); dup2(nullfd, 2);

            execlp("notify-send", "notify-send", 
                   "-i", "video-x-generic",
                   title, body, 
                   "--action", action_arg,
                   NULL);
            exit(0);
        }
        exit(0); // El hijo muere rápido
    }
    // El padre (main) sigue su vida
    wait(NULL); // Limpia al primer hijo zombie inmediato
}

// Obtiene geometría usando pipe+fork+exec (Sin popen/shell)
int get_slurp_geometry(char *buffer, size_t size) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return 0;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);    // Cerrar lectura
        dup2(pipefd[1], 1);  // Redirigir stdout al pipe
        close(pipefd[1]);    // Cerrar escritura original
        execlp("slurp", "slurp", "-d", NULL); // -d para que no moleste en tiling
        exit(1);
    }

    close(pipefd[1]); // Padre cierra escritura
    
    // Leer salida
    FILE *stream = fdopen(pipefd[0], "r");
    if (!stream) return 0;

    if (fgets(buffer, size, stream) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0; // Trim newline
    }
    fclose(stream);

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// --- Funciones Principales ---

void stop_recording() {
    int fd = open(PID_FILE, O_RDONLY);
    if (fd < 0) {
        printf("No hay grabación activa (No PID file).\n");
        return;
    }

    char pid_buf[16] = {0};
    if (read(fd, pid_buf, sizeof(pid_buf) - 1) <= 0) {
        close(fd);
        unlink(PID_FILE);
        return;
    }
    close(fd);

    pid_t pid = atoi(pid_buf);
    
    // Verificar si sigue vivo antes de intentar matar
    if (!is_process_running(pid)) {
        printf("Limpiando archivo PID obsoleto (Proceso %d no existe).\n", pid);
        unlink(PID_FILE);
        return;
    }

    if (kill(pid, SIGINT) == 0) {
        printf("Finalizando grabación (PID: %d)...\n", pid);
        
        // Espera inteligente: poll cada 50ms hasta que muera o timeout de 2s
        for (int i = 0; i < 40; i++) {
            if (!is_process_running(pid)) break;
            usleep(50000); 
        }
    } else {
        perror("Error al enviar señal");
    }

    notify_user("Omarchy Rec", "Grabación guardada correctamente.", "");
    unlink(PID_FILE);
}

void start_recording() {
    // 1. Check atómico y limpieza de stale locks
    int fd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        if (errno == EEXIST) {
            // El archivo existe. Verificamos si el proceso sigue vivo.
            FILE *f = fopen(PID_FILE, "r");
            pid_t old_pid;
            if (f && fscanf(f, "%d", &old_pid) == 1) {
                fclose(f);
                if (is_process_running(old_pid)) {
                    printf("Ya hay una grabación en curso (PID %d).\n", old_pid);
                    return;
                }
                printf("Detectado cierre inesperado previo. Limpiando...\n");
                unlink(PID_FILE);
                // Intentamos abrir de nuevo recursivamente (solo 1 nivel)
                start_recording(); 
                return;
            }
            if (f) fclose(f);
        } else {
            perror("Error al acceder al lockfile");
            return;
        }
    }
    // Si llegamos acá, tenemos el 'fd' abierto y bloqueado para nosotros.
    // Pero lo cerramos por ahora porque forkear es el siguiente paso real.
    // Mantenemos el archivo creado para reservar el lock.
    close(fd);

    // 2. Obtener geometría (Bloqueante UI)
    char geometry[128] = {0};
    if (!get_slurp_geometry(geometry, sizeof(geometry)) || strlen(geometry) == 0) {
        unlink(PID_FILE); // Usuario canceló
        return;
    }

    // 3. Preparar archivo destino
    char filename[512];
    get_safe_filepath(filename, sizeof(filename));

    // 4. Forkear grabación
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        unlink(PID_FILE);
        return;
    }

    if (pid == 0) { // HIJO
        int dev_null = open("/dev/null", O_WRONLY);
        dup2(dev_null, 1);
        dup2(dev_null, 2);
        
        // Array de argumentos directo (Anti-injection)
        char *args[] = {
            "wf-recorder",
            "-g", geometry,
            "-f", filename,
            "-p", "preset=ultrafast", // Bajo CPU usage
            "--pixel-format", "yuv420p", // Compatibilidad máxima (Telegram/WhatsApp)
            NULL
        };
        
        execvp("wf-recorder", args);
        exit(127);
    }

    // PADRE
    // Escribir PID real al archivo
    FILE *f = fopen(PID_FILE, "w");
    if (f) {
        fprintf(f, "%d", pid);
        fclose(f);
    }
    
    printf("Grabando: %s\n", filename);
    
    // Verificación rápida de arranque (100ms)
    usleep(100000);
    if (waitpid(pid, NULL, WNOHANG) == pid) {
        printf("Error: wf-recorder murió al inicio.\n");
        unlink(PID_FILE);
        notify_user("Error", "No se pudo iniciar la grabación.", "");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        // Modo toggle por defecto si no hay args? 
        // No, mejor respetar standard CLI.
        printf("Uso: %s [start|stop|status]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) start_recording();
    else if (strcmp(argv[1], "stop") == 0) stop_recording();
    else if (strcmp(argv[1], "status") == 0) {
        // Status check más inteligente
        FILE *f = fopen(PID_FILE, "r");
        pid_t pid;
        if (f && fscanf(f, "%d", &pid) == 1) {
            fclose(f);
            if (is_process_running(pid)) printf("recording\n");
            else printf("idle (stale pid)\n");
        } else {
            if (f) fclose(f);
            printf("idle\n");
        }
    }
    else printf("Comando inválido.\n");

    return 0;
}
