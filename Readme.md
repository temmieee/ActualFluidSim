\# Fluid Simulation

This project is a real‑time fluid simulation built with OpenGL 4.6. It uses compute shaders to handle physics and rendering directly on the GPU, so you can watch tens of thousands of particles swirl, collide, and flow like liquid.

\## What it does

\-Physics on the GPU

&#x20; Each particle is a little sphere with position, velocity, and density. Compute shaders update them every frame, applying gravity, collisions, pressure, and viscosity forces. Spatial hashing is used so particles only interact with nearby neighbors.



\-Rendering

&#x20; Instead of drawing spheres one by one, the simulation raymarches through a 3D density texture. This gives you soft, volumetric visuals with scattering, transmittance, reflection, and refraction. Foam is generated when the fluid gets turbulent.



\-Performance

&#x20; The system can handle around 50,000 particles smoothly on modern hardware. Beyond that, physics calculations (not rendering) start to stress the GPU due to hash collisions and heavy neighbor lookups.


Sources:

https://matthias-research.github.io/pages/publications/sca03.pdf

https://web.archive.org/web/20250106201614/http://www.ligum.umontreal.ca/Clavet-2005-PVFS/pvfs.pdf

https://sph-tutorial.physics-simulation.org/pdf/SPH\_Tutorial.pdf

https://web.archive.org/web/20140725014123/https://docs.nvidia.com/cuda/samples/5\_Simulations/particles/doc/particles.pdf

https://developer.download.nvidia.com/presentations/2010/gdc/Direct3D\_Effects.pdf

https://cg.informatik.uni-freiburg.de/publications/2012\_CGI\_sprayFoamBubbles.pdf
https://graphics.cs.utah.edu/research/projects/ipbf/ipbf.pdf
Thank to @SebastianLague and @cem\_yuksel.

