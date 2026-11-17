/* Windows port: posix_spawn(p) over CreateProcess (win32/platform_win32.c), enough for
   mpeg.c's ffmpeg pipes: file actions are dup2s onto 0/1/2 only; the pid is the
   Windows process id, which waitpid/kill (sys/wait.h) map back to its handle. */
#ifndef XSG_W32_SPAWN_H
#define XSG_W32_SPAWN_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int n; int fd[8], to[8]; } posix_spawn_file_actions_t;
typedef struct { int unused; } posix_spawnattr_t;
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa);
int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa, int fd, int to);
int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fa,
                 const posix_spawnattr_t *attr, char *const argv[], char *const envp[]);
int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fa,
                const posix_spawnattr_t *attr, char *const argv[], char *const envp[]);
#ifdef __cplusplus
}
#endif
#endif
