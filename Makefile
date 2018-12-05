CXX = g++
CPPFLAGS = -I/home/rpeccatte/lib/opencv3.4/include -std=c++11
LDFLAGS = -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs -lopencv_core -L/home/rpeccatte/lib/opencv3.4/lib -L/usr/lib -lpthread
RM = rm -f

PROG = julia_bw2
SRC = $(PROG:%=%.cpp)
OBJ = $(PROG:%=%.o)
EXTERNALS = palettes.h

all: $(PROG)

$(PROG): $(OBJ)
	$(CXX) $< $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(PROG) $(EXTERNALS)
