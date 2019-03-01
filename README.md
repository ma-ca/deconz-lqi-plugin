# deCONZ Lqi Plugin

This plugin writes all ZDP `Mgmt_Lqi_rsp` command frames (ClusterID=0x8031) to a database table. The deCONZ core software polls the neighbor tables from every router devices on a regular basis. This plugin captures the responses and writes the data to a database.

The purpose is to capture all parents and children from all neighbor tables and to track the most recent changes to the neighbor tables. In case an endpoint drops off the network it should be possible to find the last parent device in the lqi_history database. 

The database table lqi stores the most recent neighbor tables from all routers. The lqi_history table keeps the last 5 changes for every neighbor table entry. The default database path is `/run/shm/lqi.db` unless changed in the source. The directory `/run/shm` on Raspbian is located in RAM and therefore lost after reboot. However, the lqi databse is recreated again within an hour.  

Normally an endpoint should rejoin the network if it looses the connection to its parent router. But sometimes an endpoint might still connect to a parent router but the router does not have that endpoint in the neighbor table anymore. That endpoint will no longer receive any commands and is orphaned.

Sending a `Mgmt_Leave_req` to the last known router might force the orphaned endpoint to rejoin the network again.

# Using the database tables lqi and lqi_history


```
sqlite3 /run/shm/lqi.db "SELECT * FROM lqi;"

sqlite3 /run/shm/lqi.db "SELECT DISTINCT neighborNwkAddr, neighborExtAddr FROM lqi;"

sqlite3 /run/shm/lqi.db "SELECT count(*), neighborNwkAddr, neighborExtAddr, srcAddr \
                         FROM lqi_history \
                         GROUP by neighborNwkAddr,srcAddr \
                         ORDER by count(*) DESC;"
```


# Installation

The plugin has been tested on Raspbian stretch.

### Prepare compile



Download deCONZ development package

    wget http://www.dresden-elektronik.de/rpi/deconz-dev/deconz-dev-2.05.59.deb 

Install deCONZ development package

    sudo dpkg -i deconz-dev-2.05.59.deb 
    sudo apt install -f



### Compile the plugin.

    git clone https://github.com/ma-ca/deconz-lqi-plugin.git 
    cd deconz-lqi-plugin 
    qmake && make


Copy the plugin to the deCONZ plugins folder

    sudo cp libde_lqi_plugin.so /usr/share/deCONZ/plugins

Restart deCONZ


# Zigbee neighbor table and Mgmt_Lqi_rsp command frame

The following description is referring to the Zigbee specification, chapter 2.4.4.4.2 Mgmt_Lqi_rsp.

### Format of the `Mgmt_Lqi_rsp` Command Frame:


Name | Bytes
---- | -----
Status | 1
NeighborTableEntries | 1
StartIndex | 1
NeighborTableListCount | 1
NeighborTableList | Variable


### NeighborTableList Record Format:

Name | Bits | Description
---- | ---- | -----------
Extended PAN Id | 64 | The 64-bit extended PAN identifier of the neighboring device.
Extended address | 64 | 64-bit IEEE address that is unique to every device. <br>If this value is unknown at the time of the request, this field shall be set to 0xffffffffffffffff.
Network address | 16 | The 16-bit network address of the neighboring device.
Device type | 2 | The type of the neighbor device: 0x00 = ZigBee coordinator 0x01 = ZigBee router <br> 0x02 = ZigBee end device <br> 0x03 = Unknown
RxOnWhenIdle | 2 | Indicates if neighbor's receiver is enabled during idle portions of the CAP:<br>0x00 = Receiver is off <br>0x01 = Receiver is on <br>0x02 = unknown
Relationship | 3 | The relationship between the neighbor and the current device: <br>0x00 = neighbor is the parent <br>0x01 = neighbor is a child <br>0x02 = neighbor is a sibling <br>0x03 = None of the above <br>0x04 = previous child
Reserved | 1 | This reserved bit shall be set to 0.
Permit joining | 2 | An indication of whether the neighbor device is accepting join requests: <br>0x00 = neighbor is not accepting join requests <br>0x01 = neighbor is accepting join requests <br>0x02 = unknown
Reserved | 6 | Each of these reserved bits shall be set to 0.
Depth | 8 | The tree depth of the neighbor device. A value of 0x00 indicates that the device is the ZigBee coordinator for the network.
LQI | 8 | The estimated link quality for RF transmis- sions from this device. 
 

### Table lqi

The database tables lqi and lqi_history are created by this plugin.


Column | Type
------ | ----
srcAddr | TEXT PRIMARY KEY
tableIndex | INTEGER PRIMARY KEY
tableEntries | INTEGER
neighborExtPanId | TEXT
neighborExtAddr | TEXT
neighborNwkAddr | TEXT
deviceType | TEXT
rxOnWhenIdle | INTEGER
relationship | TEXT
permitJoin | INTEGER
depth | INTEGER
lqiLinkQuality | INTEGER
timestamp | TEXT


### Table lqi_history

Column | Type
------ | ----
id | INTEGER PRIMARY KEY AUTOINCREMENT
srcAddr | TEXT
tableIndex | INTEGER
tableEntries | INTEGER
neighborExtPanId | TEXT
neighborExtAddr | TEXT
neighborNwkAddr | TEXT
deviceType | TEXT
rxOnWhenIdle | INTEGER
relationship | TEXT
permitJoin | INTEGER
depth | INTEGER
lqiLinkQuality | INTEGER
timestamp | TEXT


