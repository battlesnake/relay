sources := $(wildcard *.c) $(shell find c_modules -name '*.c' -and -not -name '*_example.c')

.PHONY: clean tags demo

demo: client_example.out

client_example.out: $(sources)
	gcc -std=gnu99 -Og -lpthread -Ic_modules -DDEMO=relay -DDEMO_relay -Wall -Werror -Wextra -o $@ $^

clean:
	rm -f -- *.out tags

tags:
	ctags -R

demo0: demo
	@echo -e '\e[32;1mFilter\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.5 && ./client_example.out localhost 49501 bravo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client_example.out localhost 49501 alpha tere tere eestimaa'
	node server

demo1: demo
	@echo -e '\e[32;1mFanout x1 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.5 && ./client_example.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client_example.out localhost 49501 alpha tere tere eestimaa'
	node server

demo2: demo
	@echo -e '\e[32;1mFanout x2 x1\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 0.5 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client_example.out localhost 49501 alpha tere tere eestimaa'
	node server

demo3: demo
	@echo -e '\e[32;1mFanout x2 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 0.5 && ./client_example.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client_example.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 0.9 && ./client_example.out localhost 49501 alpha tere tere eestimaa'
	node server
