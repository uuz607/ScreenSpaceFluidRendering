# Screen Space Fluid Rendering

Real-time fluid simulation and rendering using Vulkan and screen-space surface reconstruction.


## Overview

This project implements a real-time fluid simulation and rendering system based on:

- **Vulkan API**
- **Position-Based Fluids (PBF)**
- **Screen-Space Fluid rendering**
- **Compute Shader-based workload distribution**
- **Asynchronous Compute**

The goal is to build a modern, modular rendering framework capable of simulating and visualizing fluid surfaces efficiently on the GPU.


## Requirements
- Vulkan SDK (1.2+)
- CMake 3.20+
- Windows / Linux
- Vulkan-compatible GPU

## Demo

<p align="center">
  <img src="Assets/FluidRendering.gif" width="300" height="200"/>
  <br/>
  Fluid rendering with PBF simulation
</p>

## References 

- Miles Macklin and Matthias Müller. 2013. Position based fluids. ACM Trans. Graph. 32, 4, Article 104 (July 2013), 12 pages. https://doi.org/10.1145/2461912.2461984
- Wladimir J. van der Laan, Simon Green, and Miguel Sainz. 2009. Screen space fluid rendering with curvature flow. In Proceedings of the 2009 symposium on Interactive 3D graphics and games (I3D '09). Association for Computing Machinery, New York, NY, USA, 91–98. https://doi.org/10.1145/1507149.1507164

## Acknowledgement
This project is based on or inspired by:
- Vulkan examples https://github.com/SaschaWillems/Vulkan
- PositionBasedFluids https://github.com/kuriishi/PositionBasedFluids

## License

The code is released under the MIT license. See the `LICENSE` file for the details.
