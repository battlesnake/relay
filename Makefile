sources := $(wildcard *.c) $(shell find c_modules -name '*.c' -and -not -name '*_example.c')

examples := $(patsubst %.c, %, $(wildcard *_example.c))

.PHONY: clean tags demo

demo: $(examples:%=%.out)

%.out: %.c $(sources)
	gcc -std=gnu99 -g -O0 -lpthread -Ic_modules -DDEMO_$(*F:%_example=%) -DDEBUG_relay -Wall -Werror -Wextra -o $@ $^

clean:
	rm -f -- *.out tags

tags:
	ctags -R

demo0: demo
	@echo -e '\e[32;1mFilter\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 1.3 && ./relay_client_example.out localhost 49501 echo || sleep 3'
	tmux splitw -d -- sh -c 'sleep 1.5 && ./relay_client_example.out localhost 49501 bravo || sleep 3'
	tmux splitw -d -- sh -c 'sleep 1.7 && ./relay_client_example.out localhost 49501 alpha tere tere eestimaa || sleep 3'
	node server

demo1: demo
	@echo -e '\e[32;1mFanout x1 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 1.3 && ./relay_client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 1.5 && ./relay_client_example.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 1.7 && ./relay_client_example.out localhost 49501 alpha tere tere eestimaa'
	node server

demo2: demo
	@echo -e '\e[32;1mFanout x2 x1\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 1.3 && ./relay_client_example.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 1.5 && ./relay_client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 1.7 && ./relay_client_example.out localhost 49501 alpha tere tere eestimaa'
	node server

demo3: demo
	@echo -e '\e[32;1mFanout x2 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 1.3 && ./relay_client_example.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 1.5 && ./relay_client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 1.7 && ./relay_client_example.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 1.9 && ./relay_client_example.out localhost 49501 alpha tere tere eestimaa'
	node server
