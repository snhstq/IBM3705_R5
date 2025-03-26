# IBM3705_R5
## About the 3705 emulator project
This project aims at delivering an IBM 3705-II emulator, specifically designed to connect to the Hercules mainframe emulator.  
The 3705 emulator allows running  IBM NCP software up to NCP version 3 (The last NCP version supporting a 3705-II). The package also includes a 3274 PU T2 emulator as well as a 3271 Cluster emulator. This enables 3270 emulation software, like x3270 or tn3270,  to connect to the 3705 via using the SLDC or BSC protocol. In addtion, it is even possible to connect actual IBM hardware to the 3705 emulator using a Data Link Switch. 
  
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
Please make sure to review the issues section of the GitHub project.   
  
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

Below picture shows the high-level architecture with all components.  
![High Level architecture of the 3705 emulator](/Images/Overview.png)


### starting the 3705
The 3705 may be started before of after Hercules is started. The (TCP/IP) channel conection is made as soon as both ends are available. As such, the 3705 can be stopped and started at any point without the need to restart Hercules.
The 3705 is started from directory SIMH_3705_R5 with the command:

./BIN/i3705 3705-128k.cnf 

3705-128k.cnf is the configuration file for a 128k channel attached 3705. Other config files available are: 3705-64k.cnf (64K 3705) and 3705-256k.cnf (256K 3705).  
Similary, there are 3 configuration files for a remote 3705: 3705r-64k.cnf, 3705r-128k.cnf and 3705r-256k.cnf. 

After the start command the following messages appear:
```
CS-T2: Thread 21095 started succesfully...
CS-T2: Scanner initialized with 4 lines...
CA-T2: Main thread 21094 started succesfully...
PNL: Thread 21096 started succesfully... 
LIB: Thread 21097 started succesfully...
CA: Adapter thread 21093 started sucessfully... 
CA1: Waiting for channel connection on TCP port 37051 
CA2: Waiting for channel connection on TCP port 37053 
LIB: Using TCP network Address 192.168.2.71 on eth0 for 327x connections
LIB: Line-0 ready, waiting for connection on TCP port 37520
LIB: Line-1 ready, waiting for connection on TCP port 37521
LIB: Line-2 ready, waiting for connection on TCP port 37522
LIB: Line-3 ready, waiting for connection on TCP port 37523
CPU: Reset... 
CPU: MEMORYSIZE 256K bytes 

IBM 3705 II simulator V3.11-0
CPU: Reset... 
CPU: MEMORYSIZE 128K bytes 
CPU: checking CA1: status = 0 
CPU: CA1  enabled
CPU: Loading MaxiROS at X'00000'
CA1: New bus connection on 3705 port 37051, socket fd is 20, ip is : 192.168.2.16, port : 49879 
CA1: New tag connection on 3705 port 37051, socket fd is 21, ip is : 192.168.2.16, port : 49880 
CA1: Connected to device 0660
```
The last message indicates a succesful connection to Hercules device 660.   
The 3705 is now ready to be loaded with an NCP.   


### Preparing the MVS system
This section is generic. It applies to a CPIPO build of MVS, TK5 or whatever other ways the MVS system has been created.    
Copy file ‘ncpssp.3350’ for the 3705 emulator package ‘Hercules Files’ directory to your Hercules/hyperion dasd folder.

Update the Hercules cnf file  and add ncpssp.3350 to the dasd configuration.
Be sure to select an address that is generated as a 3350.
For TK5 the address range 240-24F are 3350’s.  Below 244 is used.
...

0244 3350 dasd/ncpssp.3350&emsp;&emsp;&emsp<=== Added
...

Now add the 3705 to the hercules configuration file. Make sure to use an address that is generated as a 3705.
For TK5 this is 660. Make sure to comment out the existing definition for device 660. For TK5 the updates should be made to tk5_default.cnf:  
```
#  
# NCP VTAM  
#  
0660 3705 adaptip=192.168.1.05 port=37051  <=== Added. 192.168.1.05 should be replaced with the IP address of your 3705 host.   
#0660 3705 lport=${N660PORT:=37051} locncpnm=N07 rmtncpnm=N08 unitsz=252 ackspeed=1000  
0661 3705 lport=${N661PORT:=37052} locncpnm=N10 rmtncpnm=N11 idblk=017 idnum=00018 locsuba=10 rmtsuba=11 unitsz=252 ackspeed=1000  
0662 3705 lport=${N662PORT:=37053} locncpnm=N12 rmtncpnm=N13 idblk=017 idnum=00019 locsuba=12 rmtsuba=13 unitsz=252 ackspeed=1000  
0663 3705 lport=${N663PORT:=37054} locncpnm=N14 rmtncpnm=N15 idblk=017 idnum=0001a locsuba=14 rmtsuba=15 unitsz=252 ackspeed=1000  
#  
```
Note: comm3705 will always display informational (CCxxxnnI) and error (CCxxxnnE) messages. When debug=yes is specified in the above configuration statement for device 660,  all Debug (CCxxxnnD) messages will be displayed too.  
Adding tracesna=yes in will display the translated SNA command’s that are sent/received.  
With standard Hercules command ‘t+ cua’ (e.g. t+ 660) you can activate the CCW trace and ‘t- 660’ will disabled it again.   

