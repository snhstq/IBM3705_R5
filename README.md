# IBM3705_R5
## Release Notes
The main updates with Release 5 of the IBM 3705 SIMH emulator:

1. Remote 3705:  
    The 3705 is now equipped with a (virtual) diskette containing the Remote Program Loader (RPL).  When the 3705 is activated with all channel adapters disabled, it will load the RPL from the diskette.
 	This allows the 3705 to be loaded over an SDLC line. The channela dapters can be disabled through the SIMH config file or via the Control Panel.
	The RPL diskette is based on an Amdahl 4705 diskette.
2. Full Duplex:  
     All The 3705 and all the SDLC components (i3274, DLSw and Trunk) now support full duplex mode. 
3.  NModem has been renamed to Trunk. Trunk was IBM's original name for a SDLC connection between two 3705's. 
4.  Control Panel:  
    Front Panel is renamed to Control Pabel. The panel visualisation has been split from the main 3705 code. This panel visualization component runs in it's own xterm window, ensuring 
    that messages comming out of the 3705 component remian visible. 
    The Control Panel is now equipped with a "Load" button which can be used to restart the 3705 without having to stop and start the emulator. 
	A "disable" setting has been added to the channel A/B switches. By setting all Channel Adapters to disable and pressing the "Load" button, 
	the 3705 loads the diskette wit the Remeote Prgram Loader and therefor becomes aremote 3705. 
4.  RS232 rework:  
    The RS232 emulation has been significantly simplified and made more accurate. The DCE emulation is no longer located in the 3271 and 3274 emulation, but is now kept in the 3705 LIB.
    The RS232 signals are therefor no longer send across the TCP/IP connection to the 3271/3274
6.  Issue Fixes: 
	Various issues have ben resolved, including the annoying issue with i3274  whereby a logon was no longer possible after a logoff.
	
## Installation
### Environments
The 3705 emulator has been tested against the following:   
- Linux for RPi Debian Buster, Bullseye and Bookworm
- Linux for Intel  Debian Bookworm
- gcc version 8.3.0 up to  version 12.2.0
- Hercules version 3.13 and most Hyperion releases up to version 4.6
- Aethra
- MVS3.8j CBIPO install 
- TK4
- TK5
- SIMH 3.11-0
- X3270

### Installing the 3705 emulator
#### Preparing the Linux environment
The 3705 emulator should run on any Linux environment.  Howver, testing has onlly be done on Raspberry Pi and Intel.
The following packages are required: 
1. apt-get install git gcc make 
2. apt-get install libncurses-dev
3. apt-get install libbsd-dev

Download the 3705 emulator package from GitHub:    
git clone https://github.com/snhstq/IBM3705_R5.git
For building the 3705 emulator go to directory IBM3705_R5  

The package comes with the following components:  
1. i3705	This is the actual 3705 emulator. 	**Build instruction: make i3705** 
2. i3274	The 3274 PU type 2 emulator (SDLC). 	**Build instruction: make i3274**
3. i3271	The 3271 cluster emulator (BSC). 	**Build instruction: make i3271**
4. DLSw		Data Link Switch (SDLC).		**Build instruction: make DLSw**
5. Trunk	Interconnection between two 3705's.	**Build instruction: make Trunk**
6. CPanel	The 3705 Control panel.			**Build instruction: make CPanel**

All these makes should end without any issues.
After a make, the executable is stored in the BIN directory.

component 1 is madatory (duh...)
The other components are optional, however, you must at least choose one of the components 2, 3 or 4. 



   

### Preparing the MVS system

## Future updates:
	
Performance improvements  
V24 (DB25) to USB interface  
Windows version  


EF & HJS (C)2025
