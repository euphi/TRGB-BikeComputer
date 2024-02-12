#!/usr/bin/python3

# Written by ChatGPT... (mostly, it created many lines of beautiful code - and some subtle, but stupid errors...)

import csv
from datetime import datetime, timedelta


def excel_timestamp_to_datetime(excel_timestamp):
    # Excel epoch is on January 1, 1900
    excel_epoch = datetime(1899, 12, 30)
    
    # Calculate the timestamp
    timestamp = excel_epoch + timedelta(days=float(excel_timestamp))

    return timestamp

def convert_csv_values(row):
    # Convert all numeric values in the row
    return {key: value.replace(',', '.') if value.replace(',', '', 1).replace('-', '', 1).isdigit() else None for key, value in row.items()}

def csv_to_line_protocol(csv_file_path, measurement):
    lines = []

    # Prepend DDL statements
    lines.append("# DDL")
    lines.append(f"CREATE DATABASE BikeComputer\n")

    # Prepend DML statements
    lines.append("# DML")
    lines.append(f"# CONTEXT-DATABASE: BikeComputer")
    lines.append(f"# CONTEXT-RETENTION-POLICY: autogen\n")

    start_distance = 0.0
    
    with open(csv_file_path, 'r') as csv_file:
        csv_reader = csv.DictReader(csv_file)
        
        for row in csv_reader:
            converted_row = convert_csv_values(row)
            
            # Set start distance to zero
            if start_distance == 0: start_distance = float(converted_row['distance'])
            converted_row['distance'] = float(converted_row['distance']) - start_distance

            timestamp = excel_timestamp_to_datetime(converted_row['Timestamp'])
            print(timestamp)
            tags = ''  # Add tags if needed, e.g., 'tag1=value1,tag2=value2'
            fields = ','.join([f'{key}={value}' for key, value in converted_row.items() if key != 'Timestamp' and value is not None])
            
            line = f'{measurement}{tags} {fields} {timestamp.strftime("%s")}'
            lines.append(line)

    return lines

def write_lines_to_file(lines, output_file_path):
    with open(output_file_path, 'w') as output_file:
        output_file.write('\n'.join(lines))

if __name__ == "__main__":
    csv_file_path = 'out.csv'
    output_file_path = 'out.txt'
    measurement = 'Grail'

    lines = csv_to_line_protocol(csv_file_path, measurement)
    write_lines_to_file(lines, output_file_path)
