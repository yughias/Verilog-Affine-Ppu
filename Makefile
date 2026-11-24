VERILATOR = verilator
FLAGS = --Wall --cc --exe --build --public --top ppu
SDL_CFLAGS = $$(pkg-config --cflags sdl2)
SDL_LIBS = $$(pkg-config --libs sdl2)
SRC = verilog/*v
SIMS = simple layer tech_demo editor

all: $(SIMS)

# make <sim>: builds verilator/<sim>.cpp into obj_<sim>/Vppu
$(SIMS):
	$(VERILATOR) $(FLAGS) -Mdir obj_$@ $(SRC) verilator/$@.cpp -CFLAGS "-O2 $(SDL_CFLAGS)" -LDFLAGS "$(SDL_LIBS)"

clean:
	rm -rf obj_*

.PHONY: all clean $(SIMS)
