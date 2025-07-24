# A note on water simulation

Without the real-time water simulation, the Augmented Reality Sandbox has very reasonable hardware requirements. Any current PC with any current graphics card should be able to run it smoothly.

The water simulation, on the other hand, places extreme load even on high-end current hardware. We therefore **recommend to turn off** the water simulation, using the `-ws 0.0 0` command line option to SARndbox, or reducing its resolution using the `-wts <width> <height>` command line option with small sizes, e.g., `-wts 200 150`, for initial testing or unless the PC running the Augmented Reality Sandbox has a top-of-the line CPU, a high-end gaming graphics card, e.g., an Nvidia GeForce 970 and the vendor-supplied proprietary drivers for that graphics card.