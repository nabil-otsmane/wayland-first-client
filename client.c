#include "xdg-shell.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include "wayland-logo.h"

#define COLOR_CHANNELS 4

#define cstring_len(s) (sizeof(s) - 1)

typedef struct state_t state_t;
struct state_t {
  struct wl_display    *display;
  struct wl_registry   *registry;
  struct wl_shm        *shm;
  struct wl_shm_pool   *shm_pool;
  struct wl_buffer     *buffer;
  struct wl_compositor *compositor;
  struct wl_surface    *surface;
  struct wl_output     *output;
  struct xdg_wm_base   *xdg_wm_base;
  struct xdg_surface   *xdg_surface;
  struct xdg_toplevel  *xdg_toplevel;
  uint32_t             w;
  uint32_t             h;
  uint32_t             shm_pool_size;
  int                  shm_fd;
  uint8_t              *shm_pool_data;
};

state_t state = {
    .w = 117,
    .h = 150,
};

void cleanup() {
  xdg_toplevel_destroy(state.xdg_toplevel);
  xdg_surface_destroy(state.xdg_surface);
  wl_surface_destroy(state.surface);
  wl_display_disconnect(state.display);
}

static void create_shared_memory_file(uint64_t size) {
  assert(state.shm != 0);
  size *= COLOR_CHANNELS;
  char name[255] = "/";
  for (uint64_t i = 1; i < cstring_len(name); i++) {
    name[i] = ((double)rand()) / (double)RAND_MAX * 26 + 'a';
  }

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  if (fd == -1)
    exit(errno);

  assert(shm_unlink(name) != -1);

  if (ftruncate(fd, size) == -1)
    exit(errno);

  state.shm_pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (state.shm_pool_data == MAP_FAILED) {
    fprintf(stderr, "mmap failedL %m\n");
    close(fd);
    exit(errno);
  }
  assert(state.shm_pool_data != NULL);
  state.shm_fd = fd;
  state.shm_pool_size = size;
  state.shm_pool = wl_shm_create_pool(state.shm, state.shm_fd, state.shm_pool_size);
  state.buffer = wl_shm_pool_create_buffer(state.shm_pool, 0, state.w, state.h, state.w * COLOR_CHANNELS, WL_SHM_FORMAT_XRGB8888);
  assert(state.shm_pool != 0);
  assert(state.buffer != 0);
}

void resize_shared_memory(uint64_t size) {
  printf("[Resizing buffer to size %lu]\n", size);
  size *= COLOR_CHANNELS;
  if (size <= state.shm_pool_size) return;
  if (ftruncate(state.shm_fd, size) == -1)
    exit(errno);

  state.shm_pool_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, state.shm_fd, 0);
  if (state.shm_pool_data == MAP_FAILED) {
    fprintf(stderr, "mmap failed %m\n");
    close(state.shm_fd);
    exit(errno);
  }
  assert(state.shm_pool_data != NULL);
  state.shm_pool_size = size;
  wl_shm_pool_resize(state.shm_pool, size);
  state.buffer = wl_shm_pool_create_buffer(state.shm_pool, 0, state.w, state.h, state.w * COLOR_CHANNELS, WL_SHM_FORMAT_XRGB8888);
}

void fatal_error_callback(void *data, struct wl_display *wl_display, void *object_id, uint32_t code, const char *message) {
  fprintf(stderr, "fatal error: target_object_id=%p code=%u error=%s\n", object_id, code, message);
  exit(EINVAL);
}

struct wl_display_listener display_listener = {
    .error = fatal_error_callback,
};

void register_global_add(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version) {
  if (strcmp("wl_shm", interface) == 0) {
    fprintf(stderr, "Binding <wl_shm> using name=%u version=%u\n", name, version);
    state.shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
  }
  if (strcmp("xdg_wm_base", interface) == 0) {
    fprintf(stderr, "Binding <xdg_wm_base> using name=%u version=%u\n", name, version);
    state.xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
  }
  if (strcmp("wl_compositor", interface) == 0) {
    fprintf(stderr, "Interface is <wl_compositor>.\n");
    state.compositor =
        wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
  }
  if (strcmp(interface, "wl_output") == 0) {
    fprintf(stderr, "Binding <wl_output> using name=%u version=%u\n", name, version);
    state.output = wl_registry_bind(wl_registry, name, &wl_output_interface, version);
  }
}

void register_global_remove(void *data, struct wl_registry *wl_registry, uint32_t name) {
  printf("<- global-remove registry@%p name:%u\n", wl_registry, name);
}

const struct wl_registry_listener registry_listener = {
    .global = register_global_add, .global_remove = register_global_remove};

void ping_callback(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
  xdg_wm_base_pong(xdg_wm_base, serial);
}

void surface_configure_resize_callback(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
  xdg_surface_ack_configure(xdg_surface, serial);
}

void toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
  state.w = width;
  state.h = height;
  resize_shared_memory(state.w * state.h);
}

void close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
  printf("close event received!\n");
  cleanup();
  exit(0);
}

struct xdg_wm_base_listener ping_listener = {
    .ping = ping_callback,
};

struct xdg_surface_listener configure_listener = {
    .configure = surface_configure_resize_callback,
};

struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure_handler,
    .close = close_handler,
};


int main(void) {
  state.display = wl_display_connect(NULL);
  if (state.display == NULL) {
    exit(errno);
  }

  state.registry = wl_display_get_registry(state.display),

  wl_display_add_listener(state.display, &display_listener, NULL);
  wl_registry_add_listener(state.registry, &registry_listener, NULL);

  wl_display_dispatch(state.display);
  wl_display_roundtrip(state.display);

  assert(state.compositor != 0);
  assert(state.shm != 0);
  assert(state.xdg_wm_base != 0);
  assert(state.surface == 0);

  state.surface = wl_compositor_create_surface(state.compositor);
  state.xdg_surface = xdg_wm_base_get_xdg_surface(state.xdg_wm_base, state.surface);
  state.xdg_toplevel = xdg_surface_get_toplevel(state.xdg_surface);
  assert(state.surface != 0);
  assert(state.xdg_surface != 0);
  assert(state.xdg_toplevel != 0);

  create_shared_memory_file(state.w * state.h);

  xdg_wm_base_add_listener(state.xdg_wm_base, &ping_listener, NULL);
  xdg_surface_add_listener(state.xdg_surface, &configure_listener, NULL);
  xdg_toplevel_add_listener(state.xdg_toplevel, &toplevel_listener, NULL);

  xdg_toplevel_set_title(state.xdg_toplevel, "My GUI App");

  wl_surface_commit(state.surface);

  while (wl_display_dispatch(state.display) != -1) {
    uint32_t *pixels = (uint32_t *)state.shm_pool_data;
    for (uint32_t i = 0; i < state.w * state.h; i++) {
      uint8_t r = 0xff;
      uint8_t g = 0;
      uint8_t b = 0;
      pixels[i] = (r << 16) | (g << 8) | b;
    }
    wl_surface_attach(state.surface, state.buffer, 0, 0);
    wl_surface_commit(state.surface);
  }

  cleanup();
  return 0;
}
