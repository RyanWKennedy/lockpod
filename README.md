# An Electronic Password Safe
This is the code to a portable electronic device I have designed for holding a large number of usernames and passwords in an encrypted database that’s accessed using a single master password.  The purpose of the device is to reduce the stress of having too many passwords to remember.  This device has a microprocessor based architecture with discrete (off-chip) RAM and non-volatile flash storage.  User input/output is conducted through a capacitive touchscreen TFT color LCD.  Text input simply uses an on-screen QWERTY keyboard like on a smartphone.  Not included in this repository, but used for the device is a slightly modified version of an open-source bootloader whose source can be found here:
https://github.com/linux4sam/at91bootstrap

I feel that storing passwords on an isolated system such as this is more secure than storage on a PC or smartphone; because having a relatively small and single purpose code base, as well as no internet connection, reduces the area of potential attack vectors.

## Quick Specs
- Processor: 400MHz ARM core MPU/SoC with LCD controller
- Memory: 128MB DDR2 SDRAM
- Storage: 4MB SPI Flash
- User I/O: 480x272 TFTLCD with parallel RGB comm / Capacitive touchscreen with I2C comm

## Security
A master key based on the user's chosen master password is derived using the <a href="https://tools.ietf.org/html/rfc7914">Scrypt</a> algorithm, and that key is then used to encrypt all data with AES256.  Random number generator seeding is achieved by first requesting that the user tap the screen in a random location upon startup, while in the background a counter is running at many kilohertz.  The seed is then derived as a function of the tap coordinates as well as the count number.  Small differences in the amount of time it takes the user to tap, can produce very different count numbers.  Random numbers are used for IVs/salts, as well as for generating a random keyboard layout for the master password entry.  This prevents touch-wear from leaking information about the password.  See last two screen-shots for what this looks like.

## Screen-shots
<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_home.PNG">

<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_add.PNG">

<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_list.PNG">

<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_view.PNG">

<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_startup.PNG">

<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_locked.PNG">
