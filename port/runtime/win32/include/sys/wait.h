/* Windows port: waitpid/kill for processes started by posix_spawn (spawn.h). */
#ifndef XSG_W32_SYS_WAIT_H
#define XSG_W32_SYS_WAIT_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
#define WNOHANG 1
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0)
#define WTERMSIG(s) ((s) & 0x7f)
pid_t waitpid(pid_t pid, int *status, int options);
int kill(pid_t pid, int sig);
#ifdef __cplusplus
}
#endif
#endif