Adding tracesna=yes in de Hercules ‘conf/tk4-default’ file will display the translated SNA command’s that are sent/received.



Below table gives an overview of the IP port usage by the 3705:
| 37005 Channel Adapter | Channel Switch | IP Poert |
| --------------------- | -------------- | ---------|
|           1           |   A position   |   37501  |
|           1           |   B position   |   37503  |
|           2           |   A position   |   37504  |
|           2           |   B position   |   37505  |


The 3705 can handle 2 channel adapters, each consisting of two cannels "A" and "B".    
On a MVS3.8 system, with VTAM L2 and the NCP version supplied on volume NCPSSP, only 1 channel can be active at any one time.  
The "B" channel of the 1st channel adapter was intended as a backup in case  the "A" channel failed.   
The 2nd channel adapter was intended to be used as a connection to a backup host system. Like with the 1st channel adapter, the "B" channel was the backup for the "A" channel.  
Higher version of VTAM and NCP provide multi-channel support. When these are used, the 3705 emulator can connect to two hosts (Channel adapteer 1 to one host, the 2nd adapter to the other).    

### Starting Hercules
After the config changes have been made to Hercules, it can be started. You will see the following messages on the Hercules screen:  
CCTAG002D 1:0660: Preparing connection with remote channel adapter  
CCBUS019I 1:0660: Waiting for bus(49) connection to be established  
CCBUS019I 1:0660: Waiting for tag(50) connection to be established  
CCTAG003I 1:0660: tag connection established on socket 50  
CCBUS003I 1:0660: bus connection established on socket 49  
CCTAG019I 1:0660: connections on port 37051; Bus socket: 49, Tag socket: 50  

### IPL MVS
IPL MVS. After IPL completion 
Check that the 3705 device address is online in MVS:  
```
 d u,,,660,1                                                   
 IEE450I 09.34.55 UNIT STATUS         FRAME LAST         F      E     1A    
 UNIT TYPE STATUS  VOLSER VOLSTATE                                          
 660  3705 O                                                                
```
  
 
The MVS system can now be IPL'ed.  
### Catalog datasets
First catalog the following datasets which are on volume NCPSSP:  
- SYS1.GEN3705
- SYS1.MAC3705
- SYS1.NCPOBJ1
- SYS1.NCPSAMP
- SYS1.NCPSTG1
- SYS1.OBJ3705
- SYS1.SSPLIB  
  SYS1.NCPLOAD is also on volume NCPSSP, but no longer used. So it does not have to be cataloged.
   
### Update SYS1.PARMLIB
Make updates to the following SYS1.PARMLIB members. xx is to be replaced with the suffix of the member used during IPL.  
- LNKLSTxx : Add SYS1.SSPLIB
- VATLSTxx : Add  NCPSSP,0,2,3350&emsp;&emsp;&emsp;,N 

### Replace IFLOADRN (TK4, TK5)  
The IFLOADRN used by TK amd TK5 is a special version for loading fake IBM 3705’s.  
Remove this module by deleting it from SYS1.LINKLIB.	

The original module is in dataset SYS1.SSPLIB, which has been added to the linklist and will be used from now on.   

Note: the old IFLOADRN version is now not available anymore, meaning that it is no longer possibe to load the fake NCP's. 

Shutdown MVS and Re-IPL MVS with all these updates.

### Generating and loading the NCP
SYS1.NCPSAMP contains member NCPGEN, which is used to generated the stage1 deck for the NCP's

The sample NCP N16A contains:  
- 1 SDLC line L16A20
- 1 PU T2 P16A20
- 2 LU's T16A22A1 and T16A22A2
- 1 BSC line L16A23		
- 1 Cluster P16A23A
- 1 terminal T16A23A1

Before of after the NCP has been loaded i3274 and/or i3271 can be connected to the local and/or remote 3705:   
BIN/i3274 -cchn efoxcc1 -line 20       
BIN/i3271 -cchn efoxcc1 -line 23  
In the above example efoxcc1 is the channel attached (local) 3705.     

Copy the sample NCP N16A from SYS1.NCPSAMP to SYS1.VTAMLST  
Make sure the stage1 SYSIN DD card points to SYS1.VTAMLST the desired member (N16A in this case).  

Run job NCPGEN. It should end with RC=0 fro both steps.  
Job NPGEN created the stage1 deck in dataset SYS1.NCPSTG1 on volume NCPSSP  

Edit this member end go to the last step in the deck (is either step s16 or s17). Change the DISP field of the SYSLMOD statement to DISP=SHR (otherwise VTAM has to be stopped to allow the job to allocate the dataset).   
Submit the stage1 deck. the various steps end with either RC=0 or RC=4. Any higher return code indicates an issue  .   


Load the generated NCP into the IBM 3705  

