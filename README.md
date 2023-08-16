Profiler
===========

![Profiler.lv2](https://github.com/brummer10/Profiler.lv2/blob/main/profiler.png?raw=true)

## 

A Profiler plug to make the process of cloning external soft/hardware a bit more comfortable. 

Features a round trip measurement routine. 
This allows to load the plug, connect the output to the system output, 
loop over external gear (soft or hardware) and back to the Profiler input.

Simply press "Profile" to play the "input.wav" file to the output and record the returning input 
delayed by the measured round trip latency. 

The round-trip latency will be measured on each "Profile" start. 

Resulting recorded "target.wav" file will be perfectly in sync with the used "input.wav" file. 
Currently, both files would be saved under "$(HOMEy)/profiles/". 
The "target.wav" file will be overwritten on each Profile run, so there will be always only one target file.
You need to download it from the device in order to use it with the AIDA-X trainer.

The "input.wav" file comes as resource with the plug and get copied over to that folder, 
when no input.wav file was found there. 
This allows advanced users to use their own input.wav file by simply replace the one in that folder. 

The target.wav file get checked during record and run to a normalisation function when needed. 
(Only when the max peek in target is above the max peek in input). 

The record will be saved in the PCM24 wav format (same as the input.wav file).

The UI provide a progress bar, a vu-meter, a clipping indicator and, well the profile button.



## build:
- no build dependency check, just make
- libsndfile is needed

## install:
- git submodule init
- git submodule update
- make
- make install # will install into ~/.lv2 ... AND/OR....
- sudo make install # will install into /usr/lib/lv2
