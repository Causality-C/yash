CC=gcc
VALGRIND=valgrind
FILENAME=yash

done: 
	$(CC) -o $(FILENAME) $(FILENAME).c -lreadline
	./$(FILENAME)	
leak:
	valgrind ./$(FILENAME)
clean:
	rm $(FILENAME)
	rm *.txt

