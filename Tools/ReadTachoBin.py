#!/usr/bin/python3

__version__ = 0.1
__date__ = '2022-11-21'
__updated__ = '2022-11-21'

import struct
import csv
from array import array

from optparse import OptionParser
import os
import sys

def localize_floats(row):
    return dict(
        (key, str(row[key]).replace('.', ',')) if isinstance(row[key], float) else (key, row[key]) 
        for key in row
    )

format_struct="<LffffIBxxx"
format_names=("Timestamp", "Speed", "Temperatur", "Gradient", "Höhe", "distance", "Puls")
print(str(struct.calcsize(format_struct)) + u" bytes needed per Set")


program_name = os.path.basename(sys.argv[0])
program_version = "v0.1"
program_build_date = "%s" % __updated__

program_version_string = '%%prog %s (%s)' % (program_version, program_build_date)
#program_usage = '''usage: spam two eggs''' # optional - will be autogenerated by optparse
program_longdesc = '''Convert binary files from the ESP_BTTacho device to CSV file''' # optional - give further explanation about what the program does
program_license = "Use it as it is"


# setup option parser
parser = OptionParser(version=program_version_string, epilog=program_longdesc, description=program_license)
parser.add_option("-i", "--in", dest="infile", help="set input path [default: %default]", metavar="FILE")
parser.add_option("-o", "--out", dest="outfile", help="set output path [default: %default]", metavar="FILE")
parser.add_option("-v", "--verbose", dest="verbose", action="count", help="set verbosity level [default: %default]")

# set defaults
parser.set_defaults(outfile="./out.csv", infile="./LOG_0160.BIN", verbose=0)

# process options
(opts, args) = parser.parse_args(sys.argv[1:])

print(opts)

if opts.verbose > 0:
    print("verbosity level = %d" % opts.verbose)
if opts.infile:
    print("infile = %s" % opts.infile)
if opts.outfile:
    print("outfile = %s" % opts.outfile)


fnameIn = opts.infile
fnameOut = opts.outfile

with open(fnameIn,'rb') as file, open(fnameOut, 'w', newline='') as csvfile:
    csvwriter = csv.DictWriter(csvfile, fieldnames=format_names, dialect='excel')
    csvwriter.writeheader()
    while(1):
      pdata = array( 'B', file.read(struct.calcsize(format_struct)) )  # buffered read from input file
      print(struct.unpack(format_struct,pdata))
      tuple_dataset=dict(zip(format_names,struct.unpack(format_struct,pdata)))
      tuple_dataset["Timestamp"] = tuple_dataset["Timestamp"] / (60*60*24) + 25569  # Convert Unix Timestamp to Excel/Libreoffice Timestamp
      csvwriter.writerow(localize_floats(tuple_dataset)) # Change . to , in floats - based on https://stackoverflow.com/questions/39833555/how-to-write-a-csv-with-a-comma-as-the-decimal-separator and adapted for use with a dict 