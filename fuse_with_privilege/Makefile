all:
	gcc -Wall gfuse.c `pkg-config fuse3 --cflags --libs` -o gfuse


run:
	./gfuse -d  -o allow_other -o default_permissions ./opt