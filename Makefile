myshell: src/main.c
	gcc -Wall -Wextra -g -o $@ $^

clean:
	rm -f shell
