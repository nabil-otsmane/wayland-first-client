# Wayland Client Hello world

This repo contains a simple "hello world" for the creation of a wayland client.

I followed some tutorials to understand the process and come up with this code (I avoided copying code I did not understand):
- https://gaultier.github.io/blog/wayland_from_scratch.html
- https://wayland-book.com/wayland-display/creation.html
- https://developer.orbitrc.io/documentation/wayland/guides/introduction/

along with some documentation websites for wayland:
- https://wayland.app/protocols/wayland
- https://www.systutorials.com/docs/linux/man/3-wl_display_dispatch/

## Building and running the client

Before running the code, make sure you are running a wayland compositor and have all the necessary packages installed.

Running the client can be done using a single command:
```bash
make && ./client
```


## Running the broken version

When I was trying to add the resize functionality to my simple client, I accidentally broke Hyprland (my compositor of choice). Oops..

The version I tested this code on is "Hyprland 0.45.2". The bug should be fixed by the time you are reading this.. hopefully.

The broken version can be ran using the following command

```bash
make broken && ./client
```
