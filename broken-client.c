#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include "xdg-shell.h"

#define COLOR_CHANNELS 4

#define cstring_len(s) (sizeof(s) - 1)

struct wl_display *display;
struct wl_registry *registry;
struct wl_shm *shm;
struct wl_shm_pool *shm_pool;
struct wl_buffer *buffer;
struct wl_compositor *compositor;
struct wl_surface *surface;
struct wl_output *output;
struct xdg_wm_base *xdg_wm_base;
struct xdg_surface *xdg_surface;
struct xdg_toplevel *xdg_toplevel;

void cleanup() {
  xdg_toplevel_destroy(xdg_toplevel);
  xdg_surface_destroy(xdg_surface);
  wl_surface_destroy(surface);
  wl_display_disconnect(display);
}

void fatal_error_callback(void *data, struct wl_display *wl_display,
                          void *object_id, uint32_t code, const char *message) {
  printf("fatal error: target_object_id=%p code=%u error=%s\n",
          object_id, code, message);
  exit(EINVAL);
}

struct wl_display_listener display_listener = {
    .error = fatal_error_callback,
};

void register_global_add(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
  if (strcmp("wl_shm", interface) == 0) {
    printf("Binding <wl_shm> using name=%u version=%u\n", name, version);
    shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
  }
  if (strcmp("xdg_wm_base", interface) == 0) {
    printf("Binding <xdg_wm_base> using name=%u version=%u\n", name, version);
    xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
  }
  if (strcmp("wl_compositor", interface) == 0) {
    printf("Interface is <wl_compositor>.\n");
    compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
  }
  if (strcmp(interface, "wl_output") == 0) {
    printf("Binding <wl_output> using name=%u version=%u\n", name, version);
    output = wl_registry_bind(wl_registry, name, &wl_output_interface, version);
  }
}

void register_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
    .global = register_global_add, .global_remove = register_global_remove};

void ping_callback(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void surface_configure_resize_callback(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
}

void close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
  cleanup();
  exit(0);
}

void toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {}

struct xdg_wm_base_listener ping_listener = {
    .ping = ping_callback,
};

struct xdg_surface_listener configure_listener = {
    .configure = surface_configure_resize_callback,
};

struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = close_handler,
};


int main(void) {
  uint32_t width = 117;
  uint32_t height = 150;
  uint32_t stride = width * COLOR_CHANNELS;
  uint64_t shm_pool_size = stride * height;
  uint8_t *shm_pool_data = NULL; 

  // Generating a SHM file with random name
  char shm_file_name[255] = "/";
  for (uint64_t i = 1; i < cstring_len(shm_file_name); i++) {
    shm_file_name[i] = ((double)rand()) / (double)RAND_MAX * 26 + 'a';
  }
  int shm_fd = shm_open(shm_file_name, O_RDWR | O_EXCL | O_CREAT, 0600);
  if (shm_fd == -1)
    exit(errno);

  assert(shm_unlink(shm_file_name) != -1);

  // Setting the size of the shm_fd to the first size
  if (ftruncate(shm_fd, shm_pool_size) == -1)
    exit(errno);

  shm_pool_data = mmap(NULL, shm_pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm_pool_data == MAP_FAILED) {
    printf("mmap failed %m\n");
    close(shm_fd);
    exit(errno);
  }
  assert(shm_pool_data != NULL);

  display = wl_display_connect(NULL);
  if (display == NULL) {
    exit(errno);
  }

  registry = wl_display_get_registry(display),
  wl_display_add_listener(display, &display_listener, NULL);
  wl_registry_add_listener(registry, &registry_listener, NULL);

  wl_display_dispatch(display);
  wl_display_roundtrip(display);

  assert(compositor != 0);
  assert(shm != 0);
  assert(xdg_wm_base != 0);
  assert(surface == 0);

  surface = wl_compositor_create_surface(compositor);
  xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
  xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

  xdg_wm_base_add_listener(xdg_wm_base, &ping_listener, NULL);
  xdg_surface_add_listener(xdg_surface, &configure_listener, NULL);
  xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, NULL);

  xdg_toplevel_set_title(xdg_toplevel, "Breaking the hyprland");

  wl_surface_commit(surface);
  wl_display_roundtrip(display);

  assert(surface != 0);
  assert(xdg_surface != 0);
  assert(xdg_toplevel != 0);

  // Simulating a resize event but forgetting to change the shm_fd size
  width = 155;
  height = 270;
  stride = width * COLOR_CHANNELS;
  shm_pool_size = stride * height;

  // Creating the pool with a different size than the one in the shm_fd
  shm_pool = wl_shm_create_pool(shm, shm_fd, shm_pool_size);
  assert(shm_pool != 0);
  buffer = wl_shm_pool_create_buffer(shm_pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
  assert(buffer != 0);

  wl_surface_attach(surface, buffer, 0, 0);
  wl_surface_commit(surface);

  while (wl_display_dispatch(display) != -1);

  cleanup();
  return 0;
}

