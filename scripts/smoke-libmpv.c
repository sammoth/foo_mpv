#include <windows.h>
#include <mpv/client.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;

    HMODULE dll = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dll) {
        fprintf(stderr, "LoadLibraryEx failed: %lu\n", GetLastError());
        return 1;
    }

    mpv_handle *(*create)(void) = (void *)GetProcAddress(dll, "mpv_create");
    int (*set_option_string)(mpv_handle *, const char *, const char *) =
        (void *)GetProcAddress(dll, "mpv_set_option_string");
    int (*initialize)(mpv_handle *) = (void *)GetProcAddress(dll, "mpv_initialize");
    void (*terminate_destroy)(mpv_handle *) =
        (void *)GetProcAddress(dll, "mpv_terminate_destroy");
    if (!create || !set_option_string || !initialize || !terminate_destroy) {
        fprintf(stderr, "libmpv is missing a required export\n");
        return 1;
    }

    mpv_handle *handle = create();
    if (!handle) {
        fprintf(stderr, "mpv_create failed\n");
        return 1;
    }
    int result = set_option_string(handle, "vo", "null");
    if (result >= 0)
        result = set_option_string(handle, "ao", "null");
    if (result >= 0)
        result = initialize(handle);
    terminate_destroy(handle);
    FreeLibrary(dll);
    if (result < 0) {
        fprintf(stderr, "mpv_initialize failed: %d\n", result);
        return 1;
    }
    return 0;
}
