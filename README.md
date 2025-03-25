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

Below picture shows the high-level architecture with all components.  
![High Level architecture of the 3705 emulator](/Images/Overview.png)

   

### Preparing the MVS system
This section is generic. It applies to a CPIPO build of MVS, TK5 or whatever other ways the MVS system has been created.    
Copy file ‘ncpssp.3350’ fro the 3705 emulator package ‘Hercules Files’ directory to your Hercules/hyperion dasd folder.

Update the Hercules cnf file  and add ncpssp.3350 to the dasd configuration.
Be sure to select an address that is generated as a 3350.
For TK5 the address range 240-24F are 3350’s.  Below 244 is used.
...

0244 3350 dasd/ncpssp.3350         <=== Added
...

Now add the 3705 to the hercules configuration file. Make sure to use an address that is generated as a 3705.
For TK5 this is 660. Make sure to comment out the existing definition for device 660. For TK5 the updates should be made to tk5_default.cnf:  
\#  
\# NCP VTAM  
\#  
0660 3705 adaptip=192.168.1.05 port=37051  <=== Added. 192.168.1.05 should be replaced with the IP address of the host running your 3705.   
\#0660 3705 lport=${N660PORT:=37051} locncpnm=N07 rmtncpnm=N08 unitsz=252 ackspeed=1000  
0661 3705 lport=${N661PORT:=37052} locncpnm=N10 rmtncpnm=N11 idblk=017 idnum=00018 locsuba=10 rmtsuba=11 unitsz=252 ackspeed=1000  
0662 3705 lport=${N662PORT:=37053} locncpnm=N12 rmtncpnm=N13 idblk=017 idnum=00019 locsuba=12 rmtsuba=13 unitsz=252 ackspeed=1000  
0663 3705 lport=${N663PORT:=37054} locncpnm=N14 rmtncpnm=N15 idblk=017 idnum=0001a locsuba=14 rmtsuba=15 unitsz=252 ackspeed=1000  
\#  

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
- VATLSTxx : Add  NCPSSP,0,2,3350	,N 

### Replace IFLOADRN (TK4, TK5)  
The IFLOADN used by TK amd TK5 is a special version for loading fake IBM 3705’s.  

Restore the original IFLOADRN of IBM or, in case it does not exist:  
Copy ‘SYS1.SSPLIB(IFLOADRN)’ on NCPSSP to ‘SYS1.LINKLIB(IFLOADRN)’ on the sysres volume.  

Note: the old IFLOADRN version is now not avail anymore.

Shutdown MVS and Re-IPL MVS with all these updates.

## Operation and Use
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


The key message is the last one: “state CONNECTED” this means the end-to-end connectivity is established and the DLSw’s and the SDLC device are ready. Once in the “Connected” state, the DLSw emulator will respond to a Request to Send (RTS) from the 3705 scanner with a “Clear To Send” (CTS). In effect, the connected state will allow the 3705 to send data across the line and DLSw’s to the SDLC device. 

DLSw can be terminated with “Ctrl C”.


## Future updates:
	
Performance improvements  
V24 (DB25) to USB interface  
Windows version  


EF & HJS (C)2025
