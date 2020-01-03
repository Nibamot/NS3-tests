#!/usr/bin/env python3

import csv

avglist = [0]*20
stringavglist = ['']*20
nsta = 5
nclm = 20
nlines = 1300
checklist = ['Stations', '#Trial Number', ' Number of stations',
             ' Positionx', 'Positiony', 'Positionz', 'ResultantDistance', 'Payload',
             'Aggregation size', 'MCS', 'Bitrate(kbps)', 'Throughput',
             'Goodput', 'Tx Packets', 'Rx Packets', ' Packet Loss Ratio',
             ' Mean Delay(ms)', ' Channel Utilization', 'SNR (dB))', 'MostUsed AggregationSize',
             ' MostChosenMCS',' Aggregated frames', ' Aggregated packets',' retransmitbytes']
checklist2 = ['Stations', '#Trial Number', ' Number of stations',
              ' Positionx', 'Positiony', 'Positionz', 'Payload',
              'Aggregation size', 'MCS', 'Bitrate(kbps)', 'Throughput',
              'Goodput', 'Tx Packets', 'Rx Packets', ' Packet Loss Ratio',
              ' Mean Delay(ms)', ' Channel Utilization', 'SNR (dB))', 'MostUsed AggregationSize',
              ' MostChosenMCS',' Aggregated frames', ' Aggregated packets',' retransmitbytes']#, 'Station 0', 'Station 1','Station 2', 'Station 3', 'Station 4', 'Station 5', 'Station 6','Station 7', 'Station 8', 'Station 9'

checkstation = ['Station 0', 'Station 1','Station 2', 'Station 3', 'Station 4', 'Station 5',
                'Station 6','Station 7', 'Station 8', 'Station 9']





with open('WalkRandomRFRaggregationprop5stations.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)
    i = 0
    with open('WalkRandomRFRaggregationprop5stationsavg.csv', 'w') as newcsv:
        csvwriter = csv.writer(newcsv)
        for i, row in enumerate(csvreader):
            if not row[0] in (None, ""):
                for col in range(0,nclm):
                    if row[col] not in checklist2:
                        if row[col] == '-nan':
                            avglist[col] += 0.0
                        else:
                            try:
                                avglist[col] += float(row[col])
                            except ValueError:
                                continue
                if i == 0:
                    #csvwriter.writerow(row)
                    print(1)
                elif i != 0 and row[0] not in checklist:
                    #csvwriter.writerow(row)
                    print(1)
                if i == nlines:
                    print(avglist)
                    break
            else :
                for ncol in range(0, nclm):

                    if ncol == 19:
                        avglist[ncol] = avglist[ncol]
                        stringavglist[ncol] = str(avglist[ncol])
                    else:
                        avglist[ncol] = avglist[ncol]/nsta
                        stringavglist[ncol] = str(avglist[ncol])


                stringavglist[0]='Average'
                csvwriter.writerow(stringavglist)
                if avglist[1]==10:
                    csvwriter.writerows('\n')
                    csvwriter.writerows('\n')
                    csvwriter.writerows('\n')
                    csvwriter.writerows('\n')
                avglist = [0]*nclm
                stringavglist = ['']*nclm
        i += 3
        print(i)
        if i == nlines:
            print("HELLo")
            for ncol in range(0, nclm):
                if ncol == 19:
                    avglist[ncol] = avglist[ncol]
                    stringavglist[ncol] = str(avglist[ncol])
                else:
                    avglist[ncol] = avglist[ncol]/nsta
                    stringavglist[ncol] = str(avglist[ncol])
            stringavglist[0]='Average'
            csvwriter.writerow(stringavglist)
            if avglist[1]==10:
                csvwriter.writerows('\n')
                csvwriter.writerows('\n')
                csvwriter.writerows('\n')
                csvwriter.writerows('\n')
