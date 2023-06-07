import argparse
import os
import time
import datetime
import signal
import serial
import feedparser

# API version
API_VERSION = "0.1"

# response codes
RESPONSE_OK                     = 200
RESPONSE_BAD_REQUEST            = 400
RESPONSE_NOT_FOUND              = 404
RESPONSE_INTERNAL_SERVER_ERROR  = 500
RESPONSE_SERVICE_UNAVAILABLE    = 503

# abort flag
abort_service = False

# sigint handler
def sigint_handler(signum, frame):
  print("CTRL-C is pressed. Stopping the service.")
  abort_service = True

# respond
def respond(port, code, body=""):

  code_str = '<|{:04d}'.format(code)

  body_bytes = body.encode('cp932', errors="backslashreplace")
  body_len = len(body_bytes)
  body_len_str = '{:08x}'.format(body_len)

  res = bytearray()
  res.extend(code_str.encode('ascii'))
  res.extend(body_len_str.encode('ascii'))
  res.extend(body_bytes)
  port.write(res)
  port.flush()

# service loop
def run_service(serial_device, serial_baudrate, max_entries, verbose):

  # set signal handler
  signal.signal(signal.SIGINT, sigint_handler)

  # open serial port
  with serial.Serial( serial_device, serial_baudrate,
                      bytesize = serial.EIGHTBITS,
                      parity = serial.PARITY_NONE,
                      stopbits = serial.STOPBITS_ONE,
                      timeout = 120,
                      xonxoff = False,
                      rtscts = False,
                      dsrdtr = False ) as port:

    print(f"Started. (serial_device={serial_device}, serial_baudrate={serial_baudrate})")

    abort_service = False

    while abort_service is False:

      # find request header prefix '>', '|'
      prefix = 0
      while abort_service is False:
        c = port.read()
        if prefix == 0 and c[:1] == b'>':
          prefix = 1
        elif prefix == 1 and c[:1] == b'|':
          prefix = 2
          break
        else:
          prefix = 0

      # aborted?
      if prefix != 2:
        return

      # read request body size (8 byte hex string, 0 padding)
      request_body_size_bytes = port.read(8)
      request_body_size = int(request_body_size_bytes.decode('ascii'), 16)
      if verbose:
        print(f"got request {request_body_size} bytes.")

      # request body
      request_body_bytes = port.read(request_body_size)
      request_body_str = request_body_bytes.decode('ascii')
      if verbose:
        print(f"request: [{request_body_str}]")

      # request handler - version
      if request_body_str.startswith("/version"):
        if verbose:
          print(f"response: [{API_VERSION}]")
        respond(port, RESPONSE_OK, API_VERSION)

      # request handler - items
      elif request_body_str.startswith("/items?link="):

        # get RSS feed from Internet
        feed = feedparser.parse(request_body_str[12:])

        entries = feed.entries[:max_entries]

        res = str(len(entries)) + "\n"
        for e in entries:
          t = time.mktime(e.updated_parsed) + 9 * 3600
          dt = datetime.datetime.fromtimestamp(t).strftime('%Y-%m-%d %H:%M:%S %a')
          s = e.summary if hasattr(e, 'summary') else ""
          #a = e.author if hasattr(e, 'author') else ""
          #res += e.title + "\t" + dt + "\t" + a + "\t" + s + "\n"
          res += e.title + "\t" + dt + "\t" + s + "\n"

        if verbose:
          print(f"returned {len(entries)} items.")

        respond(port, RESPONSE_OK, res)

      else:
        if verbose:
          print(f"unknown request [{body_str}]")
        respond(port, RESPONSE_BAD_REQUEST)

    print("Stopped.")

# main
def main():

    parser = argparse.ArgumentParser()

    parser.add_argument("-d","--device", help="serial device name", default='/dev/serial0')
    parser.add_argument("-s","--baudrate", help="baud rate", type=int, default=19200)
    parser.add_argument("-e","--entries", help="max item entries", type=int, default=100)
    parser.add_argument("-v","--verbose", help="verbose mode", action='store_true', default=False)
 
    args = parser.parse_args()

    run_service(args.device, args.baudrate, args.entries, args.verbose)


if __name__ == "__main__":
    main()
