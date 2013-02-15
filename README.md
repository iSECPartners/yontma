yontma
======

yontma (You'll Never Take Me Alive!) is a tool designed to protect data on laptops from being stolen.

How It Works
------------

yontma runs as a Win32 service on BitLocker-protected laptops. If the laptop is disconneted from AC power or wired Ethernet while the screen is locked, yontma puts the machine into hibernate. This prevents a laptop thief from accessing your encrypted data later via a DMA attack while the machine is still powered on and the decryption keys are in memory.

How to Use It
-------------

Clone the repo and open yontma.sln in Visual Studio (or the free VS Express Desktop 2012). Build yontma.exe and then from an elevated cmd prompt, run: 

<pre>yontma -i</pre>

to install and run the yontma service. If you open services.msc, you will find YoNTMA in the list running as a service.