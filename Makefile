main:
	gcc --std=gnu99 -o smallsh main.c

gdb:
	gcc --std=gnu99 -g main.c -o testmain

valgrind:
	gcc --std=gnu99 -o leaky -g main.c
	valgrind --leak-check=yes --show-reachable=yes ./leaky 

git:
	git add -u
	git commit -m "updated files"
	git push

test:
	./p3testscript 2>&1

test1:
	./p3testscript > mytestresults 2>&1

ps:
	ps -o ppid,pid,pgid,sid,euser,stat,%cpu,rss,args | head -n 1; ps -eH -o ppid,pid,pgid,sid,euser,stat,%cpu,rss,args | grep klemzcea

clean:
	rm -f smallsh
	rm -f main
	rm -f testmain
	rm -f leaky
	clear
