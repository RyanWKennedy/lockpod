# An Electronic Password Safe
This is the code to a portable electronic device I have designed for holding a large number of usernames and passwords in an encrypted database that’s accessed using a single master password.  The purpose of the device is to reduce the stress of having too many passwords to remember.  This device has a microprocessor based architecture with discrete (off-chip) RAM and non-volatile flash storage.  User input/output is conducted through a capacitive touchscreen TFT color LCD.  Text input simply uses an on-screen QWERTY keyboard like on a smartphone.  Not included in this repository, but used for the device is a slightly modified version of an open-source bootloader whose source can be found here:
https://github.com/linux4sam/at91bootstrap

I feel that storing passwords on an isolated system such as this is more secure than storage on a PC or smartphone; because having a relatively small and single purpose code base, as well as no internet connection, reduces the area of potential attack vectors.

## Screen-shots
<img src="https://github.com/RyanWKennedy/lockpod/blob/master/img/IOSC_home.PNG">
