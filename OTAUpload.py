#!/usr/bin/python3
# Upload script for AsyncElegantOTA
# 
# based on example for use with platformio

import requests
import hashlib

try:
    from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
    from tqdm import tqdm
except ImportError:
    from requests_toolbelt import MultipartEncoder, MultipartEncoderMonitor
    from tqdm import tqdm

def on_upload(firmware_path, upload_url):

    with open(firmware_path, 'rb') as firmware:
        md5 = hashlib.md5(firmware.read()).hexdigest()
        firmware.seek(0)
        encoder = MultipartEncoder(fields={
            'MD5': md5, 
            'firmware': ('firmware', firmware, 'application/octet-stream')}
        )

        bar = tqdm(desc='Upload Progress',
              total=encoder.len,
              dynamic_ncols=True,
              unit='B',
              unit_scale=True,
              unit_divisor=1024
              )

        monitor = MultipartEncoderMonitor(encoder, lambda monitor: bar.update(monitor.bytes_read - bar.n))

        response = requests.post(upload_url, data=monitor, headers={'Content-Type': monitor.content_type})
        bar.close()
        print(response,response.text)

on_upload(".pio/build/trgb-esp32-s3/firmware.bin", "http://192.168.0.136/update")
