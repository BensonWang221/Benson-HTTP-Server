target=app
src=$(wildcard ./src/*.cpp ./*.cpp)
obj=$(patsubst %.cpp, %.o, $(src))
flags=-std=c++11 -g -Iinclude

$(target) : $(obj)
	g++ $^ -o $@ $(flags)

%.o:%.cpp
	g++ $< -c -o $@ $(flags)

.PHONY: clean
clean:
	-rm $(obj) $(target) -f