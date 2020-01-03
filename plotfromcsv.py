#!/usr/bin/env python3
#after 420 lines change aggregation size then mcs

from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import numpy as np
import csv
from matplotlib import cm
import matplotlib
font = {'family' : 'normal',
        'size'   : 18}


matplotlib.rc('font', **font)

fig = plt.figure()

z=[]

ygoodputnomcs0 = []
ypktlossnomcs0 = []
ychutilnomcs0 = []
yretxpcnomcs0 = []

ygoodputnomcs2 = []
ypktlossnomcs2 = []
ychutilnomcs2 = []
yretxpcnomcs2 = []

ygoodputnomcs7 = []
ypktlossnomcs7 = []
ychutilnomcs7 = []
yretxpcnomcs7 = []

ygoodputmaxmcs0 = []
ypktlossmaxmcs0 = []
ychutilmaxmcs0 = []
yretxpcmaxmcs0 = []

ygoodputmaxmcs2 = []
ypktlossmaxmcs2 = []
ychutilmaxmcs2 = []
yretxpcmaxmcs2 = []

ygoodputmaxmcs7 = []
ypktlossmaxmcs7 = []
ychutilmaxmcs7 = []
yretxpcmaxmcs7 = []




ygoodputrfrmcs0 = []
ypktlossrfrmcs0 = []
ychutilrfrmcs0 = []
yretxpcrfrmcs0 = []

ygoodputrfrmcs2 = []
ypktlossrfrmcs2 = []
ychutilrfrmcs2 = []
yretxpcrfrmcs2 = []

ygoodputrfrmcs7 = []
ypktlossrfrmcs7 = []
ychutilrfrmcs7 = []
yretxpcrfrmcs7 = []

#confidence intervals for mcs0,2,7 no,max,rfr

ygoodputnomcs0ci = []
ypktlossnomcs0ci = []
ychutilnomcs0ci = []
yretxpcnomcs0ci = []

ygoodputnomcs2ci = []
ypktlossnomcs2ci = []
ychutilnomcs2ci = []
yretxpcnomcs2ci = []

ygoodputnomcs7ci = []
ypktlossnomcs7ci = []
ychutilnomcs7ci = []
yretxpcnomcs7ci = []


ygoodputmaxmcs0ci = []
ypktlossmaxmcs0ci = []
ychutilmaxmcs0ci = []
yretxpcmaxmcs0ci = []

ygoodputmaxmcs2ci = []
ypktlossmaxmcs2ci = []
ychutilmaxmcs2ci = []
yretxpcmaxmcs2ci = []

ygoodputmaxmcs7ci = []
ypktlossmaxmcs7ci = []
ychutilmaxmcs7ci = []
yretxpcmaxmcs7ci = []

ygoodputrfrmcs0ci = []
ypktlossrfrmcs0ci = []
ychutilrfrmcs0ci = []

yretxpcrfrmcs0ci = []
ygoodputrfrmcs2ci = []
ypktlossrfrmcs2ci = []
ychutilrfrmcs2ci = []
yretxpcrfrmcs2ci = []

ygoodputrfrmcs7ci = []
ypktlossrfrmcs7ci = []
ychutilrfrmcs7ci = []
yretxpcrfrmcs7ci = []

"""axes= plt.axes()

axes.grid()
axes.set_axisbelow(True)
"""

