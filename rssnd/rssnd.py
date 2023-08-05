import os
import argparse
import time
import datetime
import signal
import serial
import requests
import feedparser
import subprocess
#import chromedriver_binary
from bs4 import BeautifulSoup
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

CHROME_BIN = "/usr/bin/chromium-browser"
CHROME_DRIVER = '/usr/lib/chromium-browser/chromedriver'

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

# sigterm handler
def sigterm_handler(signum, frame):
  print("Received SIGTERM. Stopping the service.")
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

# get RSS response
def get_rss_response(url, max_entries):

  res = None

  try:

    # get RSS feed from Internet
    feed_content = requests.get(url)
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

  except Exception as e:
    res = None

  return res

# get Z-Club response
def get_zclub_response(driver, url, max_entries):
  
  try:

    driver.get(url)

    html = driver.page_source.encode('utf-8')
    soup = BeautifulSoup(html, 'html.parser')

    res = f"""

%V%WX68000Z コミュニティサイト


      %CUT:+-ZCLUB.CUT
      %CUT
      %CUT
      %CUT
      %CUT
      %CUT
      %CUT
      %CUT
      %CUT

Z-CLUBガイドラインを制定しました。Webブラウザからご一読お願いします。

{HORIZONTAL_BAR}

"""
      
    for e in soup.find_all('div', attrs={'class':'post-card'}):

      t = e.find('h1').text.strip()
      s = e.find('p').text.strip()

      spans = e.find_all('span')
      dt = spans[2].text.strip()
      author = spans[3].text.strip()
      
      title_sjis_bytes = t.encode('cp932', errors="backslashreplace")

      ofs_bytes = 0
      num_chars = 0

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

      res += f"""
%V%W{t}\u0018


投稿者：{author}
投稿日時：{dt}

{s}

{HORIZONTAL_BAR}
"""

    res += "\n[EOF]\n"

#    driver.close()

  except Exception as e:
    print(e)
    res = None

  return res

# get Yahoo! auction response

def get_yahoo_auction_response(url, max_entries):

  url = 'https://auctions.yahoo.co.jp/category/list/2084039773/?s1=new&o1=d&mod=2&n=50'

  try:

    res = requests.get(url)
    soup = BeautifulSoup(res.text, 'html.parser')

    res = f"""
%V%WYahoo!オークション


       %CUT:+-YAHOOAUC.CUT
       %CUT
       %CUT
       %CUT
       %CUT
       %CUT

すべてのカテゴリ＞コンピュータ＞パソコン＞「X68000」の商品一覧

新着順

{HORIZONTAL_BAR}
"""
  
    for e in soup.find_all('div', attrs={'class':'Product__detail'}):

      t = e.find('h3').text.strip()
      price1 = e.find('span', attrs={'class':'Product__priceValue u-textRed'}).text.strip()
      price2 = e.find('span', attrs={'class':'Product__priceValue'}).text.strip()
      bids = e.find('span', attrs={'class':'Product__bid'}).text.strip()
      remain_time = e.find('span', attrs={'class':'Product__time'}).text.strip()

      title_sjis_bytes = t.encode('cp932', errors='backslashreplace')

      ofs_bytes = 0
      num_chars = 0

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

      res += f"""
%V%W{t}\u0018


現在：{price1}    即決：{price2}
入札：{bids}    残り：{remain_time}

{HORIZONTAL_BAR}
"""

    res += "\n[EOF]\n"

  except Exception as e:
    res = None

  return res

