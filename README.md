# NS3-tests
## Testing various scenarios using NS3

### DownLink Single AP A-MSDU/ A-MPDU  
  * [Aggregationdownlink1ap](https://github.com/Nibamot/NS3-tests/blob/master/aggregationdownlink1ap.cc) - is an example of a simple Wifi downlink scenario with A-MSDU aggregation enabled and A-MPDU disabled. using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc. Also, using netanim can illustrate the whole scenario.

### UpLink Single AP A-MSDU/ A-MPDU  
* [Aggregationuplink1ap](https://github.com/Nibamot/NS3-tests/blob/master/aggregationuplink1ap.cc) - is an example of a simple Wifi UpLink scenario with A-MSDU aggregation enabled and A-MPDU disabled. using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc.

### RFR aggregation (A-MSDU/ A-MPDU)
* [RFRaggregationwalkmobility](https://github.com/Nibamot/NS3-tests/blob/master/RFRaggregationwalkmobility.cc) - is an example of a scenario with A-MSDU aggregation enabled and A-MPDU disabled with a variable number of mobile stations in the downlink [2-20] and 2 fixed uplink stations. The stations move around a circle of radius 30m with a velocity of 2-50 m/s. Using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc.

### MAX aggregation (A-MSDU/ A-MPDU)
* [MAXaggregationwalkmobility](https://github.com/Nibamot/NS3-tests/blob/master/MAXaggregationwalkmobility.cc) - is an example of a scenario with A-MSDU aggregation enabled and A-MPDU disabled with a variable number of mobile stations in the downlink [2-20] and 2 fixed uplink stations. The stations move around a circle of radius 30m with a velocity of 2-50 m/s. Using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc.

### NO aggregation (A-MSDU/ A-MPDU)
* [NOaggregationwalkmobility](https://github.com/Nibamot/NS3-tests/blob/master/NOaggregationwalkmobility.cc) - is an example of a scenario with A-MSDU aggregation enabled and A-MPDU disabled with a variable number of mobile stations in the downlink [2-20] and 2 fixed uplink stations. The stations move around a circle of radius 30m with a velocity of 2-50 m/s. Using the [shell script](https://github.com/Nibamot/NS3-tests/blob/master/aggregation_script_256bytes.sh) we can change the values for different parameters including but not limited to MCS (Modulation and Coding scheme), maximum distance range from AP to stations, frame size, aggregation size limit etc.

## Source code (wifi/model)
The source files like [minstrel-ht-wifi-manager.cc](https://github.com/Nibamot/NS3-tests/blob/master/source/minstrel-ht-wifi-manager.cc) and [msdu-aggregator.cc](https://github.com/Nibamot/NS3-tests/blob/master/source/msdu-aggregator.cc) and their header files were edited in order to ascertain specfic scenarios. For example these files have been modified in order to add the functionalities of getting the number of packet/frames aggregated, sent and received. They also help in fixing a maximum aggregation size in runtime. Go through the code for more details. 

[Averager.py](https://github.com/Nibamot/NS3-tests/blob/master/averager.py) is used to average out the data from the CSVs depending on the number of trials for each scenario. [Plotfromcsv.py](https://github.com/Nibamot/NS3-tests/blob/master/plotfromcsv.py) is the script that uses this prcocessed data to plot graphs.