with open('WalkRandomRFRaggregationfixed5stationsavg.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)

    for i, row in enumerate(csvreader):
        if i < 84:
            if i % 14 == 10 and i!=0:
                ygoodputrfrmcs0.append(float(row[10]))
                ypktlossrfrmcs0.append(float(row[16]))
                ychutilrfrmcs0.append(float(row[19]))
                yretxpcrfrmcs0.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputrfrmcs0ci.append(float(row[10]))
                ypktlossrfrmcs0ci.append(float(row[16]))
                ychutilrfrmcs0ci.append(float(row[19]))
                yretxpcrfrmcs0ci.append(float(row[20]))

        elif i > 84 and i < 168:
            if i % 14 == 10 and i!=0:
                ygoodputrfrmcs2.append(float(row[10]))
                ypktlossrfrmcs2.append(float(row[16]))
                ychutilrfrmcs2.append(float(row[19]))
                yretxpcrfrmcs2.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputrfrmcs2ci.append(float(row[10]))
                ypktlossrfrmcs2ci.append(float(row[16]))
                ychutilrfrmcs2ci.append(float(row[19]))
                yretxpcrfrmcs2ci.append(float(row[20]))

        elif i > 168:
            if i % 14 == 10 and i!=0:
                ygoodputrfrmcs7.append(float(row[10]))
                ypktlossrfrmcs7.append(float(row[16]))
                ychutilrfrmcs7.append(float(row[19]))
                yretxpcrfrmcs7.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputrfrmcs7ci.append(float(row[10]))
                ypktlossrfrmcs7ci.append(float(row[16]))
                ychutilrfrmcs7ci.append(float(row[19]))
                yretxpcrfrmcs7ci.append(float(row[20]))
csvfile.close()

with open('WalkRandomMAXaggregationfixed5stationsavg.csv', 'r') as csvfile :
    csvreader = csv.reader(csvfile)

    for i, row in enumerate(csvreader):
        if i < 84:
            if i % 14 == 10 and i!=0:
                ygoodputmaxmcs0.append(float(row[10]))
                ypktlossmaxmcs0.append(float(row[16]))
                ychutilmaxmcs0.append(float(row[19]))
                yretxpcmaxmcs0.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputmaxmcs0ci.append(float(row[10]))
                ypktlossmaxmcs0ci.append(float(row[16]))
                ychutilmaxmcs0ci.append(float(row[19]))
                yretxpcmaxmcs0ci.append(float(row[20]))

        elif i > 84 and i < 168:
            if i % 14 == 10 and i!=0:
                ygoodputmaxmcs2.append(float(row[10]))
                ypktlossmaxmcs2.append(float(row[16]))
                ychutilmaxmcs2.append(float(row[19]))
                yretxpcmaxmcs2.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputmaxmcs2ci.append(float(row[10]))
                ypktlossmaxmcs2ci.append(float(row[16]))
                ychutilmaxmcs2ci.append(float(row[19]))
                yretxpcmaxmcs2ci.append(float(row[20]))

        elif i > 168:
            if i % 14 == 10 and i!=0:
                ygoodputmaxmcs7.append(float(row[10]))
                ypktlossmaxmcs7.append(float(row[16]))
                ychutilmaxmcs7.append(float(row[19]))
                yretxpcmaxmcs7.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputmaxmcs7ci.append(float(row[10]))
                ypktlossmaxmcs7ci.append(float(row[16]))
                ychutilmaxmcs7ci.append(float(row[19]))
                yretxpcmaxmcs7ci.append(float(row[20]))
csvfile.close()

with open('WalkRandomNOaggregationfixed5stationsavg.csv', 'r') as csvfile:
    csvreader = csv.reader(csvfile)

    for i, row in enumerate(csvreader):
        if i < 84:
            if i % 14 == 10 and i!=0:
                ygoodputnomcs0.append(float(row[10]))
                ypktlossnomcs0.append(float(row[16]))
                ychutilnomcs0.append(float(row[19]))
                yretxpcnomcs0.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputnomcs0ci.append(float(row[10]))
                ypktlossnomcs0ci.append(float(row[16]))
                ychutilnomcs0ci.append(float(row[19]))
                yretxpcnomcs0ci.append(float(row[20]))

        elif i > 84 and i < 168:
            if i % 14 == 10 and i!=0:
                ygoodputnomcs2.append(float(row[10]))
                ypktlossnomcs2.append(float(row[16]))
                ychutilnomcs2.append(float(row[19]))
                yretxpcnomcs2.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputnomcs2ci.append(float(row[10]))
                ypktlossnomcs2ci.append(float(row[16]))
                ychutilnomcs2ci.append(float(row[19]))
                yretxpcnomcs2ci.append(float(row[20]))

        elif i > 168:
            if i % 14 == 10 and i!=0:
                ygoodputnomcs7.append(float(row[10]))
                ypktlossnomcs7.append(float(row[16]))
                ychutilnomcs7.append(float(row[19]))
                yretxpcnomcs7.append(float(row[20]))

            if i % 14 == 12 and i!=0:
                ygoodputnomcs7ci.append(float(row[10]))
                ypktlossnomcs7ci.append(float(row[16]))
                ychutilnomcs7ci.append(float(row[19]))
                yretxpcnomcs7ci.append(float(row[20]))
csvfile.close()

# width of the bars
barWidth = 0.3



# The x position of bars
r1 = np.arange(len(ygoodputnomcs7))
r2 = [x + barWidth for x in r1]
r3 = [x + 0.6 for x in r1]


#1e3f63ff
#E2583E
#1e3f63ff
###MCS0 Goodput
axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
plt.bar(r1, ygoodputnomcs0, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ygoodputnomcs0ci, capsize=7, label='No Aggregation')
plt.bar(r2, ygoodputmaxmcs0, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ygoodputmaxmcs0ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ygoodputrfrmcs0, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ygoodputrfrmcs0ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Goodput [Mbps]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 4.5, ymin = 0)
plt.savefig('MCS0Goodputfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
##plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS0Goodput.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###mcs0 Ch util
plt.bar(r1, ychutilnomcs0, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ychutilnomcs0ci, capsize=7, label='No Aggregation')
plt.bar(r2, ychutilmaxmcs0, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ychutilmaxmcs0ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ychutilrfrmcs0, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ychutilrfrmcs0ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Channel Utilization [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS0ChanUtilfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)

#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS0ChanUtil.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###mcs0 pkt loss ratio
plt.bar(r1, ypktlossnomcs0, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ypktlossnomcs0ci, capsize=7, label='No Aggregation')
plt.bar(r2, ypktlossmaxmcs0, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ypktlossmaxmcs0ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ypktlossrfrmcs0, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ypktlossrfrmcs0ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Packet Loss Ratio [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS0PktLossfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS0PkLossRatio.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###MCS0 rETX
plt.bar(r1, yretxpcnomcs0, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=yretxpcnomcs0ci, capsize=7, label='No Aggregation')
plt.bar(r2, yretxpcmaxmcs0, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=yretxpcmaxmcs0ci, capsize=7, label='Max Aggregation')
plt.bar(r3, yretxpcrfrmcs0, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=yretxpcrfrmcs0ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(yretxpcnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Retranmsissions [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper right')
plt.ylim(ymax = 65, ymin = 0)
plt.savefig('MCS0Retxfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS0RetxPercent.pdf", bbox_inches='tight')
plt.show()


axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)

###mcs2 Goodput
plt.bar(r1, ygoodputnomcs2, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ygoodputnomcs2ci, capsize=7, label='No Aggregation')
plt.bar(r2, ygoodputmaxmcs2, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ygoodputmaxmcs2ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ygoodputrfrmcs2, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ygoodputrfrmcs2ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs2))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Goodput [Mbps]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 4.5, ymin = 0)
plt.savefig('MCS2Goodputfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS2Goodput.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###mcs2 Ch util
plt.bar(r1, ychutilnomcs2, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ychutilnomcs2ci, capsize=7, label='No Aggregation')
plt.bar(r2, ychutilmaxmcs2, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ychutilmaxmcs2ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ychutilrfrmcs2, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ychutilrfrmcs2ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs2))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Channel Utilization [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS2ChanUtilfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS2ChanUtil.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###mcs2 pkt loss ratio
plt.bar(r1, ypktlossnomcs2, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ypktlossnomcs2ci, capsize=7, label='No Aggregation')
plt.bar(r2, ypktlossmaxmcs2, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ypktlossmaxmcs2ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ypktlossrfrmcs2, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ypktlossrfrmcs2ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs2))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Packet Loss Ratio [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS2PktLossfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS2PkLossRatio.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)

###MCS2 rETX
plt.bar(r1, yretxpcnomcs2, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=yretxpcnomcs2ci, capsize=7, label='No Aggregation')
plt.bar(r2, yretxpcmaxmcs2, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=yretxpcmaxmcs2ci, capsize=7, label='Max Aggregation')
plt.bar(r3, yretxpcrfrmcs2, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=yretxpcrfrmcs2ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(yretxpcnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Retranmsissions [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper right')
plt.ylim(ymax = 65, ymin = 0)
plt.savefig('MCS2Retxfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS2RetxPercent.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)


###MCS7 Goodput
plt.bar(r1, ygoodputnomcs7, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ygoodputnomcs7ci, capsize=7, label='No Aggregation')
plt.bar(r2, ygoodputmaxmcs7, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ygoodputmaxmcs7ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ygoodputrfrmcs7, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ygoodputrfrmcs7ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs7))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Goodput [Mbps]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 4.5, ymin = 0)
plt.savefig('MCS7Goodputfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS7Goodput.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###MCS7 Ch util
plt.bar(r1, ychutilnomcs7, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ychutilnomcs7ci, capsize=7, label='No Aggregation')
plt.bar(r2, ychutilmaxmcs7, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ychutilmaxmcs7ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ychutilrfrmcs7, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ychutilrfrmcs7ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs7))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Channel Utilization [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS7ChanUtilfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS7ChanUtil.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()
axes.grid()
axes.set_axisbelow(True)
###MCS7 pkt loss ratio
plt.bar(r1, ypktlossnomcs7, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=ypktlossnomcs7ci, capsize=7, label='No Aggregation')
plt.bar(r2, ypktlossmaxmcs7, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=ypktlossmaxmcs7ci, capsize=7, label='Max Aggregation')
plt.bar(r3, ypktlossrfrmcs7, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=ypktlossrfrmcs7ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(ygoodputnomcs7))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Packet Loss Ratio [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper left')
plt.ylim(ymax = 100, ymin = 0)
plt.savefig('MCS7PktLossfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS7PkLossRatio.pdf", bbox_inches='tight')
plt.show()

axes= plt.axes()

axes.grid()
axes.set_axisbelow(True)

###MCS7 rETX
plt.bar(r1, yretxpcnomcs7, width = barWidth, color = '#1e3f63ff', edgecolor = 'black', yerr=yretxpcnomcs7ci, capsize=7, label='No Aggregation')
plt.bar(r2, yretxpcmaxmcs7, width = barWidth, color = '#f1f4ffff', edgecolor = 'black', yerr=yretxpcmaxmcs7ci, capsize=7, label='Max Aggregation')
plt.bar(r3, yretxpcrfrmcs7, width = barWidth, color = '#E2583E', edgecolor = 'black', yerr=yretxpcrfrmcs7ci, capsize=7, label='RFR Model')
plt.xticks([r + barWidth for r in range(len(yretxpcnomcs0))], ['1', '5', '10', '15', '20', '25'])
plt.ylabel('Retranmsissions [%]')
plt.xlabel('Bitrate [Mbps]')
plt.legend(loc='upper right')
plt.ylim(ymax = 65, ymin = 0)
plt.savefig('MCS7Retxfixed.pdf',bbox_inches='tight', transparent="True", pad_inches=0)
#plt.axis("tight")
#plt.tight_layout()
#fig.savefig("MCS7RetxPercent.pdf", bbox_inches='tight')
plt.show()
#axes= plt.axes()

#axes.grid()
