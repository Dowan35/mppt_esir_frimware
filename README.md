Base project for STM32F0 discovery board and MPPT TP custom board
=================================================================

About
-----

This software base can be used to test programs on a STM32F0Discovery board.

Soon it will also be possible to use it to write software for the MPPT TP custom
board at ESIR, University of Rennes.

Credits
-------

This project was derived from STM32-base project
<https://github.com/STM32-base/STM32-base>

It is also based on STM32 Cube.
<https://github.com/STMicroelectronics/STM32CubeF0>
The parts of STM32 Cube that have been copied here for convenience have their
own licences. See each sub-project separately to learn more about its licence.

Technical information
---------------------

On the board, microcontroller is STM32F051R8T6.

License
-------

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.

How to
------

It is assumed that this project will be compiled and uploaded to the device from
a GNU/Linux environment.

To build the project, the tools `make`, and GCC for ARM are required.

On a Debian based system, they can be installed with:

```
sudo apt-get install make gcc-arm-none-eabi
```

Once the project has been successfully built, it can be uploaded to the
microcontroller with:

```
make upload
```

For this command to work, the Open OCD tool needs to be installed. The package
name for this program should be `openocd`.

Depending on the version of Open OCD that is used, the receipe of the upload
command may need to be changed. The older version of the call to Open OCD should
be commented out.





