# Triangle mesh to signed distance field conversion

Source Code for the implementation from my bachelors thesis

## Requirements

- g++ compiler
- assimp library
- glfw library
- *optinal: python with matplotlib* 

## Build

To build the project run `make compile`.

To optimize for your current architecture use `make performance`.

After compilation, use `make clean` to prepare the required folder structure.

## Execute

The program can be run in two modes. The default mode is the model view mode.

Run it with `./a.out`.

You can optionally pass parameters to adjust the size of the signed distance field using `./a.out --sdfsize <size>`.

Inside the program, use the number keys to switch between different conversion methods. Start typing to access the in-programm terminal. Type `help` for a quick overview of the available commands.

## Benchmarking

The program can also benchmark the different algorithms. To start the benchmark mode, run `./a.out --benchmark`. This will execute the algorithms on the built-in models.

To get better results, download the [Thingy10k Dataset](https://ten-thousand-models.appspot.com/) and place it into the `./Objs/_thingy10k` folder. The folder structure then should be `./Objs/_thingy10k/raw_meshes/*.stl`.

Use `./a.out --thingy10k` to test the algorithms on the large dataset. As this can take a lot of time, you can also choose to only test a subset. Use `./a.out --thingy10k --testsize <count>` where `<count>` is the amount of models selected from the Thingy10k dataset.

After the program has finished, a `benchmark_results.json` is created in the `./output/` folder.

Use `python plot.py` to generate some visualisations from the data.

## Demo

A particle simulation has been implemented to test real-time performance. Check out the [particles](https://github.com/Ungetoastet/mesh2sdf/tree/particles) branch or [this video](https://youtu.be/JpyzwV0IOsw?si=0scliGlMmfCziREG).

