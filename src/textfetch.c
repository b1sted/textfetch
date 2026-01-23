#include <stdio.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RESET  "\x1b[0m"

int main(void) {
    struct utsname machine_info;

    if (uname(&machine_info) != 0) {
        perror("uname");
        return 1;
    }

    struct passwd *user_info = getpwuid(geteuid());

    if (!user_info) {
        perror("getpwuid");
        return 1;
    }

    if (isatty(STDOUT_FILENO)) {
        printf(ANSI_BOLD "%s@%s\n" ANSI_RESET, user_info->pw_name, machine_info.nodename);
    } else {
        printf("%s@%s\n", user_info->pw_name, machine_info.nodename);
    }

    return 0;
}