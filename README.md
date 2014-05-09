YoNTMA
======

YoNTMA (You'll Never Take Me Alive!) is a tool designed to enhance the protection of data encryption on Windows laptops. A Macintosh version is available [here](https://github.com/iSECPartners/yontma-mac).

How It Works
------------

YoNTMA runs as a Win32 service on BitLocker-protected laptops. If the laptop is disconneted from AC power or wired Ethernet while the screen is locked, YoNTMA puts the system into hibernate. This prevents a laptop thief from accessing your encrypted data later via a DMA attack while the machine is still powered on and the encryption keys are in memory.

How to Run
----------

From an elevated cmd prompt, run:

<pre>yontma -i</pre>

to install and run the YoNTMA service.

If you open services.msc, you will find You'll Never Take Me Alive! (YoNTMA) in the list running as a service.

To tell yontma to install even if it does not detect BitLocker (for example if you use Truecrypt) run:

<pre>yontma -i -f</pre>

Requirements
-------------

The machine must be Windows Vista or above and have BitLocker Drive Encryption enabled on the OS volume (typically drive C:).

If BitLocker is not enabled, yontma must be `--force`-ed on with the `-f` or `--force` option.

Build Instructions
------------------

YoNTMA builds in Visual Studio (as well as the free version, VS Express 2012 for Desktop) and has no outside dependencies. Open yontma.sln to build.

Disclaimer
----------
iSEC has written this tool and provides it to the community free of charge. While iSEC has conducted testing of the tool on different systems, it has not been tested on all models, hardware, or configurations (especially with third-party power management services). The software is being provided "as is" without warranty or support. iSEC does not assume liability for any damage caused by use of this tool.

If you experience issues with YoNTMA, you can uninstall it by entering <pre>yontma -u</pre> from an elevated cmd prompt. If this is not successful, you can follow the [manual removal instructions](https://github.com/iSECPartners/yontma/wiki/Manual-Removal-Instructions) on the wiki.
