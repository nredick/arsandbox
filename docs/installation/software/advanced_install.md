# Advanced Augmented Reality Sandbox Install

<!-- define abbreviations -->
*[ARSandbox]: Augmented Reality Sandbox

First, you will need to modify the build command shown in [Step 2](./simple_install.md#step-2-build-the-arsandbox).

Start by navigating to the ARSandbox directory:

```sh
cd <AR Sandbox directory>
```

replacing `<AR Sandbox directory>` with the actual directory path as usual.

If you already built the ARSandbox according to [Step 2](./simple_install.md#step-2-build-the-arsandbox), you first have to update its configuration. Enter into the same terminal window:

```sh
make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location> config
```

replacing `<installation location>` with your chosen location, for example `INSTALLDIR=/usr/local`.

Then, build the ARSandbox for installation in your chosen destination by entering:

```sh
make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location>
```

replacing `<installation location>` with the same location as in the previous command. You can add `-j<number of cpus>` to optimize the build process, as described in [Step 2](./simple_install.md#step-2-build-the-arsandbox).

Finally, install the ARSandbox in your chosen location by entering:

```sh
sudo make VRUI_MAKEDIR=<Vrui build system location> INSTALLDIR=<installation location> install
```

again using the same `<installation location>`.
