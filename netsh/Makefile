CC=g++

TASK=netsh

all: $(TASK)

$(TASK):
	$(CC) $(TASK).cpp -o $(TASK).o -c
	$(CC) -s $(TASK).o -o $(TASK)

clean:
	rm -f *.o $(TASK)