all:
	gcc -Wall gfuse.c `pkg-config fuse3 --cflags --libs` -o gfuse


run:
	./gfuse -d -s -o allow_other -o default_permissions /tmp/opt2