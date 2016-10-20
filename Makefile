sources := $(filter-out %_example.c, $(wildcard *.c) $(shell find c_modules -type f -name '*.c'))

.PHONY: clean

client.out: $(sources)
	gcc -std=gnu99 -Og -lpthread -Ic_modules -DDEMO=relay -DDEMO_relay -o $@ $^

clean:
	rm -f -- *.out

demo0: client.out
	@echo -e '\e[32;1mFilter\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.5 && ./client.out localhost 49501 bravo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client.out localhost 49501 alpha tere tere eestimaa'
	node server

demo1: client.out
	@echo -e '\e[32;1mFanout x1 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.5 && ./client.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client.out localhost 49501 alpha tere tere eestimaa'
	node server

demo2: client.out
	@echo -e '\e[32;1mFanout x2 x1\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 0.5 && ./client.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client.out localhost 49501 alpha tere tere eestimaa'
	node server

demo3: client.out
	@echo -e '\e[32;1mFanout x2 x2\e[0m'
	tmux splitw -d -h -- sh -c 'sleep 0.3 && ./client.out localhost 49501 echo'
	tmux splitw -d -h -- sh -c 'sleep 0.5 && ./client.out localhost 49501 echo'
	tmux splitw -d -- sh -c 'sleep 0.7 && ./client.out localhost 49501 alpha'
	tmux splitw -d -- sh -c 'sleep 0.9 && ./client.out localhost 49501 alpha tere tere eestimaa'
	node server
