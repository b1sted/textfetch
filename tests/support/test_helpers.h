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
    char *result = NULL;
    int fd = -1;
    bool remove_path = false;

    fd = mkstemp(path);
    if (fd < 0) {
        goto cleanup;
    }

    size_t length = strlen(content);
    ssize_t written = write(fd, content, length);
    if (written != (ssize_t)length) {
        remove_path = true;
        goto cleanup;
    }

    result = strdup(path);
    if (!result) {
        remove_path = true;
    }

cleanup:
    if (fd >= 0) {
        close(fd);
    }
    if (remove_path) {
        unlink(path);
    }
    return result;
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
        goto cleanup;
    }

    capture->temporary = tmpfile();
    if (!capture->temporary) {
        goto cleanup;
    }

    if (dup2(fileno(capture->temporary), STDOUT_FILENO) < 0) {
        goto cleanup;
    }

    return true;

cleanup:
    if (capture->temporary) {
        fclose(capture->temporary);
        capture->temporary = NULL;
    }
    if (capture->saved_stdout >= 0) {
        close(capture->saved_stdout);
        capture->saved_stdout = -1;
    }
    return false;
}

/* Restores stdout and returns everything captured. */
static inline char *test_capture_end(test_capture_t *capture)
{
    char *output = NULL;

    if (!capture || capture->saved_stdout < 0 || !capture->temporary) {
        goto cleanup;
    }

    fflush(stdout);

    if (dup2(capture->saved_stdout, STDOUT_FILENO) < 0) {
        goto cleanup;
    }

    close(capture->saved_stdout);
    capture->saved_stdout = -1;

    output = test_read_stream(capture->temporary);

cleanup:
    if (capture) {
        if (capture->saved_stdout >= 0) {
            close(capture->saved_stdout);
            capture->saved_stdout = -1;
        }
        if (capture->temporary) {
            fclose(capture->temporary);
            capture->temporary = NULL;
        }
    }
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
    FILE *output_file = NULL;
    FILE *error_file = NULL;
    pid_t pid = -1;
    int wait_status = 0;

    output_file = tmpfile();
    error_file = tmpfile();
    if (!output_file || !error_file) {
        goto cleanup;
    }

    pid = fork();
    if (pid < 0) {
        goto cleanup;
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

    if (waitpid(pid, &wait_status, 0) < 0) {
        goto cleanup;
    }

    result.output = test_read_stream(output_file);
    result.error = test_read_stream(error_file);

    if (WIFEXITED(wait_status)) {
        result.status = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        result.status = 128 + WTERMSIG(wait_status);
    }

cleanup:
    if (output_file) {
        fclose(output_file);
    }
    if (error_file) {
        fclose(error_file);
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
