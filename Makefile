sources := $(wildcard *.c ctcp/socket.c ctcp/select.c ctcp/linked_list.c)

.PHONY: clean

client.out: $(sources)
	gcc -std=gnu99 -Og -lpthread -o $@ $^

clean:
	rm -f -- *.out
