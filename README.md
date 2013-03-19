YoNTMA
======

YoNTMA (You'll Never Take Me Alive!) is a tool designed to enhance the protection of data encryption on laptops.

How It Works
------------

YoNTMA runs as a Win32 service on BitLocker-protected laptops. If the laptop is disconneted from AC power or wired Ethernet while the screen is locked, YoNTMA puts the system into hibernate. This prevents a laptop thief from accessing your encrypted data later via a DMA attack while the machine is still powered on and the encryption keys are in memory.

Binaries
--------

YoNTMA is available for Windows in both x86 and x64 flavors.

* [YoNTMA (Windows x86)](https://s3.amazonaws.com/yontma/v1.0/x86/yontma.exe) (MD5: e2289358ec932ac40557f0456ab646fd / SHA1: 7a701ad3ab295404b6715264711de15171f84aa5)
* [YoNTMA (Windows x64)](https://s3.amazonaws.com/yontma/v1.0/x64/yontma.exe) (MD5: 59a16dacebe638f161875a8e3eb6fa95 / SHA1: c18be7acfcef33ef7ff8e8e91bc75e698288d564)

How to Run
----------

From an elevated cmd prompt, run: 

<pre>yontma -i</pre>

to install and run the YoNTMA service.

If you open services.msc, you will find You'll Never Take Me Alive! (YoNTMA) in the list running as a service.

Requirements
-------------

The machine must be Windows Vista or above and have BitLocker Drive Encryption enabled on the OS volume (typically drive C:).

Build Instructions
------------------

YoNTMA builds in Visual Studio (as well as the free version, VS Express 2012 for Desktop) and has no outside dependencies. Open yontma.sln to build.