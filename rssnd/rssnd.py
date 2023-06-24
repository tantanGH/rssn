import os
import argparse
import time
import datetime
import signal
import serial
import requests
import feedparser

# API version
API_VERSION = "0.1"

# response codes
RESPONSE_OK                     = 200
RESPONSE_BAD_REQUEST            = 400
RESPONSE_NOT_FOUND              = 404
RESPONSE_INTERNAL_SERVER_ERROR  = 500
RESPONSE_SERVICE_UNAVAILABLE    = 503

# horizontal bar (DSHELLの改区に対応するコードで作る)
HORIZONTAL_BAR = bytes([0x84, 0xaa]).decode('cp932') * 47

# abort flag
g_abort_service = False

# sigint handler
def sigint_handler(signum, frame):
  print("CTRL-C is pressed. Stopping the service.")
  global g_abort_service
  g_abort_service = True

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

    global g_abort_service
    g_abort_service = False

    while g_abort_service is False:

      # find request header prefix '>', '|'
      prefix = 0
      while g_abort_service is False:
        if port.in_waiting < 1:
          time.sleep(0.05)
          continue
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

      # request handler - dshell
      elif request_body_str.startswith("/dshell?link="):

        try:

          # get RSS feed from Internet
          feed_content = requests.get(request_body_str[13:])
          feed = feedparser.parse(feed_content.text)

          res = ""

          # 極端なレスポンスの悪化を避けるため、記事は max_entries で指定された数だけにする(デフォルト100)
          entries = feed.entries[:max_entries]
          for e in entries:

            t = e.title if hasattr(e, 'title') else ""
            s = e.summary if hasattr(e, 'summary') else ""
            tm = time.mktime(e.updated_parsed) + 9 * 3600
            dt = datetime.datetime.fromtimestamp(tm).strftime('%Y-%m-%d %H:%M:%S %a')

            title_sjis_bytes = t.encode('cp932', errors="backslashreplace")

            ofs_bytes = 0
            num_chars = 0

            # タイトルを24dotフォントで表示する場合、折り返しが発生すると表示が重なってしまう。
            # そのためいい感じのところで行を切り、1行空行を間に挟むようにする。
            # この時 SJIS にして桁数を計算した上で、元の UTF8 の文字列を該当位置でカットする必要がある。
            while len(title_sjis_bytes) > 62:

              c = title_sjis_bytes[ ofs_bytes ]
              if (c >= 0x81 and c <= 0x9f) or (c >= 0xe0 and c <= 0xef):
                ofs_bytes += 1

              ofs_bytes += 1
              num_chars += 1
              if ofs_bytes >= 61:
                res += f"\n%V%W{t[:num_chars]}\u0018\n"
                t = t[num_chars:]
                title_sjis_bytes = title_sjis_bytes[ofs_bytes:]
                ofs_bytes = 0
                num_chars = 0

            # 残りのタイトルと日付、本文、横barを出力
            res += f"""
%V%W{t}\u0018


日付：{dt}

{s}

{HORIZONTAL_BAR}
"""

          # 最後の記事の後には [EOF] マーカーを出力しておく
          res += "\n[EOF]\n"

          if verbose:
            print(f"returned {len(entries)} items.")

          respond(port, RESPONSE_OK, res)

        except Exception as e:
          if verbose:
            print(f"unknown request [{request_body_str}]")
          respond(port, RESPONSE_BAD_REQUEST)

      else:
        if verbose:
          print(f"unknown request [{request_body_str}]")
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