v net,act,id=N16A             
```
    STC  439  IST097I  VARY     ACCEPTED
    STC  439  IST197I  SAVED CONFIGURATION N16A  READ FROM VTAMOBJ                                 
    STC  439  IST270I  370X N16A  NOW LOADED WITH LOADMOD N16A         
    STC  439  IST093I  N16A  ACTIVE                                       
   ```
After IST093I the PU, LU's and/or Cluster and Terminal can be activated.

Activation of the SDLC PU and LU:  
v net,act,id=P16A20A   
v net,act,id=T16A20A1  
v net,act,id=T16A20A2 (for the 2nd LU).

Activation of the BSC cluster and Terminal:  
v net,act,id=P16A23A  
v net,act,id=T16A23A1  

If i3274 or i3271 is started after the NCP has been loaded, the related line needs to be activated first. Note that in that case you would have seen een I/O errer on the line (IST631).  

				
## Operation and Use
### 3705 Control Panel
Open a new X-terminal(on the same system that was used to start the 3705 Emulator). And go to directory SIMH_3705_R5.   
To start the Control Panel, enter: BIN/CPanel

The 3705 control panel should now appear:  
  
![LIB panel](/Images/ControlPanel.png) 
  
The panel shows at the top right hand:  
- MEMORY SIZE : 128K	This is the 3705 memory size, taken from the cnf file.   
- IPL PHASE : 0	 This is the current IPL phase. Will range from 0 (not IPLéd) to 3 (NCP loaded).  
- FREE BUFFERS: 718	The available buffers for the NCP. Before NCP is loaded this will show 0. During NCP operation the value will fluctuate.  
- CYCLE COUNT : nnnn	Shows the content of the cycle utilization count register. Every 8 instructions this counter is incremented. It is a 15 bit register, which will wrap around after the max value is reached.  

The top center shows the DISPLAY A and DISPLAY B registers. On a real 3705 panel these are shown as individual bits. As this would clutter the emulator panel, it is shown as five hexadecimal characters: x xx xx.    
The error indicators are listed separately at the top left (box CCU CHECKS).  If an error occurs, a red “light” will flash after the relevant check.  

In the center of the panel the HEX switches are show. They are labeled A – E. The actual switches are depicted as single digits. A switch can be selected by the left or right cursor keys. The select switch will be highlighted. The value can be changed with the up and down cursor keys.   

Below the HEX switches, the DISPLAY FUNCYION SELECT switch is shown, with 10 possible settings. The default is STATUS. The switch can be “turned” by pressing PF 3 key. The switch turns clockwise. The DISPLAY FUNCTION switch is described in more detail below.   

At the bottom left corner, the Channel Adapter switches are shown. This allows to switch a channel adapter from position A, Disable and B. Each channel adapter can be connected to two hosts. A to one host, B to another. In case of a failure of the active host, the 3705 can be enable for the backup-host by switching the relevant channel adapter to “B”. In case the failing host is recovered, the channel adapter can be switched back to “A”.  
Switching channel adapter 1 is done via PF 1 key, for channel adapter 2, use PF 2. The PF 1 and PF 2 key simulate the 3 position switch on the real 3705 Control Panel. I.e. The switch goes from the upper position (A), to the center position (Disable both A and B) to the bottom position (B). From the bottom position, the switch goes the other way.   
The headers “Channel Adapter 1” and “Channel Adapter 2” are show in high-light when a TCP/IP connection exists. Note that this not mean that the channel Adapter is in use! E.g. the above screenshot shows a TCP/IP connection for Channel Adapters 1 and 2, but only channel adapter 1 is in use.
An active channel Adapter is shown as “ACTIVE”, a connected, but not active adapter is shown as “ENABLED”, a not connected adapter is show as “DISABLED”.  
In the context of the 3705 emulator, a connected adaptor is one with a TCP/IP connection to Hercules. If that connection is actually online, it is shown as “ACTIVE”, else it is “ENABLED”  

Warning: Switching a channel adapter is immediate. If the (3705) unit is still online while switching, various I/O related errors will occur. An IPL might be needed to recover from this situation. So before switching, make sure the unit is offline.  

The DISPLAY FUNCTION SELECT:
switch PF 3 changes the switch. The current selection is highlighted. The selections are:   
- STATUS: This shows the current 3705 status in the A and B DISPLAY. If there is a CCU check, a red light will appear in the CCU CHECKS box. During normal operation the display will be empty.
- STORAGE ADDRESS: This can be used to display the contents of a 3705-storage location. Enter the address using the HEX switches A-F. If a valid address is entered, the address will be shown in DISPLAY A, the contents in DISPLAY B. If an invalid address is set, the ADDRESS EXCEPT “light” will go on.
- REGISTER ADDRESS: This can be used to show the contents of one of the 3705 (input) registers. When this function is selected, HEX switch B and D will be highlighted. These can be used to enter the register address; the other switches cannot be used. The switch settings are shown in DISPLAY A, the high-order bits of byte 0 and 1. The content of the register is show in DISPLAY B, bytes 0 and 1.
- FUNCTION 1: Not yet implemented
- FUNCTION 2: Not yet implemented
- FUNCTION 3: Not yet implemented
- FUNCTION 4: Not yet implemented
- FUNCTION 5: Not yet implemented
- FUNCTION 6: Not yet implemented
- TAR&OP REGISTER: Not yet implemented.
  
