TARGET = stmfts
SRC = $(TARGET).c
FLAGS = -g -Wall

$(TARGET):
	cc $(FLAGS) $(SRC) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
