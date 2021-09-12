CC=gcc
VALGRIND=valgrind
FILENAME=yash

done: 
	$(CC) -o $(FILENAME) $(FILENAME).c -lreadline
leak:
	valgrind ./$(FILENAME)
clean:
	rm $(FILENAME)
	rm *.txt

