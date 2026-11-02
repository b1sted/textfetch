/* SPDX-License-Identifier: MIT */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Writes content to a temporary file and returns its path. */
static inline char *test_write_temp_file(const char *content)
{
    char path[] = "/tmp/textfetch-test-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return NULL;
    }

    size_t length = strlen(content);
    ssize_t written = write(fd, content, length);
    if (written != (ssize_t)length) {
        close(fd);
        unlink(path);
        return NULL;
    }

    close(fd);
    return strdup(path);
}

/* Reads an entire stream into a newly allocated string. */
static inline char *test_read_stream(FILE *stream)
{
    if (!stream) {
        return NULL;
    }

    if (fseek(stream, 0, SEEK_END) != 0) {
        return NULL;
    }

    int64_t length = (int64_t)ftell(stream);
    if (length < 0) {
        return NULL;
    }

    if (fseek(stream, 0, SEEK_SET) != 0) {
        return NULL;
    }

    char *buffer = malloc((size_t)length + 1);
    if (!buffer) {
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)length, stream);
    buffer[read_count] = '\0';

    return buffer;
}

typedef struct {
    int saved_stdout;
    FILE *temporary;
} test_capture_t;

/* Redirects stdout into a temporary file. */
static inline bool test_capture_begin(test_capture_t *capture)
{
    if (!capture) {
        return false;
    }

    capture->saved_stdout = -1;
    capture->temporary = NULL;

    fflush(stdout);

    capture->saved_stdout = dup(STDOUT_FILENO);
    if (capture->saved_stdout < 0) {
        return false;
    }

    capture->temporary = tmpfile();
    if (!capture->temporary) {
        close(capture->saved_stdout);
        capture->saved_stdout = -1;
        return false;
    }

    if (dup2(fileno(capture->temporary), STDOUT_FILENO) < 0) {
        fclose(capture->temporary);
        capture->temporary = NULL;
        close(capture->saved_stdout);
        capture->saved_stdout = -1;
        return false;
    }

    return true;
}

/* Restores stdout and returns everything captured. */
static inline char *test_capture_end(test_capture_t *capture)
{
    if (!capture || capture->saved_stdout < 0 || !capture->temporary) {
        return NULL;
    }

    fflush(stdout);

    if (dup2(capture->saved_stdout, STDOUT_FILENO) < 0) {
        close(capture->saved_stdout);
        capture->saved_stdout = -1;
        fclose(capture->temporary);
        capture->temporary = NULL;
        return NULL;
    }

    close(capture->saved_stdout);
    capture->saved_stdout = -1;

    char *output = test_read_stream(capture->temporary);
    fclose(capture->temporary);
    capture->temporary = NULL;

    return output;
}

typedef struct {
    char *output;
    char *error;
    int status;
} test_command_t;

/* Executes a binary and captures stdout, stderr, and the exit status. */
static inline test_command_t test_run_command(const char *binary, char *const argv[])
{
    test_command_t result = {NULL, NULL, -1};

    FILE *output_file = tmpfile();
    FILE *error_file = tmpfile();
    if (!output_file || !error_file) {
        if (output_file) {
            fclose(output_file);
        }
        if (error_file) {
            fclose(error_file);
        }
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        fclose(output_file);
        fclose(error_file);
        return result;
    }

    if (pid == 0) {
        if (dup2(fileno(output_file), STDOUT_FILENO) < 0) {
            _exit(127);
        }
        if (dup2(fileno(error_file), STDERR_FILENO) < 0) {
            _exit(127);
        }

        fclose(output_file);
        fclose(error_file);

        execv(binary, argv);
        _exit(127);
    }

    int wait_status = 0;
    if (waitpid(pid, &wait_status, 0) < 0) {
        fclose(output_file);
        fclose(error_file);
        return result;
    }

    result.output = test_read_stream(output_file);
    result.error = test_read_stream(error_file);
    fclose(output_file);
    fclose(error_file);

    if (WIFEXITED(wait_status)) {
        result.status = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        result.status = 128 + WTERMSIG(wait_status);
    }

    return result;
}

static inline void test_free_command(test_command_t *command)
{
    free(command->output);
    free(command->error);
    command->output = NULL;
    command->error = NULL;
}

#endif /* TEST_HELPERS_H */
