sources := $(wildcard *.c) $(shell find c_modules -name '*.c' -and -not -name '*_example.c')

progs := relay_send

examples := $(patsubst %.c, %, $(wildcard detail/*_example.c))

export HOST := ::1
export PORT := 13031

.PHONY: clean tags demo demo0 demo1 demo2 demo3 demo4 demo5 progs

demo: $(examples:%=%.out)

progs: $(progs)

$(progs): %: %.c $(sources)
	gcc -std=gnu99 -g -O2 -lpthread -Ic_modules -DSIMPLE_LOGGING -Wall -Werror -Wextra -Dprog_$@ -o $@ $^

detail/%.out: detail/%.c $(sources)
	gcc -std=gnu99 -g -O0 -lpthread -Ic_modules -DDEMO_$(*F:%_example=%) -DSIMPLE_LOGGING -Wall -Werror -Wextra -o $@ $^
# -DSIMPLE_LOGGING_DEBUG

clean:
	rm -f -- *.out tags

tags:
	ctags -R

demo_title = @printf -- '\e[32;1m%s\e[22m - %s\e[0m\n' "$(strip $1)" "$(strip $2)"

# Specify WAIT=y to make subtasks wait indefinitely rather than closing on exit
shellescape = $(subst ','\'',$1)
delay_command = printf -- "\e[1m%s\e[0m\n" "$(strip $1)" && sleep $2 && ( valgrind --quiet --leak-check=full --track-origins=yes $(call shellescape,$3); [ '$$WAIT' ] && read line )
# run_split = sh -c '$(call delay_command,$1,$2,$3)' &
run_split = @tmux splitw -d -h -- sh -c '$(call delay_command,$1,$2,$3)'

demo0: demo
	$(call demo_title, Filter, one pane receives and the other does not)
	$(call run_split, Echo, 1.3, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, No receive, 1.5, ./detail/relay_client_example.out $(HOST) $(PORT) bravo)
	$(call run_split, Yes receive, 1.7, ./detail/relay_client_example.out $(HOST) $(PORT) alpha tere tere eestimaa)
	@tmux select-layout tiled
	node server

demo1: demo
	$(call demo_title, Fanout x1 x2, Two clients sent to one echo which sends back to them)
	$(call run_split, Echo, 1.3, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, Receive, 1.5, ./detail/relay_client_example.out $(HOST) $(PORT) alpha)
	$(call run_split, Receive, 1.7, ./detail/relay_client_example.out $(HOST) $(PORT) alpha tere tere eestimaa)
	@tmux select-layout tiled
	node server

demo2: demo
	$(call demo_title, Fanout x2 x1, One client sends to two echoes which send back to it)
	$(call run_split, Echo, 1.3, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, Echo, 1.5, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, Receive x2, 1.7, ./detail/relay_client_example.out $(HOST) $(PORT) alpha tere tere eestimaa)
	@tmux select-layout tiled
	node server

demo3: demo
	$(call demo_title, Fanout x2 x2, One of two clients sends to two echoes which each send back to both clients)
	$(call run_split, Echo, 1.3, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, Echo, 1.5, ./detail/relay_client_example.out $(HOST) $(PORT) echo)
	$(call run_split, Receive x2, 1.7, ./detail/relay_client_example.out $(HOST) $(PORT) alpha)
	$(call run_split, Receive x2, 1.9, ./detail/relay_client_example.out $(HOST) $(PORT) alpha tere tere eestimaa)
	@tmux select-layout tiled
	node server

demo4: demo
	@echo -e '\e[32;1mWildcard\x1b[22m - Three clients select between four echo targets using wildcards\e[0m'
# TODO: fix why do we need the outer shell and quoted command?  It breaks if using {} or () without being wrapped in another shell
	$(call run_split, Senders, 1.5, sh -c '( \
		./detail/relay_client_example.out $(HOST) $(PORT) red:one 2>&1 | xargs -I{} printf "\e[91mred:one: {}\e[0m\n" & \
		./detail/relay_client_example.out $(HOST) $(PORT) red:two 2>&1 | xargs -I{} printf "\e[31;4mred:two: {}\e[0m\n" & \
		./detail/relay_client_example.out $(HOST) $(PORT) yellow:one 2>&1 | xargs -I{} printf "\e[93myellow:one: {}\e[0m\n" & \
		./detail/relay_client_example.out $(HOST) $(PORT) yellow:two 2>&1 | xargs -I{} printf "\e[33;4myellow:two: {}\e[0m\n" & \
	wait )' )
	$(call run_split, reds, 1, ./detail/relay_client_send_example.out $(HOST) $(PORT) 1 senderA "red:*" DATA "All reds")
	$(call run_split, ones, 1, ./detail/relay_client_send_example.out $(HOST) $(PORT) 1 senderB "*:one" DATA "All ones")
	$(call run_split, *w*, 1, ./detail/relay_client_send_example.out $(HOST) $(PORT) 1 senderC "*w*" DATA "All containing \"w\"")
	$(call run_split, ?e??o*, 1, ./detail/relay_client_send_example.out $(HOST) $(PORT) 1 senderD "?e??o*" DATA "Matching \"?e??o*\"")
	@tmux select-layout tiled
	node server

demo5:
	$(call run_split, Receive nothing, 1, ./detail/relay_client_send_example.out $(HOST) $(PORT) 1 loopy loopy DATA "Do not loopback")
	@tmux select-layout tiled
	node server

deploy:
	npm install
	tar --exclude-vcs --exclude-vcs-ignores --exclude Makefile -cz . | \
		ssh relay@relay.open-cosmos.com 'sudo ~relay/deploy.sh'
