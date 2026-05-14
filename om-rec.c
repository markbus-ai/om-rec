#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// Buffer global para parámetros extra
char extra_params[256] = {0};

void get_pid_filepath(char *buffer, size_t len) {
  const char *xdg = getenv("XDG_RUNTIME_DIR");
  if (!xdg)
    xdg = "/tmp";
  snprintf(buffer, len, "%s/om-rec.pid", xdg);
}

int is_process_running(pid_t pid) {
  if (pid <= 0)
    return 0;
  return (kill(pid, 0) == 0 || errno == EPERM);
}

void get_safe_filepath(char *buffer, size_t len) {
  const char *home = getenv("HOME");
  if (!home)
    home = "/tmp";

  char dir[256];
  snprintf(dir, sizeof(dir), "%s/Videos", home);
  mkdir(dir, 0755);

  time_t t = time(NULL);
  struct tm *tm_info = localtime(&t);
  char timestamp[64];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

  snprintf(buffer, len, "%s/rec_%s.mp4", dir, timestamp);
}

void notify_user(const char *title, const char *body) {
  if (fork() == 0) {
    if (fork() == 0) {
      setsid();
      int nullfd = open("/dev/null", O_RDWR);
      dup2(nullfd, 1);
      dup2(nullfd, 2);
      execlp("notify-send", "notify-send", "-i", "video-x-generic", title, body,
             NULL);
      exit(0);
    }
    exit(0);
  }
  wait(NULL);
}

int get_slurp_geometry(char *buffer, size_t size) {
  int pipefd[2];
  if (pipe(pipefd) == -1)
    return 0;

  pid_t pid = fork();
  if (pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], 1);
    close(pipefd[1]);
    execlp("slurp", "slurp", "-d", NULL);
    exit(1);
  }

  close(pipefd[1]);
  FILE *stream = fdopen(pipefd[0], "r");
  if (stream) {
    if (fgets(buffer, size, stream) != NULL) {
      buffer[strcspn(buffer, "\n")] = 0;
    }
    fclose(stream);
  }

  int status;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void stop_recording() {
  char pid_file[256];
  get_pid_filepath(pid_file, sizeof(pid_file));

  FILE *f = fopen(pid_file, "r");
  if (!f)
    return;

  pid_t pid;
  if (fscanf(f, "%d", &pid) != 1) {
    fclose(f);
    unlink(pid_file);
    return;
  }
  fclose(f);

  if (is_process_running(pid)) {
    kill(pid, SIGINT);
    for (int i = 0; i < 40; i++) {
      if (!is_process_running(pid))
        break;
      usleep(50000);
    }
  }

  unlink(pid_file);
  notify_user("Omarchy Rec", "Grabación guardada.");
}

void start_recording() {
  char geometry[128] = {0};
  if (!get_slurp_geometry(geometry, sizeof(geometry)) ||
      strlen(geometry) == 0) {
    return;
  }

  char filename[512];
  get_safe_filepath(filename, sizeof(filename));

  pid_t pid = fork();
  if (pid < 0)
    return;

  if (pid == 0) {
    setsid();
    int dev_null = open("/dev/null", O_WRONLY);
    dup2(dev_null, 1);
    dup2(dev_null, 2);

    // Si hay parámetros extra (como el codec), podrías necesitarlos acá.
    // Por ahora mantenemos el preset ultrafast para que no explote la CPU.
    char *args[] = {"wf-recorder", "-g", geometry,           "-f",
                    filename,      "-p", "preset=ultrafast", "--pixel-format",
                    "yuv420p",     NULL};

    // [Inferencia] Si el usuario pasó -c codec, podrías inyectarlo en el array
    // de args.
    execvp("wf-recorder", args);
    exit(1);
  }

  char pid_file[256];
  get_pid_filepath(pid_file, sizeof(pid_file));
  FILE *f = fopen(pid_file, "w");
  if (f) {
    fprintf(f, "%d", pid);
    fclose(f);
  }
}

int main(int argc, char *argv[]) {
  int opt;
  // -c espera un argumento (el codec o config)
  while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
    case 'c':
      strncpy(extra_params, optarg, sizeof(extra_params) - 1);
      break;
    default:
      fprintf(stderr, "Uso: %s [-c params] [start|stop|status|toggle]\n",
              argv[0]);
      return 1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Falta el comando.\n");
    return 1;
  }

  char *command = argv[optind];
  char pid_file[256];
  get_pid_filepath(pid_file, sizeof(pid_file));

  pid_t current_pid = 0;
  int running = 0;
  FILE *f = fopen(pid_file, "r");
  if (f) {
    if (fscanf(f, "%d", &current_pid) == 1) {
      running = is_process_running(current_pid);
    }
    fclose(f);
  }

  if (strcmp(command, "start") == 0) {
    if (running)
      printf("Ya grabando (PID %d)\n", current_pid);
    else
      start_recording();
  } else if (strcmp(command, "stop") == 0) {
    if (running)
      stop_recording();
    else {
      printf("No hay grabación activa.\n");
      unlink(pid_file);
    }
  } else if (strcmp(command, "status") == 0) {
    printf("%s\n", running ? "recording" : "idle");
  } else if (strcmp(command, "toggle") == 0) {
    if (running)
      stop_recording();
    else
      start_recording();
  } else {
    fprintf(stderr, "Comando desconocido: %s\n", command);
    return 1;
  }

  return 0;
}
