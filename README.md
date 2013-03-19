YoNTMA
======

YoNTMA (You'll Never Take Me Alive!) is a tool designed to enhance the protection of data encryption on laptops.

How It Works
------------

YoNTMA runs as a Win32 service on BitLocker-protected laptops. If the laptop is disconneted from AC power or wired Ethernet while the screen is locked, YoNTMA puts the system into hibernate. This prevents a laptop thief from accessing your encrypted data later via a DMA attack while the machine is still powered on and the encryption keys are in memory.

Binaries
--------

YoNTMA is available for Windows in both x86 and x64 flavors.

* [YoNTMA (Windows x86)](https://s3.amazonaws.com/yontma/v1.0/x86/yontma.exe) (MD5: 50c352f48b90f5968a37d9fcb1209002 / SHA1: f1384ec049e56b2563dd6a811d90909450b2ed91)
* [YoNTMA (Windows x64)](https://s3.amazonaws.com/yontma/v1.0/x64/yontma.exe) (MD5: 0d99913b3cc0119d19955624b8648b9d / SHA1: 2882f8efbe6c033790f740bae161339e561d6062)

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

Disclaimer
----------
iSEC has written this tool and provides it to the community free of charge. While iSEC has conducted testing of the tool on different systems, it has not been tested on all models, hardware, or configurations (especially with third-party power management services). The software is being provided "as is" without warranty or support. iSEC does not assume liability for any damage caused by use of this tool.

If you experience issues with YoNTMA, you can uninstall it by entering <pre>yontma -u</pre> from an elevated cmd prompt. If this is not successful, you can follow the [manual removal instructions](https://github.com/iSECPartners/yontma/wiki/Manual-Removal-Instructions) on the wiki.