The LOAD switch
switch PF 4 is used to reboot the 3705. After pressing PF4 the NCP must be reloaded. If both channel adapters are fully disabled (so both A and B are disabled) the 3705 will load the remote program loader programs from the (virtual) diskette.
The below image shows the Control Panel channel section for a remote 3705:  
  
![LIB panel](/Images/CAs.png)   

The Control panel is updated after pressing any key, except the Home key.  

Exiting the Control panel: Press the Home key.  

### LIB panel
The Line Interface Base (LIB) is the one place where all lines connect to.Each stand-alone emulator (3271,3274, DLSw, Trunk) has the -line switch as a mandatory start parameter to identify the line to which the connection should be made. Node that this line number must correspond to the line number in the NCP definition for the device being connected.   

A successful connection is identified by the message “LIB: 327x connected to line-xx”, whew xx is the line number.  

The LIB and Scanner will manage the RS232 signals. The LIB comes with a panel that shows the RS232 signals in real time.  

The panel can be activated by press “Ctrl-E” at the SIMH 3705 terminal.   
The SIM prompt “sim>” should appear. Now enter:  

sim> d shwlib 1  

The sim prompt reappears. Now enter:  

sim> c   

The LIB panel should now appear:   
![LIB panel](/Images/LIBpanel.png)    
  
The LIB panel is dynamically build based on the number of lines defined in the 3705 (Default is 4).  
The above display shows Line 20, connected, active to VTAM and in session (RTS is high).  
Line 21 is active for VTAM (DTR high), but not connected. Therefore, this must be a switched line.  
Line 22 shows connected (DSR and RI), but not active for VTAM.  
Line 23 is not connected and not active for VTAM.  
  
### 3274
The i3274 component is an SDLC multi-drop IBM 3274 emulator. The default configuration consists of two 3274's, each capable of connection with 4 3270 emulators (like x3270 or tn3270).  
The 3274 is started from directory SIMH_3705_R5:
./BIN/i3274 -cchn efoxcc1 -line 20	
-cchn is the hostname running your 3705. Line 20 is the SDLC line defined in the NCP. Instead of -cchn the IP address of your 3705 host can be specified with -ccip.   
The 3274 can also run in Full Duplex mode. In that case the -fdx switch is to be used. See section [Full Duplex](#full-duplex) for more details on Full Duplex.    
The following messages will appear:
   
```
PU2: Connection to be established with 3705 SDLC line at host efoxcc1
PU2: Connection to be established with SDLC line 20
PU2: Waiting for SDLC Line 20 connection to be established
PU2: SDLC Line 20 connection has been established
PU2: Using network Address 192.168.2.71 on eth0 for 3270 connections
PU2: 3274-0 IML ready. TN3270 can connect to port 32741 
PU2: 3274-1 IML ready. TN3270 can connect to port 32742
```

Connect your TN3270 client to the 3274's IP address with port 32741  (or 32742 for the 2nd 3274).
Logon to TSO:   
Press [RESET], followed by [CLEAR] and then again press [RESET].     
Now type: Logon applid(tso) logmode(mhp3278e)     
press [SYSREQ]  (not [ENTER] !!!)  

NB: 3270 emulatures allow to specify the LU name. These allow to conect to a specific LU. So, if you want to connect to the 2nd LU, specify LU 01 as the LU name in the 3274 emulator.   
By default there are 4 LU's defined. so 03 is the highest. If you do not specyfy a LU name, the 3274 will select the first one available.   

### 3271
The i3271 component is an BSC  multi-drop IBM 3271 emulator. The default configuration consists of two 3271's, each capable of connection with 4 3270 emulators (like x3270 or tn3270).  
The 3271 is started from directory SIMH_3705_R5:
./BIN/i3271 -cchn efoxcc1 -line 20	
-cchn is the hostname running your 3705. Line 20 is the BSC line defined in the NCP. Instead of -cchn the IP address of your 3705 host can be specified with -ccip.  
   
The following messages will appear:
```
CLU: Connection to be established with 3705 BSC line at host efoxcc1
CLU: Connection to be established with BSC line 20
CLU: Waiting for BSC Line  20 connection to be established
CLU: BSC Line 20 connection has been established
CLU: Using network Address 192.168.2.71 on eth0 for 3270 connections
CLU: 3271-0 IML ready. TN3270 can connect to port 32711 
CLU: 3271-1 IML ready. TN3270 can connect to port 32712 	
```
Connect your TN3270 client to the 3274's IP address with port 32711 (or 32712 for the 2nd cluster).  
You should shee the NETSOL welcome message.  
Logon to TSO:  
Logon HERC01  
press ENTER 

NB: 3270 emulatures allow to specify the LU name. These allow to conect to a specific yerminal. So, if you want to connect to the 2nd terminal, specify LU 01 as the terminal name in the 3271 emulator.   
By default there are 4 LU's defined. so 03 is the highest. If you do not specyfy a LU name, the 3271 will select the first one available.  

### DLSw
With DLSw you can connect a real SDLC device to i3705. You will need a real DLSw router to which the SDLC device is connected.  

The Data Link Switch (DLSw) emulator connects to the real DLSw router and to a line of the 3705 emulator.  
The real DLSw route needs to be configured properly. The config parameters relevant to the DLSw connection are (for a Cisco 2800 router):  
| Configuration Parameter | Description |
|--------------------------------------|-------------------------------------------|
| dlsw local-peer peer-id 192.168.2.91 | This is the IP address of the DLSw router |
| dlsw remote-peer 0 tcp 192.168.2.72  | This is the ip address of the host running the DLSw emulator |
| interface FastEthernet0/0 | Ethernet interface to be used |
| ip address 192.168.2.91 255.255.255.0 | IP address and netmask of the ethernet interface |
| interface Serial0/0/0  | Serial interface to which the SDLC device is attached |
| encapsulation sdlc | SDLC frame encapsulation |
| clock rate 9600| Baud rate between the DSLw router |
| sdlc role primary| DLSw router plays role of primary SDLC device |
| sdlc vmac 4000.0999.0100 | Arbitrary (virtual) MAC address associated with the SDLC device. It is not used.|
| sdlc address C1 | Address of SDLC PU connected to the DLSw router |
| sdlc xid C1 01700018 | XID of the SDLC PU. Must match VTAM switched node definition. |
| sdlc partner 4000.1020.1000 | The MAC address of the 3705. Not used. |
| sdlc dlsw C1	| Enable DLSw for this SDLC address. |




Starting DLSw:  

BIN/DLSw -peerip 192.168.2.91 -cchn efoxcc2 -line 20   

This connects DLSw to line 20 of the i3705 on host efoxcc2 and it connects to a real DLSw route which has ip address 192.168.2.91   

Instead of a hostname, an IP address can be specified with switch -cchip.  

When all goes well the following messages appear:  

DLSw: Connection to be established with peer DLSw at ip address 192.168.2.91  
DLSw: Connection to be established with SLDC line at 3705 on host efoxcc2  
DLSw: Connection to be established with SDLC line 20
DLSw: state DISCONNECTED  
DLSw: Waiting for SDLC line connection to be established  
DLSw: DLSw ready, waiting for connection on TCP port 2065  
DLSw: Waiting for DLSw peer outbound connection to be established  
DLSw: Outbound connection to peer has been established  
DLSw: SDLC line connection has been established  
DLSw: Inbound connection from peer DLSw at 192.168.2.91  
DLSw: state CIRCUIT_START  
DLSw: state CIRCUIT_START  
DLSw: state CIRCUIT_ESTABISHED  
DLSw: state CONNECT_PENDING  
DLSw: state CONNECTED  


The key message is the last one: “state CONNECTED” this means the end-to-end connectivity is established and the DLSw’s and the SDLC device are ready.  
In effect, the connected state will allow the 3705 to send data across the line and DLSw’s to the SDLC device. 

DLSw can be terminated with “Ctrl C”.

DLSw can als be started in Full Duplex mode using the -fdx switch. Of course, the NCP line defintion must be defined as full duplex as well. The -line switch must specifie the lower line address of the two defined line addresses.

See section [Full Duplex](#full-duplex) for more details on Full Duplex.  

### Trunk
Trunk  is used in an MVS3.8 environment to connect a local, channel attached 3705 to a remote 3705’s together.   
In case of systems with a higher release of VTAM and NCP, Trunk can also be used to create cross-domain links between multiple hosts by connecting e.g. two local, channel attached 3705’s.

Starting Trunk:  


BIN/Trunk -cchn1 efoxcc1 -cchn2 efoxcc3 -line1 20 -line2 20 -fdx

This connects one end of Trunk to line 20 and 21 of i3705 on host efoxcc1, the other line is connected to line 20 and line 21 of i3705 on host efoxcc3.  
Note: When using Trunk to connect to a remote 3705, the -fdx (Full Duplex) switch is mandatory. The remote 3705 “diskette” has been configured such that it expects to be loaded over a full duplex connection.  

Switches -cchn1 and -cchn2 specify the hostnames of where i3705 are running.
Instead of a hostname, an IP address can be specified with switch -cchip1 or -cchip2.
Switch -line1 specifies the line number on the first 3705, switch -line2 is for the line number on the second 3705.   
In a full duplex connection the 2nd line is always consecutive to the specified line. E.g. if the -line1 switch specifies 20, line 21 is implicitly used as well.  

When all goes well the following messages appear:  

Trunk: Connection to be established with line-1 at 3705 on host efoxcc1  
Trunk: Connection to be established with line-2 at 3705 on host efoxcc3  
Trunk: Connection to be established with line-1 20  
Trunk: Connection to be established with line-2 20  
Trunk: Full Duplex mode  
Trunk: Line 1 connection has been established  
Trunk: Line 2 connection has been established  

Note that only the connections to  line 20 is shown. The connections to line 21 are implicit because of Full Duplex mode.  
Trunk can be terminated with “Ctrl C”.  
  

### Full Duplex
All SDLC lines can be configured as Full Duplex lines. So, this applies to lines connecting to  i3274, DLSw or Trunk.  

Full Duplex must be configured in the NCP deck.  Below example shows how this is done on the LINE macro:  
```
***********************************************************************    
L03B20   LINE  ADDRESS=(020,021),  TRANSMIT AND RECEIVE ADDRESSES      X    
               DUPLEX=FULL,        MODEM IS STRAPPED FOR FULL DUPLEX   X  
               SPEED=9600,         SPEED MAY BE HIGHERCSEE NOTES)      X  
               NRZI=NO,            SPECIFY YES ONLY IF REQUIRED        X  
               NEWSYNC=NO,         CHECK MODEM REQUIREMENTS            X  
               CLOCKNG=EXT,        MODEM PROVIDES CLOCKING             X  
               POLLED=YES,                                             X  
               RETRIES=(5,10,4)    5 RETRIES, 10S DELAY, 4 SEQUENCES  
```
  
Basically the ADDRESS parameter defines a lines set comprised of two (consecutive) line numbers. In addition the DUPLEX=FULL parameter needs to be specified.  

Note that Full Duplex always requires two line addresses (a line set).   

Full Duplex is specified upon starting i3274, DLSw or Trunk with the -fdx switch. The -line parameter specifies only the lower line address. The 2nd line address is implicitly used. E.g. for the above example, i3274 is started with:  

BIN/i3274 -cchn efoxcc1 -line 20 -fdx  

In the “old days” Full Duplex would mean a significant performance gain as transmit and receive occurs simultaneously on the line set. The use of TCP/IP connections for the 3705 emulator greatly obsoletes the benefits of full duplex. However, on a Trunk line (See section [Trunk](#trunk)) there is certainly some performance gain.     

## Remote 3705
Running a remote 3705 involves the following:  
- a local (channel attached) 3705
- a remote 3705, which is a 3705 with both channel adapters disabled
- a Trunk line connection the above mentioned 3705's

SYS1.NCPSAMP on NCPSSP contains sample NCP's:
- N16B which is theNCP for the channel attached 3705
- N17B which is the NCP for the remote 3705

Copy both members to SYS1.VTAMLST
Follow the steps outlined in section [Generating and Loading the NCP](#generating-and-loading-the-ncp) to generated both NCP's.

Start the local 3705
./BIN/i3705 3705-128k.cnf 

Start the remote 3705
./BIN/i3705 3705r-128k.cnf       
Note the addition "r" in the nanme of the config file.
For the remote 3705 the following messages appear:

```
CA-T2: Main thread 161535 started succesfully...
CS-T2: Thread 161536 started succesfully...
CS-T2: Scanner initialized with 4 lines...
CA1: Waiting for channel connection on TCP port 37051 
CA2: Waiting for channel connection on TCP port 37053 
LIB: Thread 161539 started succesfully...
CA: Adapter thread 161534 started sucessfully... 
PNL: Thread 161537 started succesfully... 
LIB: Using TCP network Address 192.168.2.41 on eno1 for 327x connections
LIB: Line-0 ready, waiting for connection on TCP port 37520
LIB: Line-1 ready, waiting for connection on TCP port 37521
LIB: Line-2 ready, waiting for connection on TCP port 37522
LIB: Line-3 ready, waiting for connection on TCP port 37523
CPU: Reset... 
CPU: MEMORYSIZE 256K bytes 

IBM 3705 II simulator V3.11-0
CPU: Reset... 
CPU: MEMORYSIZE 128K bytes 
CPU: Disabling CA1
CPU: Disabling CA2
CPU: checking CA1: status = -1 
CPU: checking CA2: status = -1 
CPU: Booting 3705 from diskette... 
CPU: Loading LPG1 at X'00400'
CPU: Loading LPG2 at X'1E000'
CA1: Channel connection A closed
CA2: Channel connection A closed
```
The 3705 boots from diskette and closes the channel connections (i.e. the channels are beig disabled). Next step is to start the Trunk line:  

BIN/Trunk -cchn1 efoxcc1 -cchn2 efox41 -line1 20 -line2 20 -fdx 

```
Trunk: Connection to be established with line-1 at 3705 on host efoxcc1
Trunk: Connection to be established with line-2 at 3705 on host efox41
Trunk: Connection to be established with line-1 20
Trunk: Connection to be established with line-2 20
Trunk: Full Duplex mode
Trunk: Line 1 connection has been established
Trunk: Line 2 connection has been established
```
The trunk line is now established between the 3705 running on host efoxcc1 and the 3705 running on host efoxcc2. Both 3705's use line 20 (this must be specified in the NCP as well).  Note the -fdx switch. Thisis FUll Duplex, which is mandatory for a connection to the remote 3705 (This is how we configured the loader program).    

The 3705 is now ready to be loaded over an SDLC line. 

First load the local 3705:

v net,act,id=N16B

Wait for  message
```
IST093I  N16B     ACTIVE
```
Now the remote 3705 can be loaded:

v net,act,id=N17B
This start the NCP load over the SDLC line. Note that this will take substantial longer than loading the local 3705.
After message:
```
IST093I  N17B     ACTIVE
```
the remote 3705 is ready.   
  
The sample NCP N16B contains:  
- 1 SDLC Full DUplex trunk line L16B20
- 1 PU T4 P16B20A
- 1 SDLC leased line L16B22
- 1 PU T2 P16B22A
- 2 LU's T17622A1 and T17622A2
- 1 SDLC switched line L16B23
- 1 PU T2 P16B23A
- 8 LU's in a dnamic pool
  

The sample NCP N17B contains:  
- 1 SDLC line L17B22
- 1 PU T2 P17B22A
- 2 LU's T17B22A1 and T17B22A2
- 1 BSC line L17B23
- 1 Cluster P17B23A
- 1 terminal T17B23A1

Before of after the NCP has been loaded i3274 and/or i3271 can be connected to the local and/or remote 3705:
BIN/i3274 -cchn efoxcc1 -line 22   
BIN/i3274 -cchn efoxcc2 -line 22  
BIN/i3271 -cchn efoxcc2 -line 23
In the above example efoxcc1 is the channel attached (local) 3705 and efoxcc2 is the remote 3705.  

They above listed PU/LU's and/or Cluster/Terminal can now be activated as desired.
(If i3274 or i3271 is started after the NCP has been loaded, the related line needs to be activated first)

## NCP example for the  last version of NCP that supports a 3705.
```
***********************************************************************
*                                                                     *
*      ACF/NCP V3                                                     *
*      THIS GENERATION IS FOR AN IBM 3705-II                          *
*                                                                     *
***********************************************************************
         SPACE 2
***********************************************************************
*      PCCU SPECIFICATIONS - OS/VS (VTAM ONLY)                        *
***********************************************************************
NCPSTART PCCU  CUADDR=5A0,         3705 CONTROL UNIT ADDRESS           X
               AUTODMP=NO,         PROMPT BEFORE DUMPING NCP           X
               AUTOIPL=NO,         NO AUTOIPL AND RESTART              X
               LOADSTA=5A0-S,                                          X
               DUMPSTA=5A0-S,                                          X
               DUMPDS=NCPDUMP,     AUTODUMP REQUESTED                  X
               SUBAREA=1,                                              X
               CHANCON=COND,                                           X
               OWNER=NCPHOST,                                          X
               VFYLM=YES,                                              X
               MAXDATA=4096,                                           X
               INITEST=NO          NCP INITIALIZATION TEST
         EJECT
***********************************************************************
*      BUILD MACRO SPECIFICATIONS FOR OS                              *
***********************************************************************
NCPBUILD BUILD MAXSUBA=31,          MUST BE SAME AS IN VTAM STR DEF    X
               LOADLIB=NCPLIB,      LIBRARY FOR NCP LOAD MODULE        X
               QUALIFY=SYS1,        1ST LEVEL QUALIFIER                X
               VERSION=V3,                                             X
               TYPSYS=OS,                                              X
               MEMSIZE=256,         3705 STORAGE SIZE IS 256K          X
               TYPGEN=NCP,          NCP ONLY                           X
               MAXSSCP=2,                                              X
               NUMHSAS=2,                                              X
               BFRS=88,             NCP BUFFER SIZE                    X
               CA=(TYPE2),          CA 1 IS TYPE 2                     X
               NCPCA=(ACTIVE),      CA 1 ACTIVE                        X
               ERASE=NO,            DO NOT ERASE BUFFERS (DEFAULT)     X
               ENABLTO=2.2,         LEASED LINE ONLY (DEFAULT)         X
               MODEL=3705-2,        .                                  X
               DELAY=(0.2),                                            X
               NEWNAME=EFXNCP2,     NAME OF THIS LOAD MODULE           X
               OLT=NO,              ONLINE TEST AVAILABLE(DEFAULT)     X
               SLODOWN=12,          SLOWDOWN WHEN 12% OF BUFFERS AVAIL X
               SUBAREA=3,           SUBAREA ADDRESS = 3                X
               VRPOOL=6,                                               X
               TRACE=(YES,10)       10 ADDRESS-TRACE ENTRIES
         EJECT
***********************************************************************
*      SYSCNTRL OPTIONS FOR VTAM OR TCAM                              *
*      NOTE THAT OPERATOR CONTROLS ARE NOT INCLUDED.                  *
***********************************************************************
NCPSYSC  SYSCNTRL OPTIONS=(MODE,                                       X
               RCNTRL,RCOND,RECMD,RIMM,ENDCALL,                        X
               BHSASSC)
         EJECT
***********************************************************************
*      HOST MACRO SPECIFICATIONS OS VTAM                              *
*      UNITSZ TIMES MAXBFRU MINUS BFRPAD EQUALS MAX MESSAGE SIZE      *
*      FOR INBOUND MESSAGES                                           *
***********************************************************************
NCPHOST  HOST  INBFRS=25,          INITIAL 3705 ALLOCATION             X
               MAXBFRU=25,         VTAM BUFFER UNIT ALLOCATION         X
               BFRPAD=0,                                               X
               UNITSZ=256,                                             X
               SUBAREA=1,           SUBAREA ADDRESS = 1                X
               TIMEOUT=(120.0)     AUTO SHUT DOMN IF NO RESP IN 120SEC
         EJECT
***********************************************************************
*      CSB MACRO SPECIFICATIONS                                       *
***********************************************************************
NCPCSB   CSB   SPEED=(2400),       BUS MACH CLOCK                      X
               MOD=0,              SCANNER ADDRESS 000 TO 01F          X
               TYPE=TYPE2          TYPE 1 COMM SCANNER
         EJECT
***********************************************************************
*      PATH SPECIFICATIONS                                            *
***********************************************************************
NCP03   PATH  DESTSA=1,                                                X
               ER1=(1,1)
         EJECT
***********************************************************************
*      SPECIFICATIONS FOR SDLC LEASED LINES                           *
*      GROUP MACRO SPECIFICATIONS                                     *
***********************************************************************
SDLCGPL GROUP LNCTL=SDLC,          SYNCHRONOUS DATA LINK               X
               DIAL=NO,            REQUIRED FOR LEASED LINE            X
               REPLYTO=1.0,        USE DEFAULT                         X
               TYPE=NCP            NCP ONLY
        SPACE  2
***********************************************************************
*      LINE MACRO SPECIFICATION - FULL-DUPLEX, LEASED                 *
*      MAY BE USED FOR 3790, 3600, OR 3650                            *
*                                                                     *
*      NOTE: LINE SPEED MAY BE RAISED TO 2400 FOR                     *
*      ALL PHYSICAL UNITS AND TO 4800 FOR 3600 AND 3650               *
*      WITHOUT DOING A NEW GEN OF NCP.                                *
*      RETRIES VALUE FOR LINE SHOULD BE GREATER THAN 30               *
*      SECONDS AND LESS THAN ONE MINUTE FOR 3650.                     *
*                                                                     *
***********************************************************************
SDLC01   LINE  ADDRESS=020,        TRANSMIT AND RECEIVE ADDRESSES      X
               DUPLEX=HALF,        MODEM IS STRAPPED FOR FULL DUPLEX   X
               SPEED=56000,        SPEED MAY BE HIGHERCSEE NOTES)      X
               NRZI=NO,            SPECIFY YES ONLY IF REQUIRED        X
               NEWSYNC=NO,         CHECK MODEM REQUIREMENTS            X
               CLOCKNG=EXT,        MODEM PROVIDES CLOCKING             X
               RETRIES=(5,10,4)    5 RETRIES PER RECOVERY SEQUENCE
         SPACE 2
***********************************************************************
*      SERVICE ORDER FOR SDLC LINK                                    *
***********************************************************************
         SERVICE ORDER=(SDLCPU01)
         EJECT
***********************************************************************
*      PHYSICAL UNIT SPECIFICATIONS                                   *
***********************************************************************
SDLCPU01 PU    ADDR=C1,           POLL ADDRESS                         X
               PUTYPE=2,                                               X
               ISTATUS=ACTIVE,                                         X
               MODETAB=ISTINCLM,                                       X
               SSCPFM=USS3270,                                         X
               USSTAB=ISTINCDT,                                        X
               MAXOUT=7,          MAX PATH INFO UNITS BEFORE RESPONSE  X
               MAXDATA=1024,      MAXIMUM AMOUNT OF DATA               X
               PASSLIM=7,         .                                    X
               PACING=0,          FOR DISPLAYS AND DSC PRINTERS        X
               VPACING=0,         FOR DISPLAYS AND DSC PRINTERS        X
               DISCNT=(NO),       .                                    X
               RETRIES=(,1,4)     4 RETRIES, 1 SECOND BETWEEN
         SPACE 2
***********************************************************************
*      LOGICAL UNIT SPECIFICATIONS                                    *
***********************************************************************
SDLCLU01 LU LOCADDR=2,                                                 X
               USSTAB=MVSUSS,                                          X
               DLOGMOD=D4C32782,                                       X
               ISTATUS=ACTIVE
SDLCLU02 LU LOCADDR=3,                                                 X
               USSTAB=MVSUSS,                                          X
               DLOGMOD=D4C32782,                                       X
               ISTATUS=INACTIVE
SDLCLU03 LU LOCADDR=4,                                                 X
               DLOGMOD=D4C32782,                                       X
               ISTATUS=INACTIVE
SDLCLU04 LU LOCADDR=5,                                                 X
               DLOGMOD=D4C32782,                                       X
               ISTATUS=INACTIVE
         EJECT
***********************************************************************
*      GENEND DELIMITER                                               *
***********************************************************************
         GENEND
         END

```

## Future updates:
	
Performance improvements  
V24 (DB25) to USB interface  
Windows version  


EF & HJS (C)2025
