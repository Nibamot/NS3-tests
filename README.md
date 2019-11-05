# NS3-tests
## Testing various scenarios using NS3

### DownLink Single AP A-MSDU/ A-MPDU  
  * [Aggregationdownlink1ap](https://github.com/Nibamot/NS3-tests/blob/master/aggregationdownlink1ap.cc) - is an example of a simple Wifi downlink scenario with A-MSDU aggregation enabled and A-MPDU disabled. using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) .sh we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc. Also, using netanim can illustrate the whole scenario.

### UpLink Single AP A-MSDU/ A-MPDU  
* [Aggregationuplink1ap](https://github.com/Nibamot/NS3-tests/blob/master/aggregationuplink1ap.cc) - is an example of a simple Wifi UpLink scenario with A-MSDU aggregation enabled and A-MPDU disabled. using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) .sh we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc.

The source files like [minstrel-ht-wifi-manager.cc](https://github.com/Nibamot/NS3-tests/blob/master/source/minstrel-ht-wifi-manager.cc) and [msdu-aggregator.cc](https://github.com/Nibamot/NS3-tests/blob/master/source/msdu-aggregator.cc) and their header files were edited in order to ascertain specfic scenarios. 
