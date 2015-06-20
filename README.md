# exploreUPnP
Discover and explore all UPnP and DLNA devices in a network.

## At the moment:
discovery of all UPnP based media servers (Windows Media, Plex, minidlna, MediaTomb) is possible.
Mediatomb (http://mediatomb.cc/) servers can be explored, which means their entries are retrieved and
saved to an elasticsearch database.

## Future:
The protocal for DLNA SOAP browsing needs to be implemented.
Scans with Wireshark showed that XMS_MediaRegistrar seems to play be big role.
This service is not implemented in MediaTomb.

Feel free to commit your ideas. 

No warranty for anything.