# service loop
def run_service(serial_device, serial_baudrate, max_entries, verbose, pcm_path, alsa_device, use_oled, mcs_wait):

  # set signal handler
  signal.signal(signal.SIGINT, sigint_handler)
  signal.signal(signal.SIGTERM, sigterm_handler)

  # open serial port
  with serial.Serial( serial_device, serial_baudrate,
                      bytesize = serial.EIGHTBITS,
                      parity = serial.PARITY_NONE,
                      stopbits = serial.STOPBITS_ONE,
                      timeout = 120,
                      xonxoff = False,
                      rtscts = False,
                      dsrdtr = False ) as port:

    opts = Options()
    opts.add_argument("--headless=new")
    opts.binary_location = CHROME_BIN

    driver = webdriver.Chrome(CHROME_DRIVER, options=opts)

    print(f"Started. (serial_device={serial_device}, serial_baudrate={serial_baudrate})")

    global g_abort_service
    g_abort_service = False

    s44rasp_proc = None

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
        break

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

      # request handler - datetime with tz
      elif request_body_str.startswith("/datetime?tz="):
        try:
          tz = int(request_body_str[13:])
          dt = datetime.datetime.utcnow() + datetime.timedelta(hours=tz)
          if verbose:
            print(f"response: [{str(dt)}]")
          respond(port, RESPONSE_OK, str(dt))
        except:
          respond(port, RESPONSE_BAD_REQUEST, "")

      # request handler - datetime
      elif request_body_str.startswith("/datetime"):
        try:
          dt = datetime.datetime.utcnow()
          if verbose:
            print(f"response: [{str(dt)}]")
          respond(port, RESPONSE_OK, str(dt))
        except:
          respond(port, RESPONSE_BAD_REQUEST, "")

      # request handler - 16bit PCM existence check with s44rasp
      elif request_body_str.startswith("/pcmhead?path="):

        request_path = request_body_str[14:]
        if ".." in request_path:
          respond(port, RESPONSE_BAD_REQUEST, "")
        else:
          pcm_file_name = pcm_path + "/" + request_path
          if os.path.isfile(pcm_file_name):
            pcm_file_size = os.path.getsize(pcm_file_name)
            respond(port, RESPONSE_OK, f"{pcm_file_size}")
          else:
            respond(port, RESPONSE_NOT_FOUND, "file not found.")

      # request handler - 16bit PCM play with s44rasp
      elif request_body_str.startswith("/pcmplay?path="):

        request_path = request_body_str[14:]
        if ".." in request_path:
          respond(port, RESPONSE_BAD_REQUEST, "")
        else:
          pcm_file_name = pcm_path + "/" + request_path
          if os.path.isfile(pcm_file_name):
            pcm_file_size = os.path.getsize(pcm_file_name)
            respond(port, RESPONSE_OK, f"{pcm_file_size}")
            if s44rasp_proc is not None:
              while s44rasp_proc.poll() is None:
                s44rasp_proc.send_signal(signal.SIGINT)
                time.sleep(0.2)
            if request_path[-4:].lower() == ".mcs":
              time.sleep(mcs_wait / 1000.0)
            if use_oled:
              s44rasp_proc = subprocess.Popen(["s44rasp", "-d", alsa_device, "-o", pcm_file_name], shell=False)
            else:
              s44rasp_proc = subprocess.Popen(["s44rasp", "-d", alsa_device, pcm_file_name], shell=False)
          else:
            respond(port, RESPONSE_NOT_FOUND, "file not found.")

      # request handler - 16bit PCM stop
      elif request_body_str.startswith("/pcmstop"):
        if s44rasp_proc is not None:
          while s44rasp_proc.poll() is None:
            s44rasp_proc.send_signal(signal.SIGINT)
            time.sleep(0.2)
          s44rasp_proc = None
        respond(port, RESPONSE_OK, f"stopped.")

      # request handler - dshell
      elif request_body_str.startswith("/dshell?link="):
        url = request_body_str[13:]
        res = None
        if url.startswith("https://dev.zuiki.com/project-z/community"):
          res = get_zclub_response(driver, url, max_entries)
        elif url.startswith("https://auctions.yahoo.co.jp/"):
          res = get_yahoo_auction_response(url, max_entries)
        else:
          res = get_rss_response(url, max_entries)
        if res:
          respond(port, RESPONSE_OK, res)
        else:
          respond(port, RESPONSE_BAD_REQUEST)

      # unknown request
      else:
        print(f"unknown request [{request_body_str}]")
        respond(port, RESPONSE_BAD_REQUEST)

    driver.quit()

    print("Stopped.")


# main
def main():

    parser = argparse.ArgumentParser()

    parser.add_argument("-d","--device", help="serial device name", default='/dev/serial0')
    parser.add_argument("-s","--baudrate", help="baud rate", type=int, default=19200)
    parser.add_argument("-e","--entries", help="max item entries", type=int, default=100)
    parser.add_argument("-v","--verbose", help="verbose mode", action='store_true', default=False)

    parser.add_argument("-p", "--pcmpath", help="pcm data path", default=".")
    parser.add_argument("-a", "--alsa", help="alsa device name", default="default")
    parser.add_argument("-o", "--oled", help="oled display", action='store_true')
    parser.add_argument("-w", "--mcswait", help="wait msec for mcs data", type=int, default=2290) 

    args = parser.parse_args()

    run_service(args.device, args.baudrate, args.entries, args.verbose, args.pcmpath, args.alsa, args.oled, args.mcswait)


if __name__ == "__main__":
    main()
