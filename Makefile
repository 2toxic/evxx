CXX=g++
CFLAGS=-Wall -Wextra -Wshadow -pedantic -Wformat-security
CXXFLAGS=$(CFLAGS) --std=gnu++11
CXXLINK=-lstdc++
COMMIT_STR=$(shell printf "\\\\\"%s\\\\\"" $$(git rev-parse --short HEAD))

evx: evx.o util.o repo.o
	$(CXX) -o evx evx.o util.o repo.o $(CXXLINK)

evx.o: evx.cc evx.hh util.hh repo.hh
	$(CXX) $(CXXFLAGS) -c -o evx.o evx.cc -DEV_COMMIT=$(COMMIT_STR)

util.o: util.hh util.cc
	$(CXX) $(CXXFLAGS) -c -o util.o util.cc

repo.o: repo.hh repo.cc
	$(CXX) $(CXXFLAGS) -c -o repo.o repo.cc

clean:
	rm -f util.o repo.o evx.o evx